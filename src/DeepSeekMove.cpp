#include "DeepSeekMove.h"
#include <curl/curl.h>
#include <vector>
#include <utility>
#include <cmath>
#include <cstdlib>
using json = nlohmann::json;
namespace {
constexpr int N=15;
size_t writeResponse(char *ptr,size_t size,size_t count,void *user) { size_t n=size*count; auto *out=static_cast<std::string*>(user); if(out->size()+n>1048576) return 0; out->append(ptr,n); return n; }
bool hypotheticalWin(const json &board,int row,int col,int color) {
    const int dirs[4][2]={{1,0},{0,1},{1,1},{1,-1}};
    for(auto &d:dirs) { int count=1; for(int sign:{-1,1}) { int r=row+sign*d[0],c=col+sign*d[1]; while(r>=0&&r<N&&c>=0&&c<N&&board[r][c]==color) { ++count; r+=sign*d[0]; c+=sign*d[1]; } } if(count>=5) return true; } return false;
}
std::vector<std::pair<int,int>> legalCandidates(const json &board) {
    std::vector<std::pair<int,int>> choices;
    bool occupied=false; for(int r=0;r<N;++r) for(int c=0;c<N;++c) if(board[r][c]!=0) occupied=true;
    for(int r=0;r<N;++r) for(int c=0;c<N;++c) if(board[r][c]==0) {
        bool near=!occupied;
        for(int dr=-2;dr<=2 && !near;++dr) for(int dc=-2;dc<=2;++dc) { int rr=r+dr,cc=c+dc; if(rr>=0&&rr<N&&cc>=0&&cc<N&&board[rr][cc]!=0) { near=true; break; } }
        if(near) choices.emplace_back(r,c);
    }
    return choices;
}
std::pair<int,int> fallbackMove(const json &board,const std::vector<std::pair<int,int>> &choices,int aiColor) {
    int best=-1000000; std::pair<int,int> result=choices.front();
    for(const auto &p:choices) { int r=p.first,c=p.second; int score=14-std::abs(r-7)-std::abs(c-7);
        if(hypotheticalWin(board,r,c,aiColor)) score+=100000; else if(hypotheticalWin(board,r,c,3-aiColor)) score+=50000;
        for(int dr=-1;dr<=1;++dr) for(int dc=-1;dc<=1;++dc) { int rr=r+dr,cc=c+dc; if(rr>=0&&rr<N&&cc>=0&&cc<N&&board[rr][cc]!=0) score+=2; }
        if(score>best) { best=score; result=p; }
    } return result;
}
}
bool askDeepSeek(const json &board, int aiColor, int &row, int &col, std::string &error, bool &fallback) {
    const char *key=std::getenv("DEEPSEEK_API_KEY"); if(!key || !*key) { error="尚未配置 DeepSeek API Key，请在配置文件中填写后重启服务。"; return false; }
    auto choices=legalCandidates(board); if(choices.empty()) { error="棋盘已满，无法落子。"; return false; }
    json candidates=json::array(); for(size_t i=0;i<choices.size();++i) candidates.push_back({{"id",i},{"row",choices[i].first},{"col",choices[i].second}});
    std::string prompt="You play "+std::string(aiColor==1?"black (1), the opponent is white (2)":"white (2), the opponent is black (1)")+", and 0 is empty on a 15x15 Gomoku board. Choose the best move to win or block. You MUST choose an id from the legal candidates list, and output only JSON in this exact format: {\"move_id\": 0}. The coordinate system is row then column, zero-based. Board: "+board.dump()+". Legal candidates: "+candidates.dump();
    std::string endpoint="https://api.deepseek.com/chat/completions";
    const char *test=std::getenv("GOMOKU_TEST_API_URL"); if(test && std::string(test).find("http://127.0.0.1:")==0) endpoint=test;
    for(int attempt=0;attempt<2;++attempt) {
        json payload={{"model","deepseek-flash"},{"messages",json::array({{{"role","system"},{"content","Choose one legal Gomoku move by candidate id. Return JSON only."}},{{"role","user"},{"content",prompt + (attempt ? " Your previous response was invalid. Return a valid listed move_id only." : "")}}})},{"response_format",{{"type","json_object"}}},{"thinking",{{"type","disabled"}}},{"max_tokens",256},{"stream",false}};
        CURL *curl=curl_easy_init(); if(!curl) { error="无法初始化 API 连接。"; return false; }
        std::string response, request=payload.dump(), auth="Authorization: Bearer "+std::string(key);
        curl_slist *headers=nullptr; headers=curl_slist_append(headers,"Content-Type: application/json"); headers=curl_slist_append(headers,auth.c_str());
        curl_easy_setopt(curl,CURLOPT_URL,endpoint.c_str()); curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers); curl_easy_setopt(curl,CURLOPT_POSTFIELDS,request.c_str()); curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,static_cast<long>(request.size())); curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,writeResponse); curl_easy_setopt(curl,CURLOPT_WRITEDATA,&response); curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,10L); curl_easy_setopt(curl,CURLOPT_TIMEOUT,45L); curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L); curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
        CURLcode result=curl_easy_perform(curl); long status=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status); curl_slist_free_all(headers); curl_easy_cleanup(curl);
        if(result!=CURLE_OK) { error="无法连接 DeepSeek，请检查网络后重试。"; return false; }
        if(status!=200) { error="DeepSeek 请求失败（HTTP "+std::to_string(status)+"），请检查密钥、额度或稍后重试。"; return false; }
        try {
            auto outer=json::parse(response); auto answer=json::parse(outer.at("choices").at(0).at("message").at("content").get<std::string>());
            if(answer.contains("move_id") && answer.at("move_id").is_number_integer()) { int id=answer.at("move_id").get<int>(); if(id>=0 && static_cast<size_t>(id)<choices.size()) { row=choices[id].first; col=choices[id].second; fallback=false; return true; } }
            if(answer.contains("row") && answer.contains("col") && answer.at("row").is_number_integer() && answer.at("col").is_number_integer()) { int r=answer.at("row").get<int>(), c=answer.at("col").get<int>(); if(r>=0&&r<N&&c>=0&&c<N&&board.at(r).at(c)==0) { row=r; col=c; fallback=false; return true; } }
        } catch(...) { }
    }
    auto safe=fallbackMove(board,choices,aiColor); row=safe.first; col=safe.second; fallback=true; return true;
}
