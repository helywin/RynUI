# Tasks

## 1. 规划与基线

- [x] 1.1 建立 proposal、spec、design、tasks，记录 029 五进程基线与第 240 步诊断，运行 change strict、全仓 strict、可用 doctor 及 `git diff --check`；以 `docs:` 提交

## 2. 平台通用 ordered scene 合同

- [x] 2.1 实现仅在文本宿主同步轮次使用的批次与失败恢复；增加逐条等价、重建次数和取消重试测试；Windows MSVC Debug 定向 CTest 通过后以 `perf:` 提交

## 3. Windows 真实窗口验收

- [ ] 3.1 Windows MSVC Release 干净构建、五进程 D3D12 首帧和 240 步滚动复测、输入验收；记录 CPU、raster、绘制终态及回归后以 `test:` 提交

## 4. 集成验收

- [ ] 4.1 完整 CTest、change strict、全仓 strict、可用 doctor 与 `git diff --check`，记录未通过项；满足门槛后以 `test:` 提交
