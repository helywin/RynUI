# Unicode boundary 平台通用阶段证据

- 日期：2026-09-08
- 范围：任务 1.1–1.4；不代表 editor、Input、IME 或任何 native 平台验收完成。
- 实际平台：Windows 10.0.26200 x64，MSVC 19.51.36256.0。
- Preset：`windows-msvc` / `windows-msvc-debug`，Ninja Multi-Config，BUNDLED。
- Build：`scripts/build-windows.ps1 -Configuration Debug -SkipTests` 通过。
- CTest：在 Visual Studio Developer PowerShell 环境运行
  `ctest --preset windows-msvc-debug -R 'text_boundary|unicode_|utf8proc|dependency_lock|python_cache_clean' --output-on-failure`，13/13 通过。
- OpenSpec：doctor healthy；`validate --all --strict --no-interactive` 10/10 通过；`git diff --check` 通过。

## 锁定来源与边界

- utf8proc 2.11.3 官方 release archive，SHA256：`415189fd2c85cd6ee5ff26af500fa387de9ada1e3e316e93f7338551481d557d`，本地下载摘要与 GitHub release asset digest 一致。
- Unicode 17.0.0 `GraphemeBreakTest.txt`，SHA256：`e2d134d2c52919bace503ebb6a551c1855fe1a1faec18478c78fff254a1793ec`。
- 语料只在 configure 时下载到 build tree；生成器以 checked-in lock 为输入身份，`python -B` 生成离线测试文件，CTest 无需联网。
- 766 条 UAX#29 corpus 用例全部通过；独立用例覆盖 ASCII、NUL、combining、CJK、emoji modifier/flag/ZWJ、Hangul、CRLF、无效 UTF-8 回滚、空值、100000-byte value、size_t 最大偏移和容量溢出。
- SYSTEM 测试使用受控 CMake package fixture 验证版本与 target 合同，不冒充实际系统安装验收；BUNDLED offline 测试使用已下载真实 release source 且禁用 FetchContent 网络。
- boundary target 只以 `LINK_ONLY` 传递静态链接依赖；consumer 的 `__has_include` 和宏断言验证 utf8proc include/definitions 不传播。

## 许可证规划修正

实际发布包 `LICENSE.md` 保留 Unicode 旧版三项条件，与 `Unicode-DFS-2015` 文本匹配，而不是最初计划中的 `Unicode-3.0`。因此 runtime dependency 记录为 `MIT AND Unicode-DFS-2015`；单独下载的 Unicode 17 corpus 记录为 `Unicode-3.0`。源文件版权及许可通知仍须按上游原文保留。

## 验证过程说明

首次在普通 PowerShell 中运行 resolver CTest 时，嵌套 MSVC configure 因缺少 Developer Shell 的 rc/mt 环境失败；在正式开发环境重跑后通过。缺包断言另修正为容许 CMake 将错误信息换行；没有放宽缺包拒绝行为。

本阶段没有运行 Release、Linux、GPU/窗口或人工 IME 验收；这些对应任务保持未完成。
