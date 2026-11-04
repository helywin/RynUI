# Select 平台通用验收

实际环境为 Linux x64、GCC 16.2.1、Ninja Multi-Config，`linux-gcc-headless` / RECORDING；依赖采用锁定 BUNDLED source cache，位于本工作树忽略的 `out`。

最终 Debug 与 Release 各 43/43 CTest 通过：AutoComplete 28、Select 9、共享 Gallery/目录/完整 Theme algorithm 6。本组件每个配置 9/9，0 失败；两个配置相关 target 构建成功。

保留旧 `SelectSelection` / `onChange` 单选 API。新增合同覆盖 typed Multiple/Tags 的真实编辑、Unicode 分隔/自定义 tokenizer、Enter/Backspace/指针创建、受控拒绝/maxCount、标签移除/截断/固定与 responsive 溢出、分组过滤/稳定排序、受控搜索、labeled value 事件和保留标签、稳定 key 富草稿、交互 affix、局部 option/label/popup renderer 替换、可取消应用异步源、旧 generation/关闭/销毁结果拒绝、有限动画、只读/零尺寸、回调抛出/自毁后的清理。

10,000 项列表在 64px 窗口中普通 realized rows 不超过 6；Home/End、上下键及滚轮滚入正确，连续访问不累积普通行。离窗富编辑器先把焦点移回主编辑器并结束 preedit，返回相同 key 保留草稿。富行缓存按已访问 key 增长，删除 key 后释放；未声称全部富行缓存恒定空间。responsive 标签按实际字体/affix/标签测量分配，resize 后恢复显示，idle 不持续触发布局。

Gallery 保留原有样例并增加 grouped Multiple、Unicode Tags/自定义标签、10,000 项 RTL 自定义 option/popup/交互 prefix 与可交付合成异步源。数据明确标为 synthetic，无外部网络请求；458 个 frozen legacy IDs 保持不变。同步公开 API 文档、本组件 overlay 与生成目录。

组件构建 target 为 `rynui_select_{complete,model,component,public_api,theme}_tests`，另构建受影响 Gallery 页面、source panel、目录和完整 Theme algorithm target。实际使用 `cmake --build --preset linux-gcc-headless-debug` / `linux-gcc-headless-release`，指定相关 target、`-j2`；对应 CTest preset 使用 `--output-on-failure -R '^rynui\.(auto_complete_|select_|gallery_sample_page$|ant_design_reference_catalog$|theme_algorithm$|gallery_source_panel$|gallery_sample_source_contract$|gallery_sample_source_generated$)'`，未重复全量 CTest 或导航矩阵。

官方对照为 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Select docs、类型和 demos。Web DOM/CSS/React/portal、动态对象字段映射与废弃浏览器别名不进入 typed Core；网络 transport/线程调度由显式应用 adapter 提供。

目录生成 `--check --self-test`、clang-format 22.1.3（881 个自有文件，0 失败）、OpenSpec doctor healthy、`openspec validate --all --strict --no-interactive`（107/107）与 `git diff --check` 通过。没有新增 Theme 字段，也没有删减完整 golden shape。

边界：未在本分支运行 native/GPU 窗口或 Windows；新增 Linux GPU/driver/system font/DPI、系统 IME 候选 UI、辅助技术与 Windows/MSVC 证据保持独立。63.2、64.2、65.1 由父线程负责，本分支没有标记。Theme 复用既有 Select/Input/Tag Token，五份完整 golden 保留所有字段；Core 只更新 logical CPU scene/resources，没有改变 renderer ABI 或加入私有 SDL/GPU 上传。原始日志、下载和二进制仅在忽略的 `out`。

## 主分支后续原生组合

Linux `linux-gcc` Release：私有后台 Wayland/Vulkan/SPIR-V/system fonts，AutoComplete → Select → Pagination → Tabs → Button 共 5/5 cases、566 checks、121 frames 通过，input-area/cursor 均实际调用。分组/标签/虚拟 wheel、自定义编辑器或 popup、受控拒绝、合成异步回调和离页清理见 [主分支组合验收](select-autocomplete-integration.md)。此结果不扩展为系统 IME、跨显示器、AT 或 Windows 实测，也不关闭分类 63.2。
