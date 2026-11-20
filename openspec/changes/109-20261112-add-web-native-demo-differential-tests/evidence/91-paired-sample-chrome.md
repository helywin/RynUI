# 配对样例外围的页面装饰

配对截图继续使用真实样例工厂和原有 Gallery 布局。配对样例的外围页面装饰不提交到截图，样例自身的阴影和剪裁仍参与完整比较。普通 Gallery 的样例卡片维持原外观。

Windows HEADLESS Debug 的普通 Gallery、样例验收、原生输入和观测定向测试共 5 项通过。Windows D3D12 与 Linux 私有 Wayland/Vulkan 的 Select 均完成 10 个检查点和 30 张分块截图。Windows 的弹层旁原生灰线所在位置复验为纯白，与锁定 Web 截图一致。Select 整体配对结果尚未闭合。
