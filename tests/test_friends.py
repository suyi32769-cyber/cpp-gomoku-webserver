"""Friendship workflow and authorization with an isolated database."""
import json,os,pathlib,socket,subprocess,tempfile,time,urllib.request,urllib.error
ROOT=pathlib.Path(__file__).resolve().parent.parent
sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close();opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def req(path,data=None,cookie='',origin=None):
    headers={'Host':'127.0.0.1:8080'};body=None
    if cookie:headers['Cookie']=cookie
    if origin:headers['Origin']=origin
    if data is not None:body=json.dumps(data).encode();headers.update({'Content-Type':'application/json','X-Requested-With':'Gomoku'})
    try:
        with opener.open(urllib.request.Request(f'http://127.0.0.1:{port}'+path,body,headers),timeout=20) as r:return r.status,r.headers,json.loads(r.read())
    except urllib.error.HTTPError as e:return e.code,e.headers,json.loads(e.read())
def cookie(h):return h['Set-Cookie'].split(';')[0]
def action(who,name,verb):return req('/api/friends/action',{'username':name,'action':verb},who)[0]
with tempfile.TemporaryDirectory(dir=ROOT/'tests') as temp:
    env=os.environ.copy();env.pop('GOMOKU_PUBLIC_ORIGIN',None);env.update(GOMOKU_TEST_PORT=str(port),GOMOKU_DB_PATH=str(pathlib.Path(temp)/'test.db'))
    def start():
        proc=subprocess.Popen([str(ROOT/'bin/webserver')],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        for _ in range(100):
            if proc.poll() is not None:raise RuntimeError('server exited')
            try:req('/api/ping');return proc
            except OSError:time.sleep(.1)
        raise RuntimeError('server failed to start')
    proc=start()
    try:
        _,h,_=req('/api/session',{});guest=cookie(h);assert req('/api/friends',cookie=guest)[0]==401
        accounts=[]
        for name in ['Alice','Bob','Charlie']:
            _,h,_=req('/api/session',{}); g=cookie(h)
            code,h,_=req('/api/auth/register',{'username':name,'password':'sample-password-123'},g);assert code==200,(name,code,_);accounts.append(cookie(h))
        a,b,c=accounts
        assert action(a,'Alice','send')==409 and action(a,'Nobody','send')==409
        assert action(a,'Bob','send')==200 and action(a,'Bob','send')==409
        assert action(a,'Bob','accept')==409 and action(c,'Alice','accept')==409
        assert req('/api/friends',cookie=b)[2]['incoming']==[{'username':'Alice'}]
        assert req('/api/friends',cookie=a)[2]['outgoing']==[{'username':'Bob'}]
        assert req('/api/friends/action',{'username':'Alice','action':'accept'},b,'https://evil.example')[0]==403
        assert action(b,'Alice','accept')==200
        assert req('/api/friends',cookie=a)[2]['friends']==[{'username':'Bob','online':False}]
        assert req('/api/online',{},b)[0]==200
        assert req('/api/friends',cookie=a)[2]['friends']==[{'username':'Bob','online':True}]
        assert req('/api/friends',cookie=c)[2]['friends']==[]
        proc.terminate();proc.wait(timeout=5);proc=start()
        assert req('/api/friends',cookie=a)[2]['friends']==[{'username':'Bob','online':False}]
        assert action(b,'Alice','remove')==200
        assert req('/api/friends',cookie=a)[2]['friends']==[]
        assert action(a,'Bob','send')==200 and action(b,'Alice','reject')==200
        assert action(b,'Alice','send')==200 and action(b,'Alice','cancel')==200
        assert req('/api/friends',cookie=a)[2]['incoming']==[]
        print('PASS: friend requests, authorization, both sides, online status, restart, reject, cancel and remove')
    finally:proc.terminate();proc.wait(timeout=5)
