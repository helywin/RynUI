# 非阻塞帧调度

2026-10-02，Windows/MSVC。`OnDemandFrameLoop::tick` 不调用 wait，native `step` 共用 tick。CallbackFramePump 由真实 FrameRequestState/DirtyQueues 与 AnimationRuntime schedule observer 自动唤醒，最多保留一个 callback ticket，提前 deadline 替换，撤销后取消。token 独立于 pump 生命周期，generation/owner thread/reentry 检查拒绝旧 callback。

审查发现并修复两个问题：活动 submit 回调销毁 pump 后访问旧对象；真实 Button/Input 动画 dirty request 造成 immediate callback 压过 deadline。实现使用 tick 返回后的独立 token 检查，以及仅合并动画采样期 Core invalidation 的入口；显式 request_frame 保持下一帧语义，submit RAII 在异常时恢复状态。

测试覆盖真实属性 wake、动画 play/提前 retarget/cancel/完成、no wait、单调时间、callback 合并/旧代际/跨线程/重入、执行前和执行中销毁（含异常）、completion 显式请求、晚到下一帧请求、真实 Button/Input future deadline 与 deferred、Recording surface 挂起期间更新/最新 revision 恢复/零重复上传。

HEADLESS Debug build 退出 0，共同 CTest **21/21**；默认 Debug build 退出 0，scheduler/animation/原生 event pump/Gallery/caret 等相关 CTest **19/19**。日志 `out/035-pump-{build,tests}.log`、`out/035-pump-native-{build,tests}.log`。架构文字改动曾使旧 source contract 找不到 `blocking one-shot`；明确该词仍描述 native wait 后复跑通过。原生 OS/GPU 验收独立保存。
