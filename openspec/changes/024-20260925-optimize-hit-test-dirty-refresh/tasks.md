# Tasks

## 1. 规划与改造前基线

- [x] 1.1 完成 proposal、spec、design、tasks；运行 OpenSpec doctor、strict validate 与 `git diff --check`，英文格式提交规划
- [x] 1.2 增加固定规模与 seed 的批量命中刷新 benchmark；Windows MSVC Release 运行五个独立进程并记录旧实现结果、工作量和源码 SHA，定向 CTest 通过后英文格式提交
- [ ] 1.3 Windows 真实 D3D12 Gallery `--scroll-acceptance` 运行五个独立进程，记录改造前整帧和各 CPU 阶段、节点/交互规模、后端与未测 GPU 边界，核对每次退出码后英文格式提交证据

## 2. 平台通用刷新优化

- [ ] 2.1 实现小批量线性路径与大批量 generation/epoch slot stamp；补充重复、祖先、无关节点、slot 复用及零分配回归；Windows MSVC Debug 定向 CTest 通过后英文格式提交
- [ ] 2.2 Windows MSVC Release 干净构建并重测相同的五进程定向 benchmark；记录前后时间及工作量，相关 CTest 通过后英文格式提交证据

## 3. Windows 真实窗口验收

- [ ] 3.1 Windows MSVC Release 真实 D3D12 Gallery 滚动重测五个独立进程；记录整帧与阶段 p95、上传/draw、规模、环境及实际差异，确认视觉/交互验收退出码并英文格式提交证据

## 4. 通用集成收口

- [ ] 4.1 运行受影响完整 CTest、OpenSpec doctor、strict validate 与 `git diff --check`；记录未通过项和平台边界、核对任务证据后英文格式提交
