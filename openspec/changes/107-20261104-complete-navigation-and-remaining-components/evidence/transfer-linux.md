# Transfer Linux HEADLESS 阶段结果

本阶段为 partial 实现，23.1/23.2 保持未勾选。公开合同见 `docs/components/transfer.md`，支持清单未提升为 implemented。

- 平台：Linux GCC 16.2.1，`linux-gcc-headless`，Ninja Multi-Config，`-j2`。
- Debug：`rynui.portable.transfer_component` 通过 1/1，包含 6 个场景；与 Cascader、Gallery 页面、目录合同合并运行通过 4/4。
- Release：`rynui.portable.transfer_component` 通过 1/1，包含 6 个场景；相同组合 CTest 通过 4/4。
- 覆盖：真实 Checkbox/Button/Input/Pagination、受控选择/移动拒绝与接受、移动方向/moved keys、禁用项隔离、重排/跨列表移动保留富 Input、重复数据回滚、Unicode 搜索、fractional wheel/onScroll、oneWay 行删除、回调销毁、保留自定义列表/footer、受控重入、跨侧/外线程拒绝和窗口销毁后旧列表 callback 拒绝。
- Gallery：独立页提供受控双列表搜索/分页、oneWay 切换、真实移动回调结果及保留编辑草稿。`rynui.gallery_sample_page` 通过 Checkbox Space 与 Button Enter 驱动实际移动和离页清理。本分支源码样例 79、legacy ID 458。
- 共享校验：Core/backend 边界 1/1；源码生成 `--check`、catalog `--check --self-test`、clang-format 22.1.3、doctor healthy、strict 107/107、`git diff --check`。构建/CTest 命令与同目录 `cascader-linux.md` 相同。

原生缺口：独立 Transfer Component Token、完整本地化和自定义 action slots；完整受控异常/动态几何原子性及子浮层/IME 自动合同仍待补齐。HEADLESS 不证明 Linux GPU/真实窗口、Windows MSVC、系统 IME 或辅助技术；63.2/64.2/65.1 保持父线程任务。原始产物只在忽略的 `out/`。
