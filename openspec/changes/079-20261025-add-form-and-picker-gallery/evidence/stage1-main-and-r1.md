# 079 Form 阶段：父线程合并及 R1 修复（2026-10-25）

父基线 `f5ea39a5ab1c88537f42b6c025f1f28f15bb338f` 已包含通过 Astra 的 Form/DatePicker。显式合并 Form Gallery worker `15ad81281d627ccfe3b21e56509eaa68f88cd3a0`；仅 generated catalog 冲突，采用双方自动合并的 support overlay 重新生成后解决，同时保留 DatePicker/Form partial 范围，不手工选择一方旧生成文件。Core、Form、DatePicker 生产实现未改动。

## 初次父验证

Windows MSVC、Ninja Multi-Config、windows-msvc Debug，fresh Gallery/frame/catalog 构建后 `--forms-only` exit 0；`--inventory-only` 实测 **285 IDs、93 navigation、314 Buttons、923 interactions、596 visible interactions、280 live、67 Theme content、138 Inputs、22 Select、46 selection、13 Modal/Drawer、2 Menu、1 Dropdown、1 Collapse、3 Segmented、3 Empty、3 Result、1 Form/4 items**。八项 Gallery contracts **8/8，0.54 秒**。ignored `out/form-gallery-main-{build,focused,inventory,contracts}.log`。

## Astra R1 与实际复现

独立 Astra R1 固定 worker `15ad812` 发现一项 P2：成功反馈使用 `std::to_string(double)` 固定六位小数，允许的数量 `1.0000004` 被显示为 `1.000000`。审查原结论来自源码；其余四字段绑定、真实 ref、目录、gates、草稿/preedit/身份与追加顺序未发现 P1/P2。独立复跑现有 `--forms-only`、inventory、reference catalog、document model/viewport 均 exit 0。

父线程新增真实输入 `1.0000004`→原生保存的回归，先运行未改生产格式器的 fresh frame binary，实际 exit 1：`Form actual result/status feedback differs`，见 ignored `out/form-gallery-number-before.log`。原有整数成功检查已通过后，新的小数检查才失败；复现验证实际数字 authority 为 `1.0000004`。

## 修复与验证

成功反馈改用 `std::to_chars` 的默认浮点最短 round-trip 表示，保留实际 double，且不受 locale 小数分隔符影响；整数 3 显示为 `3`。测试通过真实 TextCommitted/保存事件进一步检查 `1.0000004` 与 `1.234567891234` 的实际 authority 和反馈文字，原有失败焦点、提交/reset、同值 `1.`、preedit、三主题/两宽度 clip/身份/gates 流程保留。

重新构建 Gallery/frame 后最终 `--forms-only` **exit 0**，ignored `out/form-gallery-number-{fix-build,fixed}.log`。未增加节点/控件/标识，inventory 不变。阶段 1.1 保持完成；2.1 DatePicker、3.1 TimePicker、4.1 最终完整 frame/原生 smoke/Astra 汇总仍待办。R1 数值显示修复将交新的独立 Astra R2，不预先声称 clean。没有运行完整 Gallery 或本阶段原生 smoke，也没有新增组件 GPU、Linux、系统 IME 或辅助技术验收。
