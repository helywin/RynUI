# Tasks

## 1. Space 对齐与方向（平台通用）

- [x] 1.1 实现 Auto/Baseline、typed orientation/旧 vertical 最后配置优先、LTR/RTL；默认 H Center/V Stretch，迁移旧 Start 意图并更新 docs/flex-space.md。public API、H/V/RTL/真实字体基线、缓存/非法更新/销毁测试在 Windows `windows-msvc-headless` Debug/Release 通过，格式/OpenSpec/diff 检查后提交。

## 2. Separator（平台通用）

- [x] 2.1 实现 SpaceSeparator typed slot、split alias、初始 item/separator 交错的共同 layout/scene/focus 顺序；空/单项不执行，富内容与 wrap/Theme/方向变化 retained。加入边界数量、顺序、slot 抛错回滚、动态销毁/零约束合同与文档，HEADLESS D/R 受影响测试与格式/OpenSpec/diff 通过后提交。

## 3. Compact 与 Button（平台通用）

- [x] 3.1 实现 typed SpaceCompact H/V/RTL/size/block、最近上下文/explicit size 优先、nested/空单项与共同 retained 测量/连接 geometry；Button 六视觉变体、dash/外角/focus/有限 wave 与状态 seam 优先接入。HEADLESS D/R mixed size/nested/status/删除捕获/idle/零资源测试与文档/格式/OpenSpec/diff 通过后提交。

## 4. 其它已有控件与 Addon（平台通用）

- [x] 4.1 接入 Input/Password/Search/RadioButton，保留 editor/IME/selection/ref 身份；实现 SpaceAddon typed 内容、四 variant、disabled/status、尺寸与 Theme 驱动视觉。加入 mixed group/H/V/RTL/三尺寸/block、嵌套、主题/状态边、键盘/clipboard/销毁合同和 docs；HEADLESS D/R 受影响测试及格式/OpenSpec/diff 通过后提交。

## 5. 集成（平台通用）

- [x] 5.1 增加 Gallery separator/Compact/mixed/Addon 样例，目录与原生收尾清单同步；生成器 self-test/check、HEADLESS D/R 全部 CTest、文档/格式/OpenSpec/diff 通过，记录实际平台/preset/结果后提交。

## 6. 平台集成

### Windows

- [ ] 6.1 `windows-msvc` Debug/Release 构建、受影响原生 CTest 和 default-stack Gallery 真窗口 smoke；实际 D3D12/DXIL/system fonts 系统及 1/1.25/1.5/2 render scale 验证 separator、连接角/单一 seam/状态优先、mixed 控件/编辑、三尺寸/H/V/RTL/nested、resize/指针命中/有限 motion/idle/销毁。保存 EXE/PNG hashes、日志与复现脚本，检查后独立提交。

### Linux

- [ ] 6.2 在实际 Linux 机器完成本 change 的窗口/GPU/shader/系统字体/input/DPI、组合控件/编辑、resize/idle/销毁验收，保存证据并独立提交；不重复平台通用合同。
