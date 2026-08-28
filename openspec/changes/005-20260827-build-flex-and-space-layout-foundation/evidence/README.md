# Layout evidence contract

本目录分别保存 Linux 与 Windows 的真实平台验收证据。两个平台使用相同字段，但必须写入独立文件、独立截图路径与对应平台身份；一个平台的结果不得填充或替代另一个平台。

`status=pending` 表示模板尚未在对应平台完成真实窗口验收。只有完成该平台 build、CTest、真实窗口操作、宽窄截图与诊断采集后，才能改为 `status=passed` 并填写数值。

字体清晰度证据必须记录 `font_logical_pixel_size`、`font_raster_pixel_size` 与 `font_raster_scale`；目标缩放下启动示例后，实际 raster size 必须与输出 scale 对应，不能用 GPU 放大 1.0 density atlas 代替。

默认字体证据还必须记录 `font_source` 与 `font_families`。Windows 应由 DirectWrite 解析 Segoe UI 系列与 Microsoft YaHei UI；Linux 应记录 Fontconfig 在该桌面配置上实际匹配的 family，不预设具体发行版字体名。若应用传入 custom font，source 必须同时反映 custom 与实际 fallback 来源。

当前未解决问题统一登记在项目级 [docs/open-issues.md](../../../../docs/open-issues.md)。Linux 原生 Wayland 的窗口 resize 阻塞、复现环境、上游对应问题和 XWayland 诊断对照进一步记录在 [docs/issues/linux-wayland-resize.md](../../../../docs/issues/linux-wayland-resize.md)。这些记录不改变 `linux-layout-evidence.md` 的 pending 状态，也不能替代 task 6.3。
