# Button wave 平台通用验收

日期：2026-10-03。实际环境为 Windows / MSVC / Ninja Multi-Config；这里只记录共同逻辑合同，GPU 窗口另见 Windows evidence。

- 按官方 Ant Design 6.6.5 的 [wave style](https://github.com/ant-design/ant-design/blob/6.6.5/components/_util/wave/style.ts) 修正：400ms 扩展与 2000ms 淡出独立，使用 ease_out_circ；默认 6dp 外扩、6dp 最终厚度、0.2 初始透明度。颜色选取遵循 [util](https://github.com/ant-design/ant-design/blob/6.6.5/components/_util/wave/util.ts) 的实际 border/background/primary 优先级。
- Button 定向测试通过；覆盖时刻零隐藏、100ms easing/几何/透明度、400ms 扩展完成且继续淡出、2000ms 清理、重复激活、颜色三级 fallback、零 spread/width/opacity、reduced/失活/销毁以及 token override。
- `windows-msvc-headless-debug` 完整 CTest：99/99，203.37 秒。
- `windows-msvc-headless-release` 完整 CTest：99/99，30.18 秒。包含 allocation、idle 和 backend boundary；共同逻辑不要求 Linux 重复运行。
- `windows-msvc-debug` Gallery frame、platform generic journey、reference catalog：3/3。Gallery 的 5ms 有界等待扩展到覆盖 2000ms，旧 Button 原生 fixture 也等待完整淡出。
- 默认 token 的 width 从 2 改为 6；五份 Theme golden 通过生产 `--write-goldens` 刷新并由完整 CTest 校验。
- clang-format 22.1.3 检查 458 个自有源文件通过；OpenSpec doctor healthy，strict validate 56/56，`git diff --check` 通过。

最初完整回归发现旧 Theme golden 与 Gallery 等待上限未同步；修正后上述完整回归通过。日志位于忽略的 `out/056-wave-common-retest.log`、`out/056-wave-gallery-retest.log`。这些结果不代替实际 GPU 像素或 Linux 原生验收。
