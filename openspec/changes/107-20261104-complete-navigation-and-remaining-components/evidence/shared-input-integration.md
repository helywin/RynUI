# 共享输入约束主线集成（U）

在主线 `f1391a29d1d68f1db6b186a775d2fd2ebc7043e0` 上仅语义导入来源 `999e57b106322afd635ff90f9cc4256cd28d8d82` 的28文件差量，不取父链或Tour WIP。保留Avatar helpers、`ff8926a5` translation phase、最终Image与J精确注册合同。来源结果单列于 [tour-input-linux.md](tour-input-linux.md)。

- Linux GCC / Ninja Multi-Config，`linux-gcc-headless` Debug/Release、`-j2`。实际命令 `cmake --preset linux-gcc-headless`、`python out/shared-input-integration/validate.py`：36个相关targets，精确58项CTest选择，两配置各 **58/58**。包含新增14类owned输入、旧pointer/focus/text/分配/Modal，受影响Image/Upload/FloatButton/AC/Mentions/TimePicker、P/F/J及Gallery host；不是全量CTest。
- 新14类每配置 **140800个断言**；三状态各40000次实际hit queries、0 allocations、20000 branch checks、0 snapshot rebuild/frame request。旧256 Inputs的selection/composition-selection各20000次、0 allocations合同保持。Image lazy preview **86**与Form Modal scope **191**条断言保持；无Modal的Form消费资格另阶段实施，本阶段不宣称已修复。
- `python out/shared-input-integration/validate-native-unit.py`：`linux-gcc` SDL text atomicity Debug/Release各 **1/1**，仅unit，不是窗口/GPU/系统IME。owned lease/grant与legacy blocks分别清理；无全局prune或Form predicate。
- 实际HEADLESS include/link与catalog guards **3/3**，source generated check **1/1**、catalog check/self-test、clang-format22.1.3（1053文件、0失败）、doctor healthy、strict **107/107**、diff check通过。两配置Runtime primitive符号均无TableComponentHost/SDL。
- 首轮准备入口误启动第二个同目录构建，已停止重复树并补prepare-only入口；原构建缺slider对象失败保留、未计通过。随后单一顺序Ninja补建/测试。实际生成头、静态库与编译/链接命令指纹存ignored out；无兄弟worktree BUILD引用。
- 任务仍 **76/135**，45/53/63.3/64.*/65.1保持开放；**101 samples / 458冻结IDs / 五套FULL67域**无内容改动。无renderer/GPU ABI/SDL Core依赖变动。

原始选择、日志、命令及指纹仅在ignored `out/shared-input-integration/`。未验证Windows/MSVC/D3D12、物理IME/AT与真实跨显示器；本阶段没有新增窗口/GPU通过声明。
