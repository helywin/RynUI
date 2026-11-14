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

有限 profile 的指标由 `calibration.POLICY` 精确枚举：geometry 上限 0.25 logical px、text geometry 0.5 logical px、flat/text interior DeltaE00 1、shape/text mass 0.05、shape contour 0.5 physical px、text contour 1 physical px、固定 2×2 local coverage / 每组行列 projection 0.05。整数 contour 的非零最小值仍为 1；0.5 上限保持更严含义。原始 byte residual 全量报告；只有独立 opaque transport sentinel 保留 byte=0 门，不用全 context 零 byte AND 否决合法 AA，也不把 bytecap 换成宽阈值。AA 来自已验 reference/geometry，最多 1 physical px，自身仍验 coverage/contour/projection；无法分离材料时报校准失败。pilot masks 必须空。

profile 是 strict v1 的严格扩展，必须声明 `calibration_kind=pilot-independent-v1`、首批三例完整 identity、zh-CN/LTR/default/motion-disabled/pinned 七字体、actual DPR、有限材料/字号/字重与可重建证据。旧手填 calibrated 声明失效。algorithm hash 包括实际数学、区域、校准、比较代码和 profile schema；完整 native source、实际 binary/configuration、工具/安装、OS/GPU/driver/browser/library、实际 font raster policy、bytes/face/weight/size 继续绑定。必需来源缺失不能用 None 计算出“已绑定”的 hash。

native 保留真实 `native-encoded-SDR/premultiplied`、actual source format/channel/clear；Web 保留 PNG RGB8/RGBA8/straight-or-opaque、实际 sRGB launch args、PNG color chunks 与 CRC。opaque 工作域必须同时有 owned raw RGBA 全 A255 扫描和 decoded 全 A255，普通 BMP 还核对 raw 字节摘要。透明控制只使用独立新 fence 的 raw RGBA；opaque raw 与生产 BMP 的两次 readback 分别保留 receipt，并先证明稳定场景字节相同。固定标准转换按 [W3C sRGB](https://www.w3.org/TR/css-color-4/#color-conversion-code) 和 [Sharma/Wu/Dalal 的 CIEDE2000 方程与数据](https://hajim.rochester.edu/ece/sites/gsharma/ciede2000/) 独立实现，不拟合 gamma/LUT 或失败直方图。

只有独立 `calibrate --suite pilot` 可以原子发布 profile。它不调用普通 run，不把 Button/Input/Select 图像作为正控制；普通 Gallery probe 仅用于来源/实际环境。固定 train/holdout、负例和 policy 在采集前写入 out，至少五次新 fence 与一次独立重启。逐侧几何/材料/font/SFNT cmap/hmtx/outline/解析颜色先验通过后才进入 positive。`candidate=max(quantization_floor, positive_max+fixed_margin)`，须不超过 hard cap、holdout 不超限且每个指定 negative 有固定 safety gap。消费时重开内容寻址 manifest、stream、raw/codec captures 并重算，不信任伪造 summary。失败只写报告，不发布有效 profile。

实际运行入口（平台结果与拒绝边界见 [Windows 阶段结论](../../openspec/changes/109-20261112-add-web-native-demo-differential-tests/evidence/13-finite-paint-calibration-windows.md)）：

```text
<compatible-python> -B tools/run_demo_differential.py calibrate --suite pilot --preset <actual-build-preset> --binary <rebuilt-gallery> --control-binary <rebuilt-rynui_demo_differential_calibration> --prepared <verified-out-prepared> --font-files <verified-out-font-files.json> --comparison-site <verified-out-site> --offline --dpr native --repetitions 5 --output out/demo-differential/calibration-fresh
```

Linux control binary 自动使用既有 private Wayland wrapper；Windows 要求实际 hidden D3D12，Linux 要求实际 Vulkan，设备名称照实记录，软件 Vulkan 不写成硬件。两平台 profile 不互用。未知材料/阴影、缺 tiles、缺实际 glyph/target、scope 或环境漂移均非零。代码与机制检查不代表有效 profile、三例 paired 或真实 fault meta 已通过；6.3–6.5 全域任务继续开放。

基础 Token 比较显式映射 border width/radius/font size 的 logical 单位；缺原生实际 Token 字段报接口缺失。文字先验内容/顺序、实际选中字体 hash/face/coverage、禁止未锁 synthesis/fallback；局部文本/run bounds 与独立 raster ink 缺一失败。Web computed font family 不代替 CDP selected faces；native `invalid-UTF8-repairs` 不代替 nominal glyph coverage。正文、icon 与 editor selected/placeholder 原始 layers 保留，后两者的完整 glyph/selection/line/clip 扩展留阶段 6–9。

固定 retained host content anchor 与内容并集分开；只减 host/fixed wrapper 原点一次，再检查内容相对位置、独立容器 width/padding。文字消费 `GlyphPrimitive.coverage` 的 byte/line ownership 和实际 Range/UA-shadow Text 分组；未观察到真实分组不退回整框。空白按 advance 验证，边框、圆角背景和 guard 不得冒充 glyph。Input/Select 材料读取 `InputPaintSnapshot` 的真实 retained fragment/effect 副本和 owner/generation；未同步、clipped、hidden 和 absent 保持不同事实，查询不触发 refresh/frame。

对接 native 冻结接口后，普通 pinned text 只接受声明 bytes/face 的 `source=pinned-file`；system、unknown、bundled-fallback 或把 icon source 用作正文均失败。实际 600 文件先通过独立 SFNT weight admission，不能 regular 改标签。component flag 按 Button/Input/Select 的实际适用字段比较，保留其他字段的 null；适用字段未知不得默认为 false，sample aggregate 不冒充最后一个子控件状态。

`demo_differential_runner_comparison_tests.py` 使用程序生成的局部像素与字体元数据控制，验证 1 logical px、小图标删除、局部空白、guard 外漏、错误字体、缺文字 bounds、无校准/过宽 threshold/mask 的拒绝。它们不运行 Pillow、浏览器、native 或 GPU，不是实际平台校准。完整 colors/gradient/shadow/alpha、长图 tiles、字体排版及真实 fault corpus 仍是后续范围。

## 有限 paint inventory 消费

`resources.paint_stack` 分别记录实际 `submitter.visible` 展开后的 command/instance membership、fragment publication revision、owner/fragment/surface/effect generation、composition program/group/coverage role，以及实际 glyph scene/atlas/font 来源。Input 的 role 数组不是绘制次序；使用其 `published_commands` 和 `command_ordinal`。const query 不同步、不请求下一帧。采图前后重读完整 paint 与 glyph inventory，变化即失败；capture binding 保留独立 run/capture nonce、step、scene revision、fence、renderer owner 和 device epoch。

Web 读取 `DOMSnapshot.getSnapshot` 的真实 paintOrder、stacking context、UA Text 与 pseudo；保留 selector/affix/popup/option 的各层，而不是把整个控件视为单一背景。四边 border、四角 radius、完整 shadow list、outline、clip、opacity group、实际 SVG paths 和 selection/caret 分别观测。shadow parser 只在括号外拆逗号。`caret-animation: manual` 和原截图 caret 行为保持；DOM 只暴露 collapsed Range edge 时，caret 宽度来自未遮蔽像素的局部贡献测量，不宣称 DOM 提供了 caret 厚度。正式图标仅消费共享 `formal_icon_source.py`；SFNT witness 使用共享 `font_priors` / `font_priors_from_bytes` / `outline_point`，不复制资源或栅格 oracle。

比较先验证 source、geometry、order 和 capture coherence，再按 flat / hard edge / glyph / soft shadow / joint / guard 分区；主分类数量之和必须等于完整 crop。已知材料的错误像素记录 visual/typography 差异，退出码为 1；缺 capture 或未覆盖完整 support 为 4；未知支持域为 3；schema/身份协议的非法字段仍按既有 2/4 处理。crop 包含两侧实际 paint support、popup、阴影尾部、outline 与至少 2 physical px guard，越出实际捕获范围时不 clamp、不缩小分母。

最多三个 source-only 可见色轴使用固定小矩阵、秩/条件数/量化、非负/和约束及实际 source-over endpoint domain；同色或共线来源只给 joint contrast plane，逐层来源身份不合并，不反推出任意 layer alpha。真正的 opacity group 完成后只应用一次。shadow profile/tail 输入是实际可见 alpha/contribution，包含 source alpha、paint opacity、group 和上层遮挡，不把弱阴影除以其 opacity 后套用 tail 上限。flat DeltaE、局部 mass/projection/contour、独立 SFNT 墨迹 witness、shadow profile/tail、joint 与 guard 全部 AND；raw byte residual 仅作诊断。

有限域明确拒绝 gradient/image、非平移变换、非标准 blend/filter、椭圆 radius、多色 border corner、未支持的 pseudo text/text shadow，以及确实切入 paint support 的 rounded mask/backdrop。仅证明不切入当前 paint support 的实际祖先 rounded mask 可以继续消费。不存在有效控制间隔或来源事实不足时保持非零，不能借此声明 6.3–6.5 完成。所有 C++/GPU/browser 行为、312 controls / 26 negatives / 16 metrics 的最终校准与普通 pilot/meta 结果均以父线程在最终整合源码上的实际执行为准；源码与纯 Python 测试不替代这些证据。
