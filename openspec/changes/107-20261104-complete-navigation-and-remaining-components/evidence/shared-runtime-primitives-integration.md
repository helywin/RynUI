# 共享运行时基础集成：Linux

起始 main：`4d722fbc19fbe0a604530d2de578556dbf368b85`。实际 Linux/GNU，CMakePresets / Ninja Multi-Config；所有构建 `-j2`。本记录区分当前 main 重新验证与导入的来源记录。

## A：保留位移所有权

- 来源：`c801bbc8858259d9136a6f574cf8c4975d5380f4`（父 `135ba70b74673bc6d7b34d1d83aa414fd0db3607`）；只合入该提交的 9 文件增量，保留 main 新增的 Node paint opacity/count/lifecycle 与 CMake 数据展示注册。来源 [独立验证](translation-ownership-linux.md) 不代替本次结果。
- 祖先写入拥有者根后停止，拥有者按父到子组合继承位移和局部 offset；所有权与 clip boundary 分离，默认初始化、销毁和复用清除标记。无 Table API、组件私有上传或 renderer ABI 修改。

- 当前 main：`cmake --preset linux-gcc-headless`；`cmake --build --preset linux-gcc-headless-{debug,release} -j2 --target rynui_translation_ownership_tests rynui_selection_component_tests rynui_modal_component_tests rynui_tabs_component_tests rynui_input_component_tests rynui_paint_opacity_tests rynui_paint_opacity_integration_tests rynui_gallery_document_viewport_tests` 均成功。
- CTest 精确选择 `^rynui\.(translation_ownership|selection_component|modal_component|tabs_component|input_component|paint_opacity|paint_opacity_integration|gallery_document_viewport|gallery_document_viewport_contract)$`：Debug **9/9**、Release **9/9**。Release 曾在最后链接尚未结束时提前运行，出现 **8 passed + 1 Not Run**（gallery_document_viewport executable 尚未生成）；完整构建成功后重跑同一 9 项全部通过，无断言失败、跳过或放宽。
- `^rynui\.(portable\.backend_boundaries|ant_design_gallery_catalog_(generator|contract))$`：Debug **3/3**，含实际 HEADLESS compile/link graph 与正负 Core include/link guards。两配置 primitive 的实际 link command 仅依赖 Runtime，`nm -C` 无 TableComponentHost/SDL 符号。
- clang-format 22.1.3 `format-code.py --check`：**1023 文件、0 失败**；doctor healthy、strict **107/107**、`git diff --check` 通过。A 交付提交由本文件对应 Git 历史标识，后续阶段记录其完整 SHA。

## 范围与不变量

- 目录生成 `--check` / `--self-test` 通过。97 个样例、458 个冻结 legacy ID、5 份各 66 顶层字段的完整 Theme golden 与起始 main 逐字节相同；未重写生成数据。
- 不增加组件完成项，change107 保持 **68/135**；Table 41.*、63.3、64.*、65.1 保持未勾选。
- 原始日志及符号输出仅在忽略的 `out/shared-runtime-primitives-integration/`。未运行 GUI/GPU、Windows/MSVC、系统 IME、AT、跨显示器或物理输入；不以 CPU 合同推断这些平台结果。
