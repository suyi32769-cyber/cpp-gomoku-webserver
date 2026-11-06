"""Short localhost benchmark. Uses a test server without the DeepSeek key."""
import datetime
import os
import pathlib
import socket
import subprocess
import time
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / 'benchmarks'
HZ = os.sysconf('SC_CLK_TCK')

def cpu_ticks(pid):
    # The comm field is enclosed in parentheses and may contain spaces.
    raw = pathlib.Path(f'/proc/{pid}/stat').read_text()
    fields = raw[raw.rfind(')') + 2:].split()
    return int(fields[11]), int(fields[12])

def assert_port_free():
    with socket.socket() as s:
        s.settimeout(1)
        if s.connect_ex(('127.0.0.1', 8080)) == 0:
            raise RuntimeError('Port 8080 is already in use; stop the existing server first.')

def main():
    assert_port_free()
    OUT.mkdir(exist_ok=True)
    report = OUT / ('local_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S') + '.txt')
    env = os.environ.copy()
    env.pop('DEEPSEEK_API_KEY', None)
    env.pop('GOMOKU_TEST_API_URL', None)
    env['NO_PROXY'] = env['no_proxy'] = '127.0.0.1,localhost'
    lines = []
    def log(text):
        print(text, flush=True)
        lines.append(text)
    server = subprocess.Popen([str(ROOT / 'bin/webserver')], cwd=ROOT, env=env,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        for _ in range(100):
            if server.poll() is not None:
                raise RuntimeError('Test server exited before it was ready.')
            try:
                with opener.open('http://127.0.0.1:8080/api/ping', timeout=1) as response:
                    assert response.status == 200 and b'pong' in response.read()
                break
            except OSError:
                time.sleep(.1)
        else:
            raise RuntimeError('Test server did not become ready.')
        log('Local Linux loopback benchmark; wrk and server share this WSL instance.')
        log('No DeepSeek calls. Each test: 6 seconds, 1 run, HTTP/1.1, server closes connections.')
        log(f'Available logical CPUs: {os.cpu_count()}, clock ticks/s: {HZ}')
        log(f'Project: {ROOT}; test server PID: {server.pid}')
        for endpoint in ('/api/ping', '/gomoku.html'):
            for connections in (1, 16, 64, 128):
                threads = min(4, connections)
                log(f'\n=== {endpoint} | connections={connections}, wrk threads={threads} ===')
                before = cpu_ticks(server.pid); start = time.monotonic()
                result = subprocess.run(['wrk', '-t', str(threads), '-c', str(connections),
                                         '-d', '6s', '--latency', '--timeout', '3s',
                                         'http://127.0.0.1:8080' + endpoint],
                                        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        timeout=20, env=env)
                elapsed = time.monotonic() - start; after = cpu_ticks(server.pid)
                if result.returncode != 0: raise RuntimeError(result.stdout)
                user = (after[0]-before[0])/HZ; system = (after[1]-before[1])/HZ
                log(result.stdout.rstrip())
                log(f'Server process CPU: {(user+system)/elapsed*100:.1f}% of one logical CPU; '
                    f'user {user:.2f}s, system {system:.2f}s, wall {elapsed:.2f}s')
    finally:
        server.terminate()
        try: server.wait(timeout=5)
        except subprocess.TimeoutExpired: server.kill(); server.wait()
        report.write_text('\n'.join(lines)+'\n', encoding='utf-8')
        print(f'Raw report: {report}', flush=True)

if __name__ == '__main__': main()
