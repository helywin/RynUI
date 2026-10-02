# 主题与有限反馈证据

2026-10-02，Windows/MSVC，Ninja Multi-Config，windows-msvc-headless Debug/Release。

- 新增正式 Alias opacityLoading（默认 0.65）与 Switch inner margins、handleShadow、wave、组件派生颜色/字体/focus；正确处理组件独立主色算法与 focusOutline=false，Theme identity/继承/JSON 和非法 opacity/extent 验证。
- 手柄按压伸展 30%，RTL 保持外边缘；loading 取消 pointer/keyboard 按压和 hover，保留有效焦点，抑制阴影；焦点与内容/轨道/手柄/spinner 透明度服从主题。
- 共同 RetainedSurface 支持 shadow_shape 与 shadow_fill_offset，将手柄阴影插入 track 与 handle fill 之间，保持既有十层 quad、surface 和正常阴影 identity；不修改 renderer ABI。
- TextSceneService 新增祖先内容 opacity 乘数，叠加 Text 自身 opacity，覆盖 glyph 与装饰而不触发 shaping；颜色/阴影/opacity-only 更新不测量或重跑 slots。
- Switch 有限 wave 使用第三个 scalar target 和 lazy outline range；测试覆盖重启、结束 idle、disabled/loading/wave prop/window inactive/theme motion/reduced motion/销毁，最终没有 effect/target/deadline 泄漏。
- HEADLESS selection/Switch features/Radio/Button/共同 surface/TextScene/TextComponent/Theme algorithm/runtime：Debug 9/9（3.85 秒）、Release 9/9（2.53 秒）通过。日志 out/046-switch-stage2-final.log；额外 native Theme algorithm/runtime Debug/Release 2/2 通过。
- validate_goldens.py 比较 485c9f1，五种主题只增加 Alias 一个字段、Switch 十三个字段及 identity，其余值全部保持；clang-format 22.1.3 自有 402 文件检查 0 failures。

以上是平台通用合同；Gallery 与真实 Windows GPU/系统字体/输入/DPI 验收尚属后续任务。
