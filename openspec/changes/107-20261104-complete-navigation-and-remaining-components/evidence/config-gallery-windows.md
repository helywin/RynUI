# ConfigProvider 消费者与 Gallery

2026-11-08，Windows/MSVC，正式 `windows-msvc-headless` Debug/Release、Ninja Multi-Config，默认栈，最多两个 C++ 编译进程。

74 个 typed 原生消费者读取捕获的 size/disabled/direction 默认值，并保留显式 false/Middle/Auto。Form/Group 仅显式本地 true 进入安全 OR gate；生成的 Checkbox/Radio options 通过 `SelectionOptionDisabled` 保留未设置来源。Modal 内置 footer 的默认来源不绕过 loading、busy 或输入域。

`rynui_config_provider_consumers_tests` 的 12 个真实 Host/Provider 入口在 Debug/Release 均通过：

```text
^rynui\.config_provider_
```

涵盖三来源、嵌套与双窗口、同 key 动态选项、Compact/Form/Group、实际输入/反向选区/composition/capture、晚挂弹层和 App 服务、等值 idle、getter/observer 重入与退役，另编译独立公开头 aggregate/bool 兼容断言。相关既有 Input/Selection/Form/Compound、Gallery 与 i18n 合同进入同阶段定向验证，未新增私有 Prop 框架。

Windows hidden D3D12/DXIL 的 7 条实际路径、263 项检查和 24 张局部 GPU 捕获复用 [108 Gallery 平台结果](../../108-20261107-add-native-i18n/evidence/gallery-windows.md)，不重复计数。公开迁移与范围矩阵见 [ConfigProvider 文档](../../../../docs/config-provider.md)。Linux、系统 IME、OS accessibility 和跨物理显示器保持各自证据边界。

原始日志、截图与运行索引均只保存在 ignored `out/`。
