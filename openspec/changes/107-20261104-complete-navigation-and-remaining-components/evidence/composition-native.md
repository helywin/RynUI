# Card 圆角合成与 Modal 背景模糊

2026-11-08，107 独立合成阶段。原始日志、RGBA 回读和局部截图仅保留在忽略的 `out/resume-20261107/`。

- Windows MSVC，正式 `windows-msvc-headless`：Debug、Release 各 **41/41**。覆盖 logical composition、圆角命中/焦点、裁剪变化、Card retained identity、Modal blur、共同资源的分配/上传失败与无新 dirty 重试，以及受影响 Theme/浮层/App 合同。
- Windows MSVC，正式 `windows-msvc`：Debug、Release 各 **3/3**（shader source closure、反射布局、真实 hidden D3D12/DXIL readback）。使用独立 double Gaussian 和圆角 coverage oracle；逐 pass RGBA8 量化容差固定。包含 .25–2×、分数位移、嵌套 AA、真实 Card 的 Glyph/Image/Input 与 1dp 描边、透明 alpha、halo、窗口 clamp、多 Modal 前序采样、真实 App 服务和清理。
- Windows Release Gallery：Card、Modal 各四组配置，分别 **61、32** 个局部/上下文/操作捕获，退出码均 0、布局警告均 0。发现并修正新增 Card 示例的默认 Box 子项重叠；修正后重新执行完整四组并检查局部图。

Card 的 `clipContent` 默认关闭；loading 采用同一 mask。普通后代与子阴影/焦点环进入共同 PM 组，Card 自己的 outer shadow 在组外；嵌套 coverage 各应用一次。祖先矩形 clip 改变后会重新同步 mask crop；描边使用 inset band，避免非法负 outline offset 和外缘重复 AA。

Modal blur 对实际 framebuffer 前序内容做两遍离散 Gaussian，再用 replacement 合成；未用阴影或半透明色代替背景模糊。当前阶段不等于 Modal 剩余 lazy/motion/拖动/resize/完整 RTL/响应宽度已完成。

- Linux Ubuntu 24.04 / WSL2，正式 `linux-gcc-release`：**3/3**（source closure、SPIR-V 布局、私有 Mutter Wayland/Vulkan 实际回读）。驱动为 Mesa llvmpipe，属于 CPU 软件 Vulkan，不能据此声称独立显卡硬件覆盖。补齐 Pillow 并重建当前 Gallery 后，私有 Wayland 下 Card、Modal 各四组配置分别 **61、32** 个捕获，退出码均 0、布局警告均 0；初次依赖缺失未计为通过。

本阶段不启用尚未认证的 Linear/Conic paint 或 nonRound arc cap。后台 GPU 回读不代表系统 IME、跨实际显示器或 OS accessibility adapter 实测。Linux GPU 与 Gallery 结果独立记录，不以 Windows 结果代替。
