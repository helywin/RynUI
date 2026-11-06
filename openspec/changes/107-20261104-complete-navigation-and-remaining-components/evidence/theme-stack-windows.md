# Theme / Select 默认栈回归

Windows/MSVC，正式 `windows-msvc-headless` 的 Debug/Release，Ninja Multi-Config，最多 2 jobs。保持原编译选项与默认栈；测试后台运行并抑制系统崩溃对话框。原始输出只在忽略的 `out/107-theme-stack/`。

解析结果使用每次调用独立的堆工作区，私有快照构造减少大值中转，ThemeScope 直接在堆上构造候选快照；Select 子主题只发布实际使用的 Input override。Theme binding 不再缓存尚未通过校验的输入。Card 夹具仅调整存储，保留原嵌套控件与失败断言。

实际构建使用 `cmake --build --preset windows-msvc-headless-debug` / `windows-msvc-headless-release`，定向构建并运行 portable 的 `card_theme`、`card_component`、`theme_scope_component`、`theme_runtime`、`theme_algorithm`、`default_theme`、`theme_public_api`、`select_theme`、`select_component`、`select_public_api`、`empty_result_theme`、`prop`、`theme_allocation`：两种配置均 **13/13** 通过。集成批次另含 Card/Empty、Listy 两个新入口，属于其各自阶段，不计入本阶段数量。

原先出现 `0xC00000FD` 的 Card 两条路径现已通过；新增同一非法绑定连续拒绝、有效恢复及 dispose 断订阅回归通过。24 层 Theme 挂载/释放、父子失败发布校验及现有五套 golden 比较通过，golden 未改动。10,000 次等值更新检查为零分配、零多余帧请求。

clang-format 22.1.3 检查 1060 个自有源文件、0 失败；OpenSpec doctor healthy、strict 107/107，diff 检查通过。此阶段没有原生 GPU、Linux 或新增组件视觉完成结论，不勾选尚未完成的组件与平台任务。
