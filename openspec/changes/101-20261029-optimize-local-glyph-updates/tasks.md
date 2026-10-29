# Tasks

## 1. 平台通用：等长局部更新

- [ ] 1.1 实现 alias-safe 等长原位替换；正式 MSVC HEADLESS Debug/Release 验证外部/自引用/双向重叠、邻居、dirty、错误输入及准备失败，保存修复前后证据。
- [ ] 1.2 为 1k/10k/100k 背景的固定 4 glyph 更新运行三轮微基准，记录分配/复制字节和耗时；确认无全 store 分配并记录源码、构建配置与结果。

## 2. 平台通用：容量与集成

- [ ] 2.1 衔接 100 的资源上限修复；追加 1,000 glyph、容量内删除/恢复、能力上限、上传失败及 epoch 更新的 MSVC HEADLESS Debug/Release 合同与三轮增长计数通过，保留原始日志。
- [ ] 2.2 main 集成后执行受影响 Text/Input/renderer 合同、format、OpenSpec 1.14 doctor/strict 与 diff check，完成独立审查并记录 TUNE-PERF-001/002 的修复提交及证据；提交推送后核对远端 SHA。
