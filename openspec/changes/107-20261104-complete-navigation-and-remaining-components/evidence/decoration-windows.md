# Watermark / Affix / BorderBeam Windows 验收

Windows/MSVC，正式 `windows-msvc-headless` Debug/Release 与 `windows-msvc-release`，Ninja Multi-Config、最多 2 compiler、默认栈。锁定 Ant Design 6.6.5；原始输出、源码身份与截图仅在 ignored `out/107-decoration-integration/`、`out/107-decoration-native-r2/`。

实现共享文字/图像水印、原生目标 Affix 与圆角周长单色 BorderBeam，复用既有 scene/stores/上传及 retained translation；无私有 SDL 路径或 GPU ABI 改动。修正内层 slot 误用外层 build context 导致的漏测量/命中；保留 Theme 堆上解析与 rvalue 构造。样例 Card 内输入/说明改用纵向 Flex，并加真实几何断言。Windows baseline 脚本使用 Git POSIX 路径。

HEADLESS 构建 decoration component/gallery、portable Theme algorithm/runtime/public API/default、retained_translation_phase、paint_opacity、image_rounding；同范围 CTest：Debug **21/21**、Release **21/21**。样例布局修正后只增量构建并重跑 Gallery，Debug/Release 各 **3/3**。九个组件 cases 覆盖真实 glyph/image、异步/取消/重入/退役、Affix 命中/目标/释放/异常、Beam 周长 phase/暂停/离屏/清理与 Theme；公开头编译探针在组件 target 内。

五套 Theme golden 由新构建的 portable theme_algorithm `--write-goldens` 生成；逐项核对原有字段数值完全相同，仅追加 Watermark/BorderBeam/Affix 三域及 identity。两个历史 baseline 精确比较通过。

真实 hidden D3D12/DXIL、系统字体、scale **1.5**，通过 `tools/run_gallery_visual_acceptance.py --page watermark --page affix --page border-beam --cases 4` 运行四组宽度 **1440/960/728/480**，主题分别 Default/Dark/Dark+Compact/Default。Watermark **40**、Affix **36**、Beam **108**，共 **184** 个有效捕获；三路 exit 0。复查输入/说明分离与实际沿周长运动；固定时钟四帧检查运动、reduced motion 不推进/无 deadline。Affix 有两条往返像素告警，差异 bbox 仅位于 onChange 反馈文字（分别 467×23、401×20 pixels），其余像素相同，不把它们写成零告警。

格式 clang-format 22.1.3、Gallery catalog/source 生成检查、doctor/strict 与 diff 检查通过；更新支持状态后目录 CTest **2/2**。未验证 Linux、其他实际 DPI、系统 IME/AT 或前台跨屏交互；本次 hidden 路径未调用平台输入区域/光标同步，不扩大为这些平台合同通过。
