"""Check a live model move without changing the running game or printing secrets."""
import json, os, pathlib, subprocess, sys, urllib.request
root=pathlib.Path(__file__).resolve().parent.parent
line=(root/'deepseek-key.local.txt').read_text(encoding='utf-8-sig').splitlines()[0]
if not line.startswith('DEEPSEEK_API_KEY='): raise SystemExit('Key configuration format invalid')
key=line.partition('=')[2].strip()
if not key or key=='PASTE_YOUR_API_KEY_HERE': raise SystemExit('Key not configured')
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
with opener.open('http://127.0.0.1:8080/api/game/state',timeout=5) as response: board=json.load(response)['board']
env=os.environ.copy(); env['DEEPSEEK_API_KEY']=key; env.pop('GOMOKU_TEST_API_URL',None)
result=subprocess.run([sys.argv[1]],input=json.dumps(board),text=True,capture_output=True,env=env,timeout=110,check=True)
answer=json.loads(result.stdout)
if not answer['ok']: raise SystemExit('Live request failed: '+answer['error'])
row,col=answer['row'],answer['col']
valid=0<=row<15 and 0<=col<15 and board[row][col]==0
print('Live API result:', 'legal' if valid else 'invalid', '; source:', 'local fallback' if answer['fallback'] else 'DeepSeek')
if not valid: raise SystemExit(1)
