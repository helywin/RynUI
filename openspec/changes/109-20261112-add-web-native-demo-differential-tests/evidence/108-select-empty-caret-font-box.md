# 空 Select 编辑器的光标绘制

选项已选中、弹层打开且查询为空时，光标在主字体框中居中，以 1 个逻辑像素宽度进入设备覆盖率计算。输入查询后恢复普通编辑光标，编辑器、选区和搜索回调保持既有路径。

Windows MSVC HEADLESS Debug、Release 和 Linux GCC Release，各通过 2/2 项 Select/Input 定向 CTest。隐藏 D3D12 和私有 Wayland/Vulkan 均完成 Select 的 10 个检查点。原生与固定 Chromium 的实际相对光标几何均为 `(12, 75, 1, 19)`。Windows 1.5× 回读的上缘两列灰度为 227/241，实心段为 199/227，原生和 Chromium 一致。

此前在 Windows `542fa1b3` 进行的独立校准通过 38 项实采，正式配对三例全部完成且正确发布报告，最终 0/3 通过。上述光标修复尚未计入该正式判定。
