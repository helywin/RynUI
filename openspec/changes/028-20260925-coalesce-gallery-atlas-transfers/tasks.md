# Tasks

## 1. 规划与基线

- [x] 1.1 建立 proposal、spec、design 和任务，引用 027 五进程旧基线，运行可用的 OpenSpec 校验与 `git diff --check`，以 `docs:` 提交

## 2. 平台通用上传合同

- [x] 2.1 实现有界纹理 transfer chunk 和纯布局回归，覆盖 512-byte 对齐、多页、跨块及 buffer/纹理顺序；Windows MSVC Debug 定向 CTest 和真实 D3D12 Gallery 通过后以 `refactor:` 提交

## 3. Windows 真实窗口验收

- [x] 3.1 Windows MSVC Release 完整干净构建，五进程 D3D12 首帧和滚动复测；记录 transfer、CPU、draw 与终态，通过后以 `test:` 提交

## 4. 集成验收

- [ ] 4.1 运行完整 CTest、change strict validate、全仓 strict validate、OpenSpec doctor（若 CLI 支持）与 `git diff --check`；记录未通过项，满足门槛后以 `test:` 提交
