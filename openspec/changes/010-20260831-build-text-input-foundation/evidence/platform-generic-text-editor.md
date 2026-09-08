# TextEditorState 平台通用阶段证据

- 日期：2026-09-08；范围：任务 2.1–2.5。
- 实际平台：Windows 10.0.26200 x64 / MSVC 19.51.36256.0。
- Preset：`windows-msvc` / `windows-msvc-debug`，Ninja Multi-Config，BUNDLED。
- `scripts/build-windows.ps1 -Configuration Debug -SkipTests` 通过。
- Developer PowerShell 中 `ctest --preset windows-msvc-debug -R 'text_editor|text_boundary|unicode_|utf8proc|dependency_lock|python_cache_clean' --output-on-failure`：15/15 通过。
- `rynui_text_editor_allocation_benchmark`：10000 cycles、90000 operations、0 heap allocations；retained capacity 25485 bytes、owners=1，103 个 allocation failure points 全部验证原子回滚。单次 Debug 运行 elapsed_us=252730（不作为性能门槛）。
- editor 只依赖 internal boundary target，无 Component/Scene、renderer、SDL 或回调依赖；操作不能触发 UI side effect。
- 用例包含 stale/reused owner、wrong-thread、失效初始值、反向 selection、Shift collapse、grapheme-safe delete/insert、combining 合并、CJK/flags/emoji、CR/LF 清理、scalar maxLength/完整 cluster 截断、capacity failure、只读与禁用状态。
- 词选择规则：letter/mark/number/connector 同类连续段，空白和标点各自连续段；symbol/emoji 为单个完整 grapheme。输入句柄始终按 generation 重新解析。
- Debug 分配探针发现通用 `std::swap(TextBoundaryMap)` 会分配 MSVC checked-iterator proxy，现用 vector member swap，未关闭 Debug 检查或放宽零分配门槛。
- 本阶段只验证 committed text 算法；真实 IME、clipboard、history、Input 与平台 native 验收仍由后续任务负责。
