# Design

## Context

见 proposal.md。规划时 main `6118f677` 包含 Avatar，Statistic `0634ccd4` 和 Rate `cea1552c` 尚待独立审查；这些规划时 worker SHA 不作为发布证据。实施依赖已在 [Core 冻结记录](evidence/01-core-baseline.md) 更新为实际已审、已构建、已发布的 main `29154cda`。现有 Gallery 实测 390 IDs、295 live、389 Buttons、153 Inputs；源码有 36 个 `add_*` 样例函数。三个支持条目目前仍为 planned。生产目录通过共同 anchor 定位样例，091 已有有限窗口浮层及实际 fill/focus 检查可复用。

## Goals / Non-Goals

**Goals:** 将已审查并发布的三个组件组合为可体验流程，保留旧内容与真实编辑状态，验证主要事件和实际绘制/命中。

**Non-Goals:** 不在 Gallery change 中扩展 Core/API/Theme，不添加图片、网络、Timer、动画或新业务模型。完整 Gallery/native 仅在用户指定收尾时统一运行，组件 GPU/人工视觉、Linux、系统 IME/AT 不由本 change 宣称完成。

## Decisions

- **先完成组件依赖。** 1.2 记录 088/089/090 各自实际发布 SHA 与 main 检查，再实施样例。相比从 worker 导入 WIP，这避免用临时 API 写出无法在 main 构建的示例；若发现 Core 问题，在所属组件 change 修复后再引用。
- **追加独立保留内容。** 在既有 Timeline 样例后追加成员、指标和评分函数。业务状态属于 GalleryState，普通属性变化使用 Signal/Binding，不重新执行已有样例；所有恢复控件放在被隐藏/禁用区域外。
- **使用真实数据和浮层。** 头像文字由原生输入更新，Group 的真实 overflow 展示隐藏成员；指标使用 typed 数值和字符串，精度按公开合同截断/补位而非改写业务值；评分通过真实指针/键盘 onChange 回写 authority，富 Tooltip 中使用保留 Input。不上浮一个外部 Text 仿造核心显示或分数。
- **依赖所属区域定位。** 按样例 owner 和组件 snapshot 查找其控件，不用全局相同类型下标或非唯一按钮标签；目录点击经真实 navigation request 和共同生产 anchor，旧 Date/Time 的精确 anchor 预期同步加入新入口。
- **一次聚焦矩阵。** 新 `--members-metrics-only` 接入默认完整入口，覆盖完整业务流程及 Default/Dark/Compact × 1200/560 有限窗口。采样实际 glyph/fill/hit，交互按钮检查四边 focus；没有焦点语义的纯展示 Avatar/Statistic 不伪造 focus 要求。旧代表只按受影响合同选择，不重跑所有 scoped flags。

## Risks / Trade-offs

- [overflow 或富 Tooltip 错用 document clip] → 区分窗口层，实际验证可见成员、滚动、原生输入命中及关闭/恢复。
- [格式切换掩盖精度变化] → 断言实际值和反馈，使用公开格式器的规则；不用 `std::to_string` 的固定六位小数反馈。
- [loading/隐藏留下输入或复活旧 popup] → 验证真实焦点、preedit、门禁和恢复，按完整 ComponentId/editor 检查身份。
- [装饰被新样例自身区域裁剪] → 给原生控件留必要外边距，检查实际四边 coverage，不降低 clip/visibility 断言。
- [新增样例影响旧盘点] → 390 旧 IDs、36 旧函数和 70 非目标支持条目比对；新数量以最终程序实测为准。

## Migration Plan

Gallery 保留性基线使用 `6118f677` 的既有内容；Core/API/Theme/goldens/CMake 的冻结基线在依赖集成后、实施前记录准确 main SHA。平台通用检查使用 Windows MSVC `windows-msvc-debug`、Ninja Multi-Config、最多 2 jobs。每个已验证阶段英文 Conventional Commit 并只推 main，核远端 SHA；不推 t3 分支、不自动 PR/archive。

## 父主分支集成更新（2026-10-28）

worker `862ff37c` 已通过独立 Astra R1。父线程在已发布 main `ee289de2ad6851ededdddd1c45bd20612b7579a8`
上显式合并并重新构建/运行实际流程；该 main 包含已审 AutoComplete/Tree、原生 RC 和系统字体默认值。
父验收冻结全部当前 Core/API/Theme/goldens/CMake 与 renderer，不把旧 `29154cda` 的缺少后续功能视为当前基线。

旧 390 IDs、36 个样例函数及调用顺序继续保持；函数体对照当前 main。095 已验证的默认 ThemeConfig 静态引用、
ThemeProps 堆对象与 const 参数传递属于父基线修复，合并必须保留，不能被旧 worker 的历史函数体覆盖。
原 `compare-baseline.py` 仅记录 worker 历史冻结检查；父集成使用独立脚本绑定上述准确 main，
不改写历史断言。仅父进度说明可更新，未变更首版业务功能或平台验收边界。

父新构建后的八项合同发现一条 095 遗漏更新的旧断言：Gallery 构建合同仍要求磁盘 shader 文件名。
仅将 `verify_token_gallery_contract.cmake` 的两条复制路径预期替换为最终 exe 的
`rynui_embed_target_shaders(rynui_token_gallery)` 调用；生产资源代码和其余合同原文保持。
保留修复前失败日志，父保留性脚本单独核对这项精确替换，其余 550 个冻结 blob 继续逐个相等。
