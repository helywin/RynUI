# 内嵌资源与示例部署

Windows 示例的 shader 使用标准 `.rc` / `RCDATA` 链接进最终 exe。构建继续用 shadercross 生成
Quad、Glyph、RoundedEffect 的 DXIL / SPIR-V；renderer 从 exe 内的只读 bytes 创建 pipeline，
运行时不再查找旁置 `shaders/`，也不需要 shadercross。普通示例默认通过系统字体链补足字形，
不要求随 exe 复制 `fonts/`。显式配置的应用字体和专门验收字体仍可使用。

## 构建接入

仓库内使用 SDL renderer 的 executable 在链接后调用：

```cmake
target_link_libraries(my_example PRIVATE RynUI::RynUI rynui_renderer_sdl)
rynui_embed_target_shaders(my_example)
```

该 helper 仅接入固定的 12 个内置 shader。Windows 使用原生资源编译器，将配置后的标准资源脚本
直接加入 executable；不把 `.res` 藏进静态库。shader 输入变化会触发资源重编译与最终链接。
其他平台将同一固定集合编译为只读 C++ 数据。资源读取接口属于具体 renderer backend，
Core 和 renderer/common 不依赖 Windows SDK。

应用自己的图标、二进制等资源可直接使用普通 `.rc` 和 `target_sources`，不需要额外清单语言或资源包协议。
RynUI 内置资源使用 `RYNUI_SHADER_` 名称前缀，应用资源应避免占用该前缀。

## 字体与分发边界

普通 text/button/layout/Token Gallery 示例不配置固定验收字体路径。应用显式字体仍优先，
Windows 通过 DirectWrite、Linux 通过 Fontconfig 查询系统字体补足 coverage；缺少必要字形时保留明确诊断。
系统字体不随程序分发，也不硬编码字体文件路径。专门验收入口和测试可显式使用锁定字体。

运行 Gallery 的固定字体验收模式前，显式准备其 fallback 文件：

```powershell
cmake --build --preset windows-msvc-debug --target rynui_token_gallery_acceptance_fonts
```

Release 使用 `windows-msvc-release`；Linux 使用对应正式 preset。此目标会构建 Gallery 并准备
`fonts/latin.ttf` 与 `fonts/cjk.otf`，不属于默认构建。普通 Gallery 启动始终默认系统字体；
复制 exe 进行普通部署时无需复制这些验收文件。

exe 搬移验证只证明不再依赖旁置 shader/默认字体目录；系统运行库、GPU 驱动及显式应用资产仍按其部署要求提供。
具体平台的实际验收结果见 change 095 的 evidence；Windows 通过不代表 Linux/Vulkan 已验收。
