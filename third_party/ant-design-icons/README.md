# Ant Design 图标资源

来源：`@ant-design/icons-svg` 4.6.0（Ant Design 6.6.5 所用 icons 6.3.4 依赖该资源版本）。归档 URL、SHA256、上游 commit 与许可证集中记录于 `cmake/dependencies/RynUIDependencyLock.cmake`。SVG 来自发布包 `inline-svg/`，许可证来自相同 commit 的仓库根目录。`manifest.json` 锁定每个文件与生成结果。

这些 SVG 离线转换成私有的 CFF 轮廓容器并内嵌到库中，继续使用 FreeType/GPU glyph atlas；保留原 viewBox 和贝塞尔曲线。该容器不会进入普通文字 fallback chain，不需要安装系统字体或部署外部字体文件。当前覆盖十四个具名的单色图标（033 的九个加上 034 的 copy、check、edit、down、up）；不提供通用 SVG、双色或旋转 API。

维护时使用锁定的 FontTools 4.60.1：

```powershell
python -m pip install --target out/icon-tools fonttools==4.60.1
$env:PYTHONPATH = 'out/icon-tools'
python tools/generate_icon_assets.py
python tools/generate_icon_assets.py --check
```

正常 CMake configure/build 不执行生成工具。生成时间戳固定；`--check` 要求逐字节可重现。MIT 许可证保留在本目录的 LICENSE，安装包也应包含该许可。
