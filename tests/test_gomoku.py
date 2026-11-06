"""End-to-end test with a local fake DeepSeek server; no paid API calls."""
import json, os, pathlib, socket, sqlite3, subprocess, tempfile, threading, time, urllib.error, urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer

ROOT=pathlib.Path(__file__).resolve().parent.parent
urllib.request.install_opener(urllib.request.build_opener(urllib.request.ProxyHandler({})))
class Mock(BaseHTTPRequestHandler):
    moves=iter([(14,0),(14,1),(14,2),(14,3)])
    def do_POST(self):
        assert self.headers['Authorization']=='Bearer test-only-key'
        payload=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        assert payload['model']=='deepseek-flash'
        row,col=next(self.moves)
        out=json.dumps({'choices':[{'message':{'content':json.dumps({'row':row,'col':col})}}]}).encode()
        self.send_response(200); self.send_header('Content-Length',str(len(out))); self.end_headers(); self.wfile.write(out)
    def log_message(self,*args): pass

def req(path, data=None):
    headers={}; body=None
    if data is not None:
        body=json.dumps(data).encode(); headers={'Content-Type':'application/json','X-Requested-With':'Gomoku'}
    r=urllib.request.Request('http://127.0.0.1:8080'+path,body,headers)
    try:
        with urllib.request.urlopen(r,timeout=12) as response: return response.status,json.loads(response.read())
    except urllib.error.HTTPError as e: return e.code,json.loads(e.read())

def main():
    mock=HTTPServer(('127.0.0.1',0),Mock); threading.Thread(target=mock.serve_forever,daemon=True).start()
    env=os.environ.copy(); env.update(DEEPSEEK_API_KEY='test-only-key',GOMOKU_TEST_API_URL=f'http://127.0.0.1:{mock.server_port}/chat/completions',NO_PROXY='127.0.0.1,localhost',no_proxy='127.0.0.1,localhost')
    temp=tempfile.TemporaryDirectory(dir=ROOT/'tests'); env['GOMOKU_DB_PATH']=str(pathlib.Path(temp.name)/'test.sqlite3')
    proc=subprocess.Popen([str(ROOT/'bin/webserver')],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
    try:
        for _ in range(100):
            if proc.poll() is not None: raise RuntimeError(proc.stderr.read().decode())
            try: code,state=req('/api/game/state'); break
            except OSError: time.sleep(.1)
        else: raise AssertionError('server did not start')
        assert code==200 and state['turn']=='human' and state['ai_configured']
        with urllib.request.urlopen('http://127.0.0.1:8080/gomoku.html') as page: assert '五子棋' in page.read().decode()
        assert req('/api/game/new',{})[0]==200
        assert req('/api/game/ai',{})[0]==409
        for col in range(5):
            code,state=req('/api/game/move',{'row':0,'col':col}); assert code==200 and state['board'][0][col]==1
            if col==0: assert req('/api/game/move',{'row':0,'col':0})[0]==409
            if col<4:
                code,state=req('/api/game/ai',{}); assert code==200 and state['board'][14][col]==2 and state['turn']=='human'
        assert state['winner']=='human' and state['turn']=='finished'
        assert req('/api/game/move',{'row':1,'col':1})[0]==409
        history=req('/api/game/history')[1]['games']; assert history[0]['winner']=='human' and history[0]['moves']==9
        replay=req('/api/game/replay',{'id':history[0]['id']})[1]['moves']; assert len(replay)==9 and replay[0]['player']==1 and replay[1]['player']==2
        assert req('/api/game/new',{})[1]['moves']==0
        with sqlite3.connect(env['GOMOKU_DB_PATH']) as db:
            db.execute("CREATE TRIGGER fail_move BEFORE INSERT ON moves BEGIN SELECT RAISE(ABORT, 'test failure'); END")
        assert req('/api/game/move',{'row':7,'col':7})[0]==500
        unchanged=req('/api/game/state')[1]; assert unchanged['moves']==0 and unchanged['board'][7][7]==0 and unchanged['turn']=='human'
        with sqlite3.connect(env['GOMOKU_DB_PATH']) as db:
            db.execute('DROP TRIGGER fail_move')
        raw=socket.create_connection(('127.0.0.1',8080)); raw.sendall(b'POST /api/game/move HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nContent-Type: application/json\r\nX-Requested-With: Gomoku\r\nContent-Length: 20\r\n\r\n{"row":'); time.sleep(.1); raw.sendall(b' 7, "col": 7}'); received=b''
        while True:
            part=raw.recv(4096)
            if not part: break
            received+=part
        raw.close(); assert b'200 OK' in received, received
        proc.terminate(); proc.wait(timeout=5)
        proc=subprocess.Popen([str(ROOT/'bin/webserver')],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
        for _ in range(100):
            if proc.poll() is not None: raise RuntimeError('restart failed')
            try: code,state=req('/api/game/state'); break
            except OSError: time.sleep(.1)
        assert code==200 and state['moves']==1 and state['board'][7][7]==1 and state['turn']=='ai'
        assert len(req('/api/game/history')[1]['games'])==2
        print('PASS: AI mock, rules, history, replay, rollback, reset, fragmented request, restart persistence')
    finally:
        if proc.poll() is None: proc.terminate(); proc.wait(timeout=5)
        mock.shutdown(); temp.cleanup()

if __name__=='__main__': main()
