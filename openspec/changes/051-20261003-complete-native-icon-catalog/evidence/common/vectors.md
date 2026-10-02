# Typed 向量字体合同

2026-10-03，Windows / MSVC / Ninja Multi-Config，`windows-msvc-headless` Debug/Release 平台通用验收。

- 受影响 CTest 15/15，耗时 9.20 / 6.70 秒；包含 Icon vector/component/catalog、FontRuntime、Text scene/component/frame、Theme、Tooltip、Input/Password/Search、Typography interaction、Button 与 backend boundaries。
- `icon_vector_tests.cpp` 单独首先 include 公开 Icon header，检验 immutable copy/source 身份与只读路径接口；确定性生成字节、OTTO 标识、全文件 OpenType checksum、真实 FreeType load/raster、HarfBuzz em advance、Quadratic/Cubic 与反向 winding 孔洞。
- 偏移 viewBox 与 2:1 非正方形保持比例/居中；64 paths、4096-command 长 contour 通过真实 raster，超限/非有限坐标/负 viewBox/无效 contour/role/opacity 在 source 变更前拒绝。
- 内置→自定义→内置保留根组件/主 scene，源缓存复用、2x density 换 font 身份、额外 layers 对齐/opacity/销毁清理；字体服务析构释放自有内置与自定义 bytes/双 FreeType faces，外部字体保持有效。
- 生成器只使用 C++，无运行时 Python/FontTools/renderer/OS 依赖；backend boundary 守卫通过。clang-format 22.1.3 check 428 文件、OpenSpec doctor/strict 51/51、diff check 通过。

CFF/Type2 与 OpenType 依据链接保存在 `docs/icon.md`。长 contour 使用无嵌套全局子程序，每个 charstring 分块有界，所有 command 共享 4096 上限；字体输出有 2 MiB 硬上限。原始日志位于开发机 `out/icon-vector-common-{debug,release}.log` 和 `out/icon-vector-{doctor,openspec}.log`。本阶段不替代 GPU/真实窗口验收。
