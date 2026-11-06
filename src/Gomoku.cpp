#include "Gomoku.h"
#include "DeepSeekMove.h"
#include "FiveChessMove.h"
#include "GameStore.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <array>
#include <mutex>
#include <cstdlib>
#include <string>
#include <memory>
#include <sys/stat.h>
#include <cerrno>
#include <stdexcept>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <sys/random.h>
#include <cctype>
#include <openssl/evp.h>
#include <openssl/crypto.h>
using json = nlohmann::json;
namespace {
constexpr int N=15;
struct Game {
    std::array<std::array<int,N>,N> board{};
    std::string turn="human", winner="", lastError="", aiMoveSource="", opponent="fivechess";
    bool busy=false; int revision=0, moves=0, humanColor=1, level=3;
    int64_t gameId=0;
    std::mutex lock;
    std::chrono::steady_clock::time_point window=std::chrono::steady_clock::now();
    int requests=0;
};
thread_local Game *currentGame=nullptr;
thread_local std::string currentOwner;
#define game (*currentGame)

std::unique_ptr<GameStore> store;
std::mutex storeMutex, sessionsMutex;
std::unordered_map<std::string,std::shared_ptr<Game>> sessions;
std::atomic<int> aiInFlight{0};
std::mutex onlineMutex;
std::unordered_map<std::string,std::chrono::steady_clock::time_point> onlinePlayers;
struct AiGuard { ~AiGuard() { --aiInFlight; } };
bool publicMode() { const char *v=std::getenv("GOMOKU_PUBLIC_ORIGIN"); return v && *v; }
std::string cookieToken(const HttpRequest &req) {
    std::string cookies=req.get_header("Cookie"), key="gomoku_session="; size_t start=0;
    while(start<cookies.size()) { size_t end=cookies.find(';',start); std::string part=cookies.substr(start,end-start); size_t first=part.find_first_not_of(" \t"); if(first!=std::string::npos) part=part.substr(first); if(part.compare(0,key.size(),key)==0) { std::string token=part.substr(key.size()); if(token.size()!=64 || token.find_first_not_of("0123456789abcdef")!=std::string::npos) return ""; return token; } if(end==std::string::npos) break; start=end+1; } return "";
}
std::string randomToken() { unsigned char bytes[32]; size_t done=0; while(done<sizeof(bytes)) { ssize_t n=getrandom(bytes+done,sizeof(bytes)-done,0); if(n<0 && errno==EINTR) continue; if(n<=0) throw std::runtime_error("random failed"); done+=size_t(n); } static const char hex[]="0123456789abcdef"; std::string token; for(unsigned char b:bytes){ token+=hex[b>>4]; token+=hex[b&15]; } return token; }

std::string hexBytes(const unsigned char *data,size_t n) { static const char h[]="0123456789abcdef"; std::string out; out.reserve(n*2); for(size_t i=0;i<n;++i){out+=h[data[i]>>4];out+=h[data[i]&15];} return out; }
std::string tokenDigest(const std::string &token) { unsigned char out[32]; unsigned int n=0; if(EVP_Digest(token.data(),token.size(),out,&n,EVP_sha256(),nullptr)!=1 || n!=32) throw std::runtime_error("digest error"); return hexBytes(out,n); }
std::string passwordHash(const std::string &password,const std::string &salt) { unsigned char out[32]; if(EVP_PBE_scrypt(password.data(),password.size(),reinterpret_cast<const unsigned char*>(salt.data()),salt.size(),16384,8,5,32*1024*1024,out,sizeof(out))!=1) throw std::runtime_error("scrypt failed"); return hexBytes(out,sizeof(out)); }
std::string ownerForToken(const std::string &token,json *account=nullptr) {
    std::lock_guard<std::mutex> db(storeMutex); auto user=store->authSession(tokenDigest(token));
    if(account) *account=user;
    if(!user.is_null()) return "u:"+std::to_string(user.at("id").get<int64_t>());
    return token;
}
std::string sessionCookie(const std::string &token) { std::string result="gomoku_session="+token+"; Path=/; HttpOnly; SameSite=Strict; Max-Age=2592000"; if(publicMode()) result+="; Secure"; return result; }
std::atomic<int> authInFlight{0};
struct AuthGuard { ~AuthGuard(){--authInFlight;} };
json state() { return {{"game_id",game.gameId},{"human_color",game.humanColor},{"opponent",game.opponent},{"level",game.level},{"board",game.board},{"turn",game.turn},{"winner",game.winner},{"busy",game.busy},{"revision",game.revision},{"moves",game.moves},{"error",game.lastError},{"ai_move_source",game.aiMoveSource},{"deepseek_enabled",!publicMode()},{"ai_configured",std::getenv("DEEPSEEK_API_KEY") && *std::getenv("DEEPSEEK_API_KEY")}}; }
void restore(const json &v) {
    game.humanColor=v.value("human_color",1); game.opponent=v.value("opponent",std::string("deepseek")); game.level=v.value("level",3); game.board=v.at("board").get<decltype(game.board)>(); game.turn=v.at("turn").get<std::string>(); game.winner=v.at("winner").get<std::string>(); game.revision=v.at("revision").get<int>(); game.moves=v.at("moves").get<int>(); game.gameId=v.at("game_id").get<int64_t>(); game.aiMoveSource=v.value("ai_move_source",std::string()); game.lastError.clear(); game.busy=false;
}
void reply(HttpResponse &res, int status, const json &body) { res.set_status_code(status); res.set_header("Content-Type","application/json; charset=utf-8"); res.set_header("Cache-Control","no-store"); res.set_body(body.dump()); }
bool originOK(const HttpRequest &req) {
    std::string host=req.get_header("Host"), origin=req.get_header("Origin");
    if(publicMode()) { std::string expected=std::getenv("GOMOKU_PUBLIC_ORIGIN"); if(expected.compare(0,8,"https://")!=0) return false; std::string authority=expected.substr(8); return !authority.empty() && authority.find('/')==std::string::npos && host==authority && (origin.empty() || origin==expected); }
    if(host!="127.0.0.1:8080" && host!="localhost:8080") return false;
    return origin.empty() || origin=="http://127.0.0.1:8080" || origin=="http://localhost:8080";
}
bool allowed(const HttpRequest &req, HttpResponse &res, const char *method) {
    if(!originOK(req)) { reply(res,403,{{"error","请求来源不受允许"}}); return false; }
    if(req.get_method()!=method) { reply(res,405,{{"error","请求方法不正确"}}); return false; }
    if(std::string(method)=="POST" && (req.get_header("X-Requested-With")!="Gomoku" || req.get_header("Content-Type").find("application/json")!=0)) { reply(res,403,{{"error","请求格式不正确"}}); return false; }
    return true;
}
bool win(int r,int c,int color) {
    const int dirs[4][2]={{1,0},{0,1},{1,1},{1,-1}};
    for(auto &d:dirs) { int count=1; for(int sign:{-1,1}) { int x=r+sign*d[0],y=c+sign*d[1]; while(x>=0&&x<N&&y>=0&&y<N&&game.board[x][y]==color) { ++count; x+=sign*d[0]; y+=sign*d[1]; } } if(count>=5) return true; } return false;
}
void place(int r,int c,int color) {
    game.board[r][c]=color; ++game.moves; ++game.revision;
    if(win(r,c,color)) { game.winner=color==game.humanColor?"human":"ai"; game.turn="finished"; }
    else if(game.moves==N*N) { game.winner="draw"; game.turn="finished"; }
    else game.turn=color==game.humanColor?"ai":"human";
}

}
void registerGomokuRoutes(HttpServer &server) {
    const char *dbPath=std::getenv("GOMOKU_DB_PATH");
    if(!dbPath || !*dbPath) { if(mkdir("data",0700)!=0 && errno!=EEXIST) throw std::runtime_error("无法创建棋局数据目录"); dbPath="data/gomoku.sqlite3"; }
    store.reset(new GameStore(dbPath));
    server.add_route("/api/session",[](const HttpRequest &req,HttpResponse &res){
        if(!allowed(req,res,"POST")) return;
        std::lock_guard<std::mutex> lock(sessionsMutex);
        std::string token=cookieToken(req);
        if(!token.empty()) { json user; auto owner=ownerForToken(token,&user); std::lock_guard<std::mutex> db(storeMutex); if(!store->loadCurrent(owner).is_null()) { reply(res,200,{{"ok",true},{"username",user.is_null()?json(nullptr):user.at("username")}}); return; } }
        static auto window=std::chrono::steady_clock::now(); static int issued=0;
        auto now=std::chrono::steady_clock::now(); if(now-window>std::chrono::minutes(1)) { window=now; issued=0; } if(++issued>60) { reply(res,429,{{"error","创建会话过于频繁，请稍后重试"}}); return; }
        try { token=randomToken(); auto fresh=std::make_shared<Game>(); currentGame=fresh.get(); currentOwner=token; { std::lock_guard<std::mutex> db(storeMutex); store->startNew(token,state()); } res.set_header("Set-Cookie",sessionCookie(token)); reply(res,200,{{"ok",true},{"username",nullptr}}); }
        catch(...) { reply(res,500,{{"error","无法创建访客会话"}}); }
    });
    server.add_route("/api/leaderboard",[](const HttpRequest &req,HttpResponse &res){
        if(!allowed(req,res,"GET")) return;
        try { std::lock_guard<std::mutex> db(storeMutex); reply(res,200,{{"players",store->leaderboard()}}); }
        catch(...) { reply(res,500,{{"error","排行榜加载失败"}}); }
    });
    auto registerSessionRoute=[&server](const std::string &path, HandleFunc handler) {
        server.add_route(path,[handler](const HttpRequest &req,HttpResponse &res){
            if(!originOK(req)) { reply(res,403,{{"error","请求来源不受允许"}}); return; }
            auto token=cookieToken(req); if(token.empty()) { reply(res,401,{{"error","请刷新页面以创建访客会话"}}); return; }
            auto owner=ownerForToken(token);
            std::shared_ptr<Game> visitor;
            { std::lock_guard<std::mutex> guard(sessionsMutex); auto it=sessions.find(owner); if(it!=sessions.end()) visitor=it->second; else {
                if(sessions.size()>=512) { for(auto i=sessions.begin();i!=sessions.end();) { if(i->second.use_count()==1) i=sessions.erase(i); else ++i; } }
                if(sessions.size()>=512) { reply(res,503,{{"error","服务繁忙，请稍后再试"}}); return; }
                json saved; { std::lock_guard<std::mutex> db(storeMutex); saved=store->loadCurrent(owner); } if(saved.is_null()) { reply(res,401,{{"error","会话无效，请刷新页面"}}); return; }
                visitor=std::make_shared<Game>(); currentGame=visitor.get(); restore(saved); sessions.emplace(owner,visitor);
            } }
            currentGame=visitor.get(); currentOwner=owner;
            { std::lock_guard<std::mutex> lock(visitor->lock); auto now=std::chrono::steady_clock::now(); if(now-visitor->window>std::chrono::minutes(1)) { visitor->window=now; visitor->requests=0; } if(++visitor->requests>180) { reply(res,429,{{"error","请求过于频繁，请稍后重试"}}); return; } }
            handler(req,res);
        });
    };
    auto registerAuth=[&](const std::string &path,bool registering){
        registerSessionRoute(path,[registering](const HttpRequest &req,HttpResponse &res){
            if(!allowed(req,res,"POST")) return;
            if(currentOwner.compare(0,2,"u:")==0) { reply(res,409,{{"error","请先退出当前账号"}}); return; }
            std::string username,password;
            try { auto data=json::parse(req.get_body()); username=data.at("username").get<std::string>(); password=data.at("password").get<std::string>(); }
            catch(...) { reply(res,400,{{"error","请输入用户名和密码"}}); return; }
            if(username.size()<3 || username.size()>24 || username.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::string::npos || password.size()<10 || password.size()>128) { reply(res,400,{{"error","用户名需为 3–24 位英文字母、数字或下划线；密码需为 10–128 字节"}}); return; }
            static std::mutex throttle; static auto window=std::chrono::steady_clock::now(); static int attempts=0;
            { std::lock_guard<std::mutex> lock(throttle); auto now=std::chrono::steady_clock::now(); if(now-window>std::chrono::minutes(1)){window=now;attempts=0;} if(++attempts>30){reply(res,429,{{"error","登录请求过于频繁，请稍后重试"}});return;} }
            if(authInFlight.fetch_add(1)>=1) { --authInFlight; reply(res,503,{{"error","验证繁忙，请稍后重试"}});return; } AuthGuard authGuard;
            try {
                json user; { std::lock_guard<std::mutex> db(storeMutex); user=store->findUser(username); }
                std::string salt=registering?randomToken().substr(0,32):(user.is_null()?std::string(32,'0'):user.at("salt").get<std::string>());
                std::string hash=passwordHash(password,salt); std::fill(password.begin(),password.end(),'\0');
                if(!registering && (user.is_null() || user.at("hash").get<std::string>().size()!=hash.size() || CRYPTO_memcmp(user.at("hash").get<std::string>().data(),hash.data(),hash.size())!=0)) { reply(res,401,{{"error","用户名或密码错误"}}); return; }
                std::string newToken=randomToken(), newHash=tokenDigest(newToken);
                std::lock_guard<std::mutex> lock(sessionsMutex);
                auto it=sessions.find(currentOwner); if(it!=sessions.end() && it->second.use_count()>2) { reply(res,409,{{"error","当前棋局正在处理，请稍后重试"}}); return; }
                bool ok; { std::lock_guard<std::mutex> db(storeMutex); ok=registering?store->registerUser(username,salt,hash,currentOwner,newHash):store->loginUser(user.at("id").get<int64_t>(),currentOwner,newHash); }
                if(!ok) { reply(res,409,{{"error","用户名已被使用"}});return; }
                sessions.erase(currentOwner); { std::lock_guard<std::mutex> online(onlineMutex); onlinePlayers.erase(currentOwner); } res.set_header("Set-Cookie",sessionCookie(newToken)); reply(res,200,{{"ok",true},{"username",registering?username:user.at("username").get<std::string>()}});
            } catch(...) { reply(res,500,{{"error","账号操作失败，请稍后重试"}}); }
        });
    };
    registerAuth("/api/auth/register",true); registerAuth("/api/auth/login",false);
    registerSessionRoute("/api/auth/logout",[](const HttpRequest &req,HttpResponse &res){
        if(!allowed(req,res,"POST")) return;
        auto token=cookieToken(req); { std::lock_guard<std::mutex> db(storeMutex); store->logoutSession(tokenDigest(token)); }
        std::string cookie="gomoku_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0";if(publicMode())cookie+="; Secure";res.set_header("Set-Cookie",cookie);reply(res,200,{{"ok",true}});
    });
    registerSessionRoute("/api/online",[](const HttpRequest &req,HttpResponse &res){
        if(!allowed(req,res,"POST")) return;
        std::lock_guard<std::mutex> lock(onlineMutex);
        auto now=std::chrono::steady_clock::now();
        for(auto i=onlinePlayers.begin();i!=onlinePlayers.end();) {
            if(now-i->second>std::chrono::seconds(30)) i=onlinePlayers.erase(i); else ++i;
        }
        onlinePlayers[currentOwner]=now;
        reply(res,200,{{"online",onlinePlayers.size()},{"window_seconds",30}});
    });
    registerSessionRoute("/api/friends",[](const HttpRequest &req,HttpResponse &res){
        if(!allowed(req,res,"GET")) return;
        if(currentOwner.compare(0,2,"u:")!=0) { reply(res,401,{{"error","请先登录账号"}});return; }
        try {
            auto userId=std::stoll(currentOwner.substr(2));json result;
            { std::lock_guard<std::mutex> db(storeMutex);result=store->friendList(userId); }
            { std::lock_guard<std::mutex> lock(onlineMutex);auto now=std::chrono::steady_clock::now();
              for(auto &item:result["friends"]) { auto it=onlinePlayers.find("u:"+std::to_string(item["id"].get<int64_t>()));item["online"]=it!=onlinePlayers.end() && now-it->second<=std::chrono::seconds(30);item.erase("id"); } }
            for(auto &item:result["incoming"]) item.erase("id");
            for(auto &item:result["outgoing"]) item.erase("id");
            reply(res,200,result);
        } catch(...) { reply(res,500,{{"error","好友列表加载失败"}}); }
    });
    registerSessionRoute("/api/friends/action",[](const HttpRequest &req,HttpResponse &res){
        if(!allowed(req,res,"POST")) return;
        if(currentOwner.compare(0,2,"u:")!=0) { reply(res,401,{{"error","请先登录账号"}});return; }
        std::string username,action;
        try { auto data=json::parse(req.get_body());username=data.at("username").get<std::string>();action=data.at("action").get<std::string>(); }
        catch(...) { reply(res,400,{{"error","好友操作格式不正确"}});return; }
        if(username.size()<3 || username.size()>24 || username.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::string::npos || (action!="send" && action!="accept" && action!="reject" && action!="cancel" && action!="remove")) { reply(res,400,{{"error","用户名或操作无效"}});return; }
        try { std::string error;{ std::lock_guard<std::mutex> db(storeMutex);error=store->friendAction(std::stoll(currentOwner.substr(2)),username,action); }if(!error.empty()) { reply(res,409,{{"error",error}});return; }reply(res,200,{{"ok",true}}); }
        catch(...) { reply(res,500,{{"error","好友操作失败，请稍后再试"}}); }
    });
    registerSessionRoute("/api/game/state",[](const HttpRequest &req,HttpResponse &res){ if(!allowed(req,res,"GET")) return; std::lock_guard<std::mutex> guard(game.lock); reply(res,200,state()); });
    registerSessionRoute("/api/game/new",[](const HttpRequest &req,HttpResponse &res){ if(!allowed(req,res,"POST")) return; int humanColor=1, level=3; std::string opponent=publicMode()?"fivechess":"deepseek"; try { auto v=json::parse(req.get_body()); if(v.contains("opponent")) { opponent=v.at("opponent").get<std::string>(); if(publicMode() && opponent=="deepseek") throw std::runtime_error("disabled"); if(opponent!="fivechess" && opponent!="deepseek") throw std::runtime_error("invalid"); } if(v.contains("level")) { if(!v.at("level").is_number_integer()) throw std::runtime_error("invalid"); level=v.at("level").get<int>(); if(level<2 || level>4) throw std::runtime_error("invalid"); } if(v.contains("human_color")) { if(!v.at("human_color").is_number_integer()) throw std::runtime_error("invalid"); humanColor=v.at("human_color").get<int>(); if(humanColor!=1 && humanColor!=2) throw std::runtime_error("invalid"); } } catch(...) { reply(res,400,{{"error","对手、难度或棋子颜色无效，或该对手暂不可用"}}); return; } std::lock_guard<std::mutex> guard(game.lock); json next=state(); next["human_color"]=humanColor; next["opponent"]=opponent; next["level"]=level; next["board"]=std::array<std::array<int,N>,N>{}; next["turn"]=humanColor==1?"human":"ai"; next["winner"]=""; next["busy"]=false; next["error"]=""; next["ai_move_source"]=""; next["moves"]=0; next["revision"]=game.revision+1; try { { std::lock_guard<std::mutex> db(storeMutex); store->startNew(currentOwner,next); restore(store->loadCurrent(currentOwner)); } reply(res,200,state()); } catch(...) { reply(res,500,{{"error","无法保存新棋局，请检查数据目录"}}); } });
    registerSessionRoute("/api/game/move",[](const HttpRequest &req,HttpResponse &res){ if(!allowed(req,res,"POST")) return; int r,c; try { auto v=json::parse(req.get_body()); if(!v.at("row").is_number_integer() || !v.at("col").is_number_integer()) throw std::runtime_error("invalid"); r=v.at("row").get<int>(); c=v.at("col").get<int>(); } catch(...) { reply(res,400,{{"error","落点格式不正确"}}); return; } std::lock_guard<std::mutex> guard(game.lock); if(game.turn!="human" || game.busy || r<0 || r>=N || c<0 || c>=N || game.board[r][c]!=0) { reply(res,409,{{"error","当前不能在这里落子"}}); return; } json before=state(); game.lastError=""; place(r,c,game.humanColor); try { { std::lock_guard<std::mutex> db(storeMutex); store->saveMove(state(),r,c,game.humanColor,"human"); } reply(res,200,state()); } catch(...) { restore(before); reply(res,500,{{"error","落子保存失败，请检查数据目录"}}); } });
    registerSessionRoute("/api/game/ai",[](const HttpRequest &req,HttpResponse &res){ if(!allowed(req,res,"POST")) return; int previous=aiInFlight.fetch_add(1); if(previous>=1) { --aiInFlight; reply(res,503,{{"error","电脑正在处理其他对局，请稍后重试"}}); return; } AiGuard aiGuard; json snapshot; int revision, aiColor, level; std::string opponent; { std::lock_guard<std::mutex> guard(game.lock); if(game.turn!="ai" || game.busy) { reply(res,409,{{"error","现在不是电脑的回合或电脑正在思考"}}); return; } if(game.opponent=="deepseek" && publicMode()) { reply(res,403,{{"error","公开体验暂不开放 DeepSeek"}}); return; } if(game.opponent=="deepseek" && (!std::getenv("DEEPSEEK_API_KEY") || !*std::getenv("DEEPSEEK_API_KEY"))) { reply(res,503,{{"error","尚未配置 DeepSeek API Key，请在配置文件中填写后重启服务。"}}); return; } game.busy=true; game.lastError=""; revision=game.revision; aiColor=3-game.humanColor; opponent=game.opponent; level=game.level; snapshot=game.board; }
        int row=-1,col=-1; std::string error; bool fallback=false; bool success=opponent=="fivechess" ? chooseFiveChessMove(snapshot,3-aiColor,level,row,col) : askDeepSeek(snapshot,aiColor,row,col,error,fallback); if(!success && opponent=="fivechess") error="本地棋力引擎未能找到合法落点，请重试。"; std::lock_guard<std::mutex> guard(game.lock); if(game.revision!=revision || !game.busy || game.turn!="ai") { reply(res,409,{{"error","棋局已更改，本次电脑落子已取消"}}); return; } game.busy=false; if(!success) { game.lastError=error; reply(res,502,{{"error",error}}); return; } json before=state(); game.aiMoveSource=opponent=="fivechess"?"fivechess":fallback?"fallback":"deepseek"; place(row,col,aiColor); try { { std::lock_guard<std::mutex> db(storeMutex); store->saveMove(state(),row,col,aiColor,game.aiMoveSource); } reply(res,200,state()); } catch(...) { restore(before); reply(res,500,{{"error","电脑落子保存失败，请检查数据目录"}}); } });
    registerSessionRoute("/api/game/import-legacy",[](const HttpRequest &req,HttpResponse &res){
        if(publicMode()) { reply(res,404,{{"error","未找到"}}); return; }
        if(!allowed(req,res,"POST")) return;
        std::lock_guard<std::mutex> guard(game.lock);
        if(game.busy || game.moves>0) { reply(res,409,{{"error","请在空白棋局中导入旧棋谱"}}); return; }
        try { std::lock_guard<std::mutex> db(storeMutex); if(!store->importLegacy(currentOwner)) { reply(res,404,{{"error","没有可导入的旧版棋谱，或棋谱已导入"}}); return; } restore(store->loadCurrent(currentOwner)); reply(res,200,state()); }
        catch(...) { reply(res,500,{{"error","导入失败，请检查数据目录"}}); }
    });
    registerSessionRoute("/api/game/history",[](const HttpRequest &req,HttpResponse &res){ if(!allowed(req,res,"GET")) return; std::lock_guard<std::mutex> guard(game.lock); try { std::lock_guard<std::mutex> db(storeMutex); reply(res,200,{{"games",store->history(currentOwner)}}); } catch(...) { reply(res,500,{{"error","历史记录读取失败"}}); } });
    registerSessionRoute("/api/game/replay",[](const HttpRequest &req,HttpResponse &res){ if(!allowed(req,res,"POST")) return; int64_t id; try { auto v=json::parse(req.get_body()); if(!v.at("id").is_number_integer()) throw std::runtime_error("invalid"); id=v.at("id").get<int64_t>(); if(id<1) throw std::runtime_error("invalid"); } catch(...) { reply(res,400,{{"error","棋局编号无效"}}); return; } std::lock_guard<std::mutex> guard(game.lock); try { std::lock_guard<std::mutex> db(storeMutex); json result=store->replay(currentOwner,id); if(result.is_null()) reply(res,404,{{"error","棋局不存在"}}); else reply(res,200,result); } catch(...) { reply(res,500,{{"error","棋谱读取失败"}}); } });
}
