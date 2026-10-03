# OTP Gallery 与完整回归

实际环境：Windows / MSVC / Ninja Multi-Config。平台通用完整构建及 CTest 使用 `windows-msvc-headless-debug` / `windows-msvc-headless-release`，两配置均 92/92；Debug 159.65 秒，Release 24.29 秒，日志 `out/054-common-full-final.log`。Gallery frame 使用 `windows-msvc` Debug/Release，各 1/1，29.92 / 8.90 秒，日志 `out/054-gallery-frame-final.log`。

Gallery 新增 12 个稳定 ID、10 个 OTP 组（46 个 retained editor）和两个操作按钮：四变体、受控/部分完成、formatter/mask、indexed separator/RTL、动态长度、Dark Compact readOnly 与 disabled。库存为 153 个稳定 ID、172 个 live sample、63 个 Theme、81 个 editor；声明交互 navigation + 303，scene 交互 navigation + 282，Button navigation + 92。官方 reference 分类/样本仍为 73/126。

完整回归首次执行时 Flex 旧测试程序崩溃；重新构建该程序后，定向重复 50 次及最终完整回归均通过。完整构建输出确认此前只有此测试程序需要重新链接；临时异常栈辅助代码已移除。最终构建与测试顺序记录于上述日志。

catalog generator --check、clang-format 22.1.3（448 个自有文件）、OpenSpec doctor/strict validate（54/54）和 git diff --check 通过。Input 支持范围明确保留 visual bidi 待收尾；Windows 实际窗口和 Linux 的验收分别记录，不以此共同逻辑结果代替。
