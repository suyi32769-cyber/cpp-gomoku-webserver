#pragma once
#include <SQLiteCpp/SQLiteCpp.h>
#include <nlohmann/json.hpp>
#include <string>

// Callers serialize access; SQLiteCpp resources and transactions use RAII.
class GameStore {
public:
    explicit GameStore(const std::string &path);
    nlohmann::json loadCurrent(const std::string &owner);
    void saveMove(const nlohmann::json &snapshot, int row, int col, int player, const std::string &source);
    void startNew(const std::string &owner, const nlohmann::json &snapshot);
    nlohmann::json findUser(const std::string &username);
    bool registerUser(const std::string &username,const std::string &salt,const std::string &hash,const std::string &guest,const std::string &tokenHash);
    bool loginUser(long long userId,const std::string &guest,const std::string &tokenHash);
    nlohmann::json authSession(const std::string &tokenHash);
    void logoutSession(const std::string &tokenHash);
    bool importLegacy(const std::string &owner);
    nlohmann::json friendList(long long userId);
    std::string friendAction(long long userId,const std::string &username,const std::string &action);
    nlohmann::json leaderboard();
    nlohmann::json history(const std::string &owner);
    nlohmann::json replay(const std::string &owner, long long id);
private:
    SQLite::Database db;
};
