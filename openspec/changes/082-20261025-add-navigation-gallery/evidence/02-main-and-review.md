# 独立 R1 与 main 交付

全新 Astra xhigh R1 固定审查 `e3d63aa9e1f56032badc86c6f68bf976f43ff4f6` 相对 `c083fed` 的全部十四个文件，结论 CLEAN、无新的可行动 P1/P2。实际使用 worker 自有 Debug 二进制，执行 `--navigation-only`（131.15 秒）、`--inventory-only`（11.75 秒）、`--forms-only`（97.80 秒）、`--panels-only`（40.85 秒）、`--states-only`（79.32 秒）、`--date-picker-only`（180.90 秒），全部退出 0。旧数字精度、Collapse 自有 Select、Date 无 blur 同值 reset 与 retained 身份断言均通过。

reviewer 另运行 reference catalog、document/model 与 viewport 源码合同、generator check、基线比较和 diff check；worker 没有 model/viewport 独立 executable，因此没有把它们写成二进制执行通过。八项 contracts 的 worker LastTest.log 已核实。核对源码/obj/exe 哈希、时间戳和 Ninja 记录；frame exe 复验前后 SHA-256 一致：`A97B00ACAE6A764C8C9CC7F61EDDB9BEA987FA00521D0FFEE675C1876B43B336`。本轮审查是既有二进制复验，不是独立重新构建。

样例恢复按钮位于受控隐藏/禁用区域之外，目录经共用 production anchor helper 到达实际内容；ownership 定位与普通更新保留组件/editor/草稿。旧 297 IDs、28 个样例函数体与顺序、71 个非目标 overlay 保持；Date fixture 的 y=780 定位有实际 glyph/HitTest 检查，原 reset/draft/preedit/focus 断言未放宽。

父树从已推送 `c083fed5a72f644b491118c6440908e0354d241e` 以 `git merge --ff-only e3d63aa` 导入完整历史。`git diff --quiet e3d63aa -- src include tests examples gallery design-tokens` 退出 0，没有父线程新实现或 Core/Theme/CMake/golden 变更；原未跟踪截图保持。本阶段平台通用运行证据使用上述实际 worker 构建与独立复验，不重复已通过的 scoped 矩阵；父树检查格式、doctor/strict、generator 和差异，并重建本地 `rynui_token_gallery` 可执行文件。

最终 inventory 为 **319 IDs、93 navigation、337 Buttons、1072 interactions（634 visible）、284 live samples、144 Inputs、2 Breadcrumbs、1 Steps**；其余完整计数见 [01-gallery.md](01-gallery.md)。父线程将组件收尾清单更新为已提供交互 Gallery，仍保留 API 首版缺口及平台边界。

父树 `windows-msvc-debug` 的 `rynui_token_gallery` 重新构建退出 0，日志 `out/navigation-gallery-main-executable-build.log`；generator check/self-test、clang-format 22.1.3 的 **631** 个源码检查、doctor healthy、strict **81/81** 和 diff check 通过。

父线程只提交推送 main，安全 fetch 后比较远端 `refs/heads/main` 与本地 HEAD；SHA 核对结果记录在交付消息。不推送 `t3/*`，不创建 PR 或 archive。完整 frame/native 统一验收仍待 TimePicker Gallery 齐备后完成，不把本阶段 Recording 或本地构建记为 GPU、原生窗口、Linux、系统 IME 或辅助技术完成。

## 后续统一验收

079 在固定 main `22ee615` 上实际重建后通过完整 frame 与八项 contracts **9/9**（frame 1001.43 秒），新构建 Gallery 的原生 `--smoke` 实际为 Direct3D 12 / DXIL、七帧、exit 0；源码 guard 与二进制身份保持。该轮覆盖已发布的本组导航样例，准确命令、inventory 和独立平台边界见 [079 最终记录](../../079-20261025-add-form-and-picker-gallery/evidence/final-main-acceptance.md)。该后续记录不将本组组件人工视觉、完整 GPU/DPI、Linux 或系统 IME/AT 自动记为完成。
