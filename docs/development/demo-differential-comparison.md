# 差分比较基础接口

`tools/demo_differential/regions.py` 提供 pilot 比较器的 logical rectangle、单一声明 sample-origin 平移、reference/native bounds 并集、DPR floor/ceil、至少 2 physical px guard 和严格 RGBA crop。空 ROI、缺 tile/guard、尺寸不一致直接失败；不搜索最佳平移、不按 target 各自重新对齐，也不 resize。颜色、clip、阴影 support 与完整 tiles 的上层合同仍须在调用处验证。

局部 raster 指标保留最大 channel residual、变化连通块、两侧 foreground occupancy、coverage mass 和双向 contour 距离。每个必需 glyph/run/icon 都须独立调用，不能用整图均值稀释一个小图标；两侧同为空白不会得到存在性通过。background 必须是独立声明/校准值，不从当前 native 失败图估计。以上为原始编码 byte/轮廓度量，尚不构成色彩转换、容差校准或完整比较 verdict。

PNG/BMP 解码所需 Pillow 单独锁在 `comparison-dependencies.lock.json`：12.0.0，wheel/source URL、实际 SHA256、完整安装文件和所带 license hashes。`comparison_dependencies.prepare(<fresh ignored out>)` 只解包到任务目录，`image_module(<site-packages>)` 先核文件集合/hash，再核实际 import origin/version；不使用已安装的全局第三方包或隐式 fallback。活动 Python 按 ABI 验证并记录实际 `sys.version`；Windows BUNDLED 解释器归档另外固定，见下。

当前锁提供 Linux/Windows x64、非 free-threaded CPython 3.14 ABI wheel；ABI 是二进制兼容条件，同一 ABI 的其他 Python patch 版本不被 gate。其他 ABI/32 bit 应明确报无兼容锁，不能假装自动可用。Linux 既有 wheel 路线保留。Windows 已通过实际 CPython 3.14.0 + Pillow 12.0.0 的 CPU import、PNG/BMP 往返与 fresh/offline 重复控制；这不计 GPU 或 paired 平台验收。

## Windows x64 解释器准备

`comparison_runtime.discover_interpreters()` 只探测当前 Python 和 PythonCore 3.14 注册路径，验证 CPython/实际 executable、3.14、64 bit、`cp314-win_amd64` 与非 free-threaded；不通过 PATH 猜工具。先运行发现命令；若返回兼容解释器，可显式选择 `mode='SYSTEM', python=r'<返回的 executable>'`。SYSTEM 解释器按既有 ABI 合同允许 3.14 patch release，并记录实际版本和 executable hash；不会接受 Python 3.10。发现为空时显式选择 BUNDLED，不自动改模式：

```powershell
$bootstrapPython = 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe'
& $bootstrapPython -B -c "import sys,json; sys.path.insert(0,'tools'); from demo_differential.comparison_runtime import discover_interpreters; print(json.dumps(discover_interpreters(),indent=2))"
$runtime = (& $bootstrapPython -B -c "import sys,json; sys.path.insert(0,'tools'); from demo_differential.comparison_runtime import prepare_runtime; print(json.dumps(prepare_runtime('out/demo-differential/comparison-win',mode='BUNDLED')))") | ConvertFrom-Json
$comparisonPython = $runtime.python
$pythonFlags = $runtime.python_flags
& $comparisonPython @pythonFlags tools/demo_reference_web/bootstrap.py --mode BUNDLED --output out/demo-differential/prepared
# 主 runner 的其余参数见 demo-differential-testing.md。
# 用 & $comparisonPython @pythonFlags $runtime.runner run ... 启动，
# 并传 --comparison-site $runtime.comparison_site。
```

BUNDLED 固定 [官方 CPython 3.14.0 embeddable amd64 zip](https://www.python.org/downloads/release/python-3140/)，URL/bytes SHA256、完整 36 文件和 LICENSE.txt hash 在比较器锁；不运行 installer、pip 或修改系统 Python。Python 3.10 只用于标准库 zip 引导。所有解释器/DLL/配置、Pillow wheel/site、原始 runtime 记录与 hash 缓存在 out。新目录必需；归档缓存损坏即拒绝，不能联网覆盖消错；`offline=True` 不发网络请求，`downloads='<directory>'` 只按两个已锁 SHA256 导入归档。

依据 [CPython Windows embeddable 说明](https://docs.python.org/3.14/using/windows.html#the-embeddable-package)，只改 out 内 `python314._pth` 为标准库 zip、解释器目录和本 checkout tools 的显式路径，不启用 `import site`。启动 flags 为 `-E -s -S -B`；`$runtime.runner` 是 out 内的简短入口，固定本 checkout tools 的 import 路径后运行原 runner，避免 SYSTEM embeddable 的外部 `._pth` 指向另一 checkout。Pillow 由 `--comparison-site` 经 `image_module()` 验证后加入 sys.path，不把未经校验的第三方目录写进 `._pth`。`verify_runtime(<out/runtime>)` 重新检查锁、解释器完整文件/配置、入口、实际 ABI、Pillow 完整文件与 CPU codec。换 checkout/移动目录后必须重建。

CPU 控制入口（先显式准备真实 archive caches；这些不启动页面或 Node 包脚本）：

```powershell
& $comparisonPython @pythonFlags -m unittest discover -s tests -p demo_differential_windows_preparation_tests.py
$env:RYNUI_COMPARATOR_WHEEL = 'out/demo-differential/comparison-win/comparison/pillow-12.0.0-cp314-cp314-win_amd64.whl'
& $comparisonPython @pythonFlags -m unittest discover -s tests -p demo_differential_comparison_dependencies_tests.py
```

Windows Web CPU 控制需要 `out/demo-differential/archives` 中的真实 Node/browser/source/npm 归档，可由父执行正常 Web prepare 填充；缺归档失败。它只执行 Node/browser `--version`，在 pnpm 安装前明确停止；真实 frozen-lockfile 安装、浏览器动作/图像与 pilot 仍由父侧运行。比较器 `prepare(..., offline=True)` 也支持单独的 ABI 兼容解释器调用；runner 的自动准备调用须传递其 `--offline` 选项，传显式 `--comparison-site` 可避免自动准备路径。

`demo_differential_regions_tests.py` 的确定性程序控制只证明 ROI/局部度量，`demo_differential_comparison_dependencies_tests.py` 验证真实 archive 安装及损坏拒绝。这些不代替两侧真实组件执行、五次正控制/留出负控制、有效 profile 或实际 GPU 红绿。缺失的上层校准、Token/text/alpha/clip/overflow AND 判定仍按 109 tasks 跟踪，不根据当前差异学习宽松阈值。

## Pilot AND 判定

`comparison.py` 复用上述 regions API：每 checkpoint 固定完整 sample origin，仅作一次刚性平移；共同 logical union 用 floor/ceil × actual DPR，再加入至少 2 physical px guard。control、context 与 guard 分开登记，缺图/边界、裁图尺寸、未校准的 fractional origin phase 失败；不 tight crop 各 target、不 resize 或搜索配准。纯背景和小图标各自有局部 foreground、mass、contour 与最大 byte residual 门槛，不以全图均值判绿。

现阶段明确的 metric identity/hard cap：`geometry.logical_px <= 0.25`，`pixels.coverage_mass_relative_error <= 0.05`，`pixels.contour_distance_px <= 0.5`，`text.coverage_mass_relative_error <= 0.05`，`text.contour_distance_px <= 1`。未有独立颜色转换/噪声校准前，`pixels.max_channel_residual` hard cap 为严格 `0`，不任意采用宽 byte 阈值。每个有效 profile 必须包含这些指标且无重复/未知项；实际 threshold 必须等于 `max(quantization_floor, positive_max + margin)`，不超过声明和代码 hard cap，且 `negative_min > threshold`。pilot masks 必须空。

profile 使用既有 strict v1 schema，要求 `calibrated=true`、至少 5 次正控制，以及匹配 comparator/regions algorithm hash、完整 source closure、两侧 browser/native GPU/driver/metrics/font 环境 hash、字体 bytes/face 和独立 corpus hash。绑定值来自 `binding()`，报告记录该值以供独立校准使用；runner 不从当前待判差异生成 threshold 或正控制。缺匹配校准、色域/channel/alpha 或有效字体时 environment/calibration 非零，不能把 capture 存在当 pass。当前原生 `native-encoded-SDR`/premultiplied 与浏览器 SDR/straight 还没有可默认为等价的校准，保留这个失败边界。

基础 Token 比较显式映射 border width/radius/font size 的 logical 单位；缺原生实际 Token 字段报接口缺失。文字先验内容/顺序、实际选中字体 hash/face/coverage、禁止未锁 synthesis/fallback；局部文本/run bounds 与独立 raster ink 缺一失败。Web computed font family 不代替 CDP selected faces；native `invalid-UTF8-repairs` 不代替 nominal glyph coverage。正文、icon 与 editor selected/placeholder 原始 layers 保留，后两者的完整 glyph/selection/line/clip 扩展留阶段 6–9。

文字/图标存在性使用锁定 Web 实际 computed background 作为独立材料输入，当前只支持透明继承已声明白底或 opaque RGB；未知/半透明材料等待后续颜色校准，非零失败。presence 单独检查 run/icon 内部，不让外侧白色 guard 或主色按钮背景充当 glyph ink。case 声明的正文和 icon 即使两侧同时缺失也会失败；expected 不来自 native 图像或测量值。

对接 native 冻结接口后，普通 pinned text 只接受声明 bytes/face 的 `source=pinned-file`；system、unknown、bundled-fallback 或把 icon source 用作正文均失败。实际 600 文件先通过独立 SFNT weight admission，不能 regular 改标签。component flag 按 Button/Input/Select 的实际适用字段比较，保留其他字段的 null；适用字段未知不得默认为 false，sample aggregate 不冒充最后一个子控件状态。

`demo_differential_runner_comparison_tests.py` 使用程序生成的局部像素与字体元数据控制，验证 1 logical px、小图标删除、局部空白、guard 外漏、错误字体、缺文字 bounds、无校准/过宽 threshold/mask 的拒绝。它们不运行 Pillow、浏览器、native 或 GPU，不是实际平台校准。完整 colors/gradient/shadow/alpha、长图 tiles、字体排版及真实 fault corpus 仍是后续范围。
