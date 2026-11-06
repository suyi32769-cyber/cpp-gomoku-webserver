#pragma once
#include <nlohmann/json.hpp>
bool chooseFiveChessMove(const nlohmann::json &board, int humanColor, int level, int &row, int &col);
