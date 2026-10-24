# Tasks

## 1. 平台通用 API 与静态布局模型

- [ ] 1.1 新增 typed reactive Skeleton/standalone Props、typed content、比例宽度与组合布局模型；用 MSVC windows-msvc-headless Debug/Release 的 public API 和 model focused 测试验证默认/形态/有限短零恢复，提交可编译示例及 docs/skeleton.md 的 API 与 packing 合同。

## 2. 平台通用 Theme、retained host 与 scene

- [ ] 2.1 实现 Skeleton 完整 Theme 链及最终派生验证，保持旧 31 IIFE 原文顺序和旧 typed goldens；用新 binary 生成五份 goldens并深比较父 580a8c3，Theme focused 测试验证 inactive seed、继承、dirty、identity 和父后代原子拒绝恢复。
- [ ] 2.2 接入独立 retained host/窗口服务、实际 logical scene quads、有限分配与 content clip；实现 loading 身份恢复、空内容隐藏布局、无占位交互及 idle；在同一正式 preset Debug/Release 的 component focused 测试中验证真实 scene、三 Theme、slot 次数并记录证据。

## 3. 平台通用输入组合与最终集成

- [ ] 3.1 增加真实 Input/Button/Select、controlled open 抑制、preedit/refs/draft、同步卸载重建、其他 owner block、Modal/Spin/隐藏动画组合验收；focused Debug/Release 测试通过，完善文档 C++ 可编译例与本 change 实际证据。
- [ ] 3.2 运行一次去重 affected union（Skeleton、Theme/runtime/scope、Spin/Collapse/Modal、相关输入/Select、scene/Core/backend guards），保留 fresh goldens 深比较证据；运行 clang-format 22 check、OpenSpec doctor/strict 与 git diff --check，记录精确命令、结果和未覆盖平台边界后本地提交。
