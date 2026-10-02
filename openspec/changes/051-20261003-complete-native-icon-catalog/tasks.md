# Tasks

## 1. 完整离线资源（平台通用）

- [ ] 1.1 安全提取锁定 848 图标并生成 uint16_t typed 目录、逐层 CFF/codepoints/颜色角色/manifest；保留旧十四值与 Tooltip 箭头，更新资源文档。生成器 check、资产 verification、全部名称非空/三类数量/层序合同、HEADLESS D/R 受影响 CTest、格式/OpenSpec/diff 通过后提交，记录 Windows preset。

## 2. 共同保留旋转（平台通用）

- [ ] 2.1 扩展 logical glyph pivot/angle 与 TextScene transform patch，升级共同 packed ABI/能力声明/SDL attributes/HLSL/reference；更新 renderer-contract。加入不同宽高比/density/clip/非法值/版本拒绝/无 reshape 与 coverage upload 的合同，HEADLESS D/R 受影响 CTest 与格式/OpenSpec/diff 通过后提交。

## 3. Icon 双色与 motion（平台通用）

- [ ] 3.1 实现 typed reactive source/name、双色主副色、Theme 默认、逐层同节点保留 scene、rotate/spin；motion/reduced-motion/失活与销毁清理。更新公开 API/docs，加入 layer 顺序/对齐/主题/状态/身份/缓存/idle/slot 与旧 action 图标回归，HEADLESS D/R 受影响测试及格式/OpenSpec/diff 通过后提交。

## 4. Typed 自定义向量（平台通用）

- [ ] 4.1 实现不可变 viewBox 与 Move/Line/Quadratic/Cubic/Close/color role 向量、确定性内存 CFF/OpenType 构造与共同 font cache/lifetime；校验 64 paths/layers、4096 commands、2 MiB 上限。加入真实 FreeType load/shape/raster、贝塞尔/holes、非法输入回滚、内置/自定义切换与 DPI/销毁测试，文档与 HEADLESS D/R/格式/OpenSpec/diff 通过后提交。

## 5. 集成（平台通用）

- [ ] 5.1 Gallery 添加 outlined/filled/two-tone/旋转/spin/自定义样例与目录计数，更新 support overlay 和原生收尾清单；生成器 self-test/check、完整 HEADLESS D/R CTest、文档/格式/OpenSpec/diff 通过，记录实际平台/preset 后提交。

## 6. 平台集成

### Windows

- [ ] 6.1 `windows-msvc` Debug/Release 构建、受影响原生 CTest、默认 stack Gallery smoke；实际 D3D12/DXIL/系统字体窗口完成系统及 1/1.25/1.5/2 render scale 的三类/四 layer/双色与 Theme、自定义/非正方形视框、角度/spin/reduced-motion、祖先 clip、resize/命中/失活/idle/销毁验收。保存 EXE/PNG hashes、日志/脚本，检查后独立提交。

### Linux

- [ ] 6.2 在实际 Linux 机器完成本 change 的 GPU/shader/窗口/字体/DPI/input/resize/spin/清理集成验收并保存日志/readback/身份，独立提交；不重复平台通用合同。
