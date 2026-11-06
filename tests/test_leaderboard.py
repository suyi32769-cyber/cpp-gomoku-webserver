"""Leaderboard counts completed account wins, with a live mock-AI game."""
import json,os,pathlib,socket,sqlite3,subprocess,tempfile,threading,time,urllib.request,urllib.error
from http.server import BaseHTTPRequestHandler,HTTPServer
ROOT=pathlib.Path(__file__).resolve().parent.parent
class Mock(BaseHTTPRequestHandler):
    counter=0
    def do_POST(self):
        length=int(self.headers['Content-Length']);self.rfile.read(length); col=Mock.counter;Mock.counter+=1
        out=json.dumps({'choices':[{'message':{'content':json.dumps({'row':14,'col':col})}}]}).encode()
        self.send_response(200);self.send_header('Content-Length',str(len(out)));self.end_headers();self.wfile.write(out)
    def log_message(self,*args):pass
sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close();opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def req(path,data=None,cookie=''):
    headers={'Host':'127.0.0.1:8080'};body=None
    if cookie:headers['Cookie']=cookie
    if data is not None:body=json.dumps(data).encode();headers.update({'Content-Type':'application/json','X-Requested-With':'Gomoku'})
    try:
        with opener.open(urllib.request.Request(f'http://127.0.0.1:{port}'+path,body,headers),timeout=15) as r:return r.status,r.headers,json.loads(r.read())
    except urllib.error.HTTPError as e:return e.code,e.headers,json.loads(e.read())
with tempfile.TemporaryDirectory(dir=ROOT/'tests') as temp:
    mock=HTTPServer(('127.0.0.1',0),Mock);threading.Thread(target=mock.serve_forever,daemon=True).start()
    path=str(pathlib.Path(temp)/'test.db');env=os.environ.copy();env.pop('GOMOKU_PUBLIC_ORIGIN',None);env.update(GOMOKU_TEST_PORT=str(port),GOMOKU_DB_PATH=path,DEEPSEEK_API_KEY='test-only',GOMOKU_TEST_API_URL=f'http://127.0.0.1:{mock.server_port}/mock',NO_PROXY='127.0.0.1',no_proxy='127.0.0.1')
    proc=subprocess.Popen([str(ROOT/'bin/webserver')],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            if proc.poll() is not None:raise RuntimeError('server exited')
            try:req('/api/ping');break
            except OSError:time.sleep(.1)
        assert req('/api/leaderboard')[2]['players']==[]
        _,h,_=req('/api/session',{});guest=h['Set-Cookie'].split(';')[0]
        code,h,_=req('/api/auth/register',{'username':'Alice','password':'test-password-123'},guest);assert code==200;alice=h['Set-Cookie'].split(';')[0]
        assert req('/api/game/new',{'opponent':'deepseek','human_color':1},alice)[0]==200
        for col in range(5):
            code,_,state=req('/api/game/move',{'row':0,'col':col},alice);assert code==200
            if col<4:assert req('/api/game/ai',{},alice)[0]==200
        assert state['winner']=='human'
        assert req('/api/leaderboard')[2]['players']==[{'username':'Alice','wins':1}]
        with sqlite3.connect(path) as db:
            db.execute("INSERT INTO users(username,salt,password_hash) VALUES('Bob','test','test')");bob=db.execute("SELECT id FROM users WHERE username='Bob'").fetchone()[0]
            for status,winner,owner in [('finished','human','u:'+str(bob)),('finished','human','u:'+str(bob)),('finished','ai','u:'+str(bob)),('abandoned','human','u:'+str(bob)),('finished','human','a'*64)]:
                cur=db.execute('INSERT INTO games(status,moves,winner,snapshot) VALUES(?,?,?,?)',(status,9,winner,'{}'));db.execute('INSERT INTO session_games(owner,game_id) VALUES(?,?)',(owner,cur.lastrowid))
        players=req('/api/leaderboard')[2]['players'];assert players==[{'username':'Bob','wins':2},{'username':'Alice','wins':1}],players
        print('PASS: real completed win, ordering, finished-only scoring and guest exclusion')
    finally:proc.terminate();proc.wait(timeout=5);mock.shutdown()
