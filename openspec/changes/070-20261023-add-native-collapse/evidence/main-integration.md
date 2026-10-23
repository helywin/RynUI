# main 集成（2026-10-24）

- main 基线 `5cb9f599d2bb0bce01a0a601244afa677a104a15`，合并 Collapse `815a509dc468628145030ca96f2fef2c65bae9c8`，保留 Table/Menu/InputNumber 及全部既有组件。
- 共同 Theme 冲突按字段合并，保留构造参数、equality、identity/diagnostics、订阅和 dirty plumbing。新构建的 Debug Theme algorithm 重新生成五份 golden；除 identity 与新增 collapse 字段外，全部既有字段值保持不变。
- Windows MSVC `windows-msvc-headless` Debug **10/10**（12.29 秒）、Release **10/10**（5.69 秒）。范围：Collapse component/theme/public API、Theme algorithm/runtime、Switch features、Select、Modal、Dropdown 和 Table。
- 保留 `Modal → Table → Popconfirm → Dropdown → Tooltip → OTP → Select` 的前向窗口定位顺序与反向输入分发，新增 Collapse 不改写既有相对顺序。Select 未聚焦 popup cleanup 和受控拒绝 suppression 的 R1 修复一并合入。
- clang-format 566 文件，0 failures；doctor healthy；strict validate **71/71**；diff check 通过。
- R2 已确认 R1 三项修复，另有两个焦点呈现 P2（extra 的 tight clip 截断 Button focus ring；pointer→Space/Enter 没有切换 keyboard modality）正在原实现工作树独立修复。此提交不声称 R2 全部关闭。

实际输出为 ignored `out/collapse-root-{debug,release}-{build,ctest}.log` 与 `out/collapse-root-{format,doctor,validate}.log`。未运行 Collapse Gallery、GPU、Linux、人工视觉或系统 IME 验收。
