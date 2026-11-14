# 固定官方 Web reference

这是 109 的独立 Web 执行器。它运行锁定 Ant Design 源码、真实浏览器输入和观察；独立 oracle、native 执行、双侧比较与最终 verdict 由主 runner 负责。Web 执行成功不等于 native/Web 差分通过。

## 准备

在仓库根目录执行，输出目录必须尚不存在且属于当前 checkout 的 ignored `out/`：

```text
python -B tools/demo_reference_web/bootstrap.py --mode BUNDLED --output out/demo-differential/prepared
python -B tools/demo_reference_web/reference.py fonts --output out/demo-differential/fonts
python -B tools/demo_reference_web/reference.py identity
python -B -m unittest discover -s tests -p 'demo_differential_web_bootstrap_tests.py'
python -B -m unittest discover -s tests -p 'demo_differential_web_adapter_tests.py'
```

测试默认使用上述目录。独立运行目录可用 `RYNUI_WEB_PREPARED`、`RYNUI_WEB_FONTS` 指定，它们必须仍在本 checkout `out/`。仅执行五个官方 demo smoke 可给 adapter unittest 增加 `-k five`。缺工具、缺字体、缺浏览器均失败，不转为 skip。

`package.json` 的 52 个 direct 依赖均为精确版本；`pnpm-lock.yaml` 锁定传递解析。`dependencies.lock.json` 另外保存实际下载归档的 URL、integrity、SHA256、license 与包文件树摘要，包括跨平台 optional 包及 pnpm 自带包/license。Node 22.21.1、pnpm 10.23.0、React/ReactDOM 19.2.0、Playwright 1.56.1、esbuild 0.25.12、Chromium headless shell revision 1194 / 141.0.7390.37 均为显式输入。按用户最新要求，Python 只作脚本运行环境，不冻结解释器版本/二进制；检查 CPython >=3.10 兼容性并记录实际版本。Web Python 部分仅使用标准库，不额外安装比较库；比较器的第三方依赖由主 runner 自己锁定。

BUNDLED 从已核对的归档解包，逐文件检查安装包，再对新目录执行 `pnpm install --frozen-lockfile --ignore-scripts`。安装 registry 是由本次 prepare 创建的 loopback 服务，只提供独立 SHA256/integrity 校验后的锁定 tarball；store、node_modules、npm 配置和日志均在该输出目录。没有旧 store/node_modules 复制，也没有脚本安装、全局配置修改或产品 npm/CMake 依赖。归档首次准备可联网；已有完整归档时加 `--offline` 仍可从零安装。`--downloads <directory>` 仅按完整 SHA256 导入已有归档，partial 不会成为可用归档。

SYSTEM 必须显式提供 `--node <executable> --pnpm <package/bin/pnpm.cjs> --browser <executable> --source <official-source-root>`；版本、二进制、pnpm 整包及源码都必须匹配锁，没有 system-first 或自动 fallback。Linux x64 与 Windows x64 分别使用锁定归档/路径；其他 OS/arch 在下载前明确失败。Windows Node 必须保留完整官方 zip 文件树和 LICENSE；仅提供同版本的另一份 node.exe 不满足 SYSTEM。Windows browser 路径为 `chrome-win/headless_shell.exe`，Node 为 `node-v22.21.1-win-x64/node.exe`，来源与 Chromium revision 路径依据 [Playwright v1.56.1 registry](https://github.com/microsoft/playwright/blob/v1.56.1/packages/playwright-core/src/server/registry/index.ts)。

Windows 先按 [比较器解释器准备](demo-differential-comparison.md#windows-x64-解释器准备) 用已有标准库 Python 引导 out 内 CPython 3.14，然后用该解释器执行本页准备命令与整个 runner；这也避免旧 Python 3.10.11 缺少安全 tar data filter 的问题。Web 准备仍不依赖 Pillow。安装包集合须精确等于当前 OS/arch 的锁定闭包，包含 `@esbuild/win32-x64`；即使它在 npm 中标为 optional，缺失仍失败。全平台 optional tarball 继续保留在同一固定锁，版本/hash/license 不变。

Windows `browser_libraries` 由 CPU PE 解析记录 executable、全部 bundled DLL 和递归 eager imports 的实际 System32 文件 SHA256，API-set 经 System32-only loader 解析；不搜索 PATH，也不安装系统库。该指纹不证明 delay-load、运行时 GPU/driver 模块或浏览器页面执行，后者仍由实际 browser/CDP 验收提供。`--version` 只证明该锁定 executable 能报告版本。准备时清除 npm/pnpm/Node/esbuild 外部覆盖环境，安装仍仅使用锁定 Node/pnpm 与本次 loopback registry。

字体从唯一 `cmake/dependencies/RynUIDependencyLock.cmake` 读取 URL/version/hash/license，下载到 out，不另造漂移的字体版本。缺文件、错误 bytes/face/weight、意外系统 fallback 失败。验收链包含真实 Latin/CJK 400/500/600 和覆盖 U+1F642 的单色 Emoji 400；按需准备、实际 SFNT/轮廓校验、显式工具模式和未测平台边界见 [差分验收专用字体](demo-differential-fonts.md)。关闭 synthesis，按文本节点实际 weight/slant 与 Chromium selected PostScript face 对应显式来源；普通产品系统字体默认链不变。

`refresh_lock.py` 是独立维护操作，下载和校验已精确指定的解析结果后生成锁；prepare/run 不会调用它。变更锁需要独立提交与对应回归，不能用 native 输出迁移参考。

## 执行接口

```text
python -B tools/demo_reference_web/reference.py run --prepared out/demo-differential/prepared --request out/demo-differential/request.json --output out/demo-differential/web-run
```

Python API 为 `demo_reference_web.reference.execute(request, prepared_dir, output, reference='locked')`。envelope 仅允许 `schema_version:1`、`nonce`、严格 b594 case `definition` 和 `font_files`。字体项为 `{identity,path,sha256,face_index,weight}`，path 必须在本 checkout 的 out。`definition.fixture.viewport.dpr` 必须由主 runner 先解析成实际 native 数值；Web 不自行猜测 native scale，不读取 native 实测 bounds。

`identity` 返回 `adapter_id=antd665.reference-web.v1` 和 `app.tsx` 的 SHA256。主线统一 case/fixture/adaptation；本执行器不维护第二套 pilot。`official.source_paths` 必须匹配实际 bundler import closure，demo path/hash、commit、source tree、锁和 adapter identity 均核对。React 提交后读取真实组件树，核对执行的官方 demo/组件及实际公开 Props、value/defaultValue、options，不能只凭 manifest 自证模块身份。

两条通道分开记录：

- `official-demo` 直接运行五个固定官方 export，不替换它们内部 props、数据、样式或 callback。外层容器、字体、Provider 条件来自 fixture，明确不同于在线文档壳。
- `official-component-adapted` 使用相同源码模块的 Button/Input/Select。只配置公开 Props、数据、slot、容器、Theme 与父策略，不覆盖 CSS recipe。六个顶层 adaptation 项 `props/slots/data/container/theme/parent_policy` 必须逐项等于 fixture 并说明来源原因。

`container.layout='composition-v1'` 时，`props` 另含 `composition` 有限树：节点仅允许官方 `Flex/Space/Typography.Text`，公开布局/文字 Props，`control` leaf 按 target 引用控件；每个控件恰一次。Typography 内容用 literal，或 `{component:'Typography.Text',target,slot:'caption'}`。深度最多 24、节点最多 512；未知字段、重复/漏控件、任意 DOM/CSS/JS 失败。原有 row/column/wrap 也使用官方 Flex。样例完整 caption/feedback 与共同容器原点保留，不 tight crop。

适配数据结构：`props.controls[target]={component:'Button'|'Input'|'Select', ...公开Props, width?}`；`data[target]={options?,value?}` 为受控，`defaultValue?` 为非受控；`slots[target]={content?,prefix?,suffix?,caption?}` 按具体组件支持的 slot 消费。width 只转换为公开外部 width，容器 width/padding/gap 来自输入合同。未知或未消费字段失败。 `container.height=0` 明确表示自然高度（不设置 CSS height），正数为显式固定高度；不添加 overflow 裁剪。截图仍是实际 viewport，长上下文的完整覆盖必须由主 runner 显式滚动/检查点/tiles 追踪，不能把 viewport 外内容算作已捕获。wheel 在目标与 viewport 的可见交集中发送，并等待真实 wheel 事件回执；不以 wall sleep 证明派发。

`external-props.parameters.values.parent_policy` 只切换 `accept/reject`，不直接修改控件值。真实 callback 在父应用实际提交/拒绝后记录 `{proposed,committed,accepted}`。事件为 `activate/change/open-change/search/clear`，Input Enter 另记 `submit`。初始 `effective_inputs` 保持固定，动态编辑值、focus、popup、selection 和 callback 属于 state/trace。 `props.controls/data/slots` 从 React 提交后的实际组件输入重建（含真实 Icon 类型和 caption），另记录 actual_provider/actual_container；wrapper 声明元数据不作为消费证明。composition 公开节点逐项核对实际组件树，几何和字体另取 DOM/CDP。

鼠标、键盘、text commit、wheel 使用 Playwright 输入 API。disabled/loading 仍在实际 rect 上发送输入；receipt 和 trusted DOM 事件证明发送，不使用 actionability 等待把负例吞掉。找不到 target 失败；隐藏目标只能显式使用之前保存的 rect，不能以未发送动作满足零 callback。composition/异步等尚未接入的 action 明确失败；完整语义族由后续 109 阶段扩展。

## 观察、时钟与缓存

每次启动独立 headless Chromium、新 context、loopback 临时 server；context 拦截一切未声明资源和非本地网络。不连接用户浏览器或桌面，不发送物理输入。浏览器进程、临时 profile/cache/downloads 与超时清理属于本次执行。

输出 strict v1 `hello/receipt/checkpoint/complete` JSONL，所有 required 字段完整填写。观察来自 callback、DOM、computed/pseudo style、Range text box、selection、focus、scroll/clip、图标 SVG 与 Chromium `CSS.getPlatformFontsForNode`；CSS family 声明不替代实际字体来源。checkpoint 使用新 compositor screenshot，关联 nonce/case/step/scene revision/DPR/hash/paint receipt。漏动作、缺 target/图/complete、错 nonce 和截断必须非零。

页面加载前安装 Playwright JS clock，CSS 通过独立 Web Animations/CDP 控制；自动 canary 分别检查 JS timer 与 CSS 中间 opacity。截图不自动完成动画，不以 wall sleep 证明时钟。此阶段记录静态 motion-disabled 的真实 smoke/输入证据；完整动态反向、transitionend、隐藏取消和 native 同时点采样仍由后续时钟阶段验收。

UA 输入光标不属于 Web Animations。motion-disabled 使用浏览器实际支持的 `caret-animation: manual` 停止闪烁，截图保持 `caret: initial`，不隐藏光标或改变其颜色、选区；缺支持或实际焦点 editor 未应用该时钟策略时失败。clock provenance 记录该策略，同输入的新进程执行仍须得到一致截图，不能覆盖漂移缓存。

reference key 包含官方 source/dependency/adapter、完整 case/fixture/profile、font bytes/face、viewport/DPR/theme/locale/timezone/direction、实际 browser/OS 等环境；不含 native 输出。实际 GPU/backend 身份通过 CDP 查询并在 cache-hit 时另行精确核对，初始化耗时不作为兼容身份。缓存原子发布为只读目录，命中时检查完整文件集合与 SHA256，并重新执行全部浏览器动作与观察。错 key、截断或同条件截图漂移失败；不是拿旧图或旧 receipt 冒充本次执行。`--reference rebuild` 也执行并验证锁定参考，不能覆盖漂移的旧缓存消红。

截图、bundle/官方源码、完整 provenance、运行 JSON/日志和缓存仅在 out。tracked 内容只包括可复用代码、确定性锁和简短结论。这些测试不提供 native GPU、实际桌面 present、跨显示器、Windows/MSVC/D3D12、OS IME/AT 或校准后双侧视觉通过证据。

有限 factory 接线包含局部 ConfigProvider theme、Select prefix/control slot、multiple/tags/virtual/async、Input 非空 warning 与真实 feedback Text。已挂载后关闭的 popup 仍须从实际 committed portal 保留其 owner，不能因 aria-controls 被移除而伪造 missing/deferred。popup origin 只匹配实际传入并已提交的 React 元素身份，额外容器不能被折叠掉。source-feedback 的内容变化由独立逐步 equals literal 验证；未知字段或未消费子树继续失败。这些输入与行为证据不代替独立颜色校准及 paired 通过。
