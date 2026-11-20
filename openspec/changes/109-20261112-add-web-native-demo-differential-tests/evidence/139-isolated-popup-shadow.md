# Select 弹层透明合成层的阴影回放

Windows，`windows-msvc-release`、锁定 Chromium 141.0.7390.37，DPR 1.5。Select 弹层的实际 Chromium 合成命令先清空透明层，再按顺序绘制三道阴影。将透明层上的阴影逐道建模后合成到白底，可保留独立 Skia 回放的精确 RGB8 输出。

- 浏览器 Select 流程完成 10/10 检查点，Web/native 投影状态及回调一致。观察到 8 份有唯一实际合成命令的阴影记录，均和 CSS 源位置、圆角、扩展及顺序相符。
- MSVC 构建 `rynui_demo_differential_calibration` 成功。使用实际捕获来源调用 `--shadow-group-witnesses`，对弹层上方 2890 个真实像素复验，独立高斯判定的 289 个误报降至 0。保持原阈值、原始图片、捕获坐标及图层可见性检查。
- 定向 Python 测试 63/63，通过错序、错误颜色、缺失墨迹、非白底和不支持裁剪等负例。clang-format 22.1.3 检查 1255 文件，0 失败。

原始产物仅在 `out/nov19-differential/`。透明度动画、其他颜色的混合及完整三例联合验证仍待验收。
