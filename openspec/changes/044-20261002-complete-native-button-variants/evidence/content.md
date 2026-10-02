# 内容与原生入口阶段证据

2026-10-02，Windows x64，MSVC / Ninja Multi-Config，任务 3.1 平台通用实现完成。

- `windows-msvc-headless-debug`：Button/Input/Selection/Slider/Tooltip focused CTest 5/5，5.99 s。
- `windows-msvc-headless-release`：相同 focused CTest 5/5，3.90 s。
- clang-format 22.1.3 check：397 个源文件，0 failures。
- doctor：healthy；full strict：44/44；diff check：通过。

公开 typed ButtonIcon/ButtonLoadingIcon/ButtonSlots 与 icon-only 入口保留旧 lambda 无歧义。常规和加载子树一次挂载，wrapper 的内部 ComponentLayout 只计量当前项；Start/End 保留内容声明顺序，只保留一份必要 gap。内置 spinner 和 custom loading 分支互斥。图标、加载和内容更新不重新执行 slots，恢复正常图标时节点代际不变。

测试实际覆盖 custom/builtin loading End 位置、多个内容节点顺序、icon-only 三档指标中的默认/Small Circle、Round/Square、block 父约束 resize 与命中范围、显式 width 优先、无限约束自然宽度。ButtonRef 覆盖 owner thread、disabled/window active 焦点门槛、mount-only autoFocus、代际重用、嵌套重复绑定预留和 rollback、图标 slot 抛异常、destructive keyboard activation。loading delay 覆盖初始 true、等待期仍可激活、deadline 前取消、重配计时、精确截止、零延迟、reduced motion idle 与销毁取消。

复用 shared host deadline/tick，不使用睡眠、额外平台计时器或 backend 上传路径。开发测试中对 disposed host 的重挂载被现有合同正确拒绝，测试改为存活父 slot 内重用 ref及新 host 的失败回滚验证，未改变 ComponentHost 生命周期。

Wave、Gallery 和真实 Windows/Linux GPU/字体验收仍在后续任务。开发日志：`out/044-content-tests.log`，不作为提交资产。
