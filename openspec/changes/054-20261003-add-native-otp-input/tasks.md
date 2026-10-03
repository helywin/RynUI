# Tasks

## 1. 平台通用：OTP 候选模型

- [x] 1.1 实现 grapheme/单行分格、容量/空洞、单格替换/整段粘贴、formatter prepare/版本校验、authoritative echo 与部分/完成判断；补 docs/input.md 和纯 model UTF-8/CRLF/组合字/emoji/截断/异常/重入/同本格不同尾部/controlled 测试，Windows windows-msvc-headless Debug/Release OTP model/text_editor/text_reconcile/input_display 回归通过，format-code/doctor/strict validate/diff 后提交。

## 2. 平台通用：公开 API 与 retained 格子

- [x] 2.1 增加 OTPProps/OTP/OTPRef/indexed typed separator，连接 OTP host、Input 单格成功编辑 hook/居中/padding/custom mask/敏感提示；实现初值/reactive Props、三尺寸/四变体/Theme/status、动态 length 前缀复用与资源清理，补 public API/布局/mask/容量/异常 separator/无关 Content 文档测试；common Debug/Release OTP/Input/Password/Search/TextArea/Theme/retained scene 及 input_scene_allocation 通过，格式和规格校验后提交。

## 3. 平台通用：导航与会话

- [x] 3.1 实现焦点/点击全选、第一空格重定向、自动前进、RTL 左右/空 Backspace、ref/autoFocus/indexed focus callbacks、IME ownership/stamp、clipboard/readonly/disabled/单格 undo 阻止、formatter/onInput/onChange/卸载重入保护；补相应文档与测试，common Debug/Release OTP/focus/interaction/clipboard/input/typography 回归通过，格式和规格校验后提交。

## 4. 平台通用：Gallery 与完整回归

- [ ] 4.1 加入 OTP 稳定 ID 样本（受控/部分完成/formatter/mask/separator/RTL/size/variant/动态 length），更新 README/Input 支持范围并保留 visual bidi 待收尾；common Debug/Release 完整 headless CTest、Gallery frame/catalog 合同通过，记录实际平台/preset与库存，format-code/doctor/strict validate/diff/evidence 后提交。

## 5. 原生分平台验收

### Windows

- [ ] 5.1 windows-msvc Debug/Release 构建 affected native tests/Gallery，native affected CTest/default smoke 与专用真实 D3D12/DXIL OTP 窗口通过；记录系统/1/1.25/1.5/2 缩放下四变体/Theme、部分/完成/粘贴/formatter/mask/IME area、separator/RTL/动态长度、window resize 后 pointer 命中、popup/idle/dispose 的日志与 GPU readback hash/尺寸，审核截图并独立提交可复核 Windows evidence。

### Linux

- [ ] 5.2 在实际 Linux 机器 linux-native Debug/Release 完成 affected native CTest、Gallery smoke 与对应 window/GPU/input/system font/DPI/resize 证据，独立提交 Linux evidence；不重复 common 逻辑，也不以 Windows 结果代替。
