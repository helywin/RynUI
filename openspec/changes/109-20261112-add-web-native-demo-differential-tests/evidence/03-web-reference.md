# 固定 Web reference

2026-11-12，独立 Linux x64 worktree，actual base `b59442233a3963b2e14d47cf3ce6924105455bc2`，runtime 实核 `codex/gpt-6.1-sol/xhigh/priority`。只本地提交；不 fetch/rebase/push，不改 shared main。交接 patch/base absence/post hashes 均验证后才应用。

## 依赖准备阶段

- 2.1 已完成。52 个 direct pin 未变；冻结 98 个 pnpm 解析包及 pnpm 本身，共 99 个实际归档的 URL/integrity/SHA256/license、文件树及 pnpm 自带依赖。Node 22.21.1、pnpm 10.23.0、React/ReactDOM 19.2.0、Playwright 1.56.1、esbuild 0.25.12；Chromium headless shell 1194 / 141.0.7390.37，完整归档 SHA256 `d745c7210cc88b73c7991a22c9624819960d84bc3de77ac7336d2de841a8c91f`。Web Python 仅用 CPython 标准库。
- `bootstrap.py --mode BUNDLED --output <fresh-out> --offline`：从核验后的完整归档给新 loopback registry、新 store、新 node_modules 执行 `--frozen-lockfile --ignore-scripts`；实际 72 个当前平台包、`reused 0`。不是复制旧安装。SYSTEM 显式路径及精确版本/bytes 另测；没有系统优先或失败 fallback。
- `RYNUI_WEB_PREPARED=out/demo-differential/prepared-checked python -B -m unittest discover -s tests -p 'demo_differential_web_bootstrap_tests.py'`：**8/8 通过**，含 fresh BUNDLED/SYSTEM、缺依赖/错版本、实际连接失败、源码/demo/Token/tag/安装包篡改、归档截断、cache hash/环境身份负例。browser binary/完整资源树和实际共享库 hash 均核对。
- 全局 OpenSpec **2.0.0**：doctor healthy，strict **109 passed / 0 failed**；`git diff --check` 通过。启动时 list/status/apply 曾用只读复制的私有 1.14.0；用户升级后已改用 2.0.0，不改写旧版本记录或混入主线工具生成改动。

源码/安装/store/browser/profile/完整日志与 hash 索引仅在 ignored out；无 C++/HLSL 或原生 CMake 依赖修改，未运行无关 CTest/GPU 矩阵。此阶段不勾选其余 2.x，不声称 native paired、GPU、Windows、桌面 present 或系统 IME/AT 通过。

## main 合入复核

主线从不可变 `82b8c3e85b8ac5f06306631aad5b0ae1cfd6a17c` 仅取上述准备阶段的 8 个路径；未读取后续未提交 adapter。main 自有 fresh `out/demo-differential/prepared-web82` 实际 BUNDLED/offline 完成锁定归档安装，Chromium 141.0.7390.37 可执行；同命令 bootstrap tests **8/8**（25.873s）、协议 **5/5** 通过。解释器按用户最新指示只检查兼容性、记录实际 `sys.version`，不增加解释器 exact pin/hash/license 门槛；第三方库/Node/pnpm/browser/source 的既有精确锁定不降级。OpenSpec 2.0.0 doctor/strict **109/0** 与 diff check 通过。仅 2.1 新增完成，主线 **6/61**；原始文件均留 ignored out，未报告 native paired。
