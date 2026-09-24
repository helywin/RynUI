# Proposal

## Why

窗口服务已统一拥有 retained quad/effect surface，Input、Switch/Checkbox 和 Gallery 也在调用 `create_surface`/`update_surface`，但实现类型仍叫 `ButtonSceneService`、`ButtonSceneId`、`ButtonEffectData`。新增控件必须借用 Button 名称，并容易误把 Button 的固定 visual layer、spinner 或 focus 外观当成通用合同。

## What Changes

- 把 generation 安全的 surface ID、effects、diagnostics、quad/effect store 与 create/update/destroy 实现迁为内部 `RetainedSurfaceService`，窗口服务的 `surfaces()` 返回该通用类型。
- Button 的固定 visual layer 和 typed `ButtonVisualData` 留在 Button 专属头；Button 通过通用 surface API 写入它自己的视觉数据。Input、Selection 与 Gallery 引用通用 ID/effects，不复制 surface store。
- 保持既有 scene 顺序、effect topology、dirty/upload、GPU 同步及所有公开组件行为；用多 consumer 同窗和 stale ID 回归验证。
- 本 change 是纯内部迁移，不新增公开 API 或行为需求，因此设置 `skip_specs: true`。

## Capabilities

### New Capabilities

无；底层 retained surface 本已由多个组件使用。

### Modified Capabilities

无；仅把已有所有权和类型边界从 Button 名称下移到窗口通用服务。

## Impact

- 涉及 `src/component/`、Gallery 内部 reference surface、对应测试与 CMake 源文件列表；`include/ryn/` 不变。
- 013 的窗口服务所有权和 015 的材质过渡 helper 保持独立；Button focus ring、Input active shadow、Selection 几何及 spinner 不合并。
- 014 原生 IME 和各 change 的 Linux 原生窗口待验保持原状态，不以本轮重构结论替代。
