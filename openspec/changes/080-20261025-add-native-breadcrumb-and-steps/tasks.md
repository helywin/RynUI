# Tasks

## 1. 平台通用：typed API 与状态模型

- [x] 1.1 实现 Breadcrumb/Steps typed Props/items/slots 和实际状态模型，记录固定 topology、唯一 key、current/状态及激活合同；提供初始 API 文档，在 windows-msvc-headless / MSVC / Ninja Multi-Config 的 Debug/Release 通过 model/public API 检查，覆盖空/隐藏项、current 极值、状态覆盖、受控拒绝及禁用投影，不把此阶段当作可运行组件交付。

## 2. 平台通用：Breadcrumb

- [x] 2.1 实现真实 Breadcrumb host、富标题/图标/separator、当前项和 pointer/keyboard 激活、可见/禁用布局及生命周期、独立 Theme 全链路；补全 docs/breadcrumb.md 真实示例与首版限制，Debug/Release component/Theme tests 验证实际激活/HitTest 恢复、flow 重排、保留身份、有限 glyph/surface/hit clip、父覆盖/inactive seed 和最终派生非法值原子拒绝，fresh goldens 保留旧字段。

## 3. 平台通用：Steps

- [x] 3.1 实现真实 Steps host、四状态/连接线/编号及富 slots、横纵/两尺寸、controlled/default current、roving focus 和正确键盘/指针激活；补全 docs/steps.md 与 Theme，Debug/Release component/Theme tests 覆盖 current/状态推导、拒绝/重试、隐藏/重新启用、实际子控件激活、焦点来源、短/零几何 clip、Modal/Spin/回调销毁、身份与 dirty 局部性，fresh goldens 证明旧值保持。

## 4. 平台通用：组合验收

- [x] 4.1 执行受影响 Theme/Button/Focus/Modal/Spin/Segmented/Collapse 等共同回归和实际 headless 边界构建，验证两个导航组件空闲无持续帧；完成五份 fresh golden 旧字段/IIFE 比较、可编译 API 示例和准确证据，通过 clang-format 22.x、doctor、strict、diff。交父线程独立 Astra xhigh 审查，未执行的 Gallery/原生 GPU/Linux/系统 IME/辅助技术不记为通过。

## 5. 父线程 main 集成

- [x] 5.1 经全新 Astra R3 通过后，将 worker `7db4123` 显式合入已验收 Date/Form Gallery 的 main，确认已审查导航实现和父 Date/Form reset、目录/样例保持；引用 worker 实际 Debug/Release 定向验收，在父树重建 Debug 导航/Theme/Focus/Date/Form 组合回归和日期 Gallery 聚焦场景、inventory/目录 contracts。生成 fresh goldens 比较旧字段与原 IIFE，准确更新 partial 范围和证据，通过格式、doctor、strict、diff 后提交推送 main 并核对远端 SHA，不推送 t3 分支。

合并来源、独立 R3、父 Debug 36/36、日期 Gallery/inventory、八项 contracts 和平台边界见 [main-integration.md](evidence/main-integration.md)。
