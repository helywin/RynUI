# Logical scene 迁移验收

scope: platform-generic  
status: passed  
环境：Windows，MSVC x64 14.51.36231，Ninja Multi-Config，2026-10-02。

`windows-msvc-headless-debug` build/CTest exit 0，**27/27，6.64 秒**，原始记录 `common-debug-ctest.txt`。使用真实 Core、FreeType/HarfBuzz/utf8proc 与锁定字体，HEADLESS + RECORDING，无 SDL 或系统字体链接。

验收覆盖：

- literal NDC rect/translation/clip、logical radius clamp、material/UV 保留、独立 CPU/GPU 类型与 packed stride/offset。
- 12/14/16 字号、1.0/1.25/1.5/2.0 density 和四档 quarter-pixel phase 矩阵；physical raster origin/1:1 atlas texel sampling 保持。
- Button/Selection/Radio/Divider/Typography/Input/Text 的真实 logical scene；Input 768 个模拟 scale/layout 检查与独立字体 density、选区、caret 和 CJK clip；Text offscreen reentry 验证 exact logical translation。
- Recording 的 owned buffer/texture bytes、有序 Quad/Glyph/Effect draw、idle、partial/material 范围、无 CPU dirty resize、upload exception/commit failure 完整重试、无效 metrics 在 upload 前拒绝、epoch 重建。
- resize 不重写 Quad/Glyph CPU 数据、不新增字体 rasterization、不 remount、不重新分配已有 instance GPU buffer；projection-only resize 不上传 atlas。
- Core 的 QuadGpuBuffer 和直接 GPU 同步入口已移除；QuadScene 使用范围 material/geometry 更新，共同 staging 重用容量。现有 build boundary negative fixture 通过。

正式合同见 `docs/renderer-contract.md`：CPU logical scene v2 / packed GPU ABI v1。shader 文件没有改动。本证据只验收平台通用合同，Windows/Linux 原生结果单独记录。

`openspec doctor --json` healthy、全量 strict 36/36、`git diff --check` 均 exit 0。
