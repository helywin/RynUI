# Design

## Context

现有 `Icon` 在 `text_component.cpp` 映射为一个私有 Unicode glyph；`TextSceneService::icon_font` 缓存按字号/density 加载的 CFF 字体，FreeType 产生 R8 coverage，共同 glyph atlas 上传。生成工具只挑选十四个 SVG；资源包归档已在 out 中，实际归档包含 848 个 `inline-svg` 图标。`GlyphInstance` 与 packed 80-byte ABI 目前只支持平移/opacity，HLSL VS 使用轴对齐顶点。主 spec 尚未同步，033 的 typed-offline-icons 合同继续保留。

来源（2026-10-03）：[Ant Design 6.6.5 Icon API](https://ant.design/components/icon/)；锁定版本/commit/归档 hash 以 `cmake/dependencies/RynUIDependencyLock.cmake` 为准。官方 API 包含 rotate/spin/twoToneColor 与自定义图形，网络 iconfont/DOM API 不在本轮原生范围。

## Goals / Non-Goals

**Goals:** 沿用统一 text/font/atlas 生命周期，把目录与 typed 向量转换为可复用轮廓；角度与色彩有独立更新通道，维护工具可逐字节重现。

**Non-Goals:** 不引入通用 SVG/DOM parser、远程脚本、组件私有 GPU 资源或新的生产依赖。自定义原生路径提供贝塞尔命令，SVG arc/transform 需资产工具预处理，不作为 Web 兼容 API。

## Decisions

### 1. 完整资源与逐层 glyph

原十四个枚举值保持 0–13，新增名称按稳定排序追加，uint16_t 容纳完整目录。生成头与 manifest 由维护工具生成，全部 SVG 从锁定归档安全提取并逐 SHA256 校验。单色使用一个 glyph，two-tone 按原 path 绘制顺序划分颜色角色层；同色连续 path 可合并，保留路径重叠次序。layer 使用独立私有 codepoint，Tooltip 的 F000–F003 保留，额外 layer 使用 supplementary private-use，避免碰撞。两个颜色角色不代表仅两个绘制层，实际资源以原顺序生成。

使用字体轮廓而不是另建 SVG renderer，继承已有 DPI、subpixel phase、coverage 上传与 backend 合同。生成器使用锁定 FontTools 4.60.1；生产构建只消费检查入库的资源。

### 2. Icon props 与 Theme

`IconProps` 提供 typed source（具名或不可变自定义向量）、rotate、spin、双色主色/可选副色；`name` 保留便捷入口。Theme/slot 继续决定默认单色前景与字号；双色默认主色采用当前全局 colorPrimary，副色使用相同锁定调色板的浅色派生。显式色彩优先，color 更新只 patch material，布局入口不添加 visual modifier。

Icon 采用一个保留根节点与按 layer 的 text scene；layer 同原 viewBox 对齐。source 改变更新 scene 内容与字体，不重新执行父 Content；同根节点下按需创建/销毁内部 scene，不为每层增加 Component/布局节点。实际官方 inventory 为 outlined 447、filled 251、two-tone 150，连续颜色层最多四层。自定义定义限制最多 4096 path commands 与最多 64 个 paths/颜色层，坐标/viewBox 必须有限，输出字节上限 2 MiB。

### 3. 原生向量转换

不可变 `IconVector` 保存 viewBox 与 typed path 命令/color role，构造时先完整校验；通过内部确定性 Type2/CFF + OpenType 构造器形成内存字体，Quadratic 转换为等价 Cubic。字体由共同 FontRuntime 加载，源与字号/density 缓存归 TextSceneService，窗口释放时清理缓存字体。禁止 runtime shell、FontTools 或下载。相比在组件自写 raster/texture 路径，内存字体保持统一 atlas 和现有测试能力；相比接受任意字体文件，typed 路径可早期验证并控制上限。

### 4. 共同 glyph 旋转

Logical scene 增加 pivot/angle，更新 logical version；packed ABI v2 的 glyph 追加 float4 旋转基。共同 packer 在 logical 坐标中绕 pivot 转换原点，并按 viewport 宽高比打包两条基向量；HLSL VS 用 corner 在线性基上取点，UV 与 PS clip 保持独立。零旋转严格对应旧矩形。SDL input attributes、Recording backend、能力 preflight、packing/reference/contract tests 同步升级，不在组件分支选择 backend。

TextSceneService 保存 transform，几何 rebuild 与 placement patch 都重新应用，避免后续布局覆盖角度。旋转只标记 geometry，DPI/source/字号才允许重新 rasterize。每帧 spin 使用统一 animation host 与单调时间；失活分支、motion/reduced-motion、销毁取消 callback；连续 spin 启用时显式请求下一帧，静态状态 idle 无 deadline。

## Risks / Trade-offs

- [路径层顺序或 viewBox 偏移] → 全目录逐层 bounds/codepoint/颜色角色与 glyph 非空检查，双色与自定义真窗口读回。
- [CFF 构造错误] → FreeType 真实 load/shape/raster 合同，Quadratic/Cubic/holes/边界/非法输入和 cleanup 测试；固定大小和命令上限。
- [ABI 升级影响 backend] → 守卫显式版本，同步 HLSL/attributes/reference，HEADLESS + native 原生 CTest；文档更新 logical/packed 版本。
- [多 layer 开销] → 生成器记录最大 layer，使用有界按需 scene、空 scene 不产生 coverage，Gallery 记录实际数量，色彩/角度更新审计缓存计数。
- [旋转触发隐藏窗口空帧] → active branch 与 motion 检查、callback ticket/销毁测试，真实 idle 轮询。

## Migration Plan

逐阶段提交资源、变换、Icon props、向量、Gallery、Windows 验收；回退必须同时回退 ABI/shader/能力声明，不能只撤 Icon。平台通用合同在 Windows `windows-msvc-headless` Debug/Release 运行；原生 `windows-msvc` 使用 MSVC/Ninja Multi-Config。Windows 和 Linux 分别保存实际窗口/GPU/字体/input/DPI 证据，本机不勾选 Linux。
