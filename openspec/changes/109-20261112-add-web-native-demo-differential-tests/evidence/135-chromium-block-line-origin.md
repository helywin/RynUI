# Chromium 单行按钮文字绘制基线

Windows，锁定 Chromium 141.0.7390.37，DPR 1.5。Select 示例中 `Focus Select` 的 DOM 推算基线为 1319.5 CSS px，实际合成层 `drawTextBlob` 提交到 1320 CSS px。按 Blink `TextFragmentPainter` 的 CSS 像素行盒取整顺序推导绘制基线。观察条件要求文本是按钮内块级 flex 子项的唯一内容，单行 Range 与该块同界，且无变换、纵向内边距、边框或生成内容。保留原 Range 和语义坐标。

| 自动检查 | 结果 |
| --- | --- |
| `python -B tests/demo_differential_layered_tests.py` | 48/48，半像素源位置、失配观察和未提供行盒事实分支通过 |
| 锁定 Web Select 自动流程 | 10/10 检查点，Web/native 投影状态和回调相同 |
| 三个按钮标签、两张重叠截图的固定位置字体轮廓回放 | 6/6，漏判的深色文字像素从 862 降至 0，修正后各窗口归一化灰阶残差不超过 0.0024 |

原始产物保存在 `out/nov19-differential/nov21-web-block-line-v62b-windows/`。本次接受范围是单行按钮文本源推导。完整三例对比、故障注入和 Linux 实拍仍待验收。
