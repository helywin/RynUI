# Ant Design 图标资源

来源：`@ant-design/icons-svg` 4.6.0（Ant Design 6.6.5 所用 icons 6.3.4 依赖该资源版本）。归档 URL、SHA256、上游 commit 与许可证集中记录于 `cmake/dependencies/RynUIDependencyLock.cmake`。SVG 来自发布包 `inline-svg/`，许可证来自相同 commit 的仓库根目录。`manifest.json` 锁定每个文件与生成结果。

完整 848 个 SVG：outlined 447、filled 251、two-tone 150，离线转换成私有 CFF 轮廓容器并内嵌到库中，继续使用 FreeType/GPU glyph atlas；保留原 viewBox、贝塞尔曲线、逐层颜色角色/opacity 与绘制顺序。双色连续颜色层最多四层；SVG even-odd 路径在生成时转换为等价嵌套 winding。该容器不会进入普通文字 fallback chain，不需要安装系统字体或部署外部字体文件。公开 Icon 的双色/旋转/自定义行为由 051 后续阶段实现，资源生成完成不代表这些行为已完成。

原有十四个 IconName 数值与 E000–E00D 保留；新增名称按稳定顺序追加，IconName 底层 uint16_t。各图标首层 E000+index；额外层使用 supplementary private-use，Tooltip 私有箭头 F000–F003 保留。公开名字位于 include/ryn/generated/icon_names.inc，内部层表位于 src/icons/ant_design_icon_catalog.inc，manifest 保存全部源/层 bounds/角色/codepoints 与字体 SHA256。

维护时使用锁定的 FontTools 4.60.1：

```powershell
python -m pip install --target out/icon-tools fonttools==4.60.1
python tools/import_icon_assets.py --archive out/icons-svg-4.6.0.tgz --check
$env:PYTHONPATH = 'out/icon-tools'
python tools/generate_icon_assets.py
python tools/generate_icon_assets.py --check
python tools/verify_icon_assets.py
```

正常 CMake configure/build 不执行生成工具。生成时间戳固定；`--check` 要求逐字节可重现。MIT 许可证保留在本目录的 LICENSE，安装包也应包含该许可。
