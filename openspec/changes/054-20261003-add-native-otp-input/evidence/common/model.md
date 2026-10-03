# OTP 候选模型

实际环境：Windows，MSVC 18，Ninja Multi-Config，`windows-msvc-headless` Debug/Release。

两配置 `otp_model/text_editor/text_reconcile/input_display` 均 4/4 通过，最终测试耗时见 `out/054-model-final.log`。OTP model 覆盖 CR/LF、组合字、ZWJ emoji、容量上限、外部隐藏后缀与用户截断、单格/整段替换、空洞、相同本格不同尾部、重复完成判断、controlled echo、foreign/stale/retired candidate、formatter 异常及同步长度/值重入。未运行真实窗口，不作为公开 OTP 或 OS 输入验收。

clang-format 22.1.3 检查 440 个自有文件通过；doctor healthy；strict validate 54/54；git diff --check 通过。公开组件、Gallery 与分平台窗口在后续任务验证。
