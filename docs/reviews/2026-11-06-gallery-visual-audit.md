# Gallery 全目录自动视觉巡检（2026-11-06）

本轮仅验收、列举问题，没有修复组件。结论为**发现问题，不能判为全组件视觉通过**。

## 实际范围与结果

- 拉取的产品源码为 `43cbb63b`，规划提交为 `bd9e9756`；截图程序另包含本轮随后提交的验收工具改动。组件、Theme、renderer 与 shader 源码未改。
- 遍历整个现有目录，不限最近 37 个提交：73 个目录条目中，63 个组件有直接样例，形成 67 个入口、97 个样例组；Notification 在 Message 中观察，ConfigProvider 仅覆盖全局 Theme 配置。另 8 个 planned/deprecated 条目没有可运行实现。
- Windows/MSVC `windows-msvc-release`，Ninja Multi-Config，最多 2 jobs；真实 SDL/D3D12 场景、系统字体、隐藏窗口 GPU 离屏回读。窗口未显示，不同步桌面光标和输入区。
- 四组请求配置为 Default/1440、Dark/960、Dark+Compact/728、Default/480，logical 高度 900；实际设备 scale=1.5，最终 client 尺寸以产物记录为准。每个样例按实际 viewport 分段，并滚出再滚回。
- 首批 **64/67 入口完成，1,062 张局部图**；补拍 Drawer、Upload、输入焦点、Switch 动画等状态后，按每入口最近成功批次合并为 **64 个入口、93 个样例组、1,378 张局部/状态图**。该数量含各主题和滚动往返，**不等于 1,378 项视觉通过**。
- Default 宽/窄 contact sheets 覆盖全部成功样例组，并复核下表问题的原尺寸图；另观察全部 64 个成功入口的首个 Dark+Compact 样例。其余深色图已生成，未逐张宣称目视通过。无黄金图自动相等验收。

## 阻断项

| 编号 | 入口 | 结果 |
| --- | --- | --- |
| B01 | Transfer | 首批及最终程序复跑均在 `stage=0 / width=1440` 返回 `0xC00000FD`（Windows 栈溢出），无截图。 |
| B02 | Cascader | 同上。 |
| B03 | Avatar | 同上，两个样例组均未取得截图。 |

这些是隐藏原生验收进程的实际失败；未定位调用栈，不能扩大为所有交互入口必然崩溃。没有提高 EXE 栈大小来绕过。

## 原尺寸截图确认的问题

下表记录实际外观，尚未把原因归到组件实现或 Gallery 组合方式；没有修改预期来消除失败。证据图均位于本地 `out/gallery-visual-20261106/issues/Vxx.png`。

| 编号 | 组件/状态 | 实际问题 |
| --- | --- | --- |
| V01 | Search / `search.features` | 局部 Dark+Compact 示例的文字、输入框和按钮消失，只剩一个游离的蓝色 vector 图标；下方 underlined 示例正常可见。 |
| V02 | Dropdown / 自定义 popupRender | 点击“保留自定义 popup”后，浮层横跨整个窗口，覆盖侧栏；菜单与输入区被拉成整窗宽。 |
| V03 | Calendar / 480 | fullCell 的 `Details` 按钮只显示 `D`；日程备注输入框只剩空外框，文字不可读。 |
| V04 | Pagination / 480 | `1–20 / 247`、每页 20 的最后页 `13` 只显示 `1`；Table 中复用的分页也可见。 |
| V05 | Tabs / EditableCard | 默认新增、关闭动作仅剩空白按钮外框，没有 `PlusOutlined` / `CloseOutlined` 图标。样例没有用空 slot 覆盖它们。 |
| V06 | Layout / 480 / responsive | “零宽度触发器仍可聚焦”的样例中，看不到外部展开触发器；已核对整窗上下文，未仅凭局部裁图判断。键盘可达性未在本轮验证。 |
| V07 | Masonry / 480 | Card 2/4 等卡片的“真实内容高度”文字紧压并穿过上方 Input 下边框，与卡片中其它文字间距不同。 |
| V08 | Spin / loading | “正在读取”叠在保留内容按钮的“内部操作”文字上，两段文字同时可见并重叠。 |
| V09 | ColorPicker / 打开面板 | 不透明 SV 色板、色相条存在规则的浅色网格/分片接缝；透明度条本身的棋盘格不计为缺陷。 |
| V10 | ColorPicker / RGB、HSB | 通道编辑框过窄，多位数字被右侧上下箭头区截断，例如 RGB 只露出首位，无法直接读出完整数值。 |
| V11 | ColorPicker / 自定义 panel | 自定义浮层撑到窗口宽，右侧大片留白；`extra` 的“原生自定义扩展区域”文字直接压在 SV 色板左上角。 |
| V12 | Carousel / Dark+Compact | 浅色 panel 背景上仍用白色 `panel-1` 文字，标题对比不足，几乎看不见。属于实际 Gallery 示例外观，不据此认定所有 Carousel 背景均错误。 |
| V13 | Breadcrumb / 富内容 trail | “首页 / 项目”文本在上方，两个分隔箭头落在下一行；富内容 Input+Button 项把路径分隔符推离相邻文本基线。 |
| V14 | Upload / PictureCard、PictureCircle | 文件动作区过窄，Preview、Download、Remove 在单词中间断行，呈现 `Previe / w`、`Downl / oad`、`Remov / e`。 |

## 可疑项与已排除误报

- **S01：**Warning+disabled 的 TextArea/OTP 使用饱和橙色整块背景和低对比文字；需核对状态 token 合同，未归为已确认实现错误。
- **S02：**多个默认 Form 的标签出现在字段右上方，字段从左边展开；需核对布局/labelAlign 约定。
- **S03：**窄屏独立说明和控件分离换行，Input 的“非受控输入”留在上一行；Alert/Result/Mentions 的部分句号独占下一行。按样例排版可疑项记录。
- **S04：**ColorPicker HSB 触发器显示 `38.82352941176471` 这类过长小数，显示精度待核对。
- InputNumber 的默认、小号禁用、大号只读三种高度是 Props 显式设置，**不算高度不一致缺陷**；Spinner 左减右加亦是显式 mode。
- Tree 的富内容备注部分出现在固定 240dp 内滚动 viewport 底部，不能据此判为错误裁切。Typography 两条往返告警仅为裁图高度差 1px，没有据此新增渲染缺陷。
- ColorPicker 补拍曾尝试对 disabled 目标做键盘激活，属于工具失败；过滤不可激活目标后最终补拍成功，不算组件失败。
- Switch 已补拍内容、图标、RTL、组件主题及四个采样时刻，未在这些图中确认新的稳定态缺陷；请求间隔 40ms 加上 GPU 回读耗时，不能作为动画流畅度或高刷验收。

## 逐组件覆盖

“采集”仅表示四组采集完成；未列问题表示已观察画面中没有新增明确问题，不代表完整功能或全部状态通过。Input 各家族分别采集，其余按直接 owner 列出。

| 组件 | 本轮覆盖/结论 |
| --- | --- |
| QRCode | 采集基础二维码；状态按钮已执行，但部分激活时二维码在 viewport 外，不计完整状态视觉通过。 |
| Carousel | 采集；V12。未验收 autoplay 连续动画。 |
| Calendar | 两组样例分段采集；V03。 |
| FloatButton | 采集 inline 示例并补整窗固定按钮上下文；未执行所有分组展开/拖拽。 |
| Button | 两组样例采集；未穷举 hover/press 波纹动画。 |
| Input | input/search/password/text-area/otp 五入口、11 个样例组，补焦点；V01、S01/S03，非 OS IME/剪贴板验收。 |
| TreeSelect | 采集并展开代表 popup；S02。 |
| Typography | 三组样例分段采集；1px 往返裁图告警已区分。 |
| Divider | 三组样例采集。 |
| Flex | 采集。 |
| Grid | 采集。 |
| Masonry | 分段采集；V07。 |
| Splitter | 四组样例采集；未拖拽全部 handle。 |
| Layout | 三组样例与整窗上下文；V06。 |
| Space | 采集及真实文档滚动往返，未新增确认中部横线。 |
| Icon | 采集内置/旋转/vector 示例；非全部 848 个图标逐个巡检。 |
| Anchor | 采集；构建工具链例外见下。 |
| Breadcrumb | 两组样例；V13。 |
| Steps | 采集各展示类型；未逐个切换步骤。 |
| Image | 分段采集并打开预览。 |
| Upload | 采集实际 fixture 文件列表、进度、成功、卡片/圆形及 custom；V14。无真实网络传输。 |
| Transfer | B01，无图像。 |
| Cascader | B02，无图像。 |
| Menu | 采集；未遍历全部 flyout。 |
| Dropdown | 补代表触发状态；V02；未覆盖右键全部分支。 |
| Pagination | 采集；V04。 |
| Tabs | 分段采集；V05。 |
| Switch | 基础与逐帧补拍；没有流畅度结论。 |
| Checkbox | 采集基础、动态和主题样例。 |
| Radio | 采集基础及 RadioButton；非所有 shared-edge 像素相位证明。 |
| Slider | 采集；未完整拖拽所有双端/垂直滑块。 |
| Tooltip | 补代表打开状态；没有右键入口通过声明。 |
| Modal | 补 basic、confirm-loading、centered、long-body。 |
| Avatar | B03，无图像。 |
| Badge | 两组样例采集。 |
| Tag | 两组样例采集；未完整执行拖拽重排。 |
| Drawer | 补四方向打开。 |
| Popconfirm | 补代表触发状态。 |
| Alert | 采集；S03。 |
| Spin | 补 loading 状态；V08。 |
| Progress | 采集 35% 线形、Circle、Dashboard；未重新构造 0% 与所有状态组合。 |
| Table | 分段采集；共享 Pagination 的 V04；非完整 editor/排序/行内滚动交互。 |
| ColorPicker | 补 solid、gradient、HSB popup；V09/V10/V11，S04。 |
| InputNumber | 采集，显式 size 差异已排除误报。 |
| Collapse | 采集默认展开状态；未逐个切换。 |
| Segmented | 采集；未验收所有滑块过渡。 |
| Empty | 采集三种示例。 |
| Result | 采集；S03。 |
| Form | 四组样例采集；S02。未重跑完整表单交互合同。 |
| DatePicker | 四组样例与代表 popup；S02。 |
| Select | 分段采集与代表 popup；未逐一遍历搜索及虚拟列表。 |
| Card | 采集。 |
| TimePicker | 三组样例与代表 popup；S02。 |
| Descriptions | 采集。 |
| Skeleton | 采集当前 loading/占位配置；非动画验收。 |
| Popover | 补实际备注浮层打开。 |
| Message | 补消息 open/update 和 Notification open/update/placement/status。 |
| Timeline | 采集；未把静态 loading 图标误判为被裁圆弧。 |
| Statistic | 采集；未验收所有数值动画。 |
| Rate | 采集初始半星及 typed 字符；未扫全部 hover tooltip。 |
| Mentions | 采集并注入有限 ` @a` 模型文本触发；S03，无 OS 键盘/IME 声明。 |
| AutoComplete | 采集与代表 popup；S02，未跑完异步全部分支。 |
| Tree | 分段采集；固定 viewport 底部备注已排除误报。 |
| Notification | 通过 Message 页间接观察，无独立注册样例。 |
| ConfigProvider | 仅全局 Default/Dark/Compact 配置，无独立注册样例。 |
| List | deprecated，无可运行样例，未验收。 |
| Listy、Tour、Watermark、Affix、App、BorderBeam、Util | planned，无可运行样例，未验收。 |

## 工具链、验证与复用

标准 MSVC 14.51.36231 Release 在既有 `anchor_component.cpp:323` 遇到 C1001 内部编译器错误，重试仍失败。仅该 OBJ 使用本地 `_CL_=/Zc:lambda-` 后，正常 preset 增量完成 Gallery 链接；没有改 Anchor 源码。**不能把本轮二进制表述为无绕过的标准 clean build 已通过**。该例外与 B01–B03 分别记录。

本轮新增的是 `--page-acceptance --page-visual`、隐藏窗口创建支持和 PNG runner。普通 Gallery 路径未切换为隐藏，组件公共 API 未变。Pillow 10.4.0 为本机已有依赖。

必要命令：

```text
cmake --build --preset windows-msvc-release --parallel 2 --target rynui_token_gallery
python -B tools/run_gallery_visual_acceptance.py --output out/gallery-visual-<fresh-name>
ctest --test-dir out/build/windows-msvc -C Release --no-tests=error -R "^rynui\.(platform_lifecycle|platform_event_pump|frame_renderer|sdl_text_input|text_clipboard)$"
out/build/windows-msvc/tests/Release/rynui_gallery_page_acceptance_tests.exe
python -B scripts/format-code.py --check --clang-format <clang-format-22.x>
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

平台/参数合同 **5/5 CTest 通过**，Gallery 参数测试 EXE 通过；格式检查 clang-format 22.1.3，1,058 个自有源码、0 failures。doctor 健康，strict validate **107/107**，`git diff --check` 通过。没有以完整 CTest、HEADLESS D/R 或桌面窗口矩阵冒充本次实际操作。

全部原图、contact sheets、逐条记录和日志只在已忽略的 `out/`。本地带图问题清单为 `out/gallery-visual-20261106/issues.md`；机器采集记录在主批、`-extra`、`-final-states` 和 `-blocker-recheck` 四目录。原始 report 的 `source` 是采集时 committed HEAD，验收 harness 当时尚有未提交改动，不把它称作完整二进制来源指纹。

未覆盖 Linux、真实桌面 swapchain/跨屏、其它 DPI、系统 IME/AT、高刷滚动/动画流畅度，也未穷举全部交互组合。OpenSpec 105 原有 Windows 6.1/6.2、Linux 7.1 不因本次离屏巡检而勾选。
