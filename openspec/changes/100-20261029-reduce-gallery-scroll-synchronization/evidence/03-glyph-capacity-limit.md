# R1：Glyph 预留容量与后端上限

## 源码与复现

基线 `673d7181bcc116ebc36d2995cf8c8ced35b3eff2`。本阶段生产改动仅限四个 renderer/common 文件，另在既有 `scene_backend_tests.cpp` 增加回归。无公开 API、GPU ABI、shader、Core 或平台依赖变更。

先保留旧生产代码，新增 RecordingRenderer 回归：字形 stride 为 96 字节，后端上限 288 字节，先同步 2 个 glyph，再增加到 3 个。Debug/Release 均真实失败，输出 `Recording resource exceeds renderer input limit`。旧预检允许 288 字节，倍增实际申请 384 字节。修复让预检与分配共用有界容量计算，合法输入的预留收紧为 3。

修复前组合包含尚未导入生产修复的 101 测试：5 项中 glyph_replace、text_scene_service、scene_backend 三项预期失败。该原始日志保留，不把另外两项计为本修复失败；本阶段修复后仅运行三个资源目标。

## 实际执行

Windows MSVC / Ninja Multi-Config，官方 `windows-msvc-headless-{debug,release}`，最多 2 jobs，无 GUI、无并发 benchmark。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_portable_scene_backend rynui_portable_glyph_gpu_resources rynui_portable_logical_scene_packing
ctest --preset windows-msvc-headless-debug --parallel 2 --no-tests=error --output-on-failure -R '^rynui\.portable\.(scene_backend|glyph_gpu_resources|logical_scene_packing)$'
```

Release 使用对应 release preset，命令与目标相同。

| 配置 | 实际构建秒数 | CTest 结果 | 外层进程秒数 |
| --- | ---: | --- | ---: |
| Debug | 17.3369085 | 3/3，exit 0 | 0.5852173 |
| Release | 17.1119621 | 3/3，exit 0 | 0.4218133 |

新增回归覆盖 288/383 字节上限、实际新增 packed bytes、删除后容量内恢复、真正超限在上传事务前拒绝、begin 故障不被预检消耗、create/upload/commit 失败重试、epoch 更新及资源计数。原有 Glyph/packing/SceneBackend 断言保留。

clang-format 22.1.3 全量检查 804 个已跟踪源码，0 failures；显式 OpenSpec 1.14.0 doctor healthy、strict 102/102、`git diff --check` 通过。首次格式命令因 PATH 不含 clang-format 而未执行检查；指定正式工具路径后的上述结果为有效验收。

原始 before/after build/test 日志、JUnit、驱动脚本、源码和现有 EXE 指纹见 `r1-capacity-artifacts/index.json`。本阶段是实际重新构建和运行；未重复 Gallery 长矩阵或 native 性能测量。既有原生数据仍仅对应上一阶段产物，不将这些资源回归扩大为 240 Hz、Linux/GPU/IME/AT 验收。

后续 101 证据整理将 UTF-8 JSON/XML/脚本复制按 Git 约定统一 LF，索引保存归一化后和原始文件的 hash；UTF-16 原始日志字节不变。该调整仅修正跨 checkout 的证据核对，不更改测试结果。

100 task 2.1 保持未勾选，待独立复核和最终收口。
