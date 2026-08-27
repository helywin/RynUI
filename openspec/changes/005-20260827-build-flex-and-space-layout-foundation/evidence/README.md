# Layout evidence contract

本目录分别保存 Linux 与 Windows 的真实平台验收证据。两个平台使用相同字段，但必须写入独立文件、独立截图路径与对应平台身份；一个平台的结果不得填充或替代另一个平台。

`status=pending` 表示模板尚未在对应平台完成真实窗口验收。只有完成该平台 build、CTest、真实窗口操作、宽窄截图与诊断采集后，才能改为 `status=passed` 并填写数值。
