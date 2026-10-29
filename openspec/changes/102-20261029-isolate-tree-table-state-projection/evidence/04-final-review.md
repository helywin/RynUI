# Table 与 102 最终独立复核

2026-10-30，ROOT `main` 固定 `f6aeb746497d763077aeb5dc82da09fab83b1af3`，直接父 `f595211e753f8d117dd72f1390183515a9577b31`，累计基线 `033bc96bae1dbe5ee028a04f604fba074ed1cfea`。全新 Codex / gpt-6-astra / ultra / priority 审查结论 **CLEAN**，未发现约定 102 范围内可达 P1/P2；起止工作区干净。

完整产物、累计差异和相邻源码复核确认缓存失效、重入刷新、owner 销毁/重绑、异常后的完整排序与 geometry 重试、TextSceneId 完整代际守卫，以及分页后双向 Tab、外部邻居、editor 草稿/session 保留。Tree 的既有独审结论见 `02-tree-review.md`。

本次只直接执行现有程序，**39/39 exit 0**：每种配置 state_projection 七项（table_reentry、table_cell_retirement、table_retained、table_retry、tree_local、tree_invalidation、tree_retry），Table component/theme/public_api、Pagination component 四项，TreeSelect keyboard/retained 两项，portable text_component/text_component_frame/text_scene_service/focus_order/focus_lifecycle/input_component 六项；另单独执行 Release table_cache，覆盖 1k/5k/10k。这不是审查者 fresh build，也没有再次运行 benchmark、CTest 或 GUI。

270 条冻结记录匹配工作文件与 HEAD blob，953 个受保护 blob 保持；11 个源码和24个允许 EXE 的 SHA256、大小及 mtime 起止一致。三阶段全部 5184 个 Table 样本与 summary 一致。Table 故障探针每配置捕获24次至少512B的 bad_alloc；Tree Debug 5次、Release 8次，过滤至少64B，不代表任意 OOM 均可恢复。

父线程正式构建、测试及测量详见 `03-table-main.md` 和三个 windows 证据目录。最终生产版本 Debug 的47个不同合同由25项通过、夹具修正后该项1/1、另21项通过组成，不能改写为最后一个 Debug EXE 单次47/47；Release 最终26+21=47项通过。格式809文件零失败、显式 OpenSpec 1.14 doctor/strict、diff 已通过。实现提交已 fetch/push，远端 `refs/heads/main` 精确读回 `f6aeb746497d763077aeb5dc82da09fab83b1af3`。

本次仅更新审查记录、任务与审计索引，按文档阶段再运行 OpenSpec doctor/strict、apply 状态和 diff 检查。102 五项在约定平台通用范围内收口；仍保留 Table 逐行状态/scene 开销、Tree intact 访问和全局数据排序成本。没有整页240Hz、原生/GPU/Linux/系统IME/AT验收结论，不执行 archive。
