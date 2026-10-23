# Tasks

## 1. 规划与合同

- [x] 1.1 核对锁定 6.6.5 Empty/Result 源码，完成 proposal/spec/design/tasks，通过 doctor、严格验证和 diff check 后本地提交。

## 2. 平台通用 API 与 Theme

- [ ] 2.1 添加独立公开 typed API、Empty/Result Theme 默认/暗色/紧凑/继承/override/equality/hash/dirty，加入有意义的 public API/theme 测试；在独立 windows-msvc-headless MSVC Ninja Multi-Config 构建树以最多 4 jobs 构建并通过 Debug/Release 相关测试，从新 executable 生成五份 golden 并证明旧字段除 identity 外保持，再格式检查和本地提交。

## 3. 平台通用 retained 实现

- [ ] 3.1 实现固定 slot 身份、reactive 标签/状态/主题、有限宽度居中换行、短/零约束裁剪与原生 extra 操作；使用实际 node/text/glyph/surface、Button 激活、Input 编辑保持与释放测试，在同一 MSVC headless 构建树通过 Debug/Release 后格式检查和本地提交。

## 4. 平台通用收口

- [ ] 4.1 更新独立组件说明与 073 验收证据，运行 MSVC headless Debug/Release 完整 CTest、format check、doctor、strict validate 和 diff check；记录实际平台、命令、结果与未验证边界后本地提交。

本 change 的完成范围是平台通用静态展示。真实 GPU、Linux、系统 IME 和上游插画像素等价不属于本次验收，不作为未完成 checkbox，也不宣称已验证。仅本地提交，不 fetch/rebase/push、不创建 PR、不 archive。
