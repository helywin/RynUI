# Native source observation 实施

2026-11-12，Linux/GCC。此独立阶段只增加 internal `FontRuntime::source(FontIdentity)` 的只读 source view，提供实际持有的 immutable font bytes、face index/family/style 与 bold/italic 标记。资源 bytes 的借用期限为本 owner thread、该 font generation 存续期间。Gallery 后续对 bytes 做 hash；Core 不依赖 Web/JSON、SDL 或 renderer，字体选择/加载/渲染流程不变。

- 正式 `linux-gcc`，`rynui_font_runtime_tests`，Debug → Release 串行，单次 `-j2`；`ctest --preset linux-gcc-{debug,release} --no-tests=error -R '^rynui\.font_runtime$'` 各 **1/1 通过**。新增控制确认来源 bytes 与实际输入一致、foreign thread 被拒、查询不改变 coverage 状态、退役 generation 不可查询；原字体测试保留。
- HEADLESS configure 的 include/link guards 通过。首次选用现有 `rynui_portable_font_runtime` 时，其通用登记会链接整套 components；为保持本阶段定向范围，仅停止自己 main BUILD 的该宽构建，未计 pass，改用上述只链接 font 的既有测试 target。未重跑全量 CTest。
- clang-format 22.1.3、OpenSpec 1.14.0 doctor/strict 与 diff check 通过；原始构建/CTest 日志和自有 scenario WIP 精确补丁/pre-post 哈希只留 ignored out。

109 仍 **5/61**。scenario reader、hello/receipt/checkpoint/complete、真实 dispatch 与 readback 桥接尚在实施和验证，本阶段不勾选 3.x，不报告 Web/native pilot、GPU 或 Windows 通过。
