# 共享运行时基础集成：Linux

起始 main：`4d722fbc19fbe0a604530d2de578556dbf368b85`。实际 Linux/GNU，CMakePresets / Ninja Multi-Config；所有构建 `-j2`。本记录区分当前 main 重新验证与导入的来源记录。

## A：保留位移所有权

- 来源：`c801bbc8858259d9136a6f574cf8c4975d5380f4`（父 `135ba70b74673bc6d7b34d1d83aa414fd0db3607`）；只合入该提交的 9 文件增量，保留 main 新增的 Node paint opacity/count/lifecycle 与 CMake 数据展示注册。来源 [独立验证](translation-ownership-linux.md) 不代替本次结果。
- 祖先写入拥有者根后停止，拥有者按父到子组合继承位移和局部 offset；所有权与 clip boundary 分离，默认初始化、销毁和复用清除标记。无 Table API、组件私有上传或 renderer ABI 修改。

- 当前 main：`cmake --preset linux-gcc-headless`；`cmake --build --preset linux-gcc-headless-{debug,release} -j2 --target rynui_translation_ownership_tests rynui_selection_component_tests rynui_modal_component_tests rynui_tabs_component_tests rynui_input_component_tests rynui_paint_opacity_tests rynui_paint_opacity_integration_tests rynui_gallery_document_viewport_tests` 均成功。
- CTest 精确选择 `^rynui\.(translation_ownership|selection_component|modal_component|tabs_component|input_component|paint_opacity|paint_opacity_integration|gallery_document_viewport|gallery_document_viewport_contract)$`：Debug **9/9**、Release **9/9**。Release 曾在最后链接尚未结束时提前运行，出现 **8 passed + 1 Not Run**（gallery_document_viewport executable 尚未生成）；完整构建成功后重跑同一 9 项全部通过，无断言失败、跳过或放宽。
- `^rynui\.(portable\.backend_boundaries|ant_design_gallery_catalog_(generator|contract))$`：Debug **3/3**，含实际 HEADLESS compile/link graph 与正负 Core include/link guards。两配置 primitive 的实际 link command 仅依赖 Runtime，`nm -C` 无 TableComponentHost/SDL 符号。
- clang-format 22.1.3 `format-code.py --check`：**1023 文件、0 失败**；doctor healthy、strict **107/107**、`git diff --check` 通过。A 交付提交：`42d55bcce6f1b41ed94c76f2d80ba1b58b2d9dc6`；提交后 fetch/rebase/push 与 ls-remote 核对一致，ahead/behind 0/0。

## B：Checkbox 空 label guard

- 来源：`f8ed1d95a8af469fede943d5deafddca2363cffa`（父为 A 的来源提交）；在 A 完整交付之后只导入该 3 文件增量。原被动后代拒绝循环不变，只有实际存在 label 时进入；Radio 不改动，旧断言全部保留。
- 新测试包含 Text/Icon 被动内容、Button/Input/嵌套交互拒绝与清理、旁边 1000 个无关控件的无 label Checkbox。来源 [诊断记录](checkbox-label-linux.md) 的 CPU 秒仅是来源同 Debug 对象对照，本次未重跑 1k/5k/10k 诊断，不作为 main、虚拟化或 GPU 性能证据，不据此宣称全局二次复杂度已解决。

- 当前 main 构建 `cmake --build --preset linux-gcc-headless-{debug,release} -j2 --target rynui_selection_component_tests`；`ctest --preset linux-gcc-headless-{debug,release} --output-on-failure -R '^rynui\.selection_component$'`：两配置各 **1/1**，0 断言失败/Not Run。Ninja 自动恢复构建日志后重编依赖，未改变目标范围或编译选项。
- clang-format 22.1.3：**1023 文件、0 失败**；doctor healthy、strict **107/107**、diff check 通过。B 提交为首次引入本节的 Git 提交，完整 SHA 见交付回执。

## 范围与不变量

- 目录生成 `--check` / `--self-test` 通过。97 个样例、458 个冻结 legacy ID、5 份各 66 顶层字段的完整 Theme golden 与起始 main 逐字节相同；未重写生成数据。
- 不增加组件完成项，change107 保持 **68/135**；Table 41.*、63.3、64.*、65.1 保持未勾选。
- 原始日志及符号输出仅在忽略的 `out/shared-runtime-primitives-integration/`。未运行 GUI/GPU、Windows/MSVC、系统 IME、AT、跨显示器或物理输入；不以 CPU 合同推断这些平台结果。
