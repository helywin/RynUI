# Tasks

## 1. 平台通用规划

- [x] 1.1 基于本地锁定上游与现有窗口源码完成可验收设计，运行 doctor、strict、diff check 后本地提交规划。

## 2. 平台通用实现与验收

本组在 Windows 的 windows-msvc-headless preset（MSVC、Ninja Multi-Config，最多 4 jobs）完成一次平台通用验证。Debug/Release 去重受影响集合；不跑历史全套或 Gallery。headless 不代表 native/GPU/Linux/system IME/AT。

- [x] 2.1 实现 typed API、holder/ref 与共享 notice 生命周期模型及 docs/message-notification.md，通过 API/model 测试验证 key、maxCount、duration、失效引用与文档中的可编译按钮示例；R1 补充回调后重新检查 maxCount/live count 的失败复现与修复。
- [x] 2.2 完整接入 Message/Notification Theme，验证 override/继承/算法/非法值/runtime identity/序列化，新程序生成五份 goldens 并深比较旧值及父 IIFE 原文顺序不变。
- [x] 2.3 实现动态 retained slot、真实 Button/Input、逻辑场景/四角布局/有限裁剪与 tick/deadline，组件测试验证局部替换、草稿/ref 保留、idle 和 clock；R1 补充普通 notice layer 的 surface/content-range ordered scene 填充裁剪失败复现与修复。
- [x] 2.4 完成 callback 同步卸载/同 key 重入/ref 重绑、focused 关闭、Modal/Spin/隐藏/foreign blocks、covered sibling 和 child popup wheel/Escape 组合测试；R1 验证焦点外扩装饰、其他 layer/正文、trim 重入及同帧 popup anchor，Debug/Release 必要去重集合通过。

## 3. 交付

- [x] 3.1 汇总集成验收证据，运行 clang-format 22 check、显式 OpenSpec 1.14 doctor、all strict、git diff --check，阶段本地提交并交付精确 HEAD/分支/path/dirty、实际 checks 与限制。
