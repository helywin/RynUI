## Purpose

定义 shaped Glyph run 到灰度 atlas、Glyph instance 和 GPU 输出的局部更新闭环，确保文字可与 Quad 正确合成并在稳定状态保持按需渲染。

## ADDED Requirements

### Requirement: GlyphAtlas 提供稳定且无重叠的 coverage 区域
系统 SHALL 为每个 rasterized glyph 分配带安全 padding 的 atlas region，并以 Font identity、glyph id、pixel size 与 rasterization mode 作为 cache key；有效 entry 在释放或明确重建前 MUST 保持 UV 与 page identity 稳定。

#### Scenario: 首次 glyph 分配
- **WHEN** 一个未缓存的非空 glyph 请求 atlas entry
- **THEN** 系统返回边界内、带 padding 且不与已有 entry 重叠的 region，并记录一次新增 entry

#### Scenario: 重复 glyph 命中
- **WHEN** 相同 cache key 再次请求 atlas entry
- **THEN** 系统返回原 region，不新增 entry、不重复 rasterize 且不产生 texture upload

#### Scenario: 当前 atlas page 容量不足
- **WHEN** 新 glyph 无法放入当前 page
- **THEN** 系统分配新 page 或返回明确的容量错误，不得覆盖仍被 Glyph instance 引用的 entry

### Requirement: Atlas upload 限于新增 coverage region
系统 MUST 只把新增或明确失效 glyph 的 coverage rectangle 上传到对应 GPU texture region，并正确处理 row pitch 与 texture 对齐；重复 cache hit 和纯 Material 更新不得上传 atlas texture。

#### Scenario: 新 glyph 局部上传
- **WHEN** shaping 结果首次引用一个可见 glyph
- **THEN** GPU upload 覆盖该 glyph 的 atlas rectangle 及必要 padding，而不是重传完整 atlas page

#### Scenario: 颜色更新不上传 atlas
- **WHEN** Text 只改变 color 或 opacity
- **THEN** atlas upload count 与 uploaded texture bytes 保持不变

### Requirement: Glyph instance 完整描述可见输出
每个可见 glyph instance SHALL 包含屏幕位置、可见尺寸、atlas UV、atlas page、颜色、opacity 与必要的 clip/translation 数据；空白 glyph MUST 参与 advance 但不得创建可见 instance。

#### Scenario: Shaped run 生成 instance
- **WHEN** Glyph run 包含可见 glyph、空格和不同 bearing/offset
- **THEN** 可见 glyph 的 instance 位置反映 baseline、bearing 与 shaping offset，空格不生成 instance 且后续位置包含其 advance

### Requirement: Quad 与 Glyph 保持稳定合成顺序
Renderer SHALL 按 Scene 声明的稳定 Z order 绘制 Quad 与 Glyph，并以正确的 coverage alpha 混合文字颜色；不得为减少 draw call 跨越会改变视觉结果的 texture page、clip 或 Z order 边界。

#### Scenario: Quad 上的文字
- **WHEN** 不透明或半透明文字位于背景 Quad 之后的 Scene 顺序
- **THEN** 最终窗口显示背景上的抗锯齿文字，文字 coverage 与 color/opacity 正确混合

#### Scenario: 不同 atlas page 的 glyph
- **WHEN** 相邻 Glyph instance 引用不同 atlas page
- **THEN** Renderer 在保持文本顺序的前提下切换 texture binding，不交换 glyph 的视觉顺序

### Requirement: Glyph 更新遵守按需帧调度
新增 glyph、Glyph instance 变化、atlas upload 完成或窗口事件 SHALL 请求帧；没有 Dirty text、Dirty instance、atlas upload 或窗口事件时 MUST 阻塞等待且不得持续 submit。

#### Scenario: 稳定文本保持空闲
- **WHEN** 真实窗口中的文本与其他 Scene 数据保持稳定
- **THEN** 观察窗口内没有按刷新率增长的 frame submission，后续文本变化或窗口事件能够重新唤醒

### Requirement: 双平台真实窗口显示 Latin/CJK 文本
最小示例 SHALL 在 Windows/MSVC/D3D12 与 Linux/GCC/Vulkan 中使用同一 UTF-8 Latin/CJK 输入完成 shaping、fallback、atlas upload、Glyph draw、事件响应与正常退出，并输出可核对的 Font、shaping、atlas、instance、upload、draw、submit 和 idle 计数。

#### Scenario: Windows 真实窗口验收
- **WHEN** 在 Windows/MSVC preset 运行示例并触发文本与颜色更新
- **THEN** 窗口清晰显示 Latin/CJK 混排，GPU driver 为 D3D12 路径，进程正常退出且计数符合局部更新关系

#### Scenario: Linux 真实窗口验收
- **WHEN** 在 Linux/GCC preset 运行相同示例并触发文本与颜色更新
- **THEN** 窗口清晰显示相同 Latin/CJK 内容，GPU driver 为 Vulkan 路径，进程正常退出且计数符合局部更新关系
