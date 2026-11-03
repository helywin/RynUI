# Gallery 组件页面

Gallery 的独立页面改造按 [change 105](../../openspec/changes/105-20261103-redesign-gallery-component-pages/design.md) 分阶段实施。页面模型已经接入；页面宿主和界面迁移仍按该 change 的未完成任务推进，模型通过不代表原生界面已替换。

## 页面身份与目录状态

内部 [GalleryPageModel](../../examples/token_gallery/gallery_page_model.hpp) 为总览、设计介绍、Design Token 和 catalog 中每个 canonical identity 提供页面路由。Input、Search、Password、TextArea、OTP 对应同一个 Input identity 的五个分组；不增加目录条目。未知 identity 或不属于该组件的分组被拒绝，不静默切到另一页。

英文名称不区分 ASCII 大小写，中文名称和显式家族别名可以搜索。搜索与状态筛选只改变目录结果，不修改当前路由；当前页不匹配时由 `current_matches()` 给界面提供提示。目录顺序与七类计数来自实际数组。

唯一数据源仍是锁定的 [source manifest](../../gallery/ant-design/6.6.5/source-manifest.json) 和 [support overlay](../../gallery/ant-design/6.6.5/support-overlay.json)，由 [现有生成器](../../tools/update_ant_design_gallery_catalog.py) 生成离线 catalog。页面只引用原始 summary、supported_scope、missing_scope 和 evidence 标识符；native scope 与是否登记样例是不同概念。六种状态均有定义，即使当前某种状态数量为零。

目录状态计数不代表功能完成率或平台通过率；模型没有百分比字段，也不读取 tasks.md 或历史人工关闭记录。组件真正的样例归属及其工厂由后续页面登记层维护。

## 定向自动检查

```sh
cmake --build --preset linux-gcc-release --target rynui_gallery_page_model_tests rynui_gallery_document_model_tests rynui_ant_design_reference_catalog_tests
ctest --preset linux-gcc-release -R '^rynui\.(gallery_page_model|gallery_document_model|ant_design_reference_catalog|ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract)$' --output-on-failure
```

这里均为平台通用逻辑，不打开窗口，不代表 Linux/Windows 的原生 GPU 或系统输入服务验收。测试代码见 [页面模型测试](../../tests/gallery_page_model_tests.cpp)。
