# Windows GPU 生命周期验收

验收日期：2026-08-26。

## 环境

- Host：Windows 10.0.26200，x64。
- Generator：`Ninja Multi-Config`。
- Compiler：MSVC 19.51.36256。
- SDL3：`3.4.14`，revision `147a8ee32`。
- SDL GPU driver：`direct3d12`。
- Dependency mode：`BUNDLED`。

## 自动测试

```powershell
./scripts/build-windows.ps1 -Configuration Debug
./scripts/build-windows.ps1 -Configuration Release
```

Debug 与 Release 均为 12/12 CTest 通过。覆盖范围包括：

- SDL init、Window、GPU device、claim 的成功路径。
- 四个初始化失败点只回滚已取得资源。
- 正常 command buffer、swapchain、clear render pass 和 submit。
- 空 swapchain texture 不进入 render pass，但仍正确提交 command buffer。
- swapchain acquire 失败时 cancel 尚未取得 texture 的 command buffer。
- 非 Window owner thread 不得进入 GPU API。

## 真实窗口 smoke

```powershell
./out/build/windows-msvc/examples/Debug/rynui_minimal.exe --smoke
./out/build/windows-msvc/examples/Release/rynui_minimal.exe --smoke
```

两种 configuration 均创建可见窗口、完成 clear/present、自动关闭并返回退出码 0。观测摘要一致：

```text
gpu_driver=direct3d12 command_buffers=1 render_passes=1 submissions=1 no_texture_frames=0
```

正常释放顺序由故障注入测试核对为：

```text
release Window claim
  -> destroy GPU device
  -> destroy Window
  -> SDL_Quit
```
