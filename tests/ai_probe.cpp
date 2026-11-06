#include "DeepSeekMove.h"
#include <curl/curl.h>
#include <iostream>
int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    nlohmann::json board; std::cin >> board;
    int row=-1,col=-1; bool fallback=false; std::string error;
    bool ok=askDeepSeek(board,2,row,col,error,fallback);
    std::cout << nlohmann::json({{"ok",ok},{"row",row},{"col",col},{"fallback",fallback},{"error",error}}).dump() << std::endl;
}
