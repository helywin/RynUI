# Select 多选标签起始间距复验

- Windows MSVC `windows-msvc-headless-release`。`input_paint_snapshot`、`portable.input_component` 和 `portable.select_component` 共 3 项通过。多选测试覆盖 LTR、RTL 切换及标签节点保留。
- Windows MSVC `windows-msvc-release`，后台 D3D12，DPR 1.5。Select 生命周期脚本完成 10 个检查点。初始多选标签左边缘距控件左边缘 4 个逻辑像素，与锁定 Web 参考一致，旧回读为 12 个逻辑像素。
- 此次验证覆盖多选标签间距。标签尺寸、材质及完整三例差分仍待验证。
