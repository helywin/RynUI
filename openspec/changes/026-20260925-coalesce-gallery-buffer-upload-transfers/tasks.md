# Tasks

## 1. 规划与基线

- [x] 1.1 建立 proposal、spec、design 与任务，运行可用的 OpenSpec 校验和 `git diff --check`，以 `docs:` 提交
- [ ] 1.2 增加 renderer transfer 创建/映射及区域 telemetry；正式 Windows MSVC Release 干净构建，五进程真实 D3D12 Gallery 建立基线，以 `test:` 提交

## 2. 平台通用上传合同

- [ ] 2.1 增加多目标、不连续区域、容量边界和批次失败恢复回归；Windows MSVC Debug 定向测试通过后以 `test:` 提交
- [ ] 2.2 实现有界同帧 staging 与完整重传恢复，保持 SDL unmap/编码顺序和目标局部更新；相关 Debug CTest 通过后以 `refactor:` 提交

## 3. Windows 真实窗口验收

- [ ] 3.1 Windows MSVC Release 干净构建及五进程 D3D12 Gallery 滚动复测；记录工作量、CPU 和终态对照，通过后以 `test:` 提交

## 4. 集成验收

- [ ] 4.1 运行完整 CTest、change strict validate、全仓 strict validate、OpenSpec doctor（若 CLI 支持）与 `git diff --check`；记录未通过项与平台边界，满足门槛后以 `test:` 提交
