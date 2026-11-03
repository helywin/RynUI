# Gallery 组件页面

Gallery 的独立页面改造按 [change 105](../../openspec/changes/105-20261103-redesign-gallery-component-pages/design.md) 分阶段实施。页面模型和内部页面宿主已经实现；真实样例与界面迁移仍按该 change 的未完成任务推进，内部夹具通过不代表原生界面已替换。

## 页面身份与目录状态

内部 [GalleryPageModel](../../examples/token_gallery/gallery_page_model.hpp) 为总览、设计介绍、Design Token 和 catalog 中每个 canonical identity 提供页面路由。Input、Search、Password、TextArea、OTP 对应同一个 Input identity 的五个分组；不增加目录条目。未知 identity 或不属于该组件的分组被拒绝，不静默切到另一页。

英文名称不区分 ASCII 大小写，中文名称和显式家族别名可以搜索。搜索与状态筛选只改变目录结果，不修改当前路由；当前页不匹配时由 `current_matches()` 给界面提供提示。目录顺序与七类计数来自实际数组。

唯一数据源仍是锁定的 [source manifest](../../gallery/ant-design/6.6.5/source-manifest.json) 和 [support overlay](../../gallery/ant-design/6.6.5/support-overlay.json)，由 [现有生成器](../../tools/update_ant_design_gallery_catalog.py) 生成离线 catalog。页面只引用原始 summary、supported_scope、missing_scope 和 evidence 标识符；native scope 与是否登记样例是不同概念。六种状态均有定义，即使当前某种状态数量为零。

目录状态计数不代表功能完成率或平台通过率；模型没有百分比字段，也不读取 tasks.md 或历史人工关闭记录。组件真正的样例归属及其工厂由后续页面登记层维护。

## 页面宿主与状态

[GalleryPageController](../../examples/token_gallery/gallery_page_controller.hpp) 是 example 内部适配器，依赖 `WindowComponentServices`，不是公开 Router API。shell 保留一个稳定的 page holder；页面内容由 `ReferenceSurfaceHost::append_slot` 建立正确的组件与 ReferenceSurface 构建上下文，每次只追加一个页面 root。卸载通过宿主 `destroy` 通知所有参与者；追加失败时由宿主回滚半挂载子树，再清理 ReferenceSurface 索引。

导航回调只调用 `request`；外围单个事件分发返回后再调用 `process`。不得在旧控件的回调栈内直接销毁页面。相同 page/family 的 section 或 sample 跳转保留 root，不压入页面历史。后退/前进保存 page/family；草稿以该身份分组，页内位置保存稳定 identity 和局部距离，实际重排后的夹紧由 viewport 完成。

`GalleryPageDraft` 只允许字符串、布尔、整数、数值和字符串列表，不接受组件/ref/Signal、编辑器或场景句柄。factory 以这些值重建新状态；save hook 只读取已提交业务值。Password/OTP 不调用 save hook，离页清除缓存。不能把整个样例状态对象存入草稿，也不能恢复旧 preedit、选区、撤销栈或 popup open 状态。

退役先使旧 generation ticket 失效，再保存草稿、取消属于旧 root 的 pointer 状态与 focus，调用 `before_retire` 取消 Gallery 自有 scrollbar drag，然后通过宿主销毁页面。既有输入参与者负责取消 session/preedit、弹层、keyboard press、caret 和动画。异步/延迟操作捕获弱 `GalleryPageTicket`，检查 `valid()` 后才能访问页面。挂载或保存失败仍清理旧资源，并显示可手动重试的说明页；不会自动重建忙循环。

页面代码只使用 `ryn::*Props`、typed slots 和现有公开组件；内部 host、controller、ticket 与稳定测试身份不进入 `include/ryn/`。稳定身份按 page → family → sample → test 的层次登记；与文案、挂载序号、Node/Interaction/Scene identity 无关。后续样例登记层负责旧 test ID 的唯一归属映射，切页后所有运行时句柄必须重新解析。

## 定向自动检查

```sh
cmake --build --preset linux-gcc-release --target rynui_gallery_page_model_tests rynui_gallery_document_model_tests rynui_ant_design_reference_catalog_tests
ctest --preset linux-gcc-release -R '^rynui\.(gallery_page_model|gallery_document_model|ant_design_reference_catalog|ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract)$' --output-on-failure
```

这里均为平台通用逻辑，不打开窗口，不代表 Linux/Windows 的原生 GPU 或系统输入服务验收。测试代码见 [页面模型测试](../../tests/gallery_page_model_tests.cpp)。

页面生命周期的定向入口如下：

```sh
cmake --build --preset linux-gcc-release --target rynui_gallery_page_model_tests rynui_gallery_page_controller_tests
ctest --preset linux-gcc-release -R '^rynui\.gallery_page_(model|controller)$' --output-on-failure
```

[生命周期夹具](../../tests/gallery_page_controller_tests.cpp) 使用真实组件宿主、模拟文本平台和记录上传的 GPU adapter，覆盖草稿、敏感清空、受控弹层退役、迟到文本/组合输入、capture/键盘按压、scrollbar drag、回调重入、失败回滚、主题/resize 身份以及稳定后无新增上传/帧请求。它不运行 SDL、真实 IME 或 GPU 驱动。
