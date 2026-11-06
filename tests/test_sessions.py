"""Check isolation with temporary data and test-only server processes."""
import json, os, pathlib, socket, subprocess, tempfile, threading, time, urllib.request, urllib.error
ROOT=pathlib.Path(__file__).resolve().parent.parent
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def main():
    sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
    with tempfile.TemporaryDirectory(dir=ROOT/'tests') as temp:
        env=os.environ.copy();env.pop('DEEPSEEK_API_KEY',None);env.update(GOMOKU_TEST_PORT=str(port),GOMOKU_DB_PATH=str(pathlib.Path(temp)/'test.db'))
        def request(path,data=None,cookie='',host='127.0.0.1:8080',origin=None,custom=True):
            headers={'Host':host};body=None
            if cookie: headers['Cookie']=cookie
            if origin: headers['Origin']=origin
            if data is not None:
                body=json.dumps(data).encode();headers['Content-Type']='application/json'
                if custom:headers['X-Requested-With']='Gomoku'
            req=urllib.request.Request(f'http://127.0.0.1:{port}'+path,body,headers)
            try:
                with opener.open(req,timeout=15) as r:return r.status,r.headers,r.read()
            except urllib.error.HTTPError as e:return e.code,e.headers,e.read()
        def get(path,cookie=''):return request(path,cookie=cookie)
        def post(path,data,cookie=''):return request(path,data,cookie)
        def start(public=False):
            if public:env['GOMOKU_PUBLIC_ORIGIN']='https://game.example.test'
            else:env.pop('GOMOKU_PUBLIC_ORIGIN',None)
            proc=subprocess.Popen([str(ROOT/'bin/webserver')],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            for _ in range(100):
                if proc.poll() is not None:raise RuntimeError('server exited')
                try:request('/api/ping');return proc
                except OSError:time.sleep(.1)
            raise RuntimeError('server failed to start')
        proc=start()
        try:
            assert get('/api/game/state')[0]==401
            code,h,_=post('/api/session',{});assert code==200 and 'HttpOnly' in h['Set-Cookie'] and 'SameSite=Strict' in h['Set-Cookie'];a=h['Set-Cookie'].split(';')[0]
            code,h,_=post('/api/session',{});b=h['Set-Cookie'].split(';')[0];assert a!=b
            assert post('/api/online',{})[0]==401
            assert json.loads(post('/api/online',{},a)[2])=={'online':1,'window_seconds':30}
            assert json.loads(post('/api/online',{},a)[2])['online']==1
            assert json.loads(post('/api/online',{},b)[2])['online']==2
            assert post('/api/game/new',{'opponent':'fivechess'},a)[0]==200
            code,_,body=post('/api/game/move',{'row':7,'col':7},a);sa=json.loads(body);assert code==200 and sa['moves']==1
            code,_,body=get('/api/game/state',b);sb=json.loads(body);assert code==200 and sb['moves']==0 and sb['game_id']!=sa['game_id']
            assert post('/api/game/replay',{'id':sa['game_id']},b)[0]==404
            assert not json.loads(get('/api/game/history',b)[2])['games']
            assert post('/api/game/replay',{'id':sa['game_id']},a)[0]==200
            assert request('/api/game/new',{},a,origin='https://evil.example')[0]==403
            assert request('/api/game/new',{},a,custom=False)[0]==403
            # Concurrent distinct visitors must never share a board or game id.
            results=[]; errors=[]
            def visitor(i):
                try:
                    code,h,_=post('/api/session',{}); assert code==200
                    cookie=h['Set-Cookie'].split(';')[0]
                    code,_,body=post('/api/game/move',{'row':i,'col':i},cookie); state=json.loads(body)
                    assert code==200 and state['board'][i][i]==1 and state['moves']==1
                    results.append((cookie,state['game_id']))
                except BaseException as e: errors.append(e)
            threads=[threading.Thread(target=visitor,args=(i,)) for i in range(8)]
            for t in threads:t.start()
            for t in threads:t.join()
            assert not errors and len(results)==8 and len({gid for _,gid in results})==8
            proc.terminate();proc.wait(timeout=5);proc=start()
            assert json.loads(get('/api/game/state',a)[2])['moves']==1 and json.loads(get('/api/game/state',b)[2])['moves']==0
            proc.terminate();proc.wait(timeout=5);proc=start(public=True)
            host='game.example.test';origin='https://game.example.test'
            code,h,_=request('/api/session',{},host=host,origin=origin);assert code==200 and 'Secure' in h['Set-Cookie'];c=h['Set-Cookie'].split(';')[0]
            assert request('/api/game/new',{'opponent':'deepseek'},c,host,origin)[0]==400
            assert request('/api/monitor',host=host)[0]==404 and request('/monitor.html',host=host)[0]==404
            assert request('/api/game/import-legacy',{},c,host,origin)[0]==404
            assert request('/api/game/state',cookie=c,host=host,origin='https://evil.example')[0]==403
            assert request('/api/game/state',cookie=c,host='evil.example',origin=origin)[0]==403
            print('PASS: sessions, cross-user replay denial, CSRF/origin checks, restart persistence, public mode and monitor protection')
        finally:
            proc.terminate();proc.wait(timeout=5)
if __name__=='__main__':main()
