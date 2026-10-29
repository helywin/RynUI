# 099 平台通用图标替换

2026-10-29 用户恢复实施并要求 fast。本线程配置读回为 `gpt-6.1-sol / xhigh / serviceTier=priority`。规划父为 `7ca31dc26fa59a924ed7163043609c66f13c26c1`；此前的 [源码检查点](source-checkpoint.md) 是暂停时的历史状态，不再用作当前运行结论。

## 实现

- DatePicker 年／月导航使用 DoubleLeftOutlined、LeftOutlined、RightOutlined、DoubleRightOutlined，日期和星期仍为普通 Text。
- Menu 开合使用同一 retained Icon 的 RightOutlined／DownOutlined。
- Table 排序使用 ColumnHeightOutlined／ArrowUpOutlined／ArrowDownOutlined，受控拒绝和排序循环保留同一图标完整 generation。
- Pagination 翻页和跳转使用 LeftOutlined／RightOutlined／EllipsisOutlined；页码仍为 Text。预先保留 Text／Icon slots，通过内容与 visible 更新角色，不重挂载按钮；局部 Button icon_gap 为零，避免空 slot 留出图文间距。

图标继续继承原语义前景色和尺寸，通过内置图标字体渲染；未修改公开 API、Theme schema、图标资产、renderer ABI、RC、系统字体入口或 CMake。普通文字、标点及用户富 slot 不作替换。

## 新构建与实际执行

Windows MSVC，`windows-msvc-headless`／Ninja Multi-Config，最多 2 jobs。构建、测试的精确命令见 [driver](stage1-artifacts/099-msvc.ps1)。

| 配置 | 构建 exit／秒 | 定向 CTest | 测试 wrapper 秒 |
| --- | --- | --- | --- |
| Debug | 0／52.8413753 | 8/8 | 3.9922165 |
| Release | 0／71.9191343 | 8/8 | 1.5468513 |

每配置执行四个控件 DatePicker、Pagination、Table、portable Menu，加 portable Icon component／vector、Focus order 和 Input actions，各一次。新增断言比较实际 bundled IconSource、非普通文本 fallback 字体的非零 glyph、状态切换和完整 component generation；分页覆盖跳转→数字→跳转，原受控交互与销毁断言保留。

初次 driver 使用了 SDL 配置中的 Menu 目标名，HEADLESS Ninja 返回 unknown target，没有编译或执行测试；改为已有 portable 目标后完成上述正式构建，原诊断保留于索引。没有把此工具目标错误当作产品失败或测试通过。

[索引](stage1-artifacts/index.json) 保存 15 份逐字节复制的日志／命令／结果记录、10 个源码／文档指纹和 16 个 PE32+ 的 hash／size／mtime。16 个程序的 stack reserve 均为默认 1 MiB。索引中的 git_blob_sha256 来自暂存区，便于关联后续提交；不提交 EXE／OBJ／LIB。

clang-format 22.1.3 检查 804 个自有源文件零失败；显式 OpenSpec 1.14 doctor healthy、strict 99/99，diff check 通过。公开 API、五份金样、renderer／平台字体／RC 与 CMake 相对规划父逐字节不变。

## 边界

本阶段是实际新构建与平台通用 HEADLESS 测试，不是 Gallery 或原生视觉验收。Gallery inventory／目录合同、Windows Release 显示及用户反馈的滚动卡顿继续单独验证；图标测试通过不意味着性能问题已经修复。
