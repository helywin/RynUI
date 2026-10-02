# Tasks

## 1. 平台通用：多选状态与保留选项

- [x] 1.1 实现 CheckboxValue/Values、Group props/content、受控/默认多选、group disabled/skipGroup、动态 options 保留/删除/重排/失败回滚，更新 docs/checkbox.md；Windows windows-msvc-headless Debug/Release 验证 public header、Checkbox Group、selection/Radio/Switch 合同，覆盖三类值/无效输入/键盘顺序/焦点和捕获清理/富内容/回调销毁。实际使用 Windows MSVC headless；五项合同 Debug 5/5（1.57s）、Release 5/5（1.09s），格式检查 405 文件、doctor、47/47 strict 与 diff 检查通过。

## 2. 平台通用：焦点、主题与有限反馈

- [x] 2.1 实现 CheckboxRef/autoFocus/onClick、RTL、独立 Checkbox 主题算法/几何/色彩/标签/focus 与 wave 生命周期，更新 identity/JSON/goldens 和 API 文档；Windows windows-msvc-headless Debug/Release 验证 ref 绑定/跨线程/销毁、按压/禁用/半选/主题隔离/finite wave/reduced motion/idle，保持已有 golden 字段，运行受影响 Text/selection/Theme/animation 合同。九项合同 Debug 9/9（2.33s，补充身份隔离断言后 Checkbox 再通过）、Release 9/9（1.55s）；五份 golden 仅新增 Checkbox 区段和 identity，既有字段逐项不变；格式检查 405 文件通过。

## 3. 平台通用：Gallery 与集成

- [ ] 3.1 集成多选/动态 options、半选、RTL/ref/丰富标签/主题样例，更新生成原生 implemented 目录和收尾清单；Windows windows-msvc-headless Debug/Release 完整 build/CTest，native Gallery/catalog/font 合同；运行 generator self-test/check、clang-format 22/format-code.py --check、doctor、全量 strict 和 git diff --check。

## 4. 原生平台验收

### Windows

- [ ] 4.1 windows-msvc Debug/Release 完整 build 与受影响 CTest，实际 SDL/D3D12/DXIL、系统字体与 pointer/Space/Tab/ref/动态选项/controlled/disabled/RTL/主题/wave/resize/失活/销毁验收；系统 DPI 和 1/1.25/1.5/2 render scale，保存 GPU readback PNG、日志与 EXE/截图哈希核验，无动画 deadline 或闲置持续提交。

### Linux

- [ ] 4.2 实际 Linux 原生机器完成窗口系统/GPU/系统字体/输入与缩放验收，独立保存日志和 readback；不重复共同合同，不用 Windows 结果代替。
