# 098 第一阶段：真实样例与基础合同

冻结父为 `c9cc6c2aab75f03739e0540b47195af1cc897f67`，规划提交为 `b2a5ee05bc169ac69e7be7f57fc6471bc4167c6e`。本阶段完成 task 1.1；三主题双宽扩展与独立审核仍待后续任务，不把此记录当作 098 全部完成。

## 已实施

在原样例末尾追加一个函数、两个 live samples：受控层级目标与备注保存，以及真实必填 Form String 字段。提供 value/close 独立拒绝、下一次重试、清空、标题／归档禁用、全局门禁和外部恢复；Form 使用明确 control(ref)，重置按钮先聚焦真实 selector 再 reset。目录 TreeSelect 经共同生产 anchor 到达实际样例；仅该 overlay 改为如实 partial。

基础 focused 使用真实目录点击、指针、键盘、文字输入和 Form 操作，Default/1200、900 高有限视口。覆盖父／叶选择、拒绝／接受、禁用节点、一次 focused outside 仅一次请求、后续接受、门禁恢复、必填／成功／连续 reset、只读 session 门禁及完整 owner/editor/row/node/interaction 代际保持；实际 glyph/fill/hit 和树内容焦点四边 coverage 通过。三主题双宽未计作本阶段已验证。

## 实际构建与运行

Windows MSVC，官方 windows-msvc-debug、Ninja Multi-Config，最多 2 jobs。首次实际编译新增 Core 依赖、definition、anchor 和 frame TU，并链接三个目标，265.6975416 s；后续夹具／数量校准增量重建 frame，最终 build 10.8895575 s。最终源未修改生产 Core。

```text
powershell -NoProfile -ExecutionPolicy Bypass -File out/098-stage1-msvc.ps1 -Stage build
powershell -NoProfile -ExecutionPolicy Bypass -File out/098-stage1-msvc.ps1 -Stage focused
powershell -NoProfile -ExecutionPolicy Bypass -File out/098-stage1-msvc.ps1 -Stage frame-contract
powershell -NoProfile -ExecutionPolicy Bypass -File out/098-stage1-msvc.ps1 -Stage inventory
powershell -NoProfile -ExecutionPolicy Bypass -File out/098-stage1-msvc.ps1 -Stage contracts
```

- 最终新程序 `--tree-select-only`：exit 0，**44.2386608 s**。
- `--frame-contract-only`：exit 0，**92.1470494 s**。这是单独运行既有 frame contract，未运行默认完整 journey 集合。
- `--inventory-only`：exit 0，**27.4533341 s**。
- 八项精确 CTest contracts：**8/8**，real **0.27 s**，wrapper **0.3705565 s**，exit 0。

实测：**458 IDs、93 navigation、433 Buttons、1593 interactions、771 visible、307 live、68 Theme content、163 Inputs、5 Forms/8 items、4 Popovers、3 Trees、2 TreeSelect**。其余旧组件数量保持。

## 夹具与断言校准

初始数量 probe 按旧精确 inventory 断言预期失败，保留实测输出后改成精确新值；初始 mounted 计数也实测为 433/1593。原完整入口沿用的旧数值不匹配当前结构，本阶段以单独 frame contract 实跑校准为实际值，保留其他上传／绘制／身份断言。

首次新目录测试在建立滚动范围前就 scroll_to，offset 被零范围夹回，按钮仍在 y=1792。补齐首次 reconcile 后真实按钮位于 y=100，glyphs=10，保留原可见／命中和生产跳转断言。随后焦点测试误用整行宽度匹配 Tree 的内容轮廓；改为依据实际 title bounds 与当前节点 padding 计算内容区域，四边 coverage 门槛不变，未扩大 clip 或修改 Core。

generator 首次未指定模式、随后 partial 条目缺 test evidence 均被拒绝；补齐已通过 097 的 API/runtime/test evidence 后 `--write` 和最终 `--check` 通过。以上工具／夹具诊断不计作组件生产缺陷。

## 保留与边界

保留 441 个旧 IDs、41 个旧 add_* 函数体／调用序、72 个非目标 overlay 和 **557 个冻结 Core/API/Theme/golden/renderer/RC/CMake/README blobs**；见 [保留脚本](../verify-baseline.py)。普通属性更新未重跑无关 Gallery composition。格式检查 **803 文件、0 failures**，OpenSpec **1.14.0** doctor healthy、strict **98/98**，generator/diff 通过。

[本阶段索引](stage1-artifacts/index.json) 保存 32 份原始／派生记录、7 个源码指纹、3 个默认 1 MiB 栈 PE 和 4 个 OBJ／2 个 LIB；原始日志按字节保存，不提交程序二进制。本阶段未运行旧代表矩阵、完整默认 frame/native/GPU/Linux/系统 IME/AT，后续任务不得把本次 Recording 结果升级为这些验收。
