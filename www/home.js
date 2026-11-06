async function loadLobby(){
  const status=document.getElementById('connection');
  try{
    await window.sessionReady;
    const [stateResponse,historyResponse]=await Promise.all([fetch('/api/game/state',{cache:'no-store'}),fetch('/api/game/history',{cache:'no-store'})]);
    if(!stateResponse.ok||!historyResponse.ok)throw Error('request failed');
    const [state,history]=await Promise.all([stateResponse.json(),historyResponse.json()]);
    status.className='connection online';status.replaceChildren(Object.assign(document.createElement('i')),document.createTextNode('服务已连接'));
    if(state.moves>0 && !state.winner)document.getElementById('game-status').textContent='继续当前棋局 · 已落 '+state.moves+' 手';
    if(state.winner)document.getElementById('game-status').textContent='本局已结束，点击查看或重新开始';
    const count=history.games.length;document.getElementById('history-status').textContent=count?'最近 '+count+' 局 · 查看棋谱':'查看棋谱，重温每一步';
  }catch(error){status.className='connection offline';status.replaceChildren(Object.assign(document.createElement('i')),document.createTextNode('连接未就绪'));}
}
loadLobby();

const accountDialog=document.getElementById('account-dialog'), accountButton=document.getElementById('account-button'), accountForm=document.getElementById('account-form'), accountError=document.getElementById('account-error'), accountLogout=document.getElementById('account-logout');
let accountMode='login', accountName=null;
function accountView(){
  const logged=!!accountName;
  document.getElementById('account-title').textContent=logged?'你好，'+accountName:accountMode==='login'?'登录账号':'创建账号';
  document.querySelector('.account-intro').textContent=logged?'你的棋局和棋谱已关联到账号。':'登录后，棋局和历史棋谱跟随账号保存。';
  accountForm.hidden=logged; accountLogout.hidden=!logged;document.getElementById('account-switch').hidden=logged;
  document.getElementById('account-submit').textContent=accountMode==='login'?'登录':'注册并登录';
  document.getElementById('account-switch').textContent=accountMode==='login'?'没有账号？立即注册':'已有账号？返回登录';
  document.getElementById('account-password').autocomplete=accountMode==='login'?'current-password':'new-password';accountError.textContent='';
}
window.sessionReady.then(data=>{accountName=data.username;accountButton.textContent=accountName?'👤 '+accountName:'登录 / 注册';}).catch(()=>{});
accountButton.onclick=()=>{accountView();accountDialog.showModal()};
document.getElementById('account-close').onclick=()=>accountDialog.close();
accountDialog.addEventListener('click',event=>{if(event.target===accountDialog)accountDialog.close()});
document.getElementById('account-switch').onclick=()=>{accountMode=accountMode==='login'?'register':'login';accountView()};
async function accountRequest(path,body){await window.sessionReady;const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json','X-Requested-With':'Gomoku'},body:JSON.stringify(body),cache:'no-store'});const data=await r.json();if(!r.ok)throw Error(data.error||'请求失败');return data;}
accountForm.onsubmit=async event=>{event.preventDefault();const button=document.getElementById('account-submit');button.disabled=true;accountError.textContent='';try{await accountRequest('/api/auth/'+accountMode,{username:document.getElementById('account-username').value,password:document.getElementById('account-password').value});document.getElementById('account-password').value='';location.reload();}catch(error){accountError.textContent=error.message;}finally{button.disabled=false;}};
accountLogout.onclick=async()=>{accountLogout.disabled=true;try{await accountRequest('/api/auth/logout',{});location.reload();}catch(error){accountError.textContent=error.message;}finally{accountLogout.disabled=false;}};

async function loadLeaderboard(){
  const list=document.getElementById('leaderboard-list');
  try {
    const response=await fetch('/api/leaderboard',{cache:'no-store'});
    if(!response.ok)throw Error('request failed');
    const data=await response.json();list.replaceChildren();
    if(!data.players.length){const empty=document.createElement('li');empty.className='leaderboard-message';empty.textContent='还没有上榜玩家，快来赢下第一局！';list.append(empty);return;}
    data.players.forEach((player,index)=>{
      const row=document.createElement('li');row.className='leaderboard-row';
      const rank=document.createElement('span');rank.className='leaderboard-rank';rank.textContent=String(index+1).padStart(2,'0');
      const name=document.createElement('strong');name.className='leaderboard-name';name.textContent=player.username;
      const wins=document.createElement('span');wins.className='leaderboard-wins';wins.textContent=player.wins+' 胜';
      row.append(rank,name,wins);list.append(row);
    });
  } catch(error) { list.replaceChildren();const message=document.createElement('li');message.className='leaderboard-message';message.textContent='暂时无法加载，请稍后刷新';list.append(message); }
}
loadLeaderboard();

window.addEventListener('online-updated',event=>{document.getElementById('online-count').textContent=event.detail===null?'—':String(event.detail)});
