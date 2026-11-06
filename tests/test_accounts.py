"""Account tests use an isolated database, local server, and synthetic passwords."""
import json,os,pathlib,socket,sqlite3,subprocess,tempfile,time,urllib.request,urllib.error
ROOT=pathlib.Path(__file__).resolve().parent.parent
sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close();opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def req(path,data=None,cookie=''):
    headers={'Host':'127.0.0.1:8080'};body=None
    if cookie:headers['Cookie']=cookie
    if data is not None:body=json.dumps(data).encode();headers.update({'Content-Type':'application/json','X-Requested-With':'Gomoku'})
    try:
        with opener.open(urllib.request.Request(f'http://127.0.0.1:{port}'+path,body,headers),timeout=20) as r:return r.status,r.headers,json.loads(r.read())
    except urllib.error.HTTPError as e:return e.code,e.headers,json.loads(e.read())
def cookie(h):return h['Set-Cookie'].split(';')[0]
with tempfile.TemporaryDirectory(dir=ROOT/'tests') as temp:
    path=str(pathlib.Path(temp)/'test.db');env=os.environ.copy();env.pop('GOMOKU_PUBLIC_ORIGIN',None);env.update(GOMOKU_TEST_PORT=str(port),GOMOKU_DB_PATH=path)
    proc=subprocess.Popen([str(ROOT/'bin/webserver')],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            if proc.poll() is not None:raise RuntimeError('server exited')
            try:req('/api/session',{});break
            except OSError:time.sleep(.1)
        code,h,_=req('/api/session',{});guest=cookie(h)
        code,_,state=req('/api/game/move',{'row':7,'col':7},guest); assert code==200;game=state['game_id']
        code,h,_=req('/api/auth/register',{'username':'Alice_123','password':'test-password-strong'},guest);assert code==200,(code,_);login=cookie(h);assert login!=guest
        assert req('/api/game/state',cookie=guest)[0]==401
        code,_,state=req('/api/game/state',cookie=login);assert code==200 and state['game_id']==game and state['moves']==1
        assert req('/api/session',{},login)[2]['username']=='Alice_123'
        code,h,_=req('/api/session',{});other=cookie(h)
        assert req('/api/auth/login',{'username':'Alice_123','password':'incorrect-guess'},other)[0]==401
        assert req('/api/auth/register',{'username':'alice_123','password':'test-password-strong'},other)[0]==409
        assert req('/api/game/replay',{'id':game},other)[0]==404
        code,_,guestState=req('/api/game/move',{'row':5,'col':5},other); assert code==200; guestGame=guestState['game_id']
        code,h,_=req('/api/auth/login',{'username':'ALICE_123','password':'test-password-strong'},other);assert code==200,(code,_);second=cookie(h)
        assert req('/api/game/state',cookie=second)[2]['game_id']==game
        assert req('/api/online',{},login)[2]['online']==1
        assert req('/api/online',{},second)[2]['online']==1
        history=req('/api/game/history',cookie=second)[2]['games']; assert any(g['id']==guestGame and g['status']=='abandoned' for g in history)
        assert req('/api/auth/logout',{},login)[0]==200
        assert req('/api/game/state',cookie=login)[0]==401
        assert req('/api/game/state',cookie=second)[0]==200
        code,h,_=req('/api/session',{}); bobGuest=cookie(h)
        code,h,_=req('/api/auth/register',{'username':'Bob_456','password':'another-test-password'},bobGuest); assert code==200; bob=cookie(h)
        assert req('/api/game/replay',{'id':game},bob)[0]==404
        assert req('/api/auth/register',{'username':'Other_789','password':'another-test-password'},second)[0]==409
        proc.terminate();proc.wait(timeout=5)
        proc=subprocess.Popen([str(ROOT/'bin/webserver')],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        for _ in range(100):
            if proc.poll() is not None:raise RuntimeError('restart failed')
            try:req('/api/ping');break
            except OSError:time.sleep(.1)
        else:raise RuntimeError('restart timeout')
        assert req('/api/session',{},second)[2]['username']=='Alice_123'
        assert req('/api/game/state',cookie=second)[2]['game_id']==game
        assert req('/api/game/replay',{'id':game},bob)[0]==404
        with sqlite3.connect(path) as db:
            username,salt,hashed=db.execute("select username,salt,password_hash from users where username='Alice_123'").fetchone()
            assert username=='Alice_123' and len(salt)==32 and len(hashed)==64 and 'test-password-strong' not in hashed
            assert 'test-password-strong' not in pathlib.Path(path).read_bytes().decode('latin1')
        print('PASS: register, login, guest migration, isolation, password hashing, token rotation and logout')
    finally:proc.terminate();proc.wait(timeout=5)
