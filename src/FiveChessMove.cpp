#include "FiveChessMove.h"
#include "ChessEngine.h"
#include <mutex>
namespace {
std::mutex engineMutex;
std::once_flag initFlag;
bool wins(const int board[15][15], int r, int c, int color) {
    const int dirs[4][2]={{1,0},{0,1},{1,1},{1,-1}};
    for (const auto &d:dirs) { int count=1; for (int sign : {-1,1}) { int x=r+sign*d[0], y=c+sign*d[1]; while(x>=0 && x<15 && y>=0 && y<15 && board[x][y]==color) { ++count; x+=sign*d[0]; y+=sign*d[1]; } } if(count>=5) return true; }
    return false;
}
}
bool chooseFiveChessMove(const nlohmann::json &snapshot, int humanColor, int level, int &row, int &col) {
    int board[15][15];
    for(int r=0;r<15;++r) for(int c=0;c<15;++c) board[r][c]=snapshot[r][c].get<int>();
    // Always take a win or prevent an immediate loss before the positional search.
    for(int color : {3-humanColor,humanColor}) for(int r=0;r<15;++r) for(int c=0;c<15;++c)
        if(!board[r][c] && wins(board,r,c,color)) { row=r; col=c; return true; }
    std::lock_guard<std::mutex> guard(engineMutex);
    std::call_once(initFlag, [] { ChessEngine::beforeStart(); });
    auto move=ChessEngine::chooseSnapshot(board,humanColor,level);
    if(move.x<0 || move.x>=15 || move.y<0 || move.y>=15 || board[move.x][move.y]) return false;
    row=move.x; col=move.y; return true;
}
