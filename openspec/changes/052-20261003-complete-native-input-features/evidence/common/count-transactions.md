# 统计与单次编辑事务验收

环境：Windows 11、MSVC、Ninja Multi-Config、windows-msvc-headless Debug/Release。

15/15 CTest：Debug 87.63 s，Release 10.77 s。覆盖 input_count、input_component、input_variant、input_public_api、text_editor、text_editor_allocation、text_history、text_reconcile、text_clipboard_command、input_display、text_input_session、password_component、search_component、typography_interaction、input_scene_allocation。

新增合同包括 scalar/grapheme/自定义 byte 统计、标签函数、硬上限显示回退、零软上限、显式状态优先、原文不被软上限裁剪、IME preedit 不统计/不裁剪、reactive 配置不取消 IME、提交/粘贴裁剪、输出 CR/LF 归一化及最终硬限制、controlled 接受/拒绝和 authoritative 绕过、undo/redo 绕过、异常回滚、选择/值重入冲突、递归编辑拒绝、卸载后槽复用、标签异常/同步卸载、空闲不调用函数及不重执行内容。

clear/custom suffix/counter 水平排列，隐藏计数为零宽高，窄输入区宽度不为负。计数在新增 prepare_auxiliary_layout 阶段更新，使 retained 标签与尺寸先于本帧 layout；回调执行使用身份快照，随后重新解析 component/editor。

TextEditorStore 使用共享编辑状态，事务暂时持有生存期，删除立即使其退休。编辑 transform 位于发布/history.prepare 之前；回调拥有候选和函数副本，前后校验 transaction generation，authoritative 与历史恢复跳过 transform。

首次 Debug 分配基准发现每帧三次分配：MSVC 的空 vector iterator proxy 来自计数准备和先前 autoFocus 空队列 exchange。修复后，100 次独立探针两模式均 dispatch/sync allocations=0；正式 input_scene_allocation 在 256 个 Input、selection/composition-selection 各 20,000 次更新下通过零分配断言，并验证无 atlas/effect 上传及无额外 shape/measure/layout/topology。text_editor_allocation 亦通过。

对应命令：以 Debug/Release common preset 构建上述 rynui_portable_* 目标，并运行对应名称的 CTest 正则。平台窗口/GPU/OS 验收由任务 6 独立记录。
