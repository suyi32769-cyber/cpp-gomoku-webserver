#include "GameStore.h"
#include <stdexcept>
#include <algorithm>
using json=nlohmann::json;

GameStore::GameStore(const std::string &path):db(path,SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE) {
    db.setBusyTimeout(3000);
    db.exec("PRAGMA foreign_keys=ON");
    db.exec("CREATE TABLE IF NOT EXISTS games (id INTEGER PRIMARY KEY, created_at TEXT NOT NULL DEFAULT (datetime('now')), updated_at TEXT NOT NULL DEFAULT (datetime('now')), status TEXT NOT NULL, moves INTEGER NOT NULL, winner TEXT NOT NULL, snapshot TEXT NOT NULL)");
    db.exec("CREATE TABLE IF NOT EXISTS moves (game_id INTEGER NOT NULL REFERENCES games(id), number INTEGER NOT NULL, row INTEGER NOT NULL, col INTEGER NOT NULL, player INTEGER NOT NULL, source TEXT NOT NULL, PRIMARY KEY(game_id,number))");
    db.exec("CREATE TABLE IF NOT EXISTS session_games (owner TEXT NOT NULL, game_id INTEGER NOT NULL UNIQUE REFERENCES games(id), PRIMARY KEY(owner,game_id))");
    db.exec("CREATE INDEX IF NOT EXISTS session_games_owner ON session_games(owner,game_id)");
    db.exec("CREATE TABLE IF NOT EXISTS visitor_state (owner TEXT PRIMARY KEY, current_game INTEGER NOT NULL REFERENCES games(id))");
    db.exec("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, username TEXT NOT NULL COLLATE NOCASE UNIQUE, salt TEXT NOT NULL, password_hash TEXT NOT NULL, created_at TEXT NOT NULL DEFAULT (datetime('now')))");
    db.exec("CREATE TABLE IF NOT EXISTS auth_sessions (token_hash TEXT PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id), expires_at TEXT NOT NULL)");
    db.exec("CREATE TABLE IF NOT EXISTS friendships (user_low INTEGER NOT NULL REFERENCES users(id), user_high INTEGER NOT NULL REFERENCES users(id), requester INTEGER NOT NULL REFERENCES users(id), status TEXT NOT NULL CHECK(status IN ('pending','accepted')), created_at TEXT NOT NULL DEFAULT (datetime('now')), CHECK(user_low<user_high), PRIMARY KEY(user_low,user_high))");
    db.exec("CREATE INDEX IF NOT EXISTS friendships_high ON friendships(user_high,status)");
    db.exec("CREATE TABLE IF NOT EXISTS app_state (id INTEGER PRIMARY KEY CHECK(id=1), current_game INTEGER NOT NULL REFERENCES games(id))");
}
json GameStore::loadCurrent(const std::string &owner) {
    SQLite::Statement q(db,"SELECT g.id,g.snapshot FROM games g JOIN visitor_state a ON a.current_game=g.id WHERE a.owner=?");
    q.bind(1,owner); if(!q.executeStep()) return nullptr;
    auto s=json::parse(q.getColumn(1).getString()); s["game_id"]=q.getColumn(0).getInt64(); return s;
}
void GameStore::startNew(const std::string &owner, const json &snapshot) {
    SQLite::Transaction tx(db);
    SQLite::Statement abandon(db,"UPDATE games SET status='abandoned',updated_at=datetime('now') WHERE id=(SELECT current_game FROM visitor_state WHERE owner=?) AND status='active'"); abandon.bind(1,owner); abandon.exec();
    SQLite::Statement insert(db,"INSERT INTO games(status,moves,winner,snapshot) VALUES('active',0,'',?)"); insert.bind(1,snapshot.dump()); insert.exec(); auto id=db.getLastInsertRowid();
    SQLite::Statement ownership(db,"INSERT INTO session_games(owner,game_id) VALUES(?,?)"); ownership.bind(1,owner); ownership.bind(2,id); ownership.exec();
    SQLite::Statement current(db,"INSERT OR REPLACE INTO visitor_state(owner,current_game) VALUES(?,?)"); current.bind(1,owner); current.bind(2,id); current.exec(); tx.commit();
}
void GameStore::saveMove(const json &snapshot,int row,int col,int player,const std::string &source) {
    SQLite::Transaction tx(db);
    SQLite::Statement move(db,"INSERT INTO moves(game_id,number,row,col,player,source) VALUES(?,?,?,?,?,?)");
    move.bind(1,snapshot.at("game_id").get<int64_t>()); move.bind(2,snapshot.at("moves").get<int>()); move.bind(3,row); move.bind(4,col); move.bind(5,player); move.bind(6,source); move.exec();
    SQLite::Statement update(db,"UPDATE games SET snapshot=?,moves=?,winner=?,status=?,updated_at=datetime('now') WHERE id=?");
    update.bind(1,snapshot.dump()); update.bind(2,snapshot.at("moves").get<int>()); update.bind(3,snapshot.at("winner").get<std::string>()); update.bind(4,snapshot.at("turn")=="finished"?"finished":"active"); update.bind(5,snapshot.at("game_id").get<int64_t>()); update.exec();
    if(db.getChanges()!=1) throw std::runtime_error("game record missing");
    tx.commit();
}
json GameStore::findUser(const std::string &username) {
    SQLite::Statement q(db,"SELECT id,username,salt,password_hash FROM users WHERE username=? COLLATE NOCASE"); q.bind(1,username);
    if(!q.executeStep()) return nullptr;
    return {{"id",q.getColumn(0).getInt64()},{"username",q.getColumn(1).getString()},{"salt",q.getColumn(2).getString()},{"hash",q.getColumn(3).getString()}};
}
bool GameStore::registerUser(const std::string &username,const std::string &salt,const std::string &hash,const std::string &guest,const std::string &tokenHash) {
    SQLite::Transaction tx(db);
    if(!findUser(username).is_null()) return false;
    SQLite::Statement insert(db,"INSERT INTO users(username,salt,password_hash) VALUES(?,?,?)"); insert.bind(1,username);insert.bind(2,salt);insert.bind(3,hash);insert.exec();
    auto id=db.getLastInsertRowid(); std::string owner="u:"+std::to_string(id);
    SQLite::Statement games(db,"UPDATE session_games SET owner=? WHERE owner=?");games.bind(1,owner);games.bind(2,guest);games.exec();
    SQLite::Statement current(db,"UPDATE visitor_state SET owner=? WHERE owner=?");current.bind(1,owner);current.bind(2,guest);current.exec(); if(db.getChanges()!=1) throw std::runtime_error("visitor missing");
    SQLite::Statement auth(db,"INSERT INTO auth_sessions(token_hash,user_id,expires_at) VALUES(?,?,datetime('now','+30 days'))");auth.bind(1,tokenHash);auth.bind(2,id);auth.exec();tx.commit();return true;
}
bool GameStore::loginUser(long long userId,const std::string &guest,const std::string &tokenHash) {
    SQLite::Transaction tx(db); std::string owner="u:"+std::to_string(userId);
    if(guest.compare(0,2,"u:")!=0) {
        SQLite::Statement abandon(db,"UPDATE games SET status='abandoned',updated_at=datetime('now') WHERE id=(SELECT current_game FROM visitor_state WHERE owner=?) AND status='active'"); abandon.bind(1,guest); abandon.exec();
        SQLite::Statement games(db,"UPDATE session_games SET owner=? WHERE owner=?");games.bind(1,owner);games.bind(2,guest);games.exec();
        SQLite::Statement old(db,"DELETE FROM visitor_state WHERE owner=?");old.bind(1,guest);old.exec();
    }
    SQLite::Statement auth(db,"INSERT INTO auth_sessions(token_hash,user_id,expires_at) VALUES(?,?,datetime('now','+30 days'))");auth.bind(1,tokenHash);auth.bind(2,static_cast<int64_t>(userId));auth.exec();tx.commit();return true;
}
json GameStore::authSession(const std::string &tokenHash) {
    SQLite::Statement q(db,"SELECT u.id,u.username FROM auth_sessions s JOIN users u ON u.id=s.user_id WHERE s.token_hash=? AND s.expires_at>datetime('now')");q.bind(1,tokenHash);if(!q.executeStep())return nullptr;
    return {{"id",q.getColumn(0).getInt64()},{"username",q.getColumn(1).getString()}};
}
void GameStore::logoutSession(const std::string &tokenHash) { SQLite::Statement q(db,"DELETE FROM auth_sessions WHERE token_hash=?");q.bind(1,tokenHash);q.exec(); }
bool GameStore::importLegacy(const std::string &owner) {
    SQLite::Transaction tx(db);
    SQLite::Statement previous(db,"SELECT current_game FROM app_state WHERE id=1 AND current_game NOT IN (SELECT game_id FROM session_games)");
    if(!previous.executeStep()) return false;
    auto legacyCurrent=previous.getColumn(0).getInt64();
    SQLite::Statement claim(db,"INSERT INTO session_games(owner,game_id) SELECT ?,id FROM games WHERE id NOT IN (SELECT game_id FROM session_games)"); claim.bind(1,owner); claim.exec();
    SQLite::Statement current(db,"UPDATE visitor_state SET current_game=? WHERE owner=?"); current.bind(1,legacyCurrent); current.bind(2,owner); current.exec();
    tx.commit(); return true;
}
json GameStore::friendList(long long userId) {
    json friends=json::array(), incoming=json::array(), outgoing=json::array();
    SQLite::Statement q(db,"SELECT u.id,u.username,f.status,f.requester FROM friendships f JOIN users u ON u.id=CASE WHEN f.user_low=? THEN f.user_high ELSE f.user_low END WHERE f.user_low=? OR f.user_high=? ORDER BY u.username COLLATE NOCASE LIMIT 500");
    q.bind(1,static_cast<int64_t>(userId));q.bind(2,static_cast<int64_t>(userId));q.bind(3,static_cast<int64_t>(userId));
    while(q.executeStep()) { json item={{"id",q.getColumn(0).getInt64()},{"username",q.getColumn(1).getString()}}; std::string status=q.getColumn(2).getString(); if(status=="accepted") friends.push_back(item); else if(q.getColumn(3).getInt64()==userId) outgoing.push_back(item); else incoming.push_back(item); }
    return {{"friends",friends},{"incoming",incoming},{"outgoing",outgoing}};
}
std::string GameStore::friendAction(long long userId,const std::string &username,const std::string &action) {
    SQLite::Transaction tx(db);
    SQLite::Statement target(db,"SELECT id FROM users WHERE username=? COLLATE NOCASE");target.bind(1,username);if(!target.executeStep()) return "用户不存在";
    auto other=target.getColumn(0).getInt64(); if(other==userId) return "不能添加自己为好友";
    int64_t low=std::min(static_cast<int64_t>(userId),other),high=std::max(static_cast<int64_t>(userId),other);
    SQLite::Statement q(db,"SELECT status,requester FROM friendships WHERE user_low=? AND user_high=?");q.bind(1,low);q.bind(2,high);
    bool exists=q.executeStep();std::string status=exists?q.getColumn(0).getString():"";int64_t requester=exists?q.getColumn(1).getInt64():0;
    if(action=="send") {
        if(exists) return status=="accepted"?"你们已经是好友":requester==userId?"好友申请已发送":"对方向你发来了申请，请先处理";
        SQLite::Statement count(db,"SELECT COUNT(*) FROM friendships WHERE user_low=? OR user_high=?"); count.bind(1,static_cast<int64_t>(userId));count.bind(2,static_cast<int64_t>(userId));count.executeStep();if(count.getColumn(0).getInt()>=500)return "好友及申请数量已达上限";
        SQLite::Statement targetCount(db,"SELECT COUNT(*) FROM friendships WHERE user_low=? OR user_high=?");targetCount.bind(1,other);targetCount.bind(2,other);targetCount.executeStep();if(targetCount.getColumn(0).getInt()>=500)return "对方的好友及申请数量已达上限";
        SQLite::Statement insert(db,"INSERT INTO friendships(user_low,user_high,requester,status) VALUES(?,?,?,'pending')");insert.bind(1,low);insert.bind(2,high);insert.bind(3,static_cast<int64_t>(userId));insert.exec();
    } else if(action=="accept") {
        if(!exists || status!="pending" || requester==userId) return "没有可接受的好友申请";
        SQLite::Statement update(db,"UPDATE friendships SET status='accepted' WHERE user_low=? AND user_high=?");update.bind(1,low);update.bind(2,high);update.exec();
    } else if(action=="reject" || action=="cancel" || action=="remove") {
        bool permitted=exists && ((action=="remove" && status=="accepted") || (action=="reject" && status=="pending" && requester!=userId) || (action=="cancel" && status=="pending" && requester==userId));
        if(!permitted) return "无法执行此操作";
        SQLite::Statement del(db,"DELETE FROM friendships WHERE user_low=? AND user_high=?");del.bind(1,low);del.bind(2,high);del.exec();
    } else return "不支持的操作";
    tx.commit(); return "";
}
json GameStore::leaderboard() {
    json entries=json::array();
    SQLite::Statement q(db,"SELECT u.username, COUNT(*) AS wins FROM users u JOIN session_games sg ON sg.owner='u:' || u.id JOIN games g ON g.id=sg.game_id WHERE g.status='finished' AND g.winner='human' GROUP BY u.id ORDER BY wins DESC, u.username COLLATE NOCASE ASC LIMIT 3");
    while(q.executeStep()) entries.push_back({{"username",q.getColumn(0).getString()},{"wins",q.getColumn(1).getInt()}});
    return entries;
}
json GameStore::history(const std::string &owner) {
    json items=json::array(); SQLite::Statement q(db,"SELECT g.id,g.created_at,g.updated_at,g.status,g.moves,g.winner FROM games g JOIN session_games sg ON sg.game_id=g.id WHERE sg.owner=? AND g.moves>0 ORDER BY g.id DESC LIMIT 30");
    q.bind(1,owner); while(q.executeStep()) items.push_back({{"id",q.getColumn(0).getInt64()},{"created_at",q.getColumn(1).getString()},{"updated_at",q.getColumn(2).getString()},{"status",q.getColumn(3).getString()},{"moves",q.getColumn(4).getInt()},{"winner",q.getColumn(5).getString()}});
    return items;
}
json GameStore::replay(const std::string &owner, long long id) {
    SQLite::Statement exists(db,"SELECT game_id FROM session_games WHERE game_id=? AND owner=?"); exists.bind(1,static_cast<int64_t>(id)); exists.bind(2,owner); if(!exists.executeStep()) return nullptr;
    json moves=json::array(); SQLite::Statement q(db,"SELECT number,row,col,player,source FROM moves WHERE game_id=? ORDER BY number"); q.bind(1,static_cast<int64_t>(id));
    while(q.executeStep()) moves.push_back({{"number",q.getColumn(0).getInt()},{"row",q.getColumn(1).getInt()},{"col",q.getColumn(2).getInt()},{"player",q.getColumn(3).getInt()},{"source",q.getColumn(4).getString()}});
    return {{"id",id},{"moves",moves}};
}
