# Tasks

## 1. 平台通用：依赖与段落分析

- [x] 1.1 锁定 SheenBidi 3.0.0/Unicode17源与license、BUNDLED/SYSTEM resolver与fixtures，增加 Core BidiAnalysis所有权/byte边界/paragraph levels/line runs/script查询；补 docs/bidirectional-text.md、纯模型及Unicode BidiCharacterTest/BidiTest conformance、非法范围/空段/复制lifetime回归，windows-msvc-headless Debug/Release构建与相关CTest、dependency/Core guards、format-code/doctor/strict validate/diff/evidence通过后提交。

## 2. 平台通用：shaping、折行与场景

- [ ] 2.1 增加 FontRuntime direction/script参数及TextEngine按level/script/font逻辑shaping、格式控制和replacement处理；补混合Arabic/Hebrew/数字/括号/fallback/ligature/控制字符测试及文档，common Debug/Release font/text/shaping回归通过，格式/规格校验后提交。
- [ ] 2.2 增加逐行visual glyph/cluster geometry、逻辑折行后L1/L2与GlyphScene/text decoration消费，TextState direction invalidation与ellipsis前缀重新分析；补硬/软换行、空行、glyph唯一覆盖、width-only更新不shape、ellipsis/scene/cache测试文档，common Debug/Release text/glyph/scene/Typography回归通过，格式/规格校验后提交。

## 3. 平台通用：光标、选择与输入

- [ ] 3.1 实现CaretMap逻辑/视觉双索引、run/soft-wrap affinity、visual adjacent/edge/nearest和无分配range coverage；补RTL/混合/ligature/换行/相同x/非法revision/query allocation测试及文档，common Debug/Release caret/text/display/editor回归通过，格式/规格校验后提交。
- [ ] 3.2 接入Input/TextArea共用affinity与视觉键盘/指针导航、非连续selection/preedit/selected view clip、IME area/display mask、logical clipboard/删除/history；补真实端口fixture、生命周期/受控/RTL/多行/Password/OTP/零分配idle测试与docs/input.md，common Debug/Release Input家族/interaction/session/clipboard/Typography及input_scene_allocation回归通过，格式/规格校验后提交。

## 4. 平台通用：公开方向与Gallery收尾

- [ ] 4.1 加入共享TextDirection与Text/Typography/Input family reactive direction API、非法枚举先验证及回退恢复，补standalone API/retained identity/无关slots测试、Gallery稳定ID、README/支持范围/architecture文档；common Debug/Release完整headless CTest与Gallery frame/catalog合同通过，记录平台/preset和库存，format-code/doctor/strict validate/diff/evidence后提交。

## 5. 原生分平台验收

### Windows

- [ ] 5.1 windows-msvc Debug/Release构建affected native tests/Gallery、native affected CTest/default smoke及专用真实D3D12/DXIL双向窗口；系统/1/1.25/1.5/2缩放下验证Arabic/Hebrew/数字/括号与换行、direction更新、视觉caret/不连续selection/preedit、mask/IME area、resize后pointer命中及popup/idle/dispose，记录hash/尺寸并审核GPU截图，独立提交Windows evidence。

### Linux

- [ ] 5.2 在实际Linux机器linux-native Debug/Release完成affected native CTest/Gallery smoke及对应window/GPU/system font/input/DPI/resize证据，独立提交；不重复共同逻辑，不用Windows结果代替。
