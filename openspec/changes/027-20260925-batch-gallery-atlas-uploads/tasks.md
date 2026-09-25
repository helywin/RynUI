# Tasks

## 1. 规划与首次帧基线

- [x] 1.1 建立 proposal、spec、design 与任务，运行可用的 OpenSpec 校验和 `git diff --check`，以 `docs:` 提交
- [ ] 1.2 增加首次帧阶段及上传计数 telemetry；正式 Windows MSVC Release 干净构建，五进程 D3D12 Gallery 建立基线，以 `test:` 提交

## 2. 上传事务

- [ ] 2.1 实现 atlas 全页恢复与有序纹理/buffer 共享 copy pass，增加多页、row pitch 和提交失败恢复回归；Windows MSVC Debug 定向测试与真实 D3D12 Gallery 通过后以 `refactor:` 提交

## 3. Windows 真实窗口验收

- [ ] 3.1 Windows MSVC Release 干净构建及五进程 D3D12 首次帧与滚动复测；记录工作量、CPU 与终态，通过后以 `test:` 提交

## 4. 集成验收

- [ ] 4.1 运行完整 CTest、change strict validate、全仓 strict validate、OpenSpec doctor（若 CLI 支持）与 `git diff --check`；记录未通过项，满足门槛后以 `test:` 提交
