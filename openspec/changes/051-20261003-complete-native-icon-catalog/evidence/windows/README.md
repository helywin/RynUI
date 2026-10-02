# Windows Icon 原生验收

2026-10-03，Windows 11 / MSVC / Ninja Multi-Config，`windows-msvc` Debug/Release。

- 两个配置的 build 与受影响原生 CTest 各 17/17，通过用时 31.66 / 11.82 秒。覆盖 Icon、共享 glyph packing/resources/TextScene、Text/frame、Theme、Tooltip、Input/GPU/Password/Search、Typography interaction、Button 和 Gallery frame。
- 默认 Gallery `--smoke` 两个配置 exit 0，实际 Direct3D12/DXIL、win32；系统显示缩放 1.25，系统字体 Segoe UI Variable Text / Microsoft YaHei UI。实测 118 stable ids、137 live samples、60 Theme content runs，根 Content 仅一次。
- `--icon-acceptance` 两个配置分别运行 system、1、1.25、1.5、2 倍 render scale，共十次真实窗口。每次 36 次提交、18 张 GPU readback，全部 180 PNG 尺寸/SHA256 与 EXE SHA256 已验证。`runs.json` 保存参数、配置、EXE 与截图身份，`validate_native.py --verify-only` 校验保存证据；后续重新构建 EXE 后需按记录的历史版本重建才能再次比较 EXE 身份。
- 每次包含十五个 Icon Components：三类资源、四层 Wallet、自定义三层 Quadratic/Cubic/holes、非零原点宽视框、两个 clip 对照、Button slot 与保留 popup，以及 Input clear/Password/Search 原有操作图标。
- 45°/90° 绕统一视框中心旋转、主副色/派生副色/暗色品牌、source 四层切换与隐藏恢复、连续 spin/reduced motion/Theme motion=false、popup spin/关闭和祖先 48 dp clip 通过；角度更新保持 shape/raster/texture-upload 计数。
- SDL queue 注入三个 mouse 事件，经真实平台归一化后在 resize 至 1420×1000 的窗口命中 Button 并切换自定义 source；组件根、主 scene 与 Content 保留。窗口失活后三次 idle 轮询没有 deadline/额外提交，销毁无节点、scene、interaction 或动画 scope/target，字体服务释放缓存字体。
- 人工检查开发过程中 scale-1 initial/rotate-45，以及最终 `debug-system/clip-on.png`、`release-scale-2/dark-primary.png`、`release-scale-1.5/popup-spin.png`：双色/透明度、孔洞、宽视框、旋转/clip/浮层与既有操作图标显示正确。
- clang-format 22.1.3 check 430 自有文件，OpenSpec doctor healthy、strict 51/51、diff check 通过。

本证据使用 SDL queue 的受控指针注入与实际 GPU 窗口，不声明人工输入法候选交互验收。Linux 项未在本机运行，保持独立未勾选；不影响已完成的平台通用和 Windows 项。
