"""Exercise both colors using an isolated database and a local mock API."""
import http.cookiejar
import json, os, pathlib, socket, subprocess, tempfile, threading, time, urllib.request, urllib.error
from http.server import BaseHTTPRequestHandler, HTTPServer
ROOT=pathlib.Path(__file__).resolve().parent.parent
class Mock(BaseHTTPRequestHandler):
    moves=iter([(14,0),(12,0),(10,0),(8,0),(6,0)])
    def do_POST(self):
        payload=json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        assert "You play black (1)" in payload["messages"][1]["content"]
        row,col=next(self.moves)
        body=json.dumps({"choices":[{"message":{"content":json.dumps({"row":row,"col":col})}}]}).encode()
        self.send_response(200); self.send_header("Content-Length",str(len(body))); self.end_headers(); self.wfile.write(body)
    def log_message(self,*args): pass

def main():
    mock=HTTPServer(("127.0.0.1",0),Mock); threading.Thread(target=mock.serve_forever,daemon=True).start()
    sock=socket.socket(); sock.bind(("127.0.0.1",0)); port=sock.getsockname()[1]; sock.close()
    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
    def req(path,data=None):
        headers={"Host":"127.0.0.1:8080"}; body=None
        if data is not None:
            body=json.dumps(data).encode(); headers.update({"Content-Type":"application/json","X-Requested-With":"Gomoku"})
        request=urllib.request.Request(f"http://127.0.0.1:{port}"+path,body,headers)
        try:
            with opener.open(request,timeout=10) as response: return response.status,json.loads(response.read())
        except urllib.error.HTTPError as e: return e.code,json.loads(e.read())
    with tempfile.TemporaryDirectory(dir=ROOT/"tests") as temp:
        env=os.environ.copy(); env.update(DEEPSEEK_API_KEY="test-only",GOMOKU_TEST_PORT=str(port),GOMOKU_DB_PATH=str(pathlib.Path(temp)/"game.db"),GOMOKU_TEST_API_URL=f"http://127.0.0.1:{mock.server_port}/mock",NO_PROXY="127.0.0.1",no_proxy="127.0.0.1")
        def start():
            proc=subprocess.Popen([str(ROOT/"bin/webserver")],cwd=ROOT,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            for _ in range(100):
                if proc.poll() is not None: raise RuntimeError("test server exited")
                try: req("/api/session",{}); return proc
                except OSError: time.sleep(.1)
            raise RuntimeError("test server failed to start")
        proc=start()
        try:
            assert req("/api/game/new",{"human_color":3})[0]==400
            status,s=req("/api/game/new",{"human_color":2}); assert status==200 and s["turn"]=="ai" and s["human_color"]==2
            assert req("/api/game/move",{"row":0,"col":0})[0]==409
            for col in range(5):
                status,s=req("/api/game/ai",{}); assert status==200 and s["turn"]=="human" and s["board"][14-2*col][0]==1
                status,s=req("/api/game/move",{"row":0,"col":col}); assert status==200 and s["board"][0][col]==2
                if col==0:
                    proc.terminate(); proc.wait(timeout=5); proc=start()
                    restored=req("/api/game/state")[1]; assert restored["human_color"]==2 and restored["turn"]=="ai" and restored["moves"]==2
            assert s["winner"]=="human" and s["turn"]=="finished"
            replay=req("/api/game/replay",{"id":s["game_id"]})[1]["moves"]; assert replay[0]["player"]==1 and replay[0]["source"]=="deepseek" and replay[1]["player"]==2 and replay[1]["source"]=="human"
            status,s=req("/api/game/new",{"human_color":1}); assert status==200 and s["turn"]=="human"
            status,s=req("/api/game/move",{"row":7,"col":7}); assert status==200 and s["board"][7][7]==1 and s["turn"]=="ai"
            print("PASS: both colors, AI first, win, replay, invalid selection and restart persistence")
        finally:
            proc.terminate(); proc.wait(timeout=5)
    mock.shutdown()
if __name__=="__main__": main()
