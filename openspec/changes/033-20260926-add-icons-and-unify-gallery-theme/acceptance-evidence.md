# 验收记录

## 规划

用户已授权自主创建 change 并实现；本次追加滚动条悬浮/按下颜色反馈。033 strict validate 通过，全仓 27/33，通过项含 032/033；既有 013、015、016、017、018、021 失败。当前 CLI 不支持 `doctor --json`，且 `new/status/instructions` 拒绝数字开头的项目规范名称；按既有 spec-driven 结构手工建档。`git diff --check` 通过。此时仅规划完成，代码尚未实现。

## 图标资源

Ant Design 6.6.5 依赖 icons 6.3.4，其 SVG 包依赖范围从 4.6.0 起。已锁定 `@ant-design/icons-svg` 4.6.0 发布归档和 commit，九个官方单色 SVG 及 MIT 许可证随源码保存。FontTools 4.60.1 仅用于离线维护，生成器固定轮廓容器时间戳；`--check` 逐字节复现通过。Windows MSVC Debug `rynui.icon_asset_contract` 与 `rynui.dependency_lock` 2/2 通过。资源完成不代表公开 Icon 已实现。

## Icon 基础组件

公开 typed `IconProps` 支持 reactive name、tone、visible 和外部 LayoutStyle。内部沿用 Text 的 retained glyph 场景、Atlas、Theme/semantic foreground 及局部材质更新，但使用独立的内嵌图标轮廓字体，不进入普通文字 fallback chain。字体缓存以逻辑字号和实际 display scale 分离。

Windows MSVC Debug `rynui.icon_asset_contract`、`rynui.text_scene_service`、`rynui.text_component`、`rynui.text_component_frame` 4/4 通过。新测试从真实内嵌资源生成一枚非空眼睛位图，验证缓存重用、暗色文字颜色、不重跑父组件、切换眼睛状态时保持 identity/兄弟字形，以及 150% DPI 创建独立缓存。此时消费控件尚未切换图标。

## 控件、导航与滚动条

Password 的可见性操作使用 Eye/EyeInvisible；Input clear 使用 CloseCircleFilled；Search 默认操作使用 SearchOutlined。辅助操作跟随 Theme 和禁用状态，指针与键盘反馈共用语义颜色。Windows MSVC Debug 的 Password、Input clear、Search 定向测试通过。

导航使用透明 Text Button，亮暗态的前景色、悬浮/按下底色来自 Token；Gallery 的背景与根 Theme 一起切换，顶栏 Sun/Moon 图标可切换主题。73 个组件子项均为可点击 Button，使用稳定目录 identity 对应正文锚点；筛选隐藏目标时先恢复 All，再在布局更新后跳转。相关 Button、Theme、Gallery、viewport 测试通过。

滚动条的轨道和滑块分别有 idle、hover、pressed/dragging 材质；失焦、cancel、松开和几何变化会清理视觉状态。Windows MSVC Debug 的 ReferenceSurface、viewport、Gallery frame 和交互定向测试通过；Debug D3D12 连续拖动真实窗口 `--scrollbar-acceptance --acceptance-scale=1.0` 退出 0。

## Windows MSVC Release / D3D12 真实窗口

在 Windows 原生窗口使用 `windows-msvc-release`、Direct3D 12/DXIL 验收。实际系统 DPI 为 125%；`--acceptance-scale` 在固定 1280×900 像素窗口内映射逻辑 viewport，未修改系统 DPI。四种缩放各自启动窗口，滚动条验收均退出 0：

| 缩放 | 逻辑 viewport | 左栏 | 连续拖动、正文轨道/滑块、顶栏往返 |
| --- | --- | --- | --- |
| 1.0 | 1280×900 | 双列独立滚动 | passed |
| 1.25 | 1024×720 | 双列独立滚动 | passed |
| 1.5 | 853×600 | 窄布局，共享正文滚动 | passed |
| 2.0 | 640×450 | 窄布局，共享正文滚动 | passed |

每次执行 `--scrollbar-acceptance --acceptance-scale=<value>`，包含左右滚动条跨中段多次往返、每步三次输入以及同步后的 offset/几何检查。日志在本机 `out/033-release-scrollbar-*.log`；四次均 `continuous_drag=passed`、`header_navigation=passed`、`document_track=passed`、`document_drag=passed`、`exit_code=0`。双列的左栏滚轮/拖动也为 passed；窄布局左栏隐藏，因此这两项显示 not-run。

原生输入回归 `--smoke`、`--scroll-acceptance`、`--input-acceptance`、`--search-acceptance`、`--selection-acceptance`、`--password-acceptance`、`--input-clear-acceptance`、暗色 Password 与暗色 Input clear 共 9 次均退出 0。`--snapshot-navigation=click` 实际注入子项指针点击，记录 `navigation_requests=1`，正文进入 Button 条目；`--snapshot-navigation=hover` 下 Button 文字仍可见。GPU 图像显示亮暗字体、图标和窄布局均可读。

滚动条右侧正文滑块同一像素 `(1251,120)` 的 Release GPU 帧值：亮色 idle `(175,175,175)`、hover `(124,124,124)`、pressed `(81,81,81)`；暗色 idle `(94,94,94)`、hover `(140,140,140)`。数值来自导出的 BMP 像素，证明指针态有可见颜色变化。PNG 从同一 GPU 帧 BMP 无损转换，仅改变文件格式。

- [亮色导航悬浮](evidence/nav-hover-light.png) · [子项点击后](evidence/nav-click-light.png)
- [暗色导航悬浮](evidence/dark-hover.png) · [窄布局暗色](evidence/narrow-dark.png)
- [亮色 Password 与搜索图标](evidence/light-password.png) · [暗色](evidence/dark-password.png)
- [滚动条亮色悬浮](evidence/scrollbar-hover.png) · [按下](evidence/scrollbar-pressed.png) · [暗色悬浮](evidence/scrollbar-dark-hover.png)

真实窗口验收通过，不代表 Linux 窗口或 GPU 已运行；本 change 未要求 Linux 平台验收。
