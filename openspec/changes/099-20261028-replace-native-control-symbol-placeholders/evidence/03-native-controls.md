# Windows 原生图标检查

用户明确允许短暂原生窗口。最终官方 MSVC Release / Ninja Multi-Config，新构建 Gallery；四次快照均为 D3D12、1280×900、显式 acceptance render scale 1.0、默认系统文字字体及内置 Icon 字体。每次自动退出，不向其他应用发送键鼠输入。

命令：`out/build/windows-msvc/examples/Release/rynui_token_gallery.exe --snapshot=out/099-native-<control>.bmp --snapshot-control=<control> --acceptance-scale=1.0`。

| control | 秒数 | 实际观察 |
|---|---:|---|
| date-picker | 6.3674369 | 输入框打开日历，真实右箭头点击后显示 2026-11；四个单双箭头清晰，越界月操作变灰，输入焦点保留 |
| menu | 6.3849929 | 初始展开的“工具”经箭头点击收起；RightOutlined 清晰，保留键盘焦点轮廓 |
| table | 6.4596868 | 数量列真实排序点击后显示 ArrowUpOutlined，按钮焦点轮廓可见 |
| pagination | 6.3047648 | 真实下一页按钮点击后页码为 2，RightOutlined 和焦点轮廓可见，数字仍为 Text |

全部 exit 0。程序先验证实际命中仍属于目标 owner，再经生产指针路由操作。Menu/Table/Pagination 检查最终 keyboard focus；DatePicker 的导航项按既有合同不参与焦点，不强制聚焦，而是检查导航没有关闭日历或夺走原焦点。BMP 已逐张检查，不把整页其他视觉或所有问号字符概括为已解决。

保留两类夹具诊断：默认 1.5× 小窗的首个 Menu Icon 未通过 HitTest，不将该次记为通过；正式快照选择有空间的 1.0×。DatePicker 最初错误要求被动导航项获得焦点，已改为检查原设计的焦点保留。没有改变生产控件的焦点／命中规则以通过快照。

四份原始日志、结果及 BMP 的 SHA 在 `native-artifacts/index.json`；BMP 留在本机 out，仓库不加入大体积二进制截图。此验收不代表所有 DPI/视口、Linux、系统 IME 或 AT。099 的四控件文本符号替换与本机声明范围完成；独立组合审查归 100 最终收口记录。
