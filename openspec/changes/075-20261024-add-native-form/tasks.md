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

## 6. Astra R1 审查修复

- [x] 6.1 先复现 Form font_size 小于 0.5 logical dp 被解析接受但 Text 无法表示的问题；在最终派生值处覆盖 override/component algorithm 路径，验证初始/响应更新、阈值、小正数/非有限、旧 Theme 与实际 Text 状态原子性、恢复及 DPI，MSVC headless Debug/Release 定向测试通过后本地提交。
- [x] 6.2 先复现全宽 control wrapper 可见但捕获的窄 Input 全裁剪时抢焦点的问题；ref 内部代际适配使用真实控件 target/node/clip/共同窗口资格，验证全裁剪/部分可见/零高/ref 销毁与身份保留、IME 及 Modal/Spin/hidden，MSVC headless Debug/Release 定向测试通过后本地提交。
- [x] 6.3 两项 Form R1 修复验证并本地提交干净后，显式合并已验证父基线 6f1b38a53486907bfc890939335180e8ac2df4b7，保留父线程的 Theme descendant preflight/helper/既有通知顺序、Gallery 076、全部 hosts/Form token plumbing；定向组合 Debug/Release 和五份金样/IIFE 比较通过，不接入其他 WIP 或 DatePicker 分支。
- [x] 6.4 更新 Form 文档、设计与 R1 证据，运行格式、doctor、strict all、diff 和五份既有金样不变比较；保持工作树干净，仅交父线程 R2 审核，不合并父线程 WIP 或重做通用 ThemeScope preflight。

本 change 只承诺上述平台通用定向验证，不承诺全套 CTest 或 native GPU/Linux/系统 IME/人工视觉验收。构建最多 4 jobs；只复用依赖 source cache，全部输出属于本 worktree 独立构建目录。

## 7. 父线程集成

- [x] 7.1 显式合并已验证 worker 275707a 到 main，保留 076 导航与主线程改动；复现并修复关闭组件算法但存在局部 seed 时丢失父 Form token 覆盖的问题，验证静态/响应继承、局部 override、启用/关闭算法及 inherit=false；通过最终组合 Debug/Release 20 项、五份 fresh golden 旧值/IIFE 比较、格式/doctor/strict/diff，并记录 Astra R2 和父线程独立后续复核的真实结论，不提前声明 clean。

父线程 `7ff4deb` 已推送并核对远端 SHA；Astra R3 固定该提交复核完整范围和新增继承差异，未发现可操作 P1/P2，实际复跑现有 Form 四项及 Theme algorithm/runtime，Debug 6/6、Release 6/6。075 为 **11/11 完成**；079 Gallery 独立推进，平台边界不变。
