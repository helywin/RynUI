# Radio 平台通用合同

2026-10-03，Windows MSVC / Ninja Multi-Config，`windows-msvc-headless` Debug/Release。阶段 1 的动态值、最近组、键盘与 ref 合同各 8/8（1.59s/1.08s）。阶段 2 的 Radio、Theme、共同表面、rounded effect 与 renderer 合同各 17/17（5.24s/3.92s）。构建及测试原始输出保存在同目录日志中。

新增测试验证 outline/solid、三尺寸、block、H/V/RTL 的相邻圆角与共享选中边颜色；动态删除及手动销毁恢复外侧圆角、保留身份、空组释放全部效果；重着色不重新 mount/measure/shape，不污染 Switch 材质；独立算法/继承/wireframe/非法 token；共同象限裁剪 CPU coverage；有限 wave 的重启复用、取消、reduced motion、失活和销毁归零 deadline。

`validate_goldens.py` 对比阶段 1 提交 d2132a9，五份 golden 仅增加 Radio 的 16 metrics / 5 effects / 21 colors 与新 identity，所有既有 JSON 字段保持一致。407 自有 C++/HLSL 的 clang-format 22.1.3 检查通过；OpenSpec doctor healthy，strict validation 48/48。

集成阶段完整 CTest：Debug 54/54（89.31s）、Release 54/54（15.56s）。离线目录生成器 self-test/check 通过；Gallery 新增动态 outline、solid large block、vertical small RTL、独立主题与 ref 样例，98 stable IDs、114 live samples、58 Theme content runs、31 selection controls、4 RadioGroup。目录和原生收尾清单同步实现范围。

这些是共同逻辑合同；真实窗口、系统字体、GPU、输入归一化与 DPI 的分平台结果单独记录。
