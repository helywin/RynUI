# Tasks

## 1. 规划

- [x] 1.1 核实现有控件/焦点/响应/主题及锁定 6.6.5 Form 源，完成 proposal/spec/design/tasks；运行 doctor、strict all、diff check，创建本地规划提交后直接 apply。

## 2. 平台通用模型与 API

- [x] 2.1 实现 typed field、注册/卸载/保留值、受控 authority、初值 reset、required/custom、快照与 reentrancy；通过官方 windows-msvc-headless 独立 MSVC Debug/Release 构建的 focused model/public API CTest 后本地提交。

## 3. 平台通用 retained 集成

- [x] 3.1 实现 Form/FormItem host、显式控件绑定、反馈/布局、焦点及 Modal/Spin/hidden/lifetime；通过真实 Input/Select/Checkbox/InputNumber、Button submit/reset、TextArea/IME、草稿及回调销毁的 focused component CTest（同一 preset Debug/Release）。
- [x] 3.2 完成 Form Theme override/algorithm/subscription/serialization，保留既有解析块与栈修复；通过 focused Theme CTest、金样更新及除 identity/new Form keys 外的既有值语义比较（同一 preset Debug/Release）。

## 4. 父基线集成

- [x] 4.1 已验证 host 阶段本地提交且工作树干净后，以显式本地 merge 合并父基线 4efa310a256019192e01f0738ee226d16ad44396；保留 Empty/Result、26 个既有 Theme IIFE、Theme 栈修复、各 host、popup wheel content phase 和空 Table interaction，MSVC headless Debug/Release 定向组合回归及 5 份金样语义比较通过。

## 5. 收口

- [x] 5.1 交付 docs/form.md 可编译示例及明确边界、075 精确命令与结果证据；运行 clang-format 22/scripts check、doctor、strict all、diff check，通过后创建本地集成验收提交，工作树干净，不 push/PR/archive。

本 change 只承诺上述平台通用定向验证，不承诺全套 CTest 或 native GPU/Linux/系统 IME/人工视觉验收。构建最多 4 jobs；只复用依赖 source cache，全部输出属于本 worktree 独立构建目录。
