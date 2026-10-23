# 平台通用验收

## 检出与环境

- 从已提交的 `69689046bc9a99b94c98086cb3c849b79015acbc` 开始，独立分支 `t3/native-segmented-component`。
- 规划提交：`7823da2203f03267dc8834894668c587628ad020`；实现提交：`1b0ae2d96b24d611b6f875aae076b60a1fe5bc0f`。
- Windows x64 / MSVC 19.51.36256.0，`windows-msvc-headless`，Ninja Multi-Config，HEADLESS / RECORDING，BUNDLED 锁定依赖。编译最多 4 jobs。
- 构建目录为本检出的 `out/build/windows-msvc-headless`。只读取并复制 root 的环境参考脚本到本地 ignored `out/segmented-dev.ps1`，将 jobs 改为 4；没有执行 root 构建、复制 root 未提交文件或修改 root。
- Ant Design 固定为 6.6.5 / `4a39f54842eade4e565ab336ef6097cd7e723cdd`。本次读取的 `components/segmented/style/index.ts` SHA256 为 `8cfe6f723214bc49092578246883107d48a808048a6789cea67429f5d310ddaa`，与 sources.lock 一致；没有升级依赖或修改 catalog/support overlay。

## 结果

| 检查 | Debug | Release |
|---|---|---|
| Segmented public API / Theme / component | 3/3，通过，1.05 s | 3/3，通过（包含在下列 17 项集内） |
| 既有 Theme、Radio/Select/Tabs component 与 backend 边界定向回归 | 14/14，通过，4.51 s | 合并 Segmented 后 17/17，通过，2.19 s |

Debug 回归涵盖 `feedback_theme`、`tag_badge_theme` 与 portable 的 `theme_algorithm`、`theme_runtime`、`drawer_theme`、`popconfirm_theme`、`tabs_theme`、`select_theme`、`modal_theme`、`card_theme`、`tabs_component`、`select_component`、`radio_component`、`backend_boundaries`。Release 使用同样列表并加上三项 Segmented tests。原始定向输出见相邻日志。

测试覆盖受控拒绝/回写、未知/禁用显式 key、默认值/首个可用项/空列表/全禁用、响应 label/disabled、固定节点与 slot 身份、capture 取消、单组 Tab 入口、双方向循环箭头/Home/End/Enter/Space、重复激活键、失焦/窗口失活、实际横纵布局、有限父宽拉伸、等宽 resize/零宽、富标签换行/icon、selected/hover/pressed/disabled 前景与 focus-visible ring、Modal 的背景阻挡与内部键盘、Spin 阻挡/恢复、onChange 自毁/祖先销毁及 Input blur 销毁。

实际 item 几何在 1/1.25/1.5/2 四个 fixture 字体 scale 下验证；这是平台通用逻辑测试，不是 OS DPI 或 GPU 视觉验收。纯颜色 Theme 更新验证 root/sibling measure count、glyph shape count、effect geometry count 不变，slots 不重跑，idle 不请求新帧。

## 旧 Theme 字段完整性

通过 `rynui_portable_theme_algorithm --write-goldens` 生成当前快照后，运行：

```text
python openspec/changes/071-20261023-add-native-segmented/evidence/compare-theme-goldens.py
```

与 `git show 6968904:<path>` 的五份 golden 逐字段递归比较：default/dark/compact 各 2303 个旧叶字段，dark-compact/compact-dark 各 2304 个，总计 **11,517** 个全部相同。仅新增顶层 `segmented` 和更新整体 `identity`；既有 Drawer、Popup、TagBadge、feedback 及其他字段均保留。没有恢复 Theme 大型 input token 的栈复制。

## 共同检查与范围

- `python scripts/format-code.py --check`：clang-format 22.1.3，540 个文件，0 failures。
- `openspec doctor --json`：healthy；`openspec validate --all --strict --no-interactive`：67/67；`git diff --check`：通过。
- 初期迭代修正了测试辅助事件的 move button、重复 focus 请求返回值、Modal 对 Spin 阻挡后的焦点回收断言，以及销毁后测试辅助函数再次查询旧 ComponentId 的问题；最终 Debug/Release 均通过。
- 本次只声明上述平台通用验收，没有 native GPU、Linux、系统字体或人工 IME 结果。滑块动画、动态选项拓扑、RTL、OS accessibility adapter、完整 Web aliases 仍为明确首版边界。
- 本地提交；没有 push、PR、archive、子任务，也没有合并 Table/Menu/InputNumber/Collapse 或改动全局 Gallery、README、component-completion 文档。
