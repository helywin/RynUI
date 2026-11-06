# Image actions / Form 作用域主线修复：Linux

- 来源 `8e6151b877a3ee07de741150fe79aef391198864` 的四文件增量与 `071f6132718a7cfcd3556440cc7e4bd6d5629fa4` 的单行 native 来源补证，保留已交付 7dab/f757 对应实现与透明读回测试。基线 main `016c659b9df25abaf4253baa45a884e1ffbaefcd`；D 自有 33 文件已保存暂撤，不混入未交接 WIP。
- 来源 [8e 记录](image-form-linux.md) 的 84 检查及 [071 补证](image-cover-linux.md) 的 2 页/120 检查/23 帧仅为来源运行，不冒充当前 main 复测。
- 初次扩展 fixture 实际 81 检查成功、第 82 失败、后四项未执行。独立只读诊断确认：编辑器提交成功，typed onChange 正确；外层 Form 根位于 preview Modal 外，旧 Form scope eligibility 拒绝 store/touched/通知。两个字段均未改变，不将原错误消息解释为外层被写入。
- 局部修复 Form host scope eligibility：保留根快路径，fallback 只检查本 host 同一 Form/model 的活动 Modal overlay 内、完整 Item id/field owner/挂载注册票据/当前 Registration 一致的显式 Item。私有 Input、无字段布局 Item、其他 Form/窗口和失效注册不授权。通用 interactive、Registry、Input、model.available 及字段/事务 gates 保留；背景控件继续拒绝原生输入，已有程序 set/reset 语义保留。
- 新异常回归实际发现原生字段 setter 抛错后只取消校验而留下 Form UI。仅在字段发布点补 exact Form 退役并保留原异常，与既有失败批次合同一致；没有吞异常、改 source authority 或取消其他 owner。
- 实际 Linux/GNU，CMakePresets / Ninja Multi-Config / `-j2`。先原样冻结 86-check fixture：Debug/Release 各 **1/1、86 检查全部通过**；再 pending-j 更强 dispatch 条件：两配置各 **1/1、86 检查全部通过**。旧冻结材料及二进制未覆盖，中断过的诊断重建不计通过。
- 新 `rynui.form_modal_scope` Debug/Release 各 **1/1、191 检查**，覆盖 store/touched/通知、Enter/整表校验/reset、背景输入拒绝、关闭/reopen/旧 epoch、disabled/readOnly/hidden/Spin/blocked、末 Item 退休/代际重绑、private/layout/nested Form/foreign Modal/同号跨窗口、受控拒绝/接受/抛错与 setter/元数据回调内退休/重绑、异步取消/旧完成及 idle。
- 新 fixture 曾误把恢复祖先当作预览自动重开、把只读焦点当作有效文本会话；依据实际状态/调用栈修正准备后，拒绝写入/通知/命令的断言保留。
- 最终受影响选择器为 `^rynui\.(form_(model|integration|component|lifecycle|advanced|interaction|async|modal_scope)|image_(lazy_preview|component_(controlled|controlled_group|preview_source|rich|lifetime|boundaries|controller|group|stack))|input_component|modal_component|portable\.(modal_boundary|interaction_registry|text_input_session|focus_lifecycle))$`。Debug/Release 各 **24/24**，其中最终 strong lazy 各 **86**、scope 各 **191**。
- HEADLESS Core include/link 正负 guards 与 catalog check/self-test **3/3**；clang-format 22.1.3 **1029 文件、0 失败**；doctor healthy、strict **107/107**、diff check 通过。
- 此阶段不改 geometry/lease/packed ABI/shader/backend，不重跑无关 GPU 矩阵。97 samples / 458 frozen IDs / 五份 FULL 66 域 golden 保留，任务仍 **68/135**。未来 owned input 域须在独立 immutable 交付后按真实组合许可适配；当前不依赖其 WIP。
- 原始日志/源码副本/二进制仅 ignored out；独立交付 SHA 由本文件首次加入的 Git 历史与回执标识。未新增 native/GPU、Windows/MSVC/D3D12、物理 IME/AT 或实际跨显示器证据。
