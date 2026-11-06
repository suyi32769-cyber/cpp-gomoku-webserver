"""Verify explicit, local-only one-time legacy migration without user data."""
import json,os,pathlib,socket,sqlite3,subprocess,tempfile,time,urllib.request,urllib.error
ROOT=pathlib.Path(__file__).resolve().parent.parent
sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
with tempfile.TemporaryDirectory(dir=ROOT/'tests') as temp:
    dbpath=str(pathlib.Path(temp)/'test.db')
    board=[[0]*15 for _ in range(15)];board[7][7]=1
    snapshot={'board':board,'turn':'ai','winner':'','revision':1,'moves':1,'game_id':1}
    with sqlite3.connect(dbpath) as db:
        db.executescript("CREATE TABLE games (id INTEGER PRIMARY KEY, created_at TEXT NOT NULL DEFAULT (datetime('now')), updated_at TEXT NOT NULL DEFAULT (datetime('now')), status TEXT NOT NULL, moves INTEGER NOT NULL, winner TEXT NOT NULL, snapshot TEXT NOT NULL); CREATE TABLE moves (game_id INTEGER NOT NULL REFERENCES games(id), number INTEGER NOT NULL, row INTEGER NOT NULL, col INTEGER NOT NULL, player INTEGER NOT NULL, source TEXT NOT NULL, PRIMARY KEY(game_id,number)); CREATE TABLE app_state (id INTEGER PRIMARY KEY CHECK(id=1), current_game INTEGER NOT NULL REFERENCES games(id));")
        db.execute('INSERT INTO games(id,status,moves,winner,snapshot) VALUES(1,?,?,?,?)',('active',1,'',json.dumps(snapshot)));db.execute("INSERT INTO moves VALUES(1,1,7,7,1,'human')");db.execute('INSERT INTO app_state VALUES(1,1)')
    env=os.environ.copy();env.pop('GOMOKU_PUBLIC_ORIGIN',None);env.update(GOMOKU_TEST_PORT=str(port),GOMOKU_DB_PATH=dbpath)
    proc=subprocess.Popen([str(ROOT/'bin/webserver')],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    def req(path,data=None,cookie=''):
        headers={'Host':'127.0.0.1:8080'};body=None
        if data is not None:body=json.dumps(data).encode();headers.update({'Content-Type':'application/json','X-Requested-With':'Gomoku'})
        if cookie:headers['Cookie']=cookie
        try:
            with opener.open(urllib.request.Request(f'http://127.0.0.1:{port}'+path,body,headers),timeout=5) as r:return r.status,r.headers,r.read()
        except urllib.error.HTTPError as e:return e.code,e.headers,e.read()
    try:
        for _ in range(100):
            if proc.poll() is not None:raise RuntimeError('server exited')
            try:req('/api/ping');break
            except OSError:time.sleep(.1)
        _,h,_=req('/api/session',{});a=h['Set-Cookie'].split(';')[0]
        _,h,_=req('/api/session',{});b=h['Set-Cookie'].split(';')[0]
        assert req('/api/game/replay',{'id':1},b)[0]==404
        code,_,body=req('/api/game/import-legacy',{},a);state=json.loads(body)
        assert code==200 and state['game_id']==1 and state['board'][7][7]==1 and state['moves']==1
        assert req('/api/game/replay',{'id':1},a)[0]==200 and req('/api/game/replay',{'id':1},b)[0]==404
        assert req('/api/game/import-legacy',{},b)[0]==404
        print('PASS: explicit legacy import preserves board and ownership')
    finally:proc.terminate();proc.wait(timeout=5)
