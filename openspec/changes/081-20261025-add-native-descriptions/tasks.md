# Tasks

## 1. 平台通用 API 与 packing model

- [ ] 1.1 实现 typed API、固定 key 校验和独立 packing model，增加 public usage 编译、span/columns/hidden/非法配置测试及 docs/descriptions.md 用法；Windows MSVC windows-msvc-headless Debug 定向 build/CTest 通过后本地提交。

## 2. 平台通用 retained host 与 Theme

- [ ] 2.1 实现保留 slot、title/extra、有限测量/摆放/绘制、三种 size、colon、border 与隐藏边界清理；增加实际行/标签/值/边框、富内容身份和 glyph/geometry 验证并更新组件文档。
- [ ] 2.2 完整接入 Descriptions Theme chain 和 colors/metrics dirty，验证 inactive seed、最终字体、后代 preflight 拒绝/恢复；用当前可执行文件重生五组 golden，深比较基线全部旧 typed values 和 28 个 IIFE 文本/顺序，Debug 定向测试通过后本地提交。

## 3. 平台通用集成与证据

- [ ] 3.1 完成富 Input 草稿、Button/Select 事件、显隐/重排/恢复、焦点/捕获/Modal/Spin、默认/暗色/紧凑和有限/零/短约束的实际 glyph/surface/hit 测试及 teardown/idle；完成 docs 与本 change evidence，正式 windows-msvc-headless Debug/Release 受影响测试、clang-format 22.1.3 check、文档 C++ 示例检查、doctor/strict/diff 通过后本地提交。

本 change 只记录本机平台通用证据，不设置 GPU/Linux/系统 IME/AT/Gallery 完成项。独立 Astra review 由父代理负责；无子代理、远端、PR 或 archive 操作。
