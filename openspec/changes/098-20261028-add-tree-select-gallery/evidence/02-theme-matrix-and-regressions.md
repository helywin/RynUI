# 098 第二阶段：三主题双宽与组合回归

直接实施父为第一阶段 `1377dc2d7f48a497321555d4a854b1b6b8ef9fe9`，冻结 Core／旧 Gallery 父仍为 `c9cc6c2aab75f03739e0540b47195af1cc897f67`。本阶段完成 task 2.1；独立 Astra 与最终收口尚未完成，不把此记录当作 CLEAN。

## 完整 focused

`--tree-select-only` 已纳入默认完整入口，实际执行 Default/Dark/Compact（smoke step 4/0/1）× 1200/560、900 高有限视口六组，每组完整覆盖第一阶段业务流，并增加：

- 真实目录按钮→导航请求→共同生产 anchor；宽窄切换与当前布局的导航范围／缓存行为一致。
- 当前接受值与请求反馈、父／叶选择、标题更新、禁用已接受节点、原生 clear、有焦点 outside 单次请求及下一次接受。
- 三种门禁与外部恢复；保留第一阶段恢复后真实 ref 焦点／不开启弹层断言，再验证原生指针可重新操作。
- Form 必填、真实选择与提交、两次聚焦后 reset，比较实际 value/editor display/focus 与无编辑 session。
- 已打开 selector 先通过原生 FocusManager 建立 keyboard 焦点起点，完成同步并消费旧请求，再发送实际 Down；必须出现新帧请求，确认请求后才 geometry，不用无条件 sync 掩盖唤醒。
- 当前 Theme 的实际 title allocation/padding 确定 Tree 内容区域；检查根及 End 显露末行的四边 outline coverage、选中内容 fill、实际 glyph／HitTest，真实 wheel 在自己的有限 Tree 中滚动。
- 比较 editor 的 component/editor/node/interaction、TreeSelect 私有 selector/popover/tree 和每行 component/title/interaction 完整代际；验证 Gallery content/theme composition 计数不变。独立备注内容保持，原生 selector 始终只读。

恢复按钮和 Form 重置按钮实际调用各自 ref.focus；其他 ref 方法不据此宣称全覆盖。停稳焦点起点是公开输入服务操作，不使用私有选择 setter 或直接改模型。

## 诊断与修正

第一次扩展运行 Default/1200 通过，Default/560 在“拒绝关闭”后发现列表覆盖切换开关。实际把新样例的控制区放到字段上方，保留真实命中和点击；没有绕过 popup 或调用 activate 代替点击。

第二次运行 Default 双宽通过，回到 Dark/1200 时目录落在 y=-55814。新夹具遗漏宽窄切换时的导航 translation 缓存失效；按生产运行时已有 previous-narrow 处理补齐 invalidate 和窄模式导航归零，未修改生产滚动算法。

旧 Form 代表首次失败于历史导航期望：它把除早期五个迁移组件之外的目标仍视为参考卡片。本次补齐当前十五个明确迁移样例的独立期望表，包括 TreeSelect；其他条目仍精确比较原参考节点，原业务、校验、草稿／preedit／reset、glyph 和身份断言均保留。

## 实际执行

Windows MSVC / windows-msvc-debug，Ninja Multi-Config，最多 2 jobs。生产控制区修正时重新编译 definition 并链接；最终旧 Form 期望修正后再编译 frame TU、链接实际程序，最终 build exit 0，**12.8957931 s**。精确命令见 [driver](stage2-artifacts/098-stage2-msvc.ps1)。

| 入口 | 结果 | 秒数 |
| --- | --- | --- |
| `--tree-select-only` | 最终新程序六组全部通过 | 263.3908096 |
| `--forms-only` | 原 Form 业务及三主题双宽通过 | 182.6856044 |
| `--frame-contract-only` | 单独既有 frame contract 通过 | 92.1509169 |
| `--inventory-only` | 精确 inventory 通过 | 27.6203277 |
| 八项 CTest contracts | 8/8，real 0.26 s，wrapper 0.3661136 s | 0.3661136 |
| `--tree-autocomplete-only` | 旧联系人／项目树六组通过，见下方产物边界 | 268.2552533 |

没有重复运行默认完整 journey；单独 frame contract 不冒称完整 frame。旧 Tree/AutoComplete 在旧 Form 期望修正前的程序上通过；[前置产物记录](stage2-artifacts/098-before-form-anchor-fix.json) 保存其 EXE 和源码 hash。之后仅修正独立 Forms 测试，旧代表函数体、TreeSelect focused 函数体及全部生产源码逐字节保持；本阶段不重复该未改矩阵，最终程序的 focused／Form／frame／inventory／contracts 另行实际执行。

最终 inventory 仍为 **458 IDs、93 navigation、433 Buttons、1593 interactions、771 visible、307 live、68 Theme content、163 Inputs、5 Forms/8 items、4 Popovers、3 Trees、2 TreeSelect**，其他旧组件数量不变。

## 保留与验收边界

441 旧 IDs、41 个旧样例函数体／调用序、72 个非目标 overlay、557 个 Core/API/Theme/golden/renderer/RC/CMake/README blobs 保持。格式检查 803 文件零失败；显式 OpenSpec 1.14 doctor healthy／strict 98/98、generator check/self-test、diff 与保留脚本通过。[索引](stage2-artifacts/index.json) 关联 34 份成功／失败／派生记录、7 个源码指纹、3 个默认 1 MiB 栈 PE 和 4 个 OBJ／2 个 LIB；原日志按字节保留，不提交二进制。

本阶段为父 ROOT 新构建与 Recording 验证，不代表独立审核者新构建，也不代表 native window/GPU、Linux、系统 IME/AT 或完整 Gallery 统一验收。用户已要求完成剩余后收尾，范围只保留本 098 的独立复核／必要修复和发布，不启动后继功能。
