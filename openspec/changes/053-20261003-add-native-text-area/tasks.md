# Tasks

## 1. 平台通用：多行编辑与光标基础

- [x] 1.1 增加固定 TextEditMode 和共用 LF 归一化，实现按实际 TextMeasurement 行映射的 caret affinity/二维查找/行移动；补 docs/input.md 与 CRLF/formatter/limits/history/controlled/empty/trailing LF/ligature/Unicode/wrap/异常原子性测试，在 Windows windows-msvc-headless Debug/Release 构建并运行 text_editor/text_history/text_reconcile/text_caret_map/text_scene_service/input_display/selection allocation 测试，format-code/doctor/strict validate/diff 通过后提交。

## 2. 平台通用：TextArea API 与尺寸

- [ ] 2.1 增加 TextAreaProps/TextArea/TextAreaRef 和 Input 共用宿主多行挂载，实现 rows/wrap/autoSize/外部尺寸优先、Theme/四变体/size/status、统计及右上 clear/下方 count 布局；补值/焦点/ref/布局/软硬限制/controlled/重入/非法配置/retained 身份文档与测试，在 common Debug/Release 运行 text_area/input_public_api/input_count/input_variant/input_component/password/search/theme/布局合同，格式和规格校验后提交。

## 3. 平台通用：多行交互与呈现

- [ ] 3.1 实现二维点击/拖选、跨行 selection/preedit coverage、Enter/primary Enter、上下/Page/行与文档 Home/End、期望 x、caret reveal/wheel、resize capture 和安全 onResize；补 IME/stamp/clipboard/history/readOnly/disabled/卸载/无关内容/scroll 不 shape-raster 测试及文档，在 common Debug/Release 运行 text_area/selection/input_scene_allocation/focus/interaction/text_scene_service/typography 回归，格式和规格校验后提交。

## 4. 平台通用：Gallery 集成

- [ ] 4.1 添加稳定 ID 的 TextArea 多行/四变体/count/autoSize/resize/readOnly 样本，更新 README/Input 支持范围且仍明确 OTP/bidi 待收尾；common Debug/Release 完整 headless CTest 和 Gallery frame 合同通过，format-code/doctor/strict validate/diff 和 evidence 后提交。

## 5. 原生分平台验收

### Windows

- [ ] 5.1 windows-msvc Debug/Release 构建 affected native tests/Gallery，运行 native affected CTest、default smoke 和专用真实 D3D12/DXIL TextArea 窗口；记录系统/1/1.25/1.5/2 缩放、四变体/Theme、换行/selection/IME区域、autoSize/count/clear、wheel/resize 后 pointer hit、popup/idle/dispose 的日志与 GPU readback hash/尺寸，审核截图并可复核后独立提交 Windows evidence。

### Linux

- [ ] 5.2 在真实 Linux 机器 linux-native Debug/Release 完成 affected native CTest、Gallery smoke 及对应 window/GPU/input/system font/DPI/resize 证据，独立提交 Linux evidence；不重复 common 逻辑也不以 Windows 结果代替。
