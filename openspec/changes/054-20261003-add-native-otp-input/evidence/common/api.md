# OTP 公开 API 与 retained 格子

实际平台 Windows，MSVC/Ninja Multi-Config，`windows-msvc-headless`。

- Debug 扩展 OTP/Input/Password/Search/TextArea/Theme/quad/text scene/input scene allocation：19/19，151.84 秒；分配基线 142.16 秒通过。
- pending 失败清理与矩阵补测后 Debug OTP model/component/public API、Input/count/display/TextArea：7/7，3.67 秒；Release 最终扩展同 19 项：19/19，12.85 秒。
- OTP tests 验证默认/受控值、hidden suffix、length 前缀 editor/scene 保留、非法配置恢复、异常 separator 回滚/被动内容限制、mask 原值与敏感提示、centering、RTL 布局、两主题×三尺寸×四变体、相同本格不同尾部、部分/完成顺序与 controlled echo、资源卸载。
- 候选快照及回调副本在 local editor 发布前准备，组 commit 以 swap 接收已准备候选；失败编辑丢弃 pending，避免后续导航意外发布。
- clang-format 22.1.3：446 文件 0 失败；doctor healthy；strict validate 54/54；diff check 通过。公开头隔离通过独立 otp_public_api target 编译。

此为共同逻辑证据，不作为真实窗口/OS 输入/Linux 验收。日志在 `out/054-api-tests.log` 与 `out/054-api-final.log`，场景保留在 tests/otp_component_tests.cpp 与 tests/otp_public_api_tests.cpp。
