# Select 打开状态的选中值与搜索光标

已有选中值且搜索为空的单选弹层使用 `selected_open_opacity` 调整编辑器文字和光标的材质。实际查询恢复主题的正常文字颜色。定向回归检查保留的文本视图和光标 Quad，以及关闭后的颜色。五套 Theme golden 使用实际解析器重新生成。

MSVC HEADLESS Release 5/5、Debug 4/4，D3D12 合同与回读 2/2。Windows DPR 1.5 的实际 Select 重新采集 10 个检查点和 30 张分块图。

打开后光标实心列由 RGB31 变为 Chromium 参考的 RGB199。顶部边缘和第二列的覆盖率仍有差异，完整配对验收继续追踪。原始产物保存在 `out/`。
