# 私有位移所有权 primitive：Linux

以下为来源提交 `c801bbc8858259d9136a6f574cf8c4975d5380f4` 的独立工作树验证记录；当前 main 的重新验证见 [共享基础集成](shared-runtime-primitives-integration.md)。

- 实际环境：Linux/GNU，`linux-gcc-headless`，Ninja Multi-Config，Debug/Release，`-j2`。
- 构建：`cmake --build --preset linux-gcc-headless-{debug,release} -j2 --target rynui_translation_ownership_tests`；CTest `-R '^rynui\.translation_ownership$'`，两配置各 **1/1** 通过。测试仅链接 Runtime，无 TableComponentHost/SDL 符号，不依赖新 Table API。
- 覆盖：祖先写到拥有者根后停止、嵌套 offset 组合/回到零、原 clip/opacity、重复同步不产生新请求、销毁/失败构造与 Node slot 默认复位。
- 共同回归：CTest `-R '^rynui\.(selection_component|modal_component|tabs_component|input_component)$'`，Debug/Release 各 **4/4** 通过。
- 格式检查（clang-format 22.1.3）、doctor healthy、strict **107/107**、diff check 通过。规则见 [位移所有权](../../../../docs/development/retained-translation.md)。
- 这是 CPU 共同机制证据；Table 专有行为在独立阶段记录。未据此验收 Windows/MSVC、GPU 或物理输入。无 GPU ABI/opacity 机制变更。
