# Theme Algorithm 平台通用验收

- 日期：2026-08-27
- OS：Microsoft Windows 11 专业工作站版 10.0.26200（Build 26200）
- Compiler：MSVC 19.51.36256.0，x64
- Configure preset：`windows-msvc`
- Build/Test preset：`windows-msvc-debug`
- Generator：Ninja Multi-Config
- Ant Design：6.5.0 / `740ad964dc2397f33e40944367b0536a7314cc32`
- Catalog SHA256：`b7e1e43a9400c42a7e545a0afa1b0a3117e1a352e2970c27870ac58e9181cd25`

## 验收结果

```text
ctest --test-dir out/build/windows-msvc -C Debug -R "^rynui\.(theme_algorithm|design_token|design_token_catalog)$" --output-on-failure
3/3 passed
exit code: 0
```

`rynui.theme_algorithm` 覆盖 Default、Dark、Compact、Dark+Compact、Compact+Dark、Seed/Alias/Button/Text override、Button 与 Text 独立 component algorithm 的默认关闭与显式开启、nested override、inherit reset、非法输入原子失败、snapshot identity 和五份 checked-in golden 的逐字节比对。

本文件只记录平台通用算法合同；不作为 Windows D3D12/DXIL 或 Linux Vulkan/SPIR-V 真实 GPU 证据。
