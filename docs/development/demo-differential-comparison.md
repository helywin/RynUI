# 差分比较基础接口

`tools/demo_differential/regions.py` 提供 pilot 比较器的 logical rectangle、单一声明 sample-origin 平移、reference/native bounds 并集、DPR floor/ceil、至少 2 physical px guard 和严格 RGBA crop。空 ROI、缺 tile/guard、尺寸不一致直接失败；不搜索最佳平移、不按 target 各自重新对齐，也不 resize。颜色、clip、阴影 support 与完整 tiles 的上层合同仍须在调用处验证。

局部 raster 指标保留最大 channel residual、变化连通块、两侧 foreground occupancy、coverage mass 和双向 contour 距离。每个必需 glyph/run/icon 都须独立调用，不能用整图均值稀释一个小图标；两侧同为空白不会得到存在性通过。background 必须是独立声明/校准值，不从当前 native 失败图估计。以上为原始编码 byte/轮廓度量，尚不构成色彩转换、容差校准或完整比较 verdict。

PNG/BMP 解码所需 Pillow 单独锁在 `comparison-dependencies.lock.json`：12.0.0，wheel/source URL、实际 SHA256、完整安装文件和所带 license hashes。`comparison_dependencies.prepare(<fresh ignored out>)` 只解包到任务目录，`image_module(<site-packages>)` 先核文件集合/hash，再核实际 import origin/version；不使用已安装的全局第三方包或隐式 fallback。Python 记录兼容性和实际 `sys.version`，不固定解释器 patch 版本。

当前锁提供 Linux/Windows x64 CPython 3.14 ABI wheel；ABI 是二进制兼容条件，同一 ABI 的其他 Python patch 版本不被 gate。其他 ABI 应明确报无兼容锁，不能假装自动可用。Linux 实际 archive install/import 已测；Windows 只有 archive/hash 元数据，未在 Windows 执行，不计平台验收。

`demo_differential_regions_tests.py` 的确定性程序控制只证明 ROI/局部度量，`demo_differential_comparison_dependencies_tests.py` 验证真实 archive 安装及损坏拒绝。这些不代替两侧真实组件执行、五次正控制/留出负控制、有效 profile 或实际 GPU 红绿。缺失的上层校准、Token/text/alpha/clip/overflow AND 判定仍按 109 tasks 跟踪，不根据当前差异学习宽松阈值。
