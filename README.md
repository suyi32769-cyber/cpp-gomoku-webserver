# C++ WebServer 五子棋系统

基于 Linux C++ WebServer 扩展的五子棋项目，包含人机对战、用户账号、好友、棋局持久化和运行监控。

## 本机运行

在 Ubuntu 安装依赖后构建：

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev libssl-dev nlohmann-json3-dev libsqlitecpp-dev libsqlite3-dev
bash setup-linux.sh
bash start-linux.sh
```


## 测试

构建后可运行 `python3 tests/test_accounts.py`、`python3 tests/test_friends.py` 与 `python3 tests/test_fivechess.py`。

## 来源与许可

本地 AI 集成 [FiveChess](https://github.com/kimlongli/FiveChess)，许可见 `third_party/FiveChess/LICENSE.txt`。

项目含公开访问保护功能，但公网部署仍需单独配置 HTTPS、服务守护、备份及安全策略。
