# 109 规划检查

2026-11-12，Linux 独立 worktree，源码基线 `48033d58061a540514ed7f60941e9880fc9b18d5`。本次只有规划文档，没有 C++/测试工具实现、构建 preset 运行或 Web/native 图像对比。

- proposal、design、单个 capability spec、tasks 全部齐备；19 个 Requirement、61 个实施/自动验证任务，**0/61 完成**。OpenSpec 的 artifact done 只表示规划文件存在。
- 只读运行当前 `generate_gallery_source.py::generate()`：114 records/114 unique；静态 target writer：490/490 unique。组件 catalog 73 项，组件 sample 110，另有 4 个 Token sample；旧视觉 route discovery 为 75 路由/71 个直接组件。计数分母明确，未来 gate 按完整有序身份而非历史 count 上限。
- 现有 `update_ant_design_gallery_catalog.py --check --self-test` 与 `check_ant_design_current_baseline.py --self-test` 通过。读取固定官方 6.6.5 demo 源文件核对 pilot 的来源和不同初始条件，未启动浏览器验证其行为。
- 只读 `git show 521a3e5e6005b5bd3b00e32cb066d38b6b4b1199` 核对 Other finite helper 接口与既有 Linux evidence；未导入该提交，未复跑其平台结果。root 后续 main 进度仅作协调信息，不计入 109 证据。
- `openspec doctor --json`：root healthy；`openspec validate --all --strict --no-interactive`：**109 passed / 0 failed**；`git diff --check` 通过。实际 CLI 为 **1.14.3**，仓库/skill 约定 1.14.0；未修改全局安装，记录此工具版本差异。

自审补充了 disabled Web 输入不可被 actionability 等待吞掉、idle 区间不得被测试 readback 影响、sample/字体/Provider 消费差异不能生成错误 oracle、缺动作/新 capture receipt/校准不可分离必须非零等合同。未运行 pilot、校准、故障注入、Linux GPU 或 Windows/MSVC/D3D12；这些结果仍由 tasks 的实际执行取得。所有原始研究源码/有序 inventory/日志仅在 ignored `out/`，107/108 任务及生产代码未改。
