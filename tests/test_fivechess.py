"""Exercise local FiveChess without an API key or the user's database."""
import http.cookiejar
import json, os, pathlib, socket, subprocess, tempfile, time, urllib.error, urllib.request
ROOT=pathlib.Path(__file__).resolve().parent.parent
sock=socket.socket(); sock.bind(("127.0.0.1",0)); port=sock.getsockname()[1]; sock.close()
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
def req(path,data=None):
    headers={"Host":"127.0.0.1:8080"}; body=None
    if data is not None:
        body=json.dumps(data).encode(); headers.update({"Content-Type":"application/json","X-Requested-With":"Gomoku"})
    r=urllib.request.Request(f"http://127.0.0.1:{port}"+path,body,headers)
    try:
        with opener.open(r,timeout=30) as response: return response.status,json.loads(response.read())
    except urllib.error.HTTPError as e: return e.code,json.loads(e.read())
def main():
    with tempfile.TemporaryDirectory(dir=ROOT/"tests") as temp:
        env=os.environ.copy(); env.pop("DEEPSEEK_API_KEY",None); env.update(GOMOKU_TEST_PORT=str(port),GOMOKU_DB_PATH=str(pathlib.Path(temp)/"game.db"))
        proc=subprocess.Popen([str(ROOT/"bin/webserver")],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
        try:
            for _ in range(100):
                if proc.poll() is not None: raise RuntimeError(proc.stderr.read().decode())
                try: req("/api/session",{}); break
                except OSError: time.sleep(.1)
            else: raise AssertionError("not started")
            assert req("/api/game/new",{"opponent":"fivechess","level":8})[0]==400
            for human in (1,2):
                for level in (2,3,4):
                    code,state=req("/api/game/new",{"human_color":human,"opponent":"fivechess","level":level}); assert code==200 and not state["ai_configured"]
                    for i in range(3):
                        if state["turn"]=="ai":
                            code,state=req("/api/game/ai",{}); assert code==200 and state["ai_move_source"]=="fivechess" and state["turn"]=="human"
                        empty=next((r,c) for r in range(15) for c in range(15) if state["board"][r][c]==0)
                        code,state=req("/api/game/move",{"row":empty[0],"col":empty[1]}); assert code==200
                    if state["turn"]=="ai":
                        code,state=req("/api/game/ai",{}); assert code==200 and state["ai_move_source"]=="fivechess"
                    replay=req("/api/game/replay",{"id":state["game_id"]})[1]["moves"]
                    assert any(m["source"]=="fivechess" and m["player"]==3-human for m in replay)
            print("PASS: local engine without API key, both colors, three depths, legal moves and replay")
        finally:
            proc.terminate(); proc.wait(timeout=5)
if __name__=="__main__": main()
