"""One live diagnostic request. Never prints credentials or model output."""
import json, pathlib, urllib.request, urllib.error
root = pathlib.Path(__file__).resolve().parent.parent
line = (root / 'deepseek-key.local.txt').read_text(encoding='utf-8-sig').splitlines()[0]
if not line.startswith('DEEPSEEK_API_KEY='):
    raise SystemExit('Key configuration format invalid')
key = line.partition('=')[2].strip()
if not key or key == 'PASTE_YOUR_API_KEY_HERE':
    raise SystemExit('Key not configured')
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
with opener.open('http://127.0.0.1:8080/api/game/state', timeout=5) as r:
    board = json.load(r)['board']
prompt = ('You are the white player (2) in 15x15 Gomoku. Human is black (1), empty is 0. '
          'Rows and columns are zero-based, 0 to 14. Choose an empty cell to win or block. '
          'Respond with JSON only: {\\"row\\": integer, \\"col\\": integer}. Board: ' + json.dumps(board))
payload = {'model':'deepseek-flash','messages':[{'role':'system','content':'You play Gomoku. Output only a valid JSON object with integer row and col.'},{'role':'user','content':prompt}], 'response_format':{'type':'json_object'},'thinking':{'type':'disabled'},'max_tokens':128,'stream':False}
req = urllib.request.Request('https://api.deepseek.com/chat/completions',data=json.dumps(payload).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+key})
try:
    with urllib.request.urlopen(req, timeout=50) as r: data=json.load(r)
    choice=data['choices'][0]; content=choice['message'].get('content') or ''
    print('finish_reason:',choice.get('finish_reason'),'content_length:',len(content))
    try:
        obj=json.loads(content); print('JSON type:',type(obj).__name__)
        if isinstance(obj,dict):
            print('fields:', ','.join(sorted(obj.keys())))
            row,col=obj.get('row'),obj.get('col')
            print('coordinate types:',type(row).__name__,type(col).__name__)
            if type(row) is int and type(col) is int:
                print('in_range:',0<=row<15 and 0<=col<15, 'empty:', 0<=row<15 and 0<=col<15 and board[row][col]==0)
    except (ValueError, TypeError): print('content is not JSON')
except urllib.error.HTTPError as e: print('HTTP status:',e.code)
except Exception as e: print('request error type:', type(e).__name__)
