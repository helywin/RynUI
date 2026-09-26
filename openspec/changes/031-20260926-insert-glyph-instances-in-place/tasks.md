# Tasks

## 1. 规划与基线

- [x] 1.1 建立 proposal、spec、design、tasks，引用 030 五进程旧基线及临时分段诊断；运行 change strict、全仓 strict、可用 doctor 和 `git diff --check`，以 `docs:` 提交

## 2. 平台通用插入合同

- [x] 2.1 实现零长度 range 原位插入与重叠源保护，新增顺序、dirty 和容量复用测试；Windows MSVC Debug 定向 CTest 通过后以 `perf:` 提交

## 3. Windows 真实窗口验收

- [x] 3.1 Windows MSVC Release 干净构建、五进程 D3D12 首帧和固定 240 步滚动复测、输入验收；记录 CPU、上传、draw 与终态后以 `test:` 提交

## 4. 集成验收

- [ ] 4.1 运行完整 CTest、change strict、全仓 strict、可用 doctor 与 `git diff --check`；记录未通过项，满足门槛后以 `test:` 提交
