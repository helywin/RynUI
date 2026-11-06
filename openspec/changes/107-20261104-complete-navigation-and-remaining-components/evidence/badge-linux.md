# Badge Linux 定向验收

这是隔离来源提交的验证记录；当前 main 的重新验证见 [Avatar/Badge/Tag 集成](avatar-badge-tag-integration.md)。
2026-11-05；源分支 `t3/implement-avatar-badge-tag`，基线 `135ba70b`；不是合并后 main 证据。固定 Ant Design 6.6.5 `4a39f54842eade4e565ab336ef6097cd7e723cdd`：公开相关组合为 Ribbon，**没有 BadgeGroup**。原生名称 `BadgeRibbon`，不虚构 Web API。

- Linux GCC 16.2.1 / BUNDLED / HEADLESS/Recording / Ninja Multi-Config，`linux-gcc-headless-debug`、`linux-gcc-headless-release`，`-j2`。独立 Badge 阶段 D/R 各 **4/4 CTest 项**，最终共同受影响集合 **57 项全部通过**；计数与 Release 目录 target 补建说明见 [Avatar 共同结果](avatar-linux.md)。
- Badge native D/R 各 **46 条断言**：任意 String/有限 double/typed 富 count、旧 int 兼容、overflow/showZero/零范围恢复、numeric midpoint/reverse/new target、真实前后数字行中间几何；Processing 实际相位及隐藏连续 16 tick 零工作/零请求，活动窗口与 reduced motion 停帧/恢复、retirement 无 deadline。
- 富 count 的 Input 以真实内容高度参与几何/命中；输入回调中替换计数工厂可安全退役该编辑器，独立主内容草稿/ref 不受影响。计数改变不重建主 Button/Input，Ribbon 保留子动作，RTL/Start-End、折角向量和局部工厂失败准确退役均覆盖。
- 实际 Badge Gallery 提供数字 +1/-3、中间帧、Processing 开关、任意文本输入/typed 图标 count、Ribbon 和真实独立子按钮 callback；与 Avatar/Tag 三页 Recording **3/3 CTest**，串行 all 模式 D/R 各 **38 检查**。无伪造进度百分比。

独立阶段实际命令（Release 换 build preset 与配置）：

```text
cmake --build --preset linux-gcc-headless-debug --target rynui_badge_native_contract_tests rynui_tag_badge_component_tests rynui_tag_badge_public_api_tests rynui_tag_badge_theme_tests -j2
ctest --test-dir out/build/linux-gcc-headless -C Debug --output-on-failure -R '^rynui\.(badge_native_contract|tag_badge_.*)$'
```

共同 Gallery/source/Theme/Form/Input、公开例和 Core guards 命令/计数见 [共同结果](avatar-linux.md)。支持 overlay 只将 Avatar/Badge/Tag 三项更新为 implemented；源 **93+3=96 samples/458 IDs**、五份 **64 字段**完整 Theme，原字段无删除。父集成必须保留 main **97/66** 并重生成 union。

本分支 GPU/原生窗口自动验收 **0 项**；不据 CPU/Recording 声称 Vulkan、Windows/MSVC/D3D12、物理 IME/AT、实际跨显示器通过。63.3/64.3/65.1 不勾选；原始 log/JSON/images/binaries 仅 ignored out。
