# Carousel target admission 来源独立阶段：Linux 平台通用结果

以下仅为隔离来源 f9b173c0 / 9f69f6ab 的结果，不是主线重测；主线见 [carousel-admission-integration.md](carousel-admission-integration.md)。

- 按专用 Astra 只读报告的生产者局部方案实施；原 probe/request/manifest/source 指纹保持不变。原问题是实际 pose 已正确而 Carousel 私有 legacy gate 较晚发布，不是 P 几何排序失败。
- 只改 Carousel host、独立 fixture/CMake、内部说明与本结果。沿真实目标祖先外→内刷新自身 slide 的实际 clip/pose/opacity/current 判据；回调后重核 owner/slide generation、current、items_generation 和作用域。root/slide/arrow/dot 按实际写入点 quiet 配对释放完整 ComponentId，未 prune/clear Registry，未改变 true 幂等语义或公共输入代数。
- Linux x86_64 / GCC / Ninja Multi-Config / `linux-gcc-headless-{debug,release}`。构建相关 Carousel/input/P/translation target `-j2`；可独立选择的 CTest **Debug 21/21、Release 21/21**（Carousel 9、input 10、P/primitive 2），0 失败。命令选择：`^rynui\.(carousel_(target_admission|effects|controlled|retained|interruptions|nested_lifecycle|callbacks|failure|gesture)|input_constraint_(ownership|algebra|lifetime|hot|callback|rollback|modal|child_modal|participant|focus_ticket)|retained_translation_phase|translation_ownership)$`。
- 新 `rynui.carousel_target_admission` 每配置 **43** 条检查，包含首帧、同帧两种 current/target 顺序、实际 Input 接收、normal ScrollX/Fade 的 q=0 拒绝/真实半程/完成、嵌套、fractional document clip、独立 legacy/owned block、隐藏/禁用/窗口 inactive、目标/producer 退休、真实 afterChange 回调异常与精确 owner 存活、替换及 cleanup。**2000** 次稳定 helper 查询实测 **0** 分配，boundary/frame revision 不变且无 deadline。
- 当前 checkout 的额外 Tour 消费/WIP 组合与上述合计为 D/R 各 **37/37**，其中另外 **16** 项不包含于此独立提交，不作为 45.1/45.2 完成依据。原 moving 红例在未提交 Tour 消费中已保留单次 sync、增加首帧/完整矩形/身份并继续原零残留 clean；没有把仅几何通过当整体通过。
- 来源基线 `20e67d51` 已含 c801 位移 primitive；功能调用依赖已交付 `999e57b1` 的 lifetime/input API。Git parent 含 P `46e5de86`（83→ff 映射）和 Theme `3f32e19b`，helper 不调用 P 新接口或 Tour/Theme 域；主线只消费本阶段差量，不导父链或 Tour WIP。
- clang-format **22.1.3 / 1045 文件 / 0 失败**，catalog check/self-test、doctor healthy、strict **107/107**、diff check 通过。没有改 Form、Core/P、renderer/SDL、main、兄弟源码或共享构建产物；本地提交不 push。
- HEADLESS CPU 合同，无新 GPU/窗口、Windows/MSVC、物理 IME/AT 或真实多显示器结论。完整日志、诊断副本、二进制和运行指纹只在 ignored out。入口不建立任意外部 ref/P writer 的通用依赖图。
