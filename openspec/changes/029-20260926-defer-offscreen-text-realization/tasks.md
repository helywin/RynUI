# Tasks

## 1. 规划与基线

- [x] 1.1 建立 proposal、spec、design 和任务，引用 028 五进程旧基线；运行 change strict、全仓 strict、可用的 doctor 与 `git diff --check`，以 `docs:` 提交

## 2. 平台通用文本合同

- [ ] 2.1 增加首次布局离屏延后、内容变化及滚动重新进入的回归，实施布局后可见性判断；Windows MSVC Debug 定向 CTest 通过后以 `perf:` 提交

## 3. Windows 真实窗口验收

- [ ] 3.1 Windows MSVC Release 干净构建与五进程 D3D12 首帧、240 步滚动复测，记录 CPU、纹理上传、滚动终态及输入验收；通过后以 `test:` 提交

## 4. 集成验收

- [ ] 4.1 运行完整 CTest、change strict、全仓 strict、OpenSpec doctor（若 CLI 支持）和 `git diff --check`；记录未通过项，满足门槛后以 `test:` 提交
