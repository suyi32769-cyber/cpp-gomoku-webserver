const historyList=document.querySelector('#history-list');
const replayDialog=document.querySelector('#replay-dialog');
const replayBoard=document.querySelector('#replay-board');
const replayCaption=document.querySelector('#replay-caption');
let replayMoves=[], replayIndex=0;

async function refreshHistory(){
  historyList.textContent='正在加载…';
  try{
    const data=await request('/api/game/history'); historyList.replaceChildren();
    if(!data.games.length){historyList.textContent='还没有棋局记录，下一步棋后会自动保存。';return;}
    for(const game of data.games){
      const row=document.createElement('button'); row.type='button'; row.className='history-row';
      const info=document.createElement('span'); const label=document.createElement('strong');
      label.textContent='棋局 #'+game.id;
      const time=document.createElement('small'); const date=new Date(game.updated_at.replace(' ','T')+'Z');
      time.textContent=(Number.isNaN(date.valueOf())?game.updated_at:date.toLocaleString('zh-CN',{hour12:false}))+' · '+game.moves+' 手';
      info.append(label,time); const result=document.createElement('span'); result.className='history-result';
      result.textContent=game.winner==='human'?'你获胜':game.winner==='ai'?'电脑获胜':game.winner==='draw'?'平局':game.status==='abandoned'?'已结束':'进行中';
      row.append(info,result); row.onclick=()=>openReplay(game.id); historyList.append(row);
    }
  }catch(e){historyList.textContent='历史记录加载失败：'+e.message;}
}
async function openReplay(id){
  try{const data=await request('/api/game/replay',{id}); replayMoves=data.moves; replayIndex=0; document.querySelector('#replay-title').textContent='棋局 #'+id; renderReplay(); replayDialog.showModal();}
  catch(e){historyList.textContent='棋谱加载失败：'+e.message;}
}
function renderReplay(){
  const cells=Array.from({length:15},()=>Array(15).fill(0));
  for(const m of replayMoves.slice(0,replayIndex)) cells[m.row][m.col]=m.player;
  replayBoard.replaceChildren();
  for(let r=0;r<15;r++) for(let c=0;c<15;c++){
    const cell=document.createElement('span'); cell.className='cell';
    if(cells[r][c]){const p=document.createElement('span');p.className='stone '+(cells[r][c]===1?'black':'white');cell.append(p);}
    const last=replayMoves[replayIndex-1]; if(last&&last.row===r&&last.col===c)cell.classList.add('last');
    replayBoard.append(cell);
  }
  const move=replayMoves[replayIndex-1];
  replayCaption.textContent=move?`第 ${replayIndex} / ${replayMoves.length} 手 · ${move.player===1?'黑棋':'白棋'}${move.source==='human'?'（玩家）':move.source==='fallback'?'（本地备用）':move.source==='fivechess'?'（FiveChess）':'（DeepSeek）'}`:`开局 · 共 ${replayMoves.length} 手`;
  document.querySelector('#replay-prev').disabled=replayIndex===0; document.querySelector('#replay-next').disabled=replayIndex===replayMoves.length;
}
document.querySelector('#refresh-history').onclick=refreshHistory;
window.addEventListener('game-updated',refreshHistory);
document.querySelector('#replay-close').onclick=()=>replayDialog.close();
document.querySelector('#replay-prev').onclick=()=>{if(replayIndex>0){replayIndex--;renderReplay();}};
document.querySelector('#replay-next').onclick=()=>{if(replayIndex<replayMoves.length){replayIndex++;renderReplay();}};
refreshHistory();
