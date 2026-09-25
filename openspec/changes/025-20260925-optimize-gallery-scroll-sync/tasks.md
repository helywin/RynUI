# Tasks

## 1. 规划与基线

- [x] 1.1 建立 proposal、spec、design 与任务，运行可用的 OpenSpec 校验与 `git diff --check`，记录 CLI 限制后以 `docs:` 提交
- [ ] 1.2 增加同步子阶段与工作量 telemetry；Windows MSVC Release 干净构建，五次真实 D3D12 Gallery 滚动建立原始基线，验收后以 `test:` 提交
- [ ] 1.3 根据基线更新 design，选定具体重复工作、等价 oracle 与回退路径，校验后以 `docs:` 提交

## 2. 平台通用局部化

- [ ] 2.1 添加离屏进入、分数 phase、clip/resize、失效与顺序回归；正式 Windows MSVC Debug 定向测试通过后以 `test:` 提交
- [ ] 2.2 优化选定同步热点并保留参考路径语义，相关 Debug CTest 通过后以 `refactor:` 提交
- [ ] 2.3 Windows MSVC Release 干净构建并重复五进程真实 D3D12 滚动；记录工作量、阶段与整帧差异，退出码通过后以 `test:` 提交

## 3. 集成验收

- [ ] 3.1 运行完整 CTest、change strict validate、全仓 strict validate、OpenSpec doctor（若 CLI 支持）与 `git diff --check`；记录未通过项与平台边界，满足门槛后以 `test:` 提交
