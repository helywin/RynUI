# 比较基础与隔离库

Linux、平台通用 CPU 小阶段。`demo_differential_regions_tests.py` **5/5**：声明 sample-origin 保留 1 logical px 偏差；DPR 1/1.25/1.5/2 数学 floor/ceil；空 ROI/缺 guard tile/resize 拒绝；大背景中的 2×2 缺图标、裁字、位移和双侧同为空白仍暴露独立 red 指标。

`demo_differential_comparison_dependencies_tests.py` **2/2**：真实锁定 Pillow 12.0.0 archive 在 fresh 自有 out 安装，核完整 bytes/licenses/file set 与实际 import，读写图像；篡改文件/复用输出/错误 ABI 拒绝。同一 Python ABI 的 patch 版本不冻结。Windows wheel 仅锁定元数据，未在 Windows 运行。

OpenSpec 2.0.0 doctor healthy、全 strict **109/0**、diff check 通过。原始日志、图、源码副本与安装产物仅 out。此次不新增 checkbox，109 仍 **15/61**：4.2 需要的上层比较与校准尚未完成；本阶段未建立有效环境 profile，没有 paired 或 GPU fault 红绿结论。未接入 Table/Progress 后续队列。
