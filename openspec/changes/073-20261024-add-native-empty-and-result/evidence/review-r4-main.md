# Astra R4：继承 Theme 发布前校验（2026-10-25）

在 main `15c356ca882959b1ff2078e8cd4e049a40354ae7` 合入 R3 `493f3fdaaf70bd55cc9a9f66dd3264aa4ac75e7d` 后，新增运行时回归实际复现 R4：父 Compact 算法使现有继承子 scope 的局部 Empty/Result font seed 1 派生为不可用字号，子解析抛错时父/兄弟已改变。修复前 `rynui.portable.theme_runtime` 失败，消息为 `failed descendant Theme resolution published a parent/sibling snapshot or notification`，见 ignored `out/theme-descendant-before-fix.log`。

公开 ThemeScope update 现在先解析受影响的现有继承子树，再提交父配置、snapshot、changed identities 和通知。解析阶段没有用户回调，失败保留全部既有状态；成功后沿用既有通知顺序。inherit=false、失效 weak child 和 snapshot 未变分支跳过。新 helper 让大 snapshot 临时量离开递归帧；不引入新的公开 API/状态，不承诺回滚任意用户通知回调产生的副作用。

新增 Theme runtime 测试覆盖 Empty/Result × 直接/间接子 scope，父/兄弟/中间/叶 scope 的 snapshot 地址、identity、generation、changed identities 与通知；连续两次提交相同失败配置仍拒绝，随后合法恢复每个订阅只通知一次。另覆盖 inherit=false 子树和已销毁 weak child。R3 的三个局部 `.49 dp` 字号拒绝、66 个非法值、阈值与实际 Text/glyph/DPI 回归继续保留。

实际 Windows MSVC `windows-msvc-headless` / Ninja Multi-Config：

- Debug **12/12，203.39 秒**；其中 Theme runtime 0.96 秒，Input scene allocation 203.36 秒。
- Release **12/12，26.87 秒**；Theme runtime 0.14 秒，Input scene allocation 10.58 秒。
- 同组涵盖 Empty/Result public API、Theme、component，Theme runtime/algorithm、Button、Switch、Input、Popconfirm、Collapse、Segmented 与 Input scene allocation。Release 的既有 allocation 输出：256 Inputs，selection/composition-selection 各 20,000 updates，均 **allocations=0、dispatch_allocations=0、sync_allocations=0**。
- `windows-msvc` Debug 既有 `rynui.theme_allocation` **1/1，0.13 秒**，验证 10,000 次等值更新零分配；不把这个 CPU 测试称为原生 GPU 验收。

精确命令使用 `out/typography-dev.ps1 -Preset windows-msvc-headless`、对应 Debug/Release targets 和 `^rynui\.(empty_result_(component|theme|public_api)|collapse_component|segmented_component|portable\.(theme_runtime|theme_algorithm|button_component|switch_features|input_component|popconfirm_component|input_scene_allocation))$`。Release 在独立配置构建完成后单独执行 CTest。原始 ignored 输出：`out/theme-descendant-{debug,release,release-build,allocation}.log`，allocation 细节在对应 CTest LastTest.log 中。

五份 golden 没有修改，两个配置的 Theme algorithm 检查通过。没有重复旧 148 项矩阵，没有新增 GPU、Linux 或系统 IME 验收。独立 Astra R5 随后复核此父任务补丁；此记录不提前声称 clean review。
