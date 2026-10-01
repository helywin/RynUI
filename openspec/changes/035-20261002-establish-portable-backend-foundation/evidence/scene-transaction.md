# 共同场景事务与 Recording

2026-10-02，Windows/MSVC x64，`windows-msvc-headless-debug`：build 退出 0，共同 CTest **20/20**。`windows-msvc-debug`：build 退出 0，受影响的 Quad/Glyph/Effect、platform 生命周期、Gallery/Button/Layout 合同 **13/13**。日志：`out/035-recording-{build,tests}.log`、`out/035-transaction-native-{build,tests}.log`。

`scene_backend_tests` 使用生产 WindowComponentServices、真实字体、Input/Button/Typography/Divider 和共同 SceneResources，逐字节核对 Quad/Glyph/Effect 与 Atlas，核对 ordered draws。覆盖 idle 零上传、局部上传、begin/部分上传/commit/异常失败后附件不可呈现及成功重试、owner/epoch 拒绝、CPU 状态保留和显式重建、deferred 后最新内容恢复。另行覆盖三类资源初建/扩容时 upload 抛异常，断言临时资源释放、旧资源保留和重试后的 live resource 数量。

SDL handle 注册表检查 owner、kind、容量、已释放 handle；实际原生 `--typography-acceptance` 经 SceneBackend 基类 attach 验证虚派发，退出 0，D3D12/DXIL，18 submits，clipboard/edit/link/divider passed。日志 `out/035-transaction-acceptance.log`，截图 `out/035-transaction-native/`。这次运行验证接入，最终 Debug/Release Windows 验收独立记录。

回滚先保存持久 retry 标记，再尽力恢复 dirty queues；下一次成功提交前必须重建全部 dirty 信息。该防护经过源码审查；没有声称模拟操作系统内存耗尽。GPU 重建仍为显式生命周期合同，没有自动 device-loss 恢复或第二真实 GPU 后端。
