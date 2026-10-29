# 源码检查点（未构建、未验收）

这是 2026-10-28 暂停时的历史检查点，规划父为 `7ca31dc26fa59a924ed7163043609c66f13c26c1`。当时用户需要使用电脑，禁止窗口操作和构建，因此只保存未提交源码草稿，不勾选任务、不提交未验证实现。2026-10-29 用户恢复实施后的新构建结果另见 [平台通用图标验证](01-bundled-control-icons.md)。

- DatePicker 四个年月导航、Menu 的开合方向、Table 的三种排序状态已改用内置 Icon。
- Pagination 的翻页和跳转使用 Icon，数字继续使用 Text；固定 slots 保持按钮身份，局部 icon_gap 为零，避免空文字／隐藏图标留下间距。
- 四个原组件测试中增加 bundled source、非普通文本字体的实际 glyph、受控拒绝和状态转换、完整 component generation 检查；分页额外覆盖跳转与数字之间的切换。测试源码已写，尚未执行。
- `docs/icon.md` 记录操作标识不使用相似文本字符的规则；普通文字和用户富 slot 不改。

实际只执行 clang-format 22.1.3 和差异检查：九个新增／修改 C++ 文件已格式化；现有 tracked 803 文件的格式检查零失败；新增未跟踪测试 helper 另行 dry-run。`git diff --check` 通过。规划阶段显式 OpenSpec 1.14 doctor healthy／strict 99/99，通过不表示实现验收。

未执行编译、CTest、inventory、性能矩阵或任何桌面输入；新程序尚不存在，当前二进制不包含这些草稿。Gallery 新 Icon slots 的实际数量、受影响合同和 Windows 显示均待后续构建核实。用户反馈的滚动卡顿仍未修复，不将图标替换当作性能改善证据。
