# Gallery 滚动同步

## 实施

- Input 字形层见 01；未跳过输入几何、caret、preedit、布局或受控更新。
- Gallery 在首次、实际布局或末轮布局尚未反映到锚点时重新解析目录，纯 translation 不扫描全部 Text；采样先等初始锚点完成，再清零计时与 reconciliation，采样期间仍要求两项计数为零。
- Button 的八个透明 loading 分段不再产生新几何，显现时同帧更新。原 retained slots、动画、主题和命中保留。
- Gallery 对 Quad 显式选择最多 1 MiB／至多 8 倍脏量的合并上传，默认资源策略保持精确 sparse range；缓存中的 clean bytes 经实际打包字节回归。
- Glyph buffer 倍增容量，新增后缀无论 CPU dirty 是否显式登记都打包并上传；异常仍使下一次完整重试。原 GPU ABI 不变。
- 修正原 telemetry 的错误参与者名称映射：旧前四项实际上不是 Button/Reference/Input/Selection；现在记录全部 40 个参与者索引，内部数组容量 64。

## 平台通用回归

官方 MSVC / Ninja Multi-Config / windows-msvc-headless，最多 2 jobs。最终 Debug 19/19（7.700217 s）、Release 19/19（4.3204874 s），涵盖 Input、Button、Quad/Glyph 资源、logical packing、Recording backend、Text、Form、Date/Time reset 和焦点生命周期。

新增检查实际覆盖透明 loading 的脏范围、位置恢复和身份；Quad 合并的字节保留、距离/字节上限及失败重试；Glyph 增量追加的容量和真实上传。首轮 Debug 在新增 Glyph 后缀断言失败，发现容量复用不能依赖原先的“每次增长都重分配”隐含行为，已显式规划追加后缀后通过；诊断保留。

## Windows 原生滚动

Release、D3D12 / DXIL、系统字体、host/render scale 1.5、物理窗口 1280×900、逻辑 viewport 853.333×600，240 步；测量时不并发构建。参考前测为 ac0d1196，本轮最终 EXE SHA256 为 `3CB37EB4835A6AABCDE5EB56A8965C32A76D62727B6EA6A235FE631FC3B66CAD`。

| 指标 | 前测 | 最终 |
|---|---:|---:|
| 平均 CPU 帧耗时 | 15.287 ms | 9.837 ms |
| p95 | 19.528 ms | 11.634 ms |
| 最大值 | 245.589 ms | 50.791 ms |
| 资源同步均值 | 3.288 ms | 1.651 ms |
| buffer 上传区间 | 22954 | 720 |
| buffer transfer 创建 | 720 | 485 |

最终 exit 0，8.1854621 s，采样期 reconciliation=0、anchor refresh=0；全部窗口退出。前测 exit 6 是启动 reconciliation=1 混入采样判定，保留原失败记录，未改掉采样区间的零额外同步检查。

初轮 culling/锚点优化约 13 ms；祖先状态缓存实验无平均收益，已撤回。单独跳过透明分段使区间碎片化到 102447，加入有界合并后降至 720。中间实验日志仅作诊断，性能表只用最终当前源码结果。

这不是最大化 4K 窗口、手动滚轮或高刷主观验收；p95 仍高于 120/144/240 Hz 帧预算，GALLERY-SCROLL-PERF-005 保持 open。Linux/GPU 其他平台、系统 IME/AT 不在本轮验收中。

## 集成及原始记录

最终 Release Recording 七入口均 exit 0：common 6.6300937 s、data 7.0953026 s、panels 12.2055067 s、forms 28.9335733 s、date-picker 52.7095309 s、time-picker 60.1486041 s、tree-select 51.2682696 s。精确 inventory 5.3919668 s、八项 contracts 0.4150168 s，全部通过。inventory 仍为 458 IDs／433 Buttons／1593 interactions／771 visible／307 live／163 Inputs。

原始命令、日志、二进制与源码 SHA 在相邻 final-artifacts 索引中固定。独立审查另行记录，不把本机执行当作审查者的新构建。

格式检查 804 个自有源文件零失败，显式 OpenSpec 1.14 doctor healthy、strict 100/100、diff check 通过。41 个执行产物均为 PE32+／1 MiB 默认栈。公开 include/ryn、Theme、平台字体／RC、shaders、五份 golden 及构建注册相对 34c92540 无差异。
