# Web 故障注入时限与离线归档安装

- Windows 一轮差分 Python 验证为 289 项，280 通过、7 跳过、2 错误。两处错误分别为 Select 故障 worker 在 40 秒退出，以及固定归档安装时的本机连接拒绝。
- Select 负例 worker 复用正式 Web worker 的 120 秒预算，并在超时时终止该 worker 的进程树。Windows、Linux 的固定 Chromium 定向回归各 1/1 通过，覆盖实际 popup 实例、错误 placeholder、额外 wrapper 以及已绑定却未挂载的 slot。
- Windows 的冷缓存固定归档 `BUNDLED` 加显式 `SYSTEM` 测试 1/1 通过。本地 registry 的连接队列按固定归档数配置，继续拒绝未锁定资源。未运行更新后的完整 Python 矩阵。
