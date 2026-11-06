"""Check actual counters, worker activity and redacted logs using an isolated DB."""
import concurrent.futures
import json
import os
import pathlib
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

ROOT=pathlib.Path(__file__).resolve().parent.parent
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def get(path):
    try:
        with opener.open('http://127.0.0.1:8080'+path,timeout=5) as r: return r.status,r.read()
    except urllib.error.HTTPError as e: return e.code,e.read()
def stats(): return json.loads(get('/api/monitor')[1])

def main():
    with socket.socket() as s:
        if s.connect_ex(('127.0.0.1',8080))==0: raise RuntimeError('Port 8080 already in use; refusing to touch the running server')
    with tempfile.TemporaryDirectory(dir=ROOT/'tests') as tmp:
        env=os.environ.copy(); env.pop('DEEPSEEK_API_KEY',None); env['GOMOKU_DB_PATH']=str(pathlib.Path(tmp)/'test.sqlite3')
        proc=subprocess.Popen([str(ROOT/'bin/webserver')],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
        try:
            for _ in range(100):
                if proc.poll() is not None: raise RuntimeError('server failed to start')
                try: baseline=stats(); break
                except OSError: time.sleep(.1)
            else: raise RuntimeError('server not ready')
            assert baseline['worker_total']==8 and len(baseline['workers'])==8
            assert isinstance(baseline['sample_monotonic_ms'], int) and isinstance(baseline['process_cpu_ms'], int)
            with opener.open('http://127.0.0.1:8080/api/ping',timeout=5) as response:
                assert response.headers['X-Route-Type']=='dynamic' and response.headers['X-Route-Pattern']=='/api/ping'
                assert json.load(response)['message']=='pong!'
            with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
                results=list(pool.map(lambda _:get('/api/ping'),range(12)))
            assert all(status==200 and json.loads(data)['message']=='pong!' for status,data in results)
            assert get('/monitor.html')[0]==200
            assert get('/unknown?test-secret-value')[0]==404
            after=stats()
            assert after['completed']>=baseline['completed']+14
            assert after['dynamic_routes']>=baseline['dynamic_routes']+12
            assert after['static_files']>=baseline['static_files']+1 and after['not_found']>=1
            assert after['epoll_events']>baseline['epoll_events'] and after['accepted']>baseline['accepted']
            assert after['sample_monotonic_ms']>=baseline['sample_monotonic_ms'] and after['process_cpu_ms']>=baseline['process_cpu_ms']
            assert any('/api/ping' in line for line in after['recent_requests'])
            assert any('[ERROR]' in line and '404' in line for line in after['recent_requests'])
            assert all('test-secret-value' not in line for line in after['recent_requests'])
            # A partial HTTP request holds a worker; the monitor request itself holds another.
            with socket.create_connection(('127.0.0.1',8080),timeout=3) as slow:
                slow.sendall(b'GET /api/ping HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n')
                for _ in range(20):
                    time.sleep(.05); sample=stats()
                    if sample['worker_active']>=2: break
                assert sample['worker_active']>=2 and sum(sample['workers'])==sample['worker_active']
                slow.sendall(b'\r\n'); slow.recv(4096)
            with socket.create_connection(('127.0.0.1',8080),timeout=3) as broken:
                broken.sendall(b'GET /api/ping HTTP/1.1\r\n')
            for _ in range(30):
                time.sleep(.05); sample=stats()
                if any('[监控] 请求读取失败' in line for line in sample['recent_requests']): break
            assert any('[ERROR]' in line and '[监控] 请求读取失败' in line for line in sample['recent_requests'])
            print('PASS: counters, CPU samples, route metadata, worker state, errors, bounded redacted log')
        finally:
            if proc.poll() is None: proc.terminate(); proc.wait(timeout=5)

if __name__=='__main__': main()
