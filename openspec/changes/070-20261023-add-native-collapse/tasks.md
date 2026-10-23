# Tasks

## 1. 平台通用实现与合同

- [x] 1.1 实现 typed API、固定 panels、retained layout/interaction 和独立 Theme tokens；新增 public API、component、theme tests，通过本 worktree windows-msvc-headless Debug focused CTest 验证受控拒绝、accordion、reflow、真实草稿、disabled/extra、Modal/Spin、callback 销毁与 material 局部更新。

## 2. 平台通用收口

- [x] 2.1 补充 docs/collapse.md 和实际 Windows MSVC/Ninja Multi-Config Debug/Release focused evidence，运行受影响的既有 Theme/Tabs/Modal/Spin 回归以及 format-code.py --check、openspec doctor --json、openspec validate --all --strict --no-interactive、git diff --check；仅本地提交，无 GPU/Linux/manual IME 声明。

## 3. Astra R1 修复与平台通用回归

- [x] 3.1 分别实跑复现并修复 disabled→enabled 缓存命中、受限 header 子绘制越界、未聚焦 Select 隐藏后旧 popup 复活；补充真实 pointer、quad/glyph、受控拒绝及重复隐藏/重开、Modal/Spin 与回调销毁测试，Windows MSVC headless Debug/Release affected tests、format、doctor、strict、diff 通过并更新本 change evidence；只做本地英文提交，不合并根分支。
