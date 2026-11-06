// Establish one anonymous session before any game or history requests.
window.sessionReady=fetch('/api/session',{method:'POST',headers:{'Content-Type':'application/json','X-Requested-With':'Gomoku'},body:'{}',cache:'no-store'}).then(async r=>{if(!r.ok){const data=await r.json();throw Error(data.error||'无法创建会话')}return r.json()});

// Lobby and game pages send an anonymous heartbeat while open.
async function updateOnline(){
  try {
    await window.sessionReady;
    const response=await fetch('/api/online',{method:'POST',headers:{'Content-Type':'application/json','X-Requested-With':'Gomoku'},body:'{}',cache:'no-store'});
    if(!response.ok)throw Error('online unavailable');
    const data=await response.json();window.dispatchEvent(new CustomEvent('online-updated',{detail:data.online}));
  } catch(error) { window.dispatchEvent(new CustomEvent('online-updated',{detail:null})); }
}
updateOnline();
setInterval(updateOnline,10000);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)updateOnline()});
