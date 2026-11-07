# Gallery 双语与配置继承

2026-11-08，Windows/MSVC；正式 `windows-msvc-headless` Debug/Release 与 `windows-msvc` Release，Ninja Multi-Config。公开应用通过 Gallery 和独立 public API 编译入口使用，未新增私有业务 API。

## 平台通用与编译合同

Release 相关集成选择单轮 80/80，包含完整无参数兼容入口、公开 i18n、六组真实 Gallery 场景、page/controller/host/source、Form/Table 和受影响的 Input/Selection/Compound。Debug 的 18 个页面及 host/frame 入口分批通过：首轮 15 项，修正旧定位/预期后的三个入口 3/3；不把失败轮次报告为全通过。

ConfigProvider 的 12 个入口各通过 Debug/Release。纯 ellipsis 的真实父 Button 点击及相关 Typography/Tooltip/Popup 合同各通过 11/11。追加的 Popover 固有尺寸修正后，360/480/640/728/960/1440 中英 Header 几何及操作直接检查、七项 popup/statistic 合同均在 Debug/Release 通过。后续修改仅定向复验受影响合同，未重复无关全量矩阵。

扩展旧 Popover 回归发现属性 getter 自毁后产生额外 StaleContext 的回归；共享默认投影现在直接返回已计算值，由原生 sink 检查 owner/generation，不再访问已退役上下文。保留公开过期 I18n handle 的错误合同。Input/Button 自毁场景、20 个既有 Popover 入口、ConfigProvider、popup/statistic、i18n context 与 Input/Button/Selection 合计各通过 Debug/Release 43/43。旧壳的六组 Debug 滚动、主题、缩放、导航与空闲场景另通过原 `--legacy-shell-only` 入口，退出 0。

源码检查覆盖 2169 条消息、112 个真实示例、73 个 catalog 和 220 条精确 literal 来源；负例检查覆盖遗漏、来源变化和乱码。格式、目录生成及 OpenSpec 结果独立记录，原 Theme golden 保持不变。

## 原生平台结果

```text
cmake --build --preset windows-msvc-release --parallel 1 --target rynui_token_gallery
python tools/run_gallery_page_acceptance.py out/build/windows-msvc/examples/Release/rynui_token_gallery.exe --preset windows-msvc --i18n --timeout 120 --log-dir out/108-gallery-windows
```

实际 SDL hidden/offscreen、D3D12/DXIL、系统字体、display scale 1.5：Modal、Select、DatePicker、Form、Table、Empty、ConfigProvider 共 7 条入口，263 项检查、87 次提交、24 张局部 GPU 捕获，runner/native 均通过。另有整帧调试图；24 不是黄金图像比较数量。

覆盖实际语言按钮 zh→en→zh、Latin/CJK/Arabic 编辑与 readback、反向选区、preedit/undo、保持打开的弹层、ConfigProvider size/direction/disabled 与显式及嵌套 false、resize、导航退役及 idle。真实配置拒绝保持原 snapshot/generation，恢复和同步语言回调重入保持较新的语言、当前 page 和活动编辑状态。

offscreen 不执行系统 input-area/cursor 桥接，因此这两项实际为 false；没有据此声明系统 IME、人工桌面、跨物理显示器或 Linux 通过。原始输出、局部图和日志只保存在 ignored `out/`。

## 修复与合同

ConfigProvider 的来源保留未设与显式 false/Middle/Auto；Group/Form 的显式本地 gate 保持权威。公开 `SelectionOptionDisabled` 兼容旧 bool 读写并保留来源，迁移说明见 `docs/config-provider.md`。

Gallery 的翻译不承担自动化身份：导航、真实标题、字段和动作通过稳定目标定位。延迟 Drawer slot 恢复自己的 draft 构建作用域。纯 ellipsis 文本使用共享 Text 路径，实际父 Button 点击不被无交互的 wrapper 拦截。Avatar 无内部滚动余量时交给外层 Popover，实际滚轮覆盖全部 126 个隐藏成员。

Popover 触发器按自身内容测量，保留显式布局约束；不会因内部通用容器填满可用宽度而把设置动作挤到下一行。Gallery 在较窄宽度保留多行顶栏高度，语言切换时不遮挡下方导航。

历史 frame 回归区分原始校验结果和本地化显示、字体图标和原生异常插画、日期键变更和 Theme/resize；仍检查实际 glyph/paint、输入、保留身份和有限窗口裁剪。
