"""Run the real C++ AI client against a local mock, without credentials or fees."""
import json, os, subprocess, sys, threading
from http.server import BaseHTTPRequestHandler, HTTPServer

class Mock(BaseHTTPRequestHandler):
    replies=[]
    def do_POST(self):
        data=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        assert data['model']=='deepseek-flash'
        reply=self.replies.pop(0)
        if reply == 'http_error':
            self.send_response(401); self.end_headers(); return
        body=json.dumps({'choices':[{'message':{'content':reply}}]}).encode()
        self.send_response(200); self.send_header('Content-Length',str(len(body))); self.end_headers(); self.wfile.write(body)
    def log_message(self,*args): pass

def main(binary):
    mock=HTTPServer(('127.0.0.1',0),Mock); threading.Thread(target=mock.serve_forever,daemon=True).start()
    env=os.environ.copy(); env.update(DEEPSEEK_API_KEY='test-only',GOMOKU_TEST_API_URL=f'http://127.0.0.1:{mock.server_port}/chat/completions',NO_PROXY='127.0.0.1',no_proxy='127.0.0.1')
    board=[[0]*15 for _ in range(15)]; board[7][7]=1
    def run(replies):
        Mock.replies=list(replies)
        p=subprocess.run([binary],input=json.dumps(board),text=True,capture_output=True,env=env,check=True,timeout=15)
        return json.loads(p.stdout)
    try:
        x=run(['{"move_id":0}']); assert x['ok'] and not x['fallback'] and board[x['row']][x['col']]==0
        x=run(['{"row":7,"col":7}', '{"move_id":1}']); assert x['ok'] and not x['fallback'] and board[x['row']][x['col']]==0
        x=run(['{"row":7,"col":7}', '{"move_id":9999}']); assert x['ok'] and x['fallback'] and board[x['row']][x['col']]==0
        x=run(['http_error']); assert not x['ok'] and '401' in x['error'] and not x['fallback']
        print('PASS: candidate ID, invalid occupied cell retry, fallback, HTTP failure')
    finally: mock.shutdown()
if __name__=='__main__': main(sys.argv[1])
