#pragma once
#include <nlohmann/json.hpp>
#include <string>
bool askDeepSeek(const nlohmann::json &board, int aiColor, int &row, int &col, std::string &error, bool &fallback);
