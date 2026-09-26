# Proposal

## Why

030 把 Gallery 滚动时 ordered scene 重建次数从 582 降到 220，但五进程最长 CPU 帧仍约 15 ms。第 240 步的临时分段计时表明约 11.5 ms 花在 `GlyphScene::replace_text`；源码中 `GlyphInstanceStore::replace` 即使向零长度 range 插入少量新字形，也为整份实例数组创建恰好大小的新 vector 并复制前后所有内容。同一帧多个文本进入视口时会重复分配和复制。

## What Changes

- 对零长度 range 的非空插入使用原 vector 的容量和原位插入，保留普通替换、删除的现有事务路径。
- 显式处理源 span 与当前 store 重叠的情况，保持旧 API 的数据结果；插入后继续准确更新 geometry/material dirty ranges。
- 增加多次中间插入、重叠源与容量复用测试，并复测 030 的五进程 D3D12 首帧与 240 步滚动，重点比较最长帧及实际上传工作量。

本 change 不改变公开 API、glyph 表示、绘制顺序、atlas 或目标 GPU buffer 策略。

## Capabilities

### New Capabilities

- `glyph-instance-insertion`: 对新文本的零长度实例 range 原位插入并保持顺序、失败和 dirty 合同。

### Modified Capabilities

无。

## Impact

涉及 `GlyphInstanceStore` 内部替换路径、平台通用测试与真实 Windows D3D12 Gallery。旧基线为 030 的 `gallery-order-batch-after.csv`。若减少 CPU 复制但 GPU 上传放大或其他指标回退，须如实报告。
