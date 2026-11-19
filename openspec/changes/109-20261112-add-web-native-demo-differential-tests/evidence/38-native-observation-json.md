# 原生观测 JSON 与 Select 时限

- Linux GCC 的 63 条实际 Select 记录重编码对照覆盖 176,141,991 字节，旧实现与新实现逐字节一致。单轮序列化实测从 4.60 秒降至 0.59 秒。解析上限、证据字段和生产进程预算保持原样。
- Windows MSVC HEADLESS Debug、Release 的原生协议与观测 CTest 各 2/2 通过。新增向量覆盖 UTF-8、转义、整数范围、浮点和负零。Linux GCC Release 协议向量通过，私有 Wayland/Vulkan 的 Select 10 个检查点及独立行为断言完整通过，并识别到 15 个外轮廓阴影实例和 5 个通用 retained effect 实例。未宣称配对视觉通过。
