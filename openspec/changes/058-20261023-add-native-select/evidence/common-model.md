# Token/API/模型阶段验收

2026-10-23，Windows，MSVC x64 / Ninja Multi-Config，preset `windows-msvc-headless`；此阶段为平台通用逻辑，不代表窗口组件或原生 GPU 验收完成。

- Debug/Release 构建通过。受影响 CTest 正则为 `(select_|theme_|input_|text_boundary|unicode|otp_|backend_boundaries|typography_|tooltip_)`，各23/23通过；Debug269.65秒（其中input_scene_allocation269.64秒），Release14.59秒。
- 新增 select_model/select_theme/select_public_api 三项：String/int64/空/未知值、动态 options、typed duplicate 原子拒绝、受控回写拒绝/echo、Unicode casefold/NFC/embedded NUL、禁用与空结果导航、公开 typed Props/slots/ref、Token 配方/独立 override/继承/reset/algorithm/dirty groups/原子失败。
- `verify_existing_tokens.py` 对照规划提交4215d5f读取5份旧 golden，忽略新增Select段与snapshot identity后逐值比较，全部一致。golden新增Select诊断与相应hash，不修改旧Token值。
- Unicode处理复用锁定utf8proc2.11.3，经内部text_boundary隔离；组件不直接依赖第三方头。configure直接依赖守卫与backend_boundaries通过。
- clang-format22.1.3检查465个自有文件，0失败；doctor healthy，strict validate58/58，git diff/cached check通过。

后续窗口交互、Gallery和Windows/Linux原生验收仍未完成；此提交只完成tasks2.1。
