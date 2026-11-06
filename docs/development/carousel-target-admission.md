# Carousel 目标输入资格的消费时机

`CarouselComponentHost::refresh_target_admission(target, viewport, document_clip)` 是组件内部入口。外部原生组件在读取 Carousel 内目标的中央输入资格前，可显式请求生产者按已经落地的几何刷新其私有 slide gate。

入口只沿目标的真实组件祖先查找 Carousel/slide，从外到内处理各 owner 一次。它复用正常 auxiliary 的 viewport + owner translation + `component_clip` 公式，以及当前项、分支、可见交集、paint opacity、disabled、window activity 的同一判据。上一项、drag preview、透明度为零和完全离屏项仍拒绝输入；独立 legacy/owned block、Modal 与其他 restriction 仍由中央规则求交。目标引用不是公共授权能力，调用者须先核对窗口和确切绑定代际。

该入口不布局、不改变 paint branch、不推进动画、不调用完整 auxiliary，不直接调用 schedule/before/after，也不上传或发布视觉资源。它写入的是 Carousel 自己拥有的私有 slide bool lane；可能通过原窗口协调触发回调。调用后必须重核调用者 revision、目标绑定和 owner/row generation。返回 false 表示 producer 路径被回调退休或改写，旧消费应立即结束。稳定重复查询不分配、不发布新输入 revision、不请求 frame/deadline；祖先遍历以实际组件树为界，不扫描无关 Carousel。

Carousel 的 root、slide、arrow、dot 按实际 private block 写入点注册 exact ComponentId 的 resource cleanup，通过 Registry 的 quiet false 配对撤销自身位。不会枚举、清空或 prune Registry，也不改变 legacy true 的幂等语义。

这是已产生 pose 对应 admission 的局部发布合同。P 仍只按真实 owner 祖先关系调度 retained translation，不建立任意外部 ref 的通用依赖图。若某个实际较晚 writer 尚会移动目标，调用者必须先解决该具体几何依赖；不能据此入口声称所有 sibling/ref 顺序都已满足。
