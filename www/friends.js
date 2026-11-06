const friendsDialog=document.getElementById('friends-dialog');
const friendsNotice=document.getElementById('friends-notice');
const friendsBadge=document.getElementById('friends-badge');
let friendsLoggedIn=false, friendsBusy=false;
async function friendsRequest(path,body){
  await window.sessionReady;
  const options=body===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json','X-Requested-With':'Gomoku'},body:JSON.stringify(body)};
  const response=await fetch(path,{...options,cache:'no-store'});const data=await response.json();
  if(!response.ok)throw Error(data.error||'请求失败');return data;
}
function friendsList(id,items,empty,render){
  const list=document.getElementById(id);list.replaceChildren();
  if(!items.length){const row=document.createElement('li');row.className='friends-empty';row.textContent=empty;list.append(row);return;}
  items.forEach(item=>{const row=document.createElement('li');row.className='friends-row';const name=document.createElement('strong');name.textContent=item.username;row.append(name);render(row,item);list.append(row)});
}
function friendButton(label,action,username){const b=document.createElement('button');b.type='button';b.textContent=label;b.className='friends-action';b.onclick=()=>friendAction(action,username);return b;}
async function refreshFriends(showError=false){
  if(!friendsLoggedIn)return;
  try{
    const data=await friendsRequest('/api/friends');
    friendsBadge.hidden=!data.incoming.length;friendsBadge.textContent=data.incoming.length?String(data.incoming.length):'';
    document.getElementById('friends-count').textContent='('+data.friends.length+')';document.getElementById('friends-in-count').textContent='('+data.incoming.length+')';document.getElementById('friends-out-count').textContent='('+data.outgoing.length+')';
    const sortedFriends=[...data.friends].sort((a,b)=>Number(b.online)-Number(a.online)||a.username.localeCompare(b.username));
    document.getElementById('friends-online-summary').textContent='在线 '+data.friends.filter(friend=>friend.online).length+' / '+data.friends.length+' · 最近 30 秒活跃';
    friendsList('friends-list',sortedFriends,'还没有好友，输入用户名发送申请吧。',(row,item)=>{const status=document.createElement('span');status.className='friend-status '+(item.online?'is-online':'');status.textContent=item.online?'● 在线':'○ 离线';row.append(status,friendButton('删除','remove',item.username));});
    friendsList('friends-incoming',data.incoming,'暂无好友申请。',(row,item)=>{row.append(friendButton('同意','accept',item.username),friendButton('拒绝','reject',item.username));});
    friendsList('friends-outgoing',data.outgoing,'暂无待处理申请。',(row,item)=>{row.append(friendButton('撤回','cancel',item.username));});
  }catch(error){if(showError)friendsNotice.textContent=error.message;}
}
async function friendAction(action,username){
  if(friendsBusy)return;
  if(action==='remove' && !confirm('确定删除好友 '+username+' 吗？'))return;
  friendsBusy=true;friendsNotice.textContent='正在处理…';
  try {await friendsRequest('/api/friends/action',{action,username});friendsNotice.textContent=action==='send'?'好友申请已发送。':'操作成功。';await refreshFriends(true);}
  catch(error){friendsNotice.textContent=error.message;}finally{friendsBusy=false;}
}
document.getElementById('friends-open').onclick=async()=>{friendsDialog.showModal();try{const session=await window.sessionReady;friendsLoggedIn=!!session.username;document.getElementById('friends-guest').hidden=friendsLoggedIn;document.getElementById('friends-content').hidden=!friendsLoggedIn;if(friendsLoggedIn)await refreshFriends(true);}catch(error){document.getElementById('friends-guest').hidden=false;}};
document.getElementById('friends-close').onclick=()=>friendsDialog.close();
document.getElementById('friends-refresh').onclick=()=>refreshFriends(true);
friendsDialog.addEventListener('click',e=>{if(e.target===friendsDialog)friendsDialog.close()});
document.getElementById('friends-login').onclick=()=>{friendsDialog.close();document.getElementById('account-button').click()};
document.getElementById('friends-form').onsubmit=e=>{e.preventDefault();friendAction('send',document.getElementById('friends-username').value.trim());};
window.sessionReady.then(session=>{friendsLoggedIn=!!session.username;if(friendsLoggedIn)refreshFriends();}).catch(()=>{});
setInterval(()=>{if(friendsLoggedIn)refreshFriends(friendsDialog.open)},20000);
