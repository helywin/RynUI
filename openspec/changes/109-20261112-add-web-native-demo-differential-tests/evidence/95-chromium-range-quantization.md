# Chromium 文本 Range 的横向量化

锁定 Blink 版本会把水平 Range 的左边向下量化到 1/64 CSS 像素。字形见证枚举该半开单元中实际可用的设备相位，保留原始像素、轮廓和覆盖判定。允许的相位不能支持唯一传递值时，不使用单相位取整推断。

合并后分层测试 37 项通过。Windows D3D12 的实际 Select 展开捕获中，原先被判为背景的字形点复验为双方相同的 RGB 244/244/244。该点的参考 DOM 横向坐标只允许相位 2 或 3。其余样例比较未据此标为通过。

[Chromium 141.0.7390.37 的 Range 边界实现](https://raw.githubusercontent.com/chromium/chromium/141.0.7390.37/third_party/blink/renderer/core/layout/inline/fragment_item.cc#L1008)
