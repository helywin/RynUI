# AutoComplete 平台通用验收

实际环境为 Linux x64、GCC 16.2.1、Ninja Multi-Config，`linux-gcc-headless` / RECORDING。四个锁定文本依赖使用本工作树 `out` 下的 BUNDLED source cache；Core 没有链接 renderer、SDL 或网络 transport。

最终 Debug 与 Release 各 43/43 CTest 通过：AutoComplete 28、Select 9、共享 Gallery/目录/完整 Theme algorithm 6。本组件每个配置 28/28，0 失败；两个配置相关 target 构建成功。

新增合同覆盖分组/不可选组头、10,000 项窗口列表、keyboard reveal、输入优先 wheel、离窗富编辑器的 IME 清理与 keyed 草稿保留、应用异步 provider/owner-thread dispatcher、旧查询/关闭/reset/销毁后的结果拒绝、交互 popup 内交付/选中后的焦点、局部 renderer 替换、TextArea 自定义编辑器与显式 Form 同值 reset、有限/降低动画与资源清理。64px 列表窗口中普通 realized rows 不超过 6；连续键盘和滚轮访问不累积普通行。富行按已访问 key 暂存非活动子树以保留草稿，缓存随访问 key 增长，删除 key 后释放。

Gallery 增加分组长列表、RTL、自定义 TextArea/popup 与可通过按钮交付的确定性异步示例；示例明确标为合成数据，不产生外部网络请求。原联系人与 Form 示例保留，458 个 frozen legacy IDs 未变。公开文档和本组件 overlay/catalog 已同步。

构建的组件 target 为 `rynui_auto_complete_{public_api,complete,component,model,integration,review,r2,r3,theme}_tests`；另构建受影响 Gallery 页面、source panel、目录和完整 Theme algorithm target。实际使用 `cmake --build --preset linux-gcc-headless-debug` / `linux-gcc-headless-release`，指定上述 target 和 `-j2`。对应两个 CTest preset 使用 `--output-on-failure -R '^rynui\.(auto_complete_|select_|gallery_sample_page$|ant_design_reference_catalog$|theme_algorithm$|gallery_source_panel$|gallery_sample_source_contract$|gallery_sample_source_generated$)'`，只运行受影响集合。

上游对照为 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的官方 AutoComplete docs、`AutoComplete.tsx` 与 demos。自定义 editor 必须同步挂载一个携带指定 value/ref/events 的 Input 或 TextArea；popup origin 必须同步挂载一次。应用负责 owner-thread 投递与唤醒，Core 仅接受当前票据的首次有效完成。

目录生成 `--check --self-test`、clang-format 22.1.3（881 个自有文件，0 失败）、OpenSpec doctor healthy、`openspec validate --all --strict --no-interactive`（107/107）与 `git diff --check` 通过。没有新增 Theme 字段，也没有删减完整 golden shape。

边界：本次未运行 native/GPU 窗口，没有新增 Linux GPU/driver/system font/DPI、系统 IME 候选 UI、辅助技术或 Windows/MSVC 证据。HEADLESS 合成事件不证明这些平台行为；63.2、64.2、65.1 留给父线程，未在本分支标记。共享 Input 仅追加 AutoComplete friend/前置声明以构建 TextArea props，未修改 InputNumber 内部或 renderer ABI。原始日志、上游副本与二进制仅在忽略的 `out`。

## 主分支后续原生组合

Linux `linux-gcc` Release：私有后台 Wayland/Vulkan/SPIR-V/system fonts，AutoComplete → Select → Pagination → Tabs → Button 共 5/5 cases、566 checks、121 frames 通过，input-area/cursor 均实际调用。分组/标签/虚拟 wheel、自定义编辑器或 popup、受控拒绝、合成异步回调和离页清理见 [主分支组合验收](select-autocomplete-integration.md)。此结果不扩展为系统 IME、跨显示器、AT 或 Windows 实测，也不关闭分类 63.2。
