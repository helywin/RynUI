# 测试夹具

这里只保存测试必须读取的确定性输入与 schema，不保存运行日志或验收产物。

- `ant-design-6.6.5/`：锁定上游的 source contract、版本差异输入和验收 schema，供 tools 的离线合同检查读取；不是每次测试生成的输出。
- `acceptance/typography-windows.json`：验收解析器的合成元数据，测试正例/反例；没有真实截图，也不构成 Windows/GPU 通过证据。实际验收通过 `--evidence out/...` 读取本地产物。
