#include "gallery_i18n.hpp"

#include <ryn/text.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace rynui::example {

std::span<const GalleryMessageDefinition> gallery_messages() {
    using namespace ryn;
    // Keep authored-message initialization within the default MSVC thread stack.
    static constexpr void (*append_batches[])(std::vector<GalleryMessageDefinition>&) = {
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.000", String{u8"Clear this draft?"}},
                  String{u8"确认清除这份草稿？"}},
                 {MessageKey{u8"gallery.complete.copy.001", String{u8"Project details need review"}},
                  String{u8"项目资料待核对"}},
                 {MessageKey{u8"gallery.complete.copy.002", String{u8"Project details updated"}},
                  String{u8"项目资料已更新"}},
                 {MessageKey{u8"gallery.complete.copy.003", String{u8"Document review"}}, String{u8"资料审核"}},
                 {MessageKey{u8"gallery.complete.copy.004", String{u8"Review documents updated"}},
                  String{u8"审核资料已更新"}},
                 {MessageKey{u8"gallery.complete.copy.005", String{u8"Background task complete"}},
                  String{u8"后台任务完成"}},
                 {MessageKey{u8"gallery.complete.copy.006", String{u8"Tour child overlay"}}, String{u8"Tour 的子浮层"}},
                 {MessageKey{u8"gallery.complete.copy.007",
                             String{u8"Closes after five seconds; pauses on hover; collapses after three notices"}},
                  String{u8"五秒后关闭；悬停暂停，超过三条折叠"}},
                 {MessageKey{u8"gallery.complete.copy.008",
                             String{u8"The native target remains clickable. Go back to inspect the editor draft."}},
                  String{u8"原生目标仍可点击；可返回检查编辑草稿。"}},
                 {MessageKey{u8"gallery.complete.copy.009",
                             String{u8"A missing target centers the step; navigation and closing remain available."}},
                  String{u8"缺失目标时步骤居中，导航与关闭仍可用。"}},
                 {MessageKey{u8"gallery.complete.copy.010", String{u8"Edit notification notes"}},
                  String{u8"编辑通知备注"}},
                 {MessageKey{u8"gallery.complete.copy.011", String{u8"Fallback"}}, String{u8"回退"}},
                 {MessageKey{u8"gallery.complete.copy.012", String{u8"Explicit processing status"}},
                  String{u8"显式演示处理状态"}},
                 {MessageKey{u8"gallery.complete.copy.013", String{u8"Native ribbon"}}, String{u8"原生 Ribbon"}},
                 {MessageKey{u8"gallery.complete.copy.014", String{u8"Service available"}}, String{u8"服务正常"}},
                 {MessageKey{u8"gallery.complete.copy.015", String{u8"Link · Activation count"}},
                  String{u8"Link · 激活计数"}},
                 {MessageKey{u8"gallery.complete.copy.016", String{u8"Disabled link"}}, String{u8"禁用 Link"}},
                 {MessageKey{u8"gallery.complete.copy.017", String{u8"Processing the project"}},
                  String{u8"正在处理项目"}},
                 {MessageKey{u8"gallery.complete.copy.018", String{u8"Project processed successfully"}},
                  String{u8"项目处理成功"}},
                 {MessageKey{u8"gallery.complete.copy.019", String{u8"Closes after five seconds; pauses on hover"}},
                  String{u8"五秒后关闭，悬停暂停"}},
                 {MessageKey{
                      u8"gallery.complete.copy.020",
                      String{u8"Resize across the lg=992 breakpoint. The outer menu trigger remains clickable and "
                             u8"keyboard accessible when the sidebar has zero width."}},
                  String{
                      u8"改变窗口宽度触发 lg=992 断点。侧栏为零宽度时，外侧菜单触发器支持点击、Tab、Enter 和 Space。"}},
                 {MessageKey{u8"gallery.complete.copy.021", String{u8"Open confirmation"}}, String{u8"打开确认框"}},
                 {MessageKey{u8"gallery.complete.copy.022", String{u8"Open drawer"}}, String{u8"打开抽屉"}},
                 {MessageKey{u8"gallery.complete.copy.023", String{u8"Anchored confirmation"}}, String{u8"锚点确认"}},
                 {MessageKey{u8"gallery.complete.copy.024",
                             String{u8"Autoplay uses a monotonic deadline and pauses on hover, focus, window "
                                    u8"deactivation, hiding and "
                                    u8"reduced motion. Background dragging is independent of Input/Popover selection "
                                    u8"and capture."}},
                  String{u8"自动播放按单调 deadline；hover、focus、窗口失活、隐藏和 reduced motion "
                         u8"暂停。背景可拖动，Input/Popover 的选择和捕获独立处理。"}},
                 {MessageKey{u8"gallery.complete.copy.025", String{u8"Toggle RTL"}}, String{u8"切换 RTL"}},
                 {MessageKey{u8"gallery.complete.copy.026", String{u8"Accept/refuse expand and filter requests"}},
                  String{u8"接受 / 拒绝展开与筛选请求"}},
                 {MessageKey{u8"gallery.complete.copy.027", String{u8"Expand/collapse the first row"}},
                  String{u8"展开 / 折叠首行"}},
                 {MessageKey{u8"gallery.complete.copy.028", String{u8"Second native target"}},
                  String{u8"第二个原生目标"}},
                 {MessageKey{u8"gallery.complete.copy.029", String{u8"Start native tour"}}, String{u8"开始原生引导"}},
                 {MessageKey{u8"gallery.complete.copy.030", String{u8"Accept/refuse navigation and closing"}},
                  String{u8"接受 / 拒绝导航与关闭"}},
                 {MessageKey{u8"gallery.complete.copy.031", String{u8"Open nested modal"}},
                  String{u8"打开嵌套 Modal"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.032", String{u8"Draft"}}, String{u8"草稿"}},
                 {MessageKey{u8"gallery.complete.copy.033", String{u8"Settings"}}, String{u8"设置"}},
                 {MessageKey{u8"gallery.complete.copy.034", String{u8"Draft"}}, String{u8"草稿"}},
                 {MessageKey{u8"gallery.complete.copy.035", String{u8"Settings"}}, String{u8"设置"}},
                 {MessageKey{u8"gallery.complete.copy.036", String{u8"Disabled"}}, String{u8"禁用"}},
                 {MessageKey{u8"gallery.complete.copy.037", String{u8"Disabled"}}, String{u8"禁用"}},
                 {MessageKey{u8"gallery.complete.copy.038", String{u8"Retained draft"}}, String{u8"保留草稿"}},
                 {MessageKey{u8"gallery.complete.copy.039", String{u8"Lazy details"}}, String{u8"按需详情"}},
                 {MessageKey{u8"gallery.complete.copy.040", String{u8"Retained panel 1"}}, String{u8"保留面板一"}},
                 {MessageKey{u8"gallery.complete.copy.041", String{u8"Retained panel 2"}}, String{u8"保留面板二"}},
                 {MessageKey{u8"gallery.complete.copy.042", String{u8"Task"}}, String{u8"任务"}},
                 {MessageKey{u8"gallery.complete.copy.043", String{u8"Quantity"}}, String{u8"数量"}},
                 {MessageKey{u8"gallery.complete.copy.044", String{u8"Status"}}, String{u8"状态"}},
                 {MessageKey{u8"gallery.complete.copy.045", String{u8"Project"}}, String{u8"项目"}},
                 {MessageKey{u8"gallery.complete.copy.046", String{u8"Value 1"}}, String{u8"数值 1"}},
                 {MessageKey{u8"gallery.complete.copy.047", String{u8"Value 2"}}, String{u8"数值 2"}},
                 {MessageKey{u8"gallery.complete.copy.048", String{u8"Create a sample task"}},
                  String{u8"创建示例任务"}},
                 {MessageKey{u8"gallery.complete.copy.049", String{u8"Awaiting update"}}, String{u8"等待更新"}},
                 {MessageKey{u8"gallery.complete.copy.050", String{u8"Day view"}}, String{u8"日视图"}},
                 {MessageKey{u8"gallery.complete.copy.051", String{u8"Week view"}}, String{u8"周视图"}},
                 {MessageKey{u8"gallery.complete.copy.052", String{u8"Week view"}}, String{u8"周视图"}},
                 {MessageKey{u8"gallery.complete.copy.053", String{u8"List"}}, String{u8"列表"}},
                 {MessageKey{u8"gallery.complete.copy.054", String{u8"Overview"}}, String{u8"概览"}},
                 {MessageKey{u8"gallery.complete.copy.055", String{u8"Draft"}}, String{u8"草稿"}},
                 {MessageKey{u8"gallery.complete.copy.056", String{u8"Overview"}}, String{u8"概览"}},
                 {MessageKey{u8"gallery.complete.copy.057", String{u8"Public API"}}, String{u8"公开 API"}},
                 {MessageKey{u8"gallery.complete.copy.058", String{u8"Footer boundary"}}, String{u8"尾部边界"}},
                 {MessageKey{u8"gallery.complete.copy.059", String{u8"Home"}}, String{u8"首页"}},
                 {MessageKey{u8"gallery.complete.copy.060", String{u8"Project"}}, String{u8"项目"}},
                 {MessageKey{u8"gallery.complete.copy.061", String{u8"Draft"}}, String{u8"草稿"}},
                 {MessageKey{u8"gallery.complete.copy.062", String{u8"Preview"}}, String{u8"预览"}},
                 {MessageKey{u8"gallery.complete.copy.063", String{u8"Display hierarchy only"}},
                  String{u8"只展示层级"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.064", String{u8"Details"}}, String{u8"资料"}},
                 {MessageKey{u8"gallery.complete.copy.065", String{u8"Review"}}, String{u8"审核"}},
                 {MessageKey{u8"gallery.complete.copy.066", String{u8"Publish"}}, String{u8"发布"}},
                 {MessageKey{u8"gallery.complete.copy.067", String{u8"Start"}}, String{u8"起点"}},
                 {MessageKey{u8"gallery.complete.copy.068", String{u8"Processing"}}, String{u8"处理中"}},
                 {MessageKey{u8"gallery.complete.copy.069", String{u8"Complete"}}, String{u8"完成"}},
                 {MessageKey{u8"gallery.complete.copy.070", String{u8"Start"}}, String{u8"开始"}},
                 {MessageKey{u8"gallery.complete.copy.071", String{u8"Create the project and enter details"}},
                  String{u8"创建项目并填写资料"}},
                 {MessageKey{u8"gallery.complete.copy.072", String{u8"Review project documents"}},
                  String{u8"核对项目材料"}},
                 {MessageKey{u8"gallery.complete.copy.073", String{u8"RynUI project"}}, String{u8"RynUI 项目"}},
                 {MessageKey{u8"gallery.complete.copy.074", String{u8"Source code"}}, String{u8"源代码"}},
                 {MessageKey{u8"gallery.complete.copy.075", String{u8"Controls"}}, String{u8"控件"}},
                 {MessageKey{u8"gallery.complete.copy.076", String{u8"Theme"}}, String{u8"主题"}},
                 {MessageKey{u8"gallery.complete.copy.077", String{u8"Documents"}}, String{u8"文档"}},
                 {MessageKey{u8"gallery.complete.copy.078", String{u8"User guide"}}, String{u8"使用指南"}},
                 {MessageKey{u8"gallery.complete.copy.079", String{u8"Read-only archive"}}, String{u8"只读归档"}},
                 {MessageKey{u8"gallery.complete.copy.080", String{u8"Assets"}}, String{u8"资源"}},
                 {MessageKey{u8"gallery.complete.copy.081", String{u8"RynUI project"}}, String{u8"RynUI 项目"}},
                 {MessageKey{u8"gallery.complete.copy.082", String{u8"Documents"}}, String{u8"文档"}},
                 {MessageKey{u8"gallery.complete.copy.083", String{u8"Archive"}}, String{u8"归档"}},
                 {MessageKey{u8"gallery.complete.copy.084", String{u8"Deliverables"}}, String{u8"交付件"}},
                 {MessageKey{u8"gallery.complete.copy.085", String{u8"Fixed start column"}}, String{u8"固定起始列"}},
                 {MessageKey{u8"gallery.complete.copy.086", String{u8"Retained virtual-row editor"}},
                  String{u8"虚拟行的保留编辑器"}},
                 {MessageKey{u8"gallery.complete.copy.087", String{u8"Fixed end column / filter"}},
                  String{u8"固定结束列 / 筛选"}},
                 {MessageKey{u8"gallery.complete.copy.088", String{u8"Step 1: native editor target"}},
                  String{u8"第一步：原生编辑目标"}},
                 {MessageKey{u8"gallery.complete.copy.089", String{u8"Step 2: interactive target"}},
                  String{u8"第二步：可操作目标"}},
                 {MessageKey{u8"gallery.complete.copy.090", String{u8"Step 3: unmounted target"}},
                  String{u8"第三步：目标未挂载"}},
                 {MessageKey{u8"gallery.complete.copy.091", String{u8"Alpha"}}, String{u8"甲 / Alpha"}},
                 {MessageKey{u8"gallery.complete.copy.092", String{u8"Beta"}}, String{u8"乙 / Beta"}},
                 {MessageKey{u8"gallery.complete.copy.093", String{u8"Disabled"}}, String{u8"禁用 / Disabled"}},
                 {MessageKey{u8"gallery.complete.copy.094", String{u8"Desktop"}}, String{u8"桌面"}},
                 {MessageKey{u8"gallery.complete.copy.095", String{u8"Number"}}, String{u8"数字"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.096", String{u8"Disabled boolean"}}, String{u8"禁用布尔"}},
                 {MessageKey{u8"gallery.complete.copy.097", String{u8"Desktop"}}, String{u8"桌面"}},
                 {MessageKey{u8"gallery.complete.copy.098", String{u8"Number"}}, String{u8"数字"}},
                 {MessageKey{u8"gallery.complete.copy.099", String{u8"Disabled boolean"}}, String{u8"禁用布尔"}},
                 {MessageKey{u8"gallery.complete.copy.100", String{u8"Number"}}, String{u8"数字"}},
                 {MessageKey{u8"gallery.complete.copy.101", String{u8"Desktop"}}, String{u8"桌面"}},
                 {MessageKey{u8"gallery.complete.copy.102", String{u8"Service"}}, String{u8"服务"}},
                 {MessageKey{u8"gallery.complete.copy.103", String{u8"Desktop"}}, String{u8"桌面"}},
                 {MessageKey{u8"gallery.complete.copy.104", String{u8"Service"}}, String{u8"服务"}},
                 {MessageKey{u8"gallery.complete.copy.105", String{u8"End option 1"}}, String{u8"末尾选项一"}},
                 {MessageKey{u8"gallery.complete.copy.106", String{u8"End option 2"}}, String{u8"末尾选项二"}},
                 {MessageKey{u8"gallery.complete.copy.107", String{u8"Desktop"}}, String{u8"桌面"}},
                 {MessageKey{u8"gallery.complete.copy.108", String{u8"Service"}}, String{u8"服务"}},
                 {MessageKey{u8"gallery.complete.copy.109", String{u8"Draft"}}, String{u8"草稿"}},
                 {MessageKey{u8"gallery.complete.copy.110", String{u8"Service"}}, String{u8"服务"}},
                 {MessageKey{u8"gallery.complete.copy.111", String{u8"Operator"}}, String{u8"操作员"}},
                 {MessageKey{u8"gallery.complete.copy.112", String{u8"Reviewer"}}, String{u8"审核员"}},
                 {MessageKey{u8"gallery.complete.copy.113", String{u8"Priority 1"}}, String{u8"优先级 1"}},
                 {MessageKey{u8"gallery.complete.copy.114", String{u8"Priority 42"}}, String{u8"优先级 42"}},
                 {MessageKey{u8"gallery.complete.copy.115", String{u8"Disabled"}}, String{u8"禁用"}},
                 {MessageKey{u8"gallery.complete.copy.116", String{u8"Editor"}}, String{u8"编辑者"}},
                 {MessageKey{u8"gallery.complete.copy.117", String{u8"Reviewer"}}, String{u8"审核者"}},
                 {MessageKey{u8"gallery.complete.copy.118", String{u8"Team"}}, String{u8"团队可见"}},
                 {MessageKey{u8"gallery.complete.copy.119", String{u8"Private"}}, String{u8"仅自己"}},
                 {MessageKey{u8"gallery.complete.copy.120", String{u8"First segment"}}, String{u8"第一段"}},
                 {MessageKey{u8"gallery.complete.copy.121", String{u8"Second segment"}}, String{u8"第二段"}},
                 {MessageKey{u8"gallery.complete.copy.122", String{u8"Settings"}}, String{u8"设置 / Settings"}},
                 {MessageKey{u8"gallery.complete.copy.123", String{u8"Tasks"}}, String{u8"任务"}},
                 {MessageKey{u8"gallery.complete.copy.124", String{u8"Day view"}}, String{u8"日视图"}},
                 {MessageKey{u8"gallery.complete.copy.125",
                             String{u8"No tasks yet. Create the first record to get started."}},
                  String{u8"暂无任务，创建第一条记录开始使用"}},
                 {MessageKey{u8"gallery.complete.copy.126", String{u8"Task saved"}}, String{u8"任务已保存"}},
                 {MessageKey{u8"gallery.complete.copy.127", String{u8"Enter the name to save."}},
                  String{u8"填写用于保存的名称"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.128",
                             String{u8"Allowed: 2026-10-05 to 2026-11-25, excluding the 13th of each month."}},
                  String{u8"允许 2026-10-05 至 2026-11-25，每月 13 日不可选"}},
                 {MessageKey{u8"gallery.complete.copy.129",
                             String{u8"HH:mm:ss, 08:00:00 to 18:00:00, excluding 12:00."}},
                  String{u8"HH:mm:ss，08:00:00 至 18:00:00，12 时不可选"}},
                 {MessageKey{u8"gallery.complete.copy.130", String{u8"Project"}}, String{u8"项目"}},
                 {MessageKey{u8"gallery.complete.copy.131", String{u8"Not saved yet"}}, String{u8"尚未保存"}},
                 {MessageKey{u8"gallery.complete.copy.132", String{u8"Not saved yet"}}, String{u8"尚未保存"}},
                 {MessageKey{u8"gallery.complete.copy.133", String{u8"Not saved yet"}}, String{u8"尚未保存"}},
                 {MessageKey{u8"gallery.complete.copy.134", String{u8"Tasks / updated"}}, String{u8"任务 / 已更新"}},
                 {MessageKey{u8"gallery.complete.copy.135", String{u8"Day view / updated"}},
                  String{u8"日视图 / 已更新"}},
                 {MessageKey{u8"gallery.complete.copy.136", String{u8"The name is saved with the other fields."}},
                  String{u8"名称与其他字段一起保存"}},
                 {MessageKey{u8"gallery.complete.copy.137", String{u8"Dates are saved as YYYY-MM-DD strings."}},
                  String{u8"日期以 YYYY-MM-DD 字符串保存"}},
                 {MessageKey{u8"gallery.complete.copy.138", String{u8"Only confirmed HH:mm:ss strings are saved."}},
                  String{u8"仅确认后的 HH:mm:ss 字符串进入保存结果"}},
                 {MessageKey{u8"gallery.complete.copy.139", String{u8"Archive"}}, String{u8"归档"}},
                 {MessageKey{u8"gallery.complete.copy.140", String{u8"Temporarily unavailable"}}, String{u8"暂不可用"}},
                 {MessageKey{u8"gallery.complete.copy.141",
                             String{u8"Sample task created. Continue editing its details."}},
                  String{u8"已创建示例任务，可继续编辑详情"}},
                 {MessageKey{
                      u8"gallery.complete.copy.142", String{u8"Saved {note}"}, {{u8"note", MessageArgType::Text}}},
                  String{u8"已保存 {note}"}},
                 {MessageKey{
                      u8"gallery.complete.copy.143", String{u8"Saved {note}"}, {{u8"note", MessageArgType::Text}}},
                  String{u8"已保存 {note}"}},
                 {MessageKey{u8"gallery.complete.copy.144",
                             String{u8"Saved destination={destination} / {note}"},
                             {{u8"destination", MessageArgType::Text}, {u8"note", MessageArgType::Text}}},
                  String{u8"已保存目标={destination} / {note}"}},
                 {MessageKey{u8"gallery.complete.copy.145",
                             String{u8"Quantity must be at least {minimum}"},
                             {{u8"minimum", MessageArgType::UInt}}},
                  String{u8"数量至少为 {minimum}"}},
                 {MessageKey{u8"gallery.complete.copy.146", String{u8"Date must be a valid YYYY-MM-DD"}},
                  String{u8"日期须为有效的 YYYY-MM-DD"}},
                 {MessageKey{u8"gallery.complete.copy.147", String{u8"Date is outside the allowed range"}},
                  String{u8"日期不在允许范围"}},
                 {MessageKey{u8"gallery.complete.copy.148", String{u8"Time must be a valid HH:mm:ss"}},
                  String{u8"时间须为有效的 HH:mm:ss"}},
                 {MessageKey{u8"gallery.complete.copy.149", String{u8"Time is outside the allowed range"}},
                  String{u8"时间不在允许范围"}},
                 {MessageKey{u8"gallery.complete.copy.150",
                             String{u8"Saved successfully{values}"},
                             {{u8"values", MessageArgType::Text}}},
                  String{u8"保存成功{values}"}},
                 {MessageKey{u8"gallery.complete.copy.151", String{u8"Not submitted yet"}}, String{u8"尚未提交"}},
                 {MessageKey{u8"gallery.complete.copy.152",
                             String{u8"Each row retains its own Input identity and draft."}},
                  String{u8"每行保持独立 Input 身份与草稿"}},
                 {MessageKey{u8"gallery.complete.copy.153",
                             String{u8"Synthetic async validation: submit, then deliver; no network access."}},
                  String{u8"合成异步校验：提交后点击交付，不访问网络"}},
                 {MessageKey{u8"gallery.complete.copy.154",
                             String{u8"Submitted: {values}"},
                             {{u8"values", MessageArgType::Text}}},
                  String{u8"已提交：{values}"}},
                 {MessageKey{u8"gallery.complete.copy.155",
                             String{u8"Validation failed: {count} issues"},
                             {{u8"count", MessageArgType::UInt}}},
                  String{u8"校验失败：{count} 项"}},
                 {MessageKey{u8"gallery.complete.copy.156",
                             String{u8"List validation failed: {count}"},
                             {{u8"count", MessageArgType::UInt}}},
                  String{u8"列表校验失败：{count}"}},
                 {MessageKey{u8"gallery.complete.copy.157", String{u8"RynUI · Header"}}, String{u8"RynUI · 页头"}},
                 {MessageKey{u8"gallery.complete.copy.158", String{u8"Light / Dark"}}, String{u8"浅色 / 深色"}},
                 {MessageKey{u8"gallery.complete.copy.159", String{u8"Dotted"}}, String{u8"点线"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.160", String{u8"order 1 / push 1"}},
                  String{u8"顺序 1 / 向后移 1"}},
                 {MessageKey{u8"gallery.complete.copy.161", String{u8"offset 8 / pull 1"}},
                  String{u8"偏移 8 / 向前移 1"}},
                 {MessageKey{u8"gallery.complete.copy.162", String{u8"logical end"}}, String{u8"逻辑末端"}},
                 {MessageKey{u8"gallery.complete.copy.163",
                             String{u8"Space · separator / baseline / Compact / mixed / Addon"}},
                  String{u8"Space · 分隔 / 基线 / 紧凑 / 混合 / 附加内容"}},
                 {MessageKey{u8"gallery.complete.copy.164", String{u8"Settings content"}}, String{u8"设置内容"}},
                 {MessageKey{u8"gallery.complete.copy.165", String{u8"Locked"}}, String{u8"已锁定"}},
                 {MessageKey{u8"gallery.complete.copy.166",
                             String{u8"Decoded PNG fixture · real RGBA/alpha pixels · application IO boundary"}},
                  String{u8"解码 PNG 样本 · 真实 RGBA/透明像素 · 应用 IO 边界"}},
                 {MessageKey{u8"gallery.complete.copy.167", String{u8"Start loading"}}, String{u8"开始加载"}},
                 {MessageKey{u8"gallery.complete.copy.168", String{u8"Deliver pixels"}}, String{u8"交付像素"}},
                 {MessageKey{u8"gallery.complete.copy.169", String{u8"Deliver failure"}}, String{u8"交付失败"}},
                 {MessageKey{u8"gallery.complete.copy.170", String{u8"Toggle visibility / cancel"}},
                  String{u8"切换可见性 / 取消"}},
                 {MessageKey{u8"gallery.complete.copy.171", String{u8"Toggle disabled"}}, String{u8"切换禁用"}},
                 {MessageKey{u8"gallery.complete.copy.172", String{u8"Contain / cover"}}, String{u8"包含 / 覆盖"}},
                 {MessageKey{u8"gallery.complete.copy.173", String{u8"Accept / refuse preview request"}},
                  String{u8"接受 / 拒绝预览请求"}},
                 {MessageKey{u8"gallery.complete.copy.174", String{u8"Open preview"}}, String{u8"打开预览"}},
                 {MessageKey{u8"gallery.complete.copy.175",
                             String{u8"Native placeholder: waiting for application source"}},
                  String{u8"原生占位：等待应用来源"}},
                 {MessageKey{u8"gallery.complete.copy.176",
                             String{u8"Native error: use Deliver pixels or fallback sample"}},
                  String{u8"原生错误：可交付像素或查看回退样例"}},
                 {MessageKey{u8"gallery.complete.copy.177", String{u8"Malformed source → real decoded fallback"}},
                  String{u8"无效来源 → 真实解码回退"}},
                 {MessageKey{u8"gallery.complete.copy.178", String{u8"Reorder stable keys"}},
                  String{u8"按稳定 key 重排"}},
                 {MessageKey{u8"gallery.complete.copy.179", String{u8"Toggle RTL"}}, String{u8"切换 RTL"}},
                 {MessageKey{u8"gallery.complete.copy.180", String{u8"Preview"}}, String{u8"预览"}},
                 {MessageKey{u8"gallery.complete.copy.181", String{u8"Remove"}}, String{u8"移除"}},
                 {MessageKey{u8"gallery.complete.copy.182", String{u8"Abort"}}, String{u8"中止"}},
                 {MessageKey{u8"gallery.complete.copy.183",
                             String{u8"Synthetic app transport · manual progress/success/failure · actual decoded "
                                    u8"thumbnail fixture"}},
                  String{u8"合成应用传输 · 手动交付进度/成功/失败 · 实际解码缩略图样本"}},
                 {MessageKey{u8"gallery.complete.copy.184", String{u8"Select repository fixture"}},
                  String{u8"选择仓库样本"}},
                 {MessageKey{u8"gallery.complete.copy.185", String{u8"Select ignored fixture"}},
                  String{u8"选择被忽略样本"}},
                 {MessageKey{u8"gallery.complete.copy.186", String{u8"Deliver progress +20%"}},
                  String{u8"交付进度 +20%"}},
                 {MessageKey{u8"gallery.complete.copy.187", String{u8"Deliver synthetic success"}},
                  String{u8"交付合成成功"}},
                 {MessageKey{u8"gallery.complete.copy.188", String{u8"Approve pending removal"}},
                  String{u8"批准待处理移除"}},
                 {MessageKey{u8"gallery.complete.copy.189", String{u8"Text / picture / card / circle"}},
                  String{u8"文字 / 图片 / 卡片 / 圆形"}},
                 {MessageKey{u8"gallery.complete.copy.190", String{u8"Custom retained file draft"}},
                  String{u8"自定义保留文件草稿"}},
                 {MessageKey{u8"gallery.complete.copy.191", String{u8"Toggle read only"}}, String{u8"切换只读"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.192", String{u8"Toggle disabled / cancel"}},
                  String{u8"切换禁用 / 取消"}},
                 {MessageKey{u8"gallery.complete.copy.193", String{u8"Accept / refuse controlled list"}},
                  String{u8"接受 / 拒绝受控列表"}},
                 {MessageKey{u8"gallery.complete.copy.194", String{u8"READY"}}, String{u8"已就绪"}},
                 {MessageKey{u8"gallery.complete.copy.195", String{u8"Edit"}}, String{u8"编辑"}},
                 {MessageKey{u8"gallery.complete.copy.196", String{u8"Save"}}, String{u8"保存"}},
                 {MessageKey{u8"gallery.complete.copy.197", String{u8"More"}}, String{u8"更多"}},
                 {MessageKey{u8"gallery.complete.copy.198", String{u8"12 dp padding"}}, String{u8"12 dp 内边距"}},
                 {MessageKey{u8"gallery.complete.copy.199", String{u8"Nested action"}}, String{u8"嵌套操作"}},
                 {MessageKey{u8"gallery.complete.copy.200", String{u8"Data remains mounted"}},
                  String{u8"数据保持挂载"}},
                 {MessageKey{u8"gallery.complete.copy.201", String{u8"Prev"}}, String{u8"上一页"}},
                 {MessageKey{u8"gallery.complete.copy.202", String{u8"Next"}}, String{u8"下一页"}},
                 {MessageKey{u8"gallery.complete.copy.203", String{u8"ScrollX / Fade"}},
                  String{u8"横向滚动 / 淡入淡出"}},
                 {MessageKey{u8"gallery.complete.copy.204", String{u8"Autoplay"}}, String{u8"自动播放"}},
                 {MessageKey{u8"gallery.complete.copy.205", String{u8"Pause"}}, String{u8"暂停"}},
                 {MessageKey{u8"gallery.complete.copy.206", String{u8"Play"}}, String{u8"播放"}},
                 {MessageKey{u8"gallery.complete.copy.207", String{u8"Vertical"}}, String{u8"纵向"}},
                 {MessageKey{u8"gallery.complete.copy.208", String{u8"Ref next"}}, String{u8"通过 ref 下一页"}},
                 {MessageKey{u8"gallery.complete.copy.209", String{u8"Year"}}, String{u8"年"}},
                 {MessageKey{u8"gallery.complete.copy.210", String{u8"Month"}}, String{u8"月"}},
                 {MessageKey{u8"gallery.complete.copy.211", String{u8"Overview"}}, String{u8"概览"}},
                 {MessageKey{u8"gallery.complete.copy.212", String{u8"Tools"}}, String{u8"工具"}},
                 {MessageKey{u8"gallery.complete.copy.213", String{u8"Logs"}}, String{u8"日志"}},
                 {MessageKey{u8"gallery.complete.copy.214", String{u8"Settings"}}, String{u8"设置"}},
                 {MessageKey{u8"gallery.complete.copy.215", String{u8"Unavailable"}}, String{u8"不可用"}},
                 {MessageKey{u8"gallery.complete.copy.216", String{u8"Reset"}}, String{u8"重置"}},
                 {MessageKey{u8"gallery.complete.copy.217", String{u8"Home"}}, String{u8"首页"}},
                 {MessageKey{u8"gallery.complete.copy.218", String{u8"Build"}}, String{u8"构建"}},
                 {MessageKey{u8"gallery.complete.copy.219", String{u8"Test"}}, String{u8"测试"}},
                 {MessageKey{u8"gallery.complete.copy.220", String{u8"More"}}, String{u8"更多"}},
                 {MessageKey{u8"gallery.complete.copy.221", String{u8"Workspace settings"}}, String{u8"工作区设置"}},
                 {MessageKey{u8"gallery.complete.copy.222", String{u8"Help and documentation"}},
                  String{u8"帮助与文档"}},
                 {MessageKey{u8"gallery.complete.copy.223", String{u8"New item"}}, String{u8"新增项目"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.224", String{u8"Tools"}}, String{u8"工具"}},
                 {MessageKey{u8"gallery.complete.copy.225", String{u8"Settings"}}, String{u8"设置"}},
                 {MessageKey{u8"gallery.complete.copy.226", String{u8"Refresh data"}}, String{u8"刷新数据"}},
                 {MessageKey{u8"gallery.complete.copy.227", String{u8"Clear selection"}}, String{u8"清空选择"}},
                 {MessageKey{u8"gallery.complete.copy.228", String{u8"Unavailable command"}}, String{u8"不可用命令"}},
                 {MessageKey{u8"gallery.complete.copy.229", String{u8"Run task"}}, String{u8"运行任务"}},
                 {MessageKey{u8"gallery.complete.copy.230", String{u8"View logs"}}, String{u8"查看日志"}},
                 {MessageKey{u8"gallery.complete.copy.231", String{u8"Apply"}}, String{u8"应用"}},
                 {MessageKey{u8"gallery.complete.copy.232", String{u8"Cancel"}}, String{u8"取消"}},
                 {MessageKey{u8"gallery.complete.copy.233", String{u8"Desktop"}}, String{u8"桌面"}},
                 {MessageKey{u8"gallery.complete.copy.234", String{u8"Service"}}, String{u8"服务"}},
                 {MessageKey{u8"gallery.complete.copy.235", String{u8"Device"}}, String{u8"设备"}},
                 {MessageKey{u8"gallery.complete.copy.236", String{u8"Added number"}}, String{u8"新增数字"}},
                 {MessageKey{u8"gallery.complete.copy.237", String{u8"Added number"}}, String{u8"新增数字"}},
                 {MessageKey{u8"gallery.complete.copy.238", String{u8"No pending load"}}, String{u8"没有待处理加载"}},
                 {MessageKey{u8"gallery.complete.copy.239",
                             String{u8"Text watermarks retain real input; switch to shared images or async sources."}},
                  String{u8"文字水印保留真实输入；可切换共享图像或异步来源。"}},
                 {MessageKey{u8"gallery.complete.copy.240",
                             String{u8"Open a notice, edit its rich content, then switch language."}},
                  String{u8"打开通知，在富内容中编辑，再切换语言。"}},
                 {MessageKey{u8"gallery.complete.copy.241", String{u8"Waiting for actual column allocation"}},
                  String{u8"等待实际分列"}},
                 {MessageKey{u8"gallery.complete.copy.242",
                             String{u8"Custom header and fullCell; detail notes are retained by date key."}},
                  String{u8"自定义 header 与 fullCell；详情内的备注由日期 key 保留。"}},
                 {MessageKey{u8"gallery.complete.copy.243",
                             String{u8"Application loader pending; use delivery controls"}},
                  String{u8"应用加载器等待中；请使用交付操作"}},
                 {MessageKey{u8"gallery.complete.copy.244", String{u8"Application loader cancelled"}},
                  String{u8"应用加载器已取消"}},
                 {MessageKey{u8"gallery.complete.copy.245", String{u8"Image onLoad: decoded pixels are ready"}},
                  String{u8"Image onLoad：解码像素已就绪"}},
                 {MessageKey{u8"gallery.complete.copy.246", String{u8"Async watermark request cancelled"}},
                  String{u8"异步水印请求已取消"}},
                 {MessageKey{u8"gallery.complete.copy.247", String{u8"Image watermark loaded"}},
                  String{u8"图像水印加载完成"}},
                 {MessageKey{u8"gallery.complete.copy.248",
                             String{u8"Watermark image failed to load; retry or switch to text."}},
                  String{u8"水印图像加载失败；可重试或切回文字。"}},
                 {MessageKey{u8"gallery.complete.copy.249", String{u8"Notice is still open"}},
                  String{u8"通知尚未关闭"}},
                 {MessageKey{u8"gallery.complete.copy.250", String{u8"Save confirmed"}}, String{u8"已确认保存"}},
                 {MessageKey{u8"gallery.complete.copy.251", String{u8"Clear confirmed"}}, String{u8"已确认清除"}},
                 {MessageKey{u8"gallery.complete.copy.252", String{u8"Native Tour target callback invoked"}},
                  String{u8"Tour 目标真实 callback"}},
                 {MessageKey{u8"gallery.complete.copy.253", String{u8"Owner delivered decoded pixels"}},
                  String{u8"所有者已交付解码像素"}},
                 {MessageKey{u8"gallery.complete.copy.254", String{u8"Stale/duplicate delivery rejected"}},
                  String{u8"已拒绝过期或重复交付"}},
                 {MessageKey{u8"gallery.complete.copy.255", String{u8"Close result: Closed"}},
                  String{u8"关闭结果：Closed"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.256", String{u8"Close result: Cancelled"}},
                  String{u8"关闭结果：Cancelled"}},
                 {MessageKey{u8"gallery.complete.copy.257",
                             String{u8"Selected: {name} ({count} files)"},
                             {{u8"name", MessageArgType::Text}, {u8"count", MessageArgType::UInt}}},
                  String{u8"已选择：{name}（{count} 个文件）"}},
                 {MessageKey{u8"gallery.complete.copy.258",
                             String{u8"Uploading: {name} ({count} files)"},
                             {{u8"name", MessageArgType::Text}, {u8"count", MessageArgType::UInt}}},
                  String{u8"正在上传：{name}（{count} 个文件）"}},
                 {MessageKey{u8"gallery.complete.copy.259",
                             String{u8"Progress: {name} ({count} files)"},
                             {{u8"name", MessageArgType::Text}, {u8"count", MessageArgType::UInt}}},
                  String{u8"进度：{name}（{count} 个文件）"}},
                 {MessageKey{u8"gallery.complete.copy.260",
                             String{u8"Completed: {name} ({count} files)"},
                             {{u8"name", MessageArgType::Text}, {u8"count", MessageArgType::UInt}}},
                  String{u8"已完成：{name}（{count} 个文件）"}},
                 {MessageKey{u8"gallery.complete.copy.261",
                             String{u8"Failed: {name} ({count} files)"},
                             {{u8"name", MessageArgType::Text}, {u8"count", MessageArgType::UInt}}},
                  String{u8"已失败：{name}（{count} 个文件）"}},
                 {MessageKey{u8"gallery.complete.copy.262",
                             String{u8"Removed: {name} ({count} files)"},
                             {{u8"name", MessageArgType::Text}, {u8"count", MessageArgType::UInt}}},
                  String{u8"已移除：{name}（{count} 个文件）"}},
                 {MessageKey{u8"gallery.complete.copy.263",
                             String{u8"Cancelled: {name} ({count} files)"},
                             {{u8"name", MessageArgType::Text}, {u8"count", MessageArgType::UInt}}},
                  String{u8"已取消：{name}（{count} 个文件）"}},
                 {MessageKey{u8"gallery.complete.copy.264",
                             String{u8"Application download callback: {name}"},
                             {{u8"name", MessageArgType::Text}}},
                  String{u8"应用下载回调：{name}"}},
                 {MessageKey{u8"gallery.complete.copy.265",
                             String{u8"PNG {png} bytes; SVG {svg} bytes ready in application memory"},
                             {{u8"png", MessageArgType::UInt}, {u8"svg", MessageArgType::UInt}}},
                  String{u8"应用内存中已准备 PNG {png} 字节；SVG {svg} 字节"}},
                 {MessageKey{u8"gallery.complete.copy.266",
                             String{u8"Submitted name={name} amount={amount} custom={custom} submitCount={count}"},
                             {{u8"name", MessageArgType::Text},
                              {u8"amount", MessageArgType::Text},
                              {u8"custom", MessageArgType::Text},
                              {u8"count", MessageArgType::UInt}}},
                  String{u8"已提交 name={name} amount={amount} custom={custom} submitCount={count}"}},
                 {MessageKey{u8"gallery.complete.copy.267",
                             String{u8"Async submitted name={name} confirmation={confirmation}"},
                             {{u8"name", MessageArgType::Text}, {u8"confirmation", MessageArgType::Text}}},
                  String{u8"实际异步提交 name={name} confirmation={confirmation}"}},
                 {MessageKey{u8"gallery.complete.copy.268", String{u8"Old snapshot cancelled"}},
                  String{u8"旧快照已取消"}},
                 {MessageKey{u8"gallery.complete.copy.269",
                             String{u8"Validation errors: {count}"},
                             {{u8"count", MessageArgType::UInt}}},
                  String{u8"校验错误 {count}"}},
                 {MessageKey{u8"gallery.sample.table.completion.title",
                             String{u8"Virtual rows, tree expansion, filters, fixed columns and draggable scrollbars"}},
                  String{u8"虚拟行、树展开、筛选、固定列与拖拽滚动条"}},
                 {MessageKey{u8"gallery.sample.tour.native.title",
                             String{u8"Real targets, retained rich steps, controlled navigation and nested overlays"}},
                  String{u8"真实目标、保留富步骤、受控导航与嵌套浮层"}},
                 {MessageKey{u8"gallery.complete.copy.272", String{u8"Enter submits; IME handles candidates first"}},
                  String{u8"Enter 提交；IME 先处理候选"}},
                 {MessageKey{u8"gallery.complete.copy.273", String{u8"Typed adapter"}}, String{u8"类型化适配器"}},
                 {MessageKey{
                      u8"gallery.complete.copy.274", String{u8"Member {index}"}, {{u8"index", MessageArgType::UInt}}},
                  String{u8"成员 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.275", String{u8"Keep at least one member"}},
                  String{u8"至少保留一位成员"}},
                 {MessageKey{u8"gallery.complete.copy.276", String{u8"Synthetic rule: taken is already in use"}},
                  String{u8"合成规则：taken 已被占用"}},
                 {MessageKey{u8"gallery.complete.copy.277", String{u8"The two names do not match"}},
                  String{u8"两次名称不一致"}},
                 {MessageKey{u8"gallery.complete.copy.278", String{u8"Validating the current snapshot"}},
                  String{u8"正在校验当前快照"}},
                 {MessageKey{u8"gallery.complete.copy.279", String{u8"Validation idle"}}, String{u8"校验空闲"}},
                 {MessageKey{u8"gallery.complete.copy.280", String{u8"Task saved"}}, String{u8"任务已保存"}},
                 {MessageKey{u8"gallery.complete.copy.281", String{u8"Task needs confirmation"}},
                  String{u8"任务需要确认"}},
                 {MessageKey{u8"gallery.complete.copy.282", String{u8"Task save failed"}}, String{u8"任务保存失败"}},
                 {MessageKey{u8"gallery.complete.copy.283", String{u8"Task processing"}}, String{u8"任务处理中"}},
                 {MessageKey{u8"gallery.complete.copy.284", String{u8"403 · Access denied"}},
                  String{u8"403 · 暂无访问权限"}},
                 {MessageKey{u8"gallery.complete.copy.285", String{u8"404 · Task not found"}},
                  String{u8"404 · 未找到任务"}},
                 {MessageKey{u8"gallery.complete.copy.286", String{u8"500 · Service unavailable"}},
                  String{u8"500 · 服务暂不可用"}},
                 {MessageKey{u8"gallery.frame.gallery.document.header-source.title", String{u8"RynUI Design System"}},
                  String{u8"RynUI 设计系统"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{
                      u8"gallery.frame.gallery.document.header-source.summary",
                      String{u8"Offline implementation reference pinned to Ant Design 6.6.5 and its source commit."}},
                  String{u8"RynUI 的离线实现参考，锁定 Ant Design 6.6.5 与对应源码提交。"}},
                 {MessageKey{u8"gallery.frame.gallery.document.introduction.title", String{u8"Introduction"}},
                  String{u8"设计介绍"}},
                 {MessageKey{
                      u8"gallery.frame.gallery.document.introduction.summary",
                      String{u8"A desktop C++ enterprise design-system baseline emphasizing clarity, consistency, "
                             u8"verifiability and evolution."}},
                  String{u8"面向桌面 C++ 的企业级设计系统基线，强调清晰、一致、可验证与可演进。"}},
                 {MessageKey{u8"gallery.frame.gallery.document.design-values.title", String{u8"Design values"}},
                  String{u8"设计价值"}},
                 {MessageKey{u8"gallery.frame.gallery.document.design-values.summary",
                             String{u8"Four values explain trade-offs; component contracts, Design Tokens and platform "
                                    u8"acceptance remain authoritative."}},
                  String{u8"四项价值用于解释取舍，不替代组件合同、Design Token 或平台验收。"}},
                 {MessageKey{u8"gallery.frame.gallery.document.foundation-tokens.title",
                             String{u8"Foundation and Design Token"}},
                  String{u8"基础样式与 Token"}},
                 {MessageKey{
                      u8"gallery.frame.gallery.document.foundation-tokens.summary",
                      String{u8"Colors, typography, spacing, corners and shadows come from pinned tokens; descriptive "
                             u8"content uses actual text."}},
                  String{u8"颜色、排版、间距、圆角与阴影来自锁定 Token，不用交互控件模拟说明内容。"}},
                 {MessageKey{u8"gallery.frame.gallery.document.component-overview.title",
                             String{u8"Component overview"}},
                  String{u8"组件总览"}},
                 {MessageKey{
                      u8"gallery.frame.gallery.document.component-overview.summary",
                      String{u8"All 73 entries across seven categories show supported subsets, missing scope and "
                             u8"traceable references."}},
                  String{u8"七类 73 项完整列出，并区分已支持子集、缺失范围与可定位证据。"}},
                 {MessageKey{u8"gallery.frame.gallery.document.live-samples.title", String{u8"Live samples"}},
                  String{u8"交互示例"}},
                 {MessageKey{
                      u8"gallery.frame.gallery.document.live-samples.summary",
                      String{u8"Only actual RynUI components enter the interactive tree; the catalog and documentation "
                             u8"remain descriptive."}},
                  String{u8"只有 RynUI 已提供的真实组件进入交互树；目录与说明保持非交互。"}},
                 {MessageKey{u8"gallery.complete.copy.299", String{u8"Documents"}}, String{u8"文档"}},
                 {MessageKey{u8"gallery.complete.copy.300", String{u8"Filter by support status"}},
                  String{u8"支持状态筛选"}},
                 {MessageKey{u8"gallery.complete.copy.301",
                             String{u8"Source: {path} · {count} reference records"},
                             {{u8"path", MessageArgType::Text}, {u8"count", MessageArgType::UInt}}},
                  String{u8"来源：{path} · {count} 项参考记录"}},
                 {MessageKey{u8"gallery.complete.copy.302", String{u8"Primary"}}, String{u8"主色"}},
                 {MessageKey{u8"gallery.complete.copy.303", String{u8"Success"}}, String{u8"成功"}},
                 {MessageKey{u8"gallery.complete.copy.304", String{u8"Warning"}}, String{u8"警告"}},
                 {MessageKey{u8"gallery.complete.copy.305", String{u8"Error"}}, String{u8"错误"}},
                 {MessageKey{u8"gallery.complete.copy.306", String{u8"Info"}}, String{u8"信息"}},
                 {MessageKey{u8"gallery.complete.copy.307", String{u8"Blue 6"}}, String{u8"蓝色 6"}},
                 {MessageKey{u8"gallery.complete.copy.308", String{u8"Green 6"}}, String{u8"绿色 6"}},
                 {MessageKey{u8"gallery.complete.copy.309", String{u8"Red 5"}}, String{u8"红色 5"}},
                 {MessageKey{u8"gallery.complete.copy.310", String{u8"Gold 6"}}, String{u8"金色 6"}},
                 {MessageKey{u8"gallery.complete.copy.311", String{u8"Purple 6"}}, String{u8"紫色 6"}},
                 {MessageKey{u8"gallery.complete.copy.312", String{u8"Font 12"}}, String{u8"字号 12"}},
                 {MessageKey{u8"gallery.complete.copy.313", String{u8"Font 14"}}, String{u8"字号 14"}},
                 {MessageKey{u8"gallery.complete.copy.314", String{u8"Font 16"}}, String{u8"字号 16"}},
                 {MessageKey{u8"gallery.complete.copy.315", String{u8"Gap 8"}}, String{u8"间距 8"}},
                 {MessageKey{u8"gallery.complete.copy.316", String{u8"Gap 16"}}, String{u8"间距 16"}},
                 {MessageKey{u8"gallery.complete.copy.317", String{u8"Gap 24"}}, String{u8"间距 24"}},
                 {MessageKey{u8"gallery.complete.copy.318", String{u8"Control 24"}}, String{u8"控件高度 24"}},
                 {MessageKey{u8"gallery.complete.copy.319", String{u8"Control 32"}}, String{u8"控件高度 32"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.320", String{u8"Control 40"}}, String{u8"控件高度 40"}},
                 {MessageKey{u8"gallery.complete.copy.321", String{u8"Radius 4"}}, String{u8"圆角 4"}},
                 {MessageKey{u8"gallery.complete.copy.322", String{u8"Radius 6"}}, String{u8"圆角 6"}},
                 {MessageKey{u8"gallery.complete.copy.323", String{u8"Radius 8"}}, String{u8"圆角 8"}},
                 {MessageKey{u8"gallery.complete.copy.324", String{u8"Elevation 1"}}, String{u8"层级 1"}},
                 {MessageKey{u8"gallery.complete.copy.325", String{u8"Elevation 2"}}, String{u8"层级 2"}},
                 {MessageKey{u8"gallery.complete.copy.326", String{u8"Elevation 3"}}, String{u8"层级 3"}},
                 {MessageKey{u8"gallery.complete.copy.327", String{u8"Default button"}}, String{u8"默认按钮"}},
                 {MessageKey{u8"gallery.complete.copy.328", String{u8"Primary button"}}, String{u8"主色按钮"}},
                 {MessageKey{u8"gallery.complete.copy.329", String{u8"Danger button"}}, String{u8"危险按钮"}},
                 {MessageKey{u8"gallery.complete.copy.330", String{u8"Left drawer"}}, String{u8"左侧抽屉"}},
                 {MessageKey{u8"gallery.complete.copy.331", String{u8"Right drawer"}}, String{u8"右侧抽屉"}},
                 {MessageKey{u8"gallery.complete.copy.332", String{u8"Top drawer"}}, String{u8"顶部抽屉"}},
                 {MessageKey{u8"gallery.complete.copy.333", String{u8"Bottom drawer"}}, String{u8"底部抽屉"}},
                 {MessageKey{u8"gallery.complete.copy.334", String{u8"Popover arrow"}}, String{u8"气泡箭头"}},
                 {MessageKey{u8"gallery.complete.copy.335", String{u8"Popover"}}, String{u8"气泡卡片"}},
                 {MessageKey{u8"gallery.complete.copy.336", String{u8"Card"}}, String{u8"卡片"}},
                 {MessageKey{u8"gallery.complete.copy.337", String{u8"Tabs left inset"}}, String{u8"标签页左侧内阴影"}},
                 {MessageKey{u8"gallery.complete.copy.338", String{u8"Tabs right inset"}},
                  String{u8"标签页右侧内阴影"}},
                 {MessageKey{u8"gallery.complete.copy.339", String{u8"Tabs top inset"}}, String{u8"标签页顶部内阴影"}},
                 {MessageKey{u8"gallery.complete.copy.340", String{u8"Tabs bottom inset"}},
                  String{u8"标签页底部内阴影"}},
                 {MessageKey{u8"gallery.complete.copy.341", String{u8"Default"}}, String{u8"默认"}},
                 {MessageKey{u8"gallery.complete.copy.342", String{u8"Dark"}}, String{u8"深色"}},
                 {MessageKey{u8"gallery.complete.copy.343", String{u8"Compact"}}, String{u8"紧凑"}},
                 {MessageKey{u8"gallery.complete.copy.344", String{u8"Dark + purple seed"}},
                  String{u8"深色 + 紫色种子"}},
                 {MessageKey{u8"gallery.complete.copy.345", String{u8"Default"}}, String{u8"默认"}},
                 {MessageKey{u8"gallery.complete.copy.346", String{u8"Primary"}}, String{u8"主色"}},
                 {MessageKey{u8"gallery.complete.copy.347", String{u8"Danger"}}, String{u8"危险"}},
                 {MessageKey{u8"gallery.complete.copy.348", String{u8"Hover me"}}, String{u8"悬停"}},
                 {MessageKey{u8"gallery.complete.copy.349", String{u8"Press me"}}, String{u8"按下"}},
                 {MessageKey{u8"gallery.complete.copy.350", String{u8"Tab focus"}}, String{u8"键盘焦点"}},
                 {MessageKey{u8"gallery.complete.copy.351", String{u8"Disabled"}}, String{u8"禁用"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.352", String{u8"Loading"}}, String{u8"加载"}},
                 {MessageKey{u8"gallery.complete.copy.353", String{u8"Desktop"}}, String{u8"桌面"}},
                 {MessageKey{u8"gallery.complete.copy.354", String{u8"Service"}}, String{u8"服务"}},
                 {MessageKey{u8"gallery.complete.copy.355", String{u8"Disabled option"}}, String{u8"禁用选项"}},
                 {MessageKey{u8"gallery.complete.copy.356", String{u8"Platform"}}, String{u8"平台"}},
                 {MessageKey{u8"gallery.complete.copy.357", String{u8"Priority"}}, String{u8"优先级"}},
                 {MessageKey{u8"gallery.complete.copy.358", String{u8"Desktop"}}, String{u8"桌面"}},
                 {MessageKey{u8"gallery.complete.copy.359", String{u8"Service"}}, String{u8"服务"}},
                 {MessageKey{u8"gallery.complete.copy.360", String{u8"High"}}, String{u8"高"}},
                 {MessageKey{u8"gallery.complete.copy.361", String{u8"Low"}}, String{u8"低"}},
                 {MessageKey{u8"gallery.complete.copy.362", String{u8"Inactive"}}, String{u8"停用"}},
                 {MessageKey{u8"gallery.complete.copy.363",
                             String{u8"Synthetic option {index}"},
                             {{u8"index", MessageArgType::UInt}}},
                  String{u8"合成选项 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.364", String{u8"Native components"}}, String{u8"原生组件"}},
                 {MessageKey{u8"gallery.complete.copy.365", String{u8"Unicode editing"}}, String{u8"Unicode 编辑"}},
                 {MessageKey{u8"gallery.complete.copy.366", String{u8"Disabled contact"}}, String{u8"禁用联系人"}},
                 {MessageKey{u8"gallery.complete.copy.367", String{u8"Inactive contact"}}, String{u8"停用联系人"}},
                 {MessageKey{u8"gallery.complete.copy.368", String{u8"Contact with notes"}}, String{u8"备注联系人"}},
                 {MessageKey{u8"gallery.complete.copy.369", String{u8"Synthetic suggestions pending"}},
                  String{u8"合成建议待交付"}},
                 {MessageKey{u8"gallery.complete.copy.370", String{u8"Available projects"}}, String{u8"可用项目"}},
                 {MessageKey{u8"gallery.complete.copy.371", String{u8"Selected projects"}}, String{u8"已选项目"}},
                 {MessageKey{
                      u8"gallery.complete.copy.372", String{u8"Item {index}"}, {{u8"index", MessageArgType::UInt}}},
                  String{u8"项目 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.373", String{u8"Retained input draft"}},
                  String{u8"保留输入草稿"}},
                 {MessageKey{u8"gallery.complete.copy.374", String{u8"Plain item"}}, String{u8"普通项目"}},
                 {MessageKey{u8"gallery.complete.copy.375", String{u8"Locked"}}, String{u8"已锁定"}},
                 {MessageKey{u8"gallery.complete.copy.376", String{u8"Editable"}}, String{u8"可编辑"}},
                 {MessageKey{u8"gallery.complete.copy.377", String{u8"Release date"}}, String{u8"发布日"}},
                 {MessageKey{u8"gallery.complete.copy.378", String{u8"Workday preset"}}, String{u8"工作日预设"}},
                 {MessageKey{u8"gallery.complete.copy.379", String{u8"Date/time preset"}}, String{u8"日期时间预设"}},
                 {MessageKey{u8"gallery.complete.copy.380", String{u8"Noon"}}, String{u8"正午"}},
                 {MessageKey{u8"gallery.complete.copy.381", String{u8"Switch paragraph direction"}},
                  String{u8"切换段落方向"}},
                 {MessageKey{u8"gallery.complete.copy.382", String{u8"Locked"}}, String{u8"已锁定"}},
                 {MessageKey{u8"gallery.complete.copy.383", String{u8"Editable"}}, String{u8"可编辑"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.384", String{u8"Locked"}}, String{u8"已锁定"}},
                 {MessageKey{u8"gallery.complete.copy.385", String{u8"Editable"}}, String{u8"可编辑"}},
                 {MessageKey{u8"gallery.complete.copy.386", String{u8"Four items per page"}}, String{u8"每页四项"}},
                 {MessageKey{u8"gallery.complete.copy.387", String{u8"Short card"}}, String{u8"短卡片"}},
                 {MessageKey{u8"gallery.complete.copy.388",
                             String{u8"Taller card\nActual content height\nRetains each editor state"}},
                  String{u8"较高卡片\n真实内容高度\n保持每项的编辑状态"}},
                 {MessageKey{u8"gallery.complete.copy.389", String{u8"Callback value: 1"}},
                  String{u8"回调结果：数值 1"}},
                 {MessageKey{u8"gallery.complete.copy.390", String{u8"Callback value: 2"}},
                  String{u8"回调结果：数值 2"}},
                 {MessageKey{u8"gallery.complete.copy.391", String{u8"Status color: green"}}, String{u8"状态色：绿色"}},
                 {MessageKey{u8"gallery.complete.copy.392", String{u8"Status color: red"}}, String{u8"状态色：红色"}},
                 {MessageKey{u8"gallery.complete.copy.393", String{u8"Getting started"}}, String{u8"上手指南"}},
                 {MessageKey{u8"gallery.complete.copy.394", String{u8"User guide"}}, String{u8"使用指南"}},
                 {MessageKey{
                      u8"gallery.complete.copy.395", String{u8"Asset {index}"}, {{u8"index", MessageArgType::UInt}}},
                  String{u8"资源 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.396",
                             String{u8"Deliverable {index}"},
                             {{u8"index", MessageArgType::UInt}}},
                  String{u8"交付件 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.397",
                             String{u8"Paragraph {index} · Fixed title/footer; retained body scrolls independently."},
                             {{u8"index", MessageArgType::UInt}}},
                  String{u8"第 {index} 段 · 标题与页脚固定，正文保留并独立滚动。"}},
                 {MessageKey{u8"gallery.complete.copy.398",
                             String{u8"Drawer paragraph {index} · Fixed title/footer; body scrolls independently."},
                             {{u8"index", MessageArgType::UInt}}},
                  String{u8"抽屉正文 {index} · 标题和页脚固定，正文独立滚动。"}},
                 {MessageKey{u8"gallery.complete.copy.399",
                             String{u8"Project check {index}"},
                             {{u8"index", MessageArgType::UInt}}},
                  String{u8"项目检查 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.400", String{u8"No date selected"}}, String{u8"未选择日期"}},
                 {MessageKey{u8"gallery.complete.copy.401", String{u8"Cancelled by footer button"}},
                  String{u8"取消来源：页脚按钮"}},
                 {MessageKey{u8"gallery.complete.copy.402", String{u8"Cancelled by close button"}},
                  String{u8"取消来源：右上角关闭"}},
                 {MessageKey{u8"gallery.complete.copy.403", String{u8"Cancelled by mask"}}, String{u8"取消来源：遮罩"}},
                 {MessageKey{u8"gallery.complete.copy.404", String{u8"Cancelled by Escape"}},
                  String{u8"取消来源：Escape"}},
                 {MessageKey{u8"gallery.complete.copy.405",
                             String{u8"Submitted value: {value}"},
                             {{u8"value", MessageArgType::Text}}},
                  String{u8"已提交数值：{value}"}},
                 {MessageKey{u8"gallery.complete.copy.406", String{u8"Submitted empty value"}}, String{u8"已提交空值"}},
                 {MessageKey{u8"gallery.complete.copy.407", String{u8"Left"}}, String{u8"左侧"}},
                 {MessageKey{u8"gallery.complete.copy.408", String{u8"Right"}}, String{u8"右侧"}},
                 {MessageKey{u8"gallery.complete.copy.409", String{u8"Plain"}}, String{u8"普通"}},
                 {MessageKey{u8"gallery.complete.copy.410", String{u8"Component Token"}}, String{u8"组件 Token"}},
                 {MessageKey{u8"gallery.complete.copy.411", String{u8"Keyed draft"}}, String{u8"键控草稿"}},
                 {MessageKey{u8"gallery.complete.copy.412", String{u8"Message / Notification"}},
                  String{u8"提示 / 通知"}},
                 {MessageKey{u8"gallery.complete.copy.413", String{u8"Notification center"}}, String{u8"通知中心"}},
                 {MessageKey{u8"gallery.complete.copy.414", String{u8"Affix"}}, String{u8"固定"}},
                 {MessageKey{u8"gallery.complete.copy.415", String{u8"RynUI · Ant Design 6"}},
                  String{u8"RynUI · Ant Design 6"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.416",
                             String{u8"Image source failed: {error}"},
                             {{u8"error", MessageArgType::Text}}},
                  String{u8"图像来源失败：{error}"}},
                 {MessageKey{u8"gallery.complete.copy.417",
                             String{u8"QR code error: {error}"},
                             {{u8"error", MessageArgType::Text}}},
                  String{u8"二维码错误：{error}"}},
                 {MessageKey{u8"gallery.complete.copy.418",
                             String{u8"Export failed: {error}"},
                             {{u8"error", MessageArgType::Text}}},
                  String{u8"导出失败：{error}"}},
                 {MessageKey{u8"gallery.complete.copy.419",
                             String{u8"Selection failed: {error}"},
                             {{u8"error", MessageArgType::Text}}},
                  String{u8"选择失败：{error}"}},
                 {MessageKey{u8"gallery.complete.copy.420",
                             String{u8"Custom panel: {date} mode={value}"},
                             {{u8"date", MessageArgType::Text}, {u8"value", MessageArgType::Int}}},
                  String{u8"自定义面板：{date} mode={value}"}},
                 {MessageKey{u8"gallery.complete.copy.421",
                             String{u8"Custom selection: {date} source={value}"},
                             {{u8"date", MessageArgType::Text}, {u8"value", MessageArgType::Int}}},
                  String{u8"自定义选择：{date} source={value}"}},
                 {MessageKey{u8"gallery.complete.copy.422", String{u8"Draft"}}, String{u8"草稿"}},
                 {MessageKey{u8"gallery.complete.copy.423", String{u8"Option one"}}, String{u8"选项一"}},
                 {MessageKey{u8"gallery.complete.copy.424", String{u8"Option two"}}, String{u8"选项二"}},
                 {MessageKey{u8"gallery.complete.copy.425", String{u8"Low"}}, String{u8"低"}},
                 {MessageKey{u8"gallery.complete.copy.426", String{u8"Medium"}}, String{u8"中"}},
                 {MessageKey{u8"gallery.complete.copy.427", String{u8"High"}}, String{u8"高"}},
                 {MessageKey{u8"gallery.complete.copy.428", String{u8"First row A"}}, String{u8"第一行 A"}},
                 {MessageKey{u8"gallery.complete.copy.429", String{u8"Second row B"}}, String{u8"第二行 B"}},
                 {MessageKey{u8"gallery.complete.copy.430", String{u8"Third row C"}}, String{u8"第三行 C"}},
                 {MessageKey{u8"gallery.complete.copy.431", String{u8"Column A"}}, String{u8"列 A"}},
                 {MessageKey{u8"gallery.complete.copy.432", String{u8"Column B"}}, String{u8"列 B"}},
                 {MessageKey{u8"gallery.complete.copy.433", String{u8"Column C"}}, String{u8"列 C"}},
                 {MessageKey{u8"gallery.complete.copy.434", String{u8"First item"}}, String{u8"第一项"}},
                 {MessageKey{u8"gallery.complete.copy.435", String{u8"Second item"}}, String{u8"第二项"}},
                 {MessageKey{u8"gallery.complete.copy.436", String{u8"Third item"}}, String{u8"第三项"}},
                 {MessageKey{u8"gallery.complete.copy.437", String{u8"Month view"}}, String{u8"月视图"}},
                 {MessageKey{u8"gallery.complete.copy.438", String{u8"Day view"}}, String{u8"日视图"}},
                 {MessageKey{u8"gallery.complete.copy.439", String{u8"Details"}}, String{u8"详情"}},
                 {MessageKey{u8"gallery.complete.copy.440", String{u8"Process"}}, String{u8"处理"}},
                 {MessageKey{u8"gallery.complete.copy.441", String{u8"Review"}}, String{u8"复核"}},
                 {MessageKey{u8"gallery.complete.copy.442", String{u8"Complete"}}, String{u8"完成"}},
                 {MessageKey{u8"gallery.complete.copy.443", String{u8"Implementation details"}}, String{u8"实现细节"}},
                 {MessageKey{u8"gallery.complete.copy.444", String{u8"Examples"}}, String{u8"示例"}},
                 {MessageKey{u8"gallery.complete.copy.445", String{u8"Processing notes"}}, String{u8"处理备注"}},
                 {MessageKey{u8"gallery.complete.copy.446",
                             String{u8"Actual zero-based columns: {columns}"},
                             {{u8"columns", MessageArgType::Text}}},
                  String{u8"实际零基列：{columns}"}},
                 {MessageKey{
                      u8"gallery.complete.copy.447", String{u8"Tab {index}"}, {{u8"index", MessageArgType::UInt}}},
                  String{u8"标签页 {index}"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.448", String{u8"Left / RTL / custom indicator"}},
                  String{u8"左侧 / RTL / 定制 indicator"}},
                 {MessageKey{u8"gallery.complete.copy.449", String{u8"Right retained panel"}},
                  String{u8"右侧保留面板"}},
                 {MessageKey{u8"gallery.complete.copy.450", String{u8"Common colors"}}, String{u8"常用颜色"}},
                 {MessageKey{u8"gallery.complete.copy.451", String{u8"China"}}, String{u8"中国"}},
                 {MessageKey{u8"gallery.complete.copy.452", String{u8"Hangzhou"}}, String{u8"杭州"}},
                 {MessageKey{u8"gallery.complete.copy.453", String{u8"Nanjing"}}, String{u8"南京"}},
                 {MessageKey{u8"gallery.complete.copy.454", String{u8"Europe"}}, String{u8"欧洲"}},
                 {MessageKey{u8"gallery.complete.copy.455", String{u8"Paris"}}, String{u8"巴黎"}},
                 {MessageKey{u8"gallery.complete.copy.456", String{u8"Disabled"}}, String{u8"禁用"}},
                 {MessageKey{u8"gallery.complete.copy.457",
                             String{u8"{count} more items"},
                             {{u8"count", MessageArgType::UInt}}},
                  String{u8"还有 {count} 项"}},
                 {MessageKey{u8"gallery.complete.copy.458",
                             String{u8"Straße / Group {index}"},
                             {{u8"index", MessageArgType::UInt}}},
                  String{u8"Straße / 分组 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.459", String{u8" seconds"}}, String{u8" 秒"}},
                 {MessageKey{u8"gallery.complete.copy.460",
                             String{u8"Raw value: {value}"},
                             {{u8"value", MessageArgType::Text}}},
                  String{u8"原值：{value}"}},
                 {MessageKey{u8"gallery.complete.copy.461", String{u8"Today"}}, String{u8"今天"}},
                 {MessageKey{u8"gallery.complete.copy.462", String{u8"Retained draft"}}, String{u8"保留草稿"}},
                 {MessageKey{u8"gallery.complete.copy.463", String{u8"Needs improvement"}}, String{u8"需要改进"}},
                 {MessageKey{u8"gallery.complete.copy.464", String{u8"Fair"}}, String{u8"一般"}},
                 {MessageKey{u8"gallery.complete.copy.465", String{u8"Satisfied"}}, String{u8"满意"}},
                 {MessageKey{u8"gallery.complete.copy.466", String{u8"Very good"}}, String{u8"很好"}},
                 {MessageKey{u8"gallery.complete.copy.467",
                             String{u8"Synthetic city {index}"},
                             {{u8"index", MessageArgType::UInt}}},
                  String{u8"合成城市 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.468", String{u8"Synthetic cities"}}, String{u8"合成城市"}},
                 {MessageKey{u8"gallery.complete.copy.469", String{u8"Contacts"}}, String{u8"联系人"}},
                 {MessageKey{u8"gallery.complete.copy.470", String{u8"RTL DirectoryTree"}}, String{u8"RTL 目录树"}},
                 {MessageKey{u8"gallery.complete.copy.471", String{u8"File"}}, String{u8"文件"}},
                 {MessageKey{u8"gallery.complete.copy.472", String{u8"Load A"}}, String{u8"加载 A"}},
                 {MessageKey{u8"gallery.complete.copy.473", String{u8"Load B"}}, String{u8"加载 B"}},
                 {MessageKey{u8"gallery.complete.copy.474", String{u8"Targets"}}, String{u8"目标"}},
                 {MessageKey{u8"gallery.complete.copy.475", String{u8"Alpha target"}}, String{u8"目标 Alpha"}},
                 {MessageKey{u8"gallery.complete.copy.476", String{u8"Beta target"}}, String{u8"目标 Beta"}},
                 {MessageKey{u8"gallery.complete.copy.477", String{u8"Gamma target"}}, String{u8"目标 Gamma"}},
                 {MessageKey{u8"gallery.complete.copy.478", String{u8"Strict RTL"}}, String{u8"严格选择 RTL"}},
                 {MessageKey{u8"gallery.complete.copy.479", String{u8"Alpha"}}, String{u8"Alpha"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.480", String{u8"Beta"}}, String{u8"Beta"}},
                 {MessageKey{u8"gallery.complete.copy.481", String{u8"Expand synthetic target"}},
                  String{u8"展开合成目标"}},
                 {MessageKey{u8"gallery.complete.copy.482", String{u8"Dynamic section"}}, String{u8"动态章节"}},
                 {MessageKey{u8"gallery.complete.copy.483", String{u8"Keyed draft"}}, String{u8"键控草稿"}},
                 {MessageKey{u8"gallery.complete.copy.484", String{u8"Dynamic section"}}, String{u8"动态章节"}},
                 {MessageKey{u8"gallery.complete.copy.485", String{u8"Component Token"}}, String{u8"组件 Token"}},
                 {MessageKey{u8"gallery.complete.copy.486", String{u8"Keyed draft"}}, String{u8"键控草稿"}},
                 {MessageKey{u8"gallery.complete.copy.487", String{u8"Message / Notification"}},
                  String{u8"提示 / 通知"}},
                 {MessageKey{u8"gallery.complete.copy.488", String{u8"Notification center"}}, String{u8"通知中心"}},
                 {MessageKey{u8"gallery.complete.copy.489", String{u8"Synthetic child"}}, String{u8"合成子节点"}},
                 {MessageKey{u8"gallery.complete.copy.490", String{u8"Synthetic target"}}, String{u8"合成目标"}},
                 {MessageKey{u8"gallery.complete.copy.491",
                             String{u8"Virtual node {index}"},
                             {{u8"index", MessageArgType::UInt}}},
                  String{u8"虚拟节点 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.492",
                             String{u8"Virtual choice {index}"},
                             {{u8"index", MessageArgType::UInt}}},
                  String{u8"虚拟选项 {index}"}},
                 {MessageKey{
                      u8"gallery.complete.copy.493", String{u8"Column {index}"}, {{u8"index", MessageArgType::UInt}}},
                  String{u8"列 {index}"}},
                 {MessageKey{
                      u8"gallery.complete.copy.494", String{u8"Card {index}"}, {{u8"index", MessageArgType::UInt}}},
                  String{u8"卡片 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.495",
                             String{u8"Native scroll content / row {index}"},
                             {{u8"index", MessageArgType::UInt}}},
                  String{u8"原生滚动内容 / 行 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.496",
                             String{u8"Formatted value: {value}"},
                             {{u8"value", MessageArgType::Text}}},
                  String{u8"格式化值：{value}"}},
                 {MessageKey{u8"gallery.complete.copy.497", String{u8"Formatted value: empty"}},
                  String{u8"格式化值：空"}},
                 {MessageKey{u8"gallery.complete.copy.498", String{u8"Overview"}}, String{u8"概览"}},
                 {MessageKey{u8"gallery.complete.copy.499", String{u8"Implementation details"}}, String{u8"实现细节"}},
                 {MessageKey{u8"gallery.complete.copy.500", String{u8"Public API"}}, String{u8"公开 API"}},
                 {MessageKey{u8"gallery.complete.copy.501", String{u8"Examples"}}, String{u8"示例"}},
                 {MessageKey{u8"gallery.complete.copy.502", String{u8"Footer boundary"}}, String{u8"尾部边界"}},
                 {MessageKey{u8"gallery.complete.copy.503", String{u8"Three by two color/alpha fixture"}},
                  String{u8"三乘二颜色/透明像素样本"}},
                 {MessageKey{u8"gallery.complete.copy.504", String{u8"Fallback pixels"}}, String{u8"回退像素"}},
                 {MessageKey{u8"gallery.complete.copy.505", String{u8"Previous"}}, String{u8"上一张"}},
                 {MessageKey{u8"gallery.complete.copy.506", String{u8"Next"}}, String{u8"下一张"}},
                 {MessageKey{u8"gallery.complete.copy.507", String{u8"Zoom in"}}, String{u8"放大"}},
                 {MessageKey{u8"gallery.complete.copy.508", String{u8"Zoom out"}}, String{u8"缩小"}},
                 {MessageKey{u8"gallery.complete.copy.509", String{u8"Rotate"}}, String{u8"旋转"}},
                 {MessageKey{u8"gallery.complete.copy.510", String{u8"Flip"}}, String{u8"翻转"}},
                 {MessageKey{u8"gallery.complete.copy.511", String{u8"Reset"}}, String{u8"重置"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.complete.copy.512", String{u8"Close"}}, String{u8"关闭"}},
                 {MessageKey{u8"gallery.shell.reference-version", String{u8"Ant Design 6.6.5"}},
                  String{u8"Ant Design 6.6.5"}},
                 {MessageKey{u8"gallery.complete.copy.514", String{u8"B · retained"}}, String{u8"B · 保留"}},
                 {MessageKey{u8"gallery.complete.copy.515", String{u8"Added number"}}, String{u8"新增数字"}},
                 {MessageKey{u8"gallery.complete.copy.516", String{u8"B · retained"}}, String{u8"B · 保留"}},
                 {MessageKey{u8"gallery.complete.copy.517", String{u8"Added number"}}, String{u8"新增数字"}},
                 {MessageKey{u8"gallery.complete.copy.518", String{u8"Alpha fixture"}}, String{u8"透明度样本"}},
                 {MessageKey{u8"gallery.complete.copy.519", String{u8"Second fixture"}}, String{u8"第二幅样本"}},
                 {MessageKey{u8"gallery.complete.copy.520", String{u8"Load local application data"}},
                  String{u8"加载本地应用数据"}},
                 {MessageKey{
                      u8"gallery.complete.copy.521", String{u8"New tab {index}"}, {{u8"index", MessageArgType::UInt}}},
                  String{u8"新页签 {index}"}},
                 {MessageKey{u8"gallery.complete.copy.522", String{u8"Home"}}, String{u8"首页"}},
                 {MessageKey{u8"gallery.complete.copy.523", String{u8"Draft"}}, String{u8"草稿"}},
                 {MessageKey{u8"gallery.complete.copy.524", String{u8"Current"}}, String{u8"当前页"}},
                 {MessageKey{u8"gallery.complete.copy.525", String{u8"Overview"}}, String{u8"概览"}},
                 {MessageKey{u8"gallery.complete.copy.526", String{u8"Settings"}}, String{u8"设置"}},
                 {MessageKey{u8"gallery.complete.copy.527", String{u8"Gamma"}}, String{u8"伽马"}},
                 {MessageKey{u8"gallery.complete.copy.528", String{u8"Design Tokens"}}, String{u8"设计令牌"}},
                 {MessageKey{u8"gallery.copy.sample.000", String{u8"A · Initially 25%; range 15%–40%"}},
                  String{u8"A · 初始 25%，范围 15%–40%"}},
                 {MessageKey{u8"gallery.copy.sample.001",
                             String{u8"C · Remaining space; moving the left divider keeps this panel unchanged"}},
                  String{u8"C · 剩余空间；拖动左侧分隔条不改变此面板"}},
                 {MessageKey{u8"gallery.copy.sample.002", String{u8"Enable/disable resizing"}},
                  String{u8"启用/禁用调整"}},
                 {MessageKey{u8"gallery.copy.sample.003", String{u8"Reset to 35% / 65%"}}, String{u8"重置 35% / 65%"}},
                 {MessageKey{u8"gallery.copy.sample.004",
                             String{u8"Controlled percentages; double-click the divider to reset"}},
                  String{u8"受控百分比；双击分隔条重置"}},
                 {MessageKey{u8"gallery.copy.sample.005", String{u8"Keep/destroy hidden content"}},
                  String{u8"保留/销毁隐藏内容"}},
                 {MessageKey{u8"gallery.copy.sample.006", String{u8"Collapse animation"}}, String{u8"折叠动画"}},
                 {MessageKey{u8"gallery.copy.sample.007", String{u8"Horizontal/vertical"}}, String{u8"水平/垂直"}},
                 {MessageKey{u8"gallery.copy.sample.008",
                             String{u8"Lazy dragging moves the preview line and commits on "
                                    u8"release; this panel always retains its content."}},
                  String{u8"lazy 拖动只移动预览线，释放后提交；此面板覆盖为始终保留内容。"}},
                 {MessageKey{u8"gallery.copy.sample.009", String{u8"Left workspace"}}, String{u8"左侧工作区"}},
                 {MessageKey{u8"gallery.copy.sample.010", String{u8"Nested upper panel"}}, String{u8"嵌套上方面板"}},
                 {MessageKey{u8"gallery.copy.sample.011", String{u8"Project workspace"}}, String{u8"项目导航"}},
                 {MessageKey{u8"gallery.copy.sample.012", String{u8"Overview"}}, String{u8"概览"}},
                 {MessageKey{u8"gallery.copy.sample.013", String{u8"Settings"}}, String{u8"设置"}},
                 {MessageKey{
                      u8"gallery.copy.sample.014",
                      String{u8"Nested Layout finds its immediate Sider and assigns the remaining space to content."}},
                  String{u8"嵌套 Layout 自动检测直属 Sider，正文取得剩余空间。"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.015", String{u8"Right tools"}}, String{u8"右侧工具"}},
                 {MessageKey{u8"gallery.copy.sample.016",
                             String{u8"Footer · Theme controls the background, padding and typography"}},
                  String{u8"Footer · Theme 控制背景、内边距和字号"}},
                 {MessageKey{u8"gallery.copy.sample.017", String{u8"Responsive collapse"}}, String{u8"响应式侧栏"}},
                 {MessageKey{u8"gallery.copy.sample.018", String{u8"Collapse/expand externally"}},
                  String{u8"外部折叠/展开"}},
                 {MessageKey{u8"gallery.copy.sample.019", String{u8"Toggle"}}, String{u8"切换"}},
                 {MessageKey{u8"gallery.copy.sample.020", String{u8"Controlled + custom trigger"}},
                  String{u8"受控 + 自定义触发器"}},
                 {MessageKey{u8"gallery.copy.sample.021",
                             String{u8"The caller updates controlled state; external buttons can "
                                    u8"also manage a sidebar with a hidden trigger."}},
                  String{u8"受控状态由调用方更新；外部按钮同样可以管理隐藏触发器的侧栏。"}},
                 {MessageKey{u8"gallery.copy.sample.022", String{u8"No trigger"}}, String{u8"无触发器"}},
                 {MessageKey{u8"gallery.copy.sample.023", String{u8"Input · Native editing features"}},
                  String{u8"Input / 输入家族 · 原生桌面功能"}},
                 {MessageKey{
                      u8"gallery.copy.sample.024",
                      String{u8"Text variants, counts/limits, ref/system hints, clearing, Unicode editing and IME"}},
                  String{u8"支持：四变体、统计/上限、ref/系统提示、清空、Unicode 编辑与 IME"}},
                 {MessageKey{u8"gallery.copy.sample.025",
                             String{u8"Input / Password / Search / TextArea / OTP: bidirectional "
                                    u8"layout, visual navigation and logical editing"}},
                  String{u8"Input / Password / Search / TextArea / OTP：双向排版、视觉导航与逻辑编辑"}},
                 {MessageKey{u8"gallery.copy.sample.026", String{u8"Controlled input"}}, String{u8"受控输入"}},
                 {MessageKey{u8"gallery.copy.sample.027", String{u8"Prefix"}}, String{u8"前"}},
                 {MessageKey{u8"gallery.copy.sample.028", String{u8"Uncontrolled input"}}, String{u8"非受控输入"}},
                 {MessageKey{u8"gallery.copy.sample.029",
                             String{u8"Search · Composed from native Input, Button and Compact"}},
                  String{u8"Search / 搜索 · 原生单行功能：复用 Input、Button 与 Compact"}},
                 {MessageKey{
                      u8"gallery.copy.sample.030",
                      String{u8"Text variants, full-width and Enter submission, custom icons and action source"}},
                  String{u8"支持：四变体、全部共用属性、自定义图标、清空来源与连接外观"}},
                 {MessageKey{u8"gallery.copy.sample.031", String{u8"Search"}}, String{u8"查询"}},
                 {MessageKey{u8"gallery.copy.sample.032",
                             String{u8"Password · Shares Input editing and the window input session"}},
                  String{u8"Password / 密码 · 复用 Input 编辑与窗口输入会话"}},
                 {MessageKey{u8"gallery.copy.sample.033", String{u8"Switch · Middle, Small, disabled and loading"}},
                  String{u8"Switch / 开关 · Middle、Small、disabled、loading"}},
                 {MessageKey{u8"gallery.copy.sample.034", String{u8"Controlled"}}, String{u8"受控"}},
                 {MessageKey{u8"gallery.copy.sample.035", String{u8"Small / Checked"}}, String{u8"Small / 已选"}},
                 {MessageKey{u8"gallery.copy.sample.036", String{u8"Disabled"}}, String{u8"禁用"}},
                 {MessageKey{u8"gallery.copy.sample.037", String{u8"Loading"}}, String{u8"加载"}},
                 {MessageKey{u8"gallery.copy.sample.038", String{u8"Checkbox · Checked, indeterminate and disabled"}},
                  String{u8"Checkbox / 多选框 · checked、indeterminate、disabled"}},
                 {MessageKey{u8"gallery.copy.sample.039", String{u8"Controlled"}}, String{u8"受控 / Control"}},
                 {MessageKey{u8"gallery.copy.sample.040", String{u8"Checked"}}, String{u8"已选 / Checked"}},
                 {MessageKey{u8"gallery.copy.sample.041", String{u8"Mixed"}}, String{u8"半选 / Mixed"}},
                 {MessageKey{u8"gallery.copy.sample.042", String{u8"Disabled"}}, String{u8"禁用 / Disabled"}},
                 {MessageKey{u8"gallery.copy.sample.043",
                             String{u8"Radio · Standalone, mutually exclusive group and disabled"}},
                  String{u8"Radio / 单选框 · 单独使用、互斥分组、disabled"}},
                 {MessageKey{u8"gallery.copy.sample.044", String{u8"Standalone radio"}},
                  String{u8"单独已选 / Standalone"}},
                 {MessageKey{u8"gallery.copy.sample.045",
                             String{u8"Typography · Ellipsis, decorations and editing in place"}},
                  String{u8"Typography · 省略、复制与原地编辑"}},
                 {MessageKey{u8"gallery.copy.sample.046",
                             String{u8"Divider · Horizontal, dashed, plain, None spacing and vertical"}},
                  String{u8"Divider · 水平、虚线、plain、None 朝向间距与垂直线"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.047", String{u8"Horizontal"}}, String{u8"文字"}},
                 {MessageKey{u8"gallery.copy.sample.048", String{u8"Vertical"}}, String{u8"文字"}},
                 {MessageKey{u8"gallery.copy.sample.049",
                             String{u8"Tooltip · Hover, focus, click, context menu, uncontrolled and centered arrow"}},
                  String{u8"Tooltip · 悬停 / 焦点 / 点击 / 右键 / 富标题 / 居中箭头"}},
                 {MessageKey{u8"gallery.copy.sample.050", String{u8"RTL / mixed scripts / clipping"}},
                  String{u8"RTL / 组合触发 / 动画"}},
                 {MessageKey{u8"gallery.copy.sample.051", String{u8"Hint"}}, String{u8"提示"}},
                 {MessageKey{u8"gallery.copy.sample.052", String{u8"Disabled"}}, String{u8"禁用 / Disabled"}},
                 {MessageKey{u8"gallery.copy.sample.053", String{u8"Click / controlled"}}, String{u8"点击 / 富标题"}},
                 {MessageKey{u8"gallery.copy.sample.054", String{u8"Retained native title input"}},
                  String{u8"保留的原生内容 / Retained title"}},
                 {MessageKey{u8"gallery.copy.sample.055", String{u8"Context menu"}}, String{u8"右键 / ContextMenu"}},
                 {MessageKey{u8"gallery.copy.sample.056", String{u8"Mixed scripts / clipping"}},
                  String{u8"组合触发 / 居中"}},
                 {MessageKey{u8"gallery.copy.sample.057", String{u8"Slider · Marks, dots, discrete range, tooltips, "
                                                                 u8"vertical, reverse, dragging, editing and labels"}},
                  String{u8"Slider · marks / dots / 离散范围 / 值提示 / 反向 / 纵向 / 整段拖动 / 编辑 / 逐端点禁用"}},
                 {MessageKey{u8"gallery.copy.sample.058", String{u8"Button · Semantic colors, variants, ghost, icons, "
                                                                 u8"shape, block, ref, loading delay and wave"}},
                  String{u8"Button · 原生颜色 / 六变体 / ghost / 图标 / shape / block / ref / loading delay / wave"}},
                 {MessageKey{u8"gallery.copy.sample.059", String{u8"End icon / Loading"}}, String{u8"End / Loading"}},
                 {MessageKey{u8"gallery.copy.sample.060", String{u8"Square"}}, String{u8"Square"}},
                 {MessageKey{u8"gallery.copy.sample.061", String{u8"Ghost dashed"}}, String{u8"Ghost dashed"}},
                 {MessageKey{u8"gallery.copy.sample.062", String{u8"Toggle loading and focus"}},
                  String{u8"切换加载并聚焦"}},
                 {MessageKey{u8"gallery.copy.sample.063", String{u8"Round block / resize"}},
                  String{u8"Round block / resize"}},
                 {MessageKey{u8"gallery.copy.sample.064", String{u8"Vertical"}}, String{u8"Vertical"}},
                 {MessageKey{u8"gallery.copy.sample.065",
                             String{u8"Switch · Retained content, icons, RTL, ref and component theme"}},
                  String{u8"Switch · retained 内容、图标、RTL、ref 与组件主题"}},
                 {MessageKey{u8"gallery.copy.sample.066", String{u8"Content and ref"}}, String{u8"Content and ref"}},
                 {MessageKey{u8"gallery.copy.sample.067", String{u8"On"}}, String{u8"开启"}},
                 {MessageKey{u8"gallery.copy.sample.068", String{u8"Off"}}, String{u8"关闭"}},
                 {MessageKey{u8"gallery.copy.sample.069", String{u8"Small icon / RTL"}}, String{u8"Small icon / RTL"}},
                 {MessageKey{u8"gallery.copy.sample.070", String{u8"Component theme"}}, String{u8"Component theme"}},
                 {MessageKey{u8"gallery.copy.sample.071", String{u8"Purple"}}, String{u8"紫色"}},
                 {MessageKey{u8"gallery.copy.sample.072", String{u8"Off"}}, String{u8"关"}},
                 {MessageKey{u8"gallery.copy.sample.073", String{u8"Focus switch"}}, String{u8"聚焦开关"}},
                 {MessageKey{u8"gallery.copy.sample.074",
                             String{u8"Checkbox · Group state, dynamic options, RTL, ref and component theme"}},
                  String{u8"Checkbox · 多选、动态保留选项、RTL、ref 与独立主题"}},
                 {MessageKey{u8"gallery.copy.sample.075", String{u8"Controlled group values"}}, String{u8"三类值"}},
                 {MessageKey{u8"gallery.copy.sample.076", String{u8"Dynamic group · Identity follows value"}},
                  String{u8"按 value 保留 identity"}},
                 {MessageKey{u8"gallery.copy.sample.077", String{u8"Reorder / add or remove options"}},
                  String{u8"重排 / 增删"}},
                 {MessageKey{u8"gallery.copy.sample.078", String{u8"Indeterminate / rich label"}},
                  String{u8"半选与富标签"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.079", String{u8"Purple / 20 dp"}}, String{u8"紫色 / 20 dp"}},
                 {MessageKey{u8"gallery.copy.sample.080", String{u8"Focus checkbox"}}, String{u8"聚焦多选框"}},
                 {MessageKey{u8"gallery.copy.sample.081", String{u8"Radio component theme / ref"}},
                  String{u8"Radio 组件主题与引用"}},
                 {MessageKey{u8"gallery.copy.sample.082", String{u8"Scoped purple / rich label / ref"}},
                  String{u8"独立紫色 / 富标签 / ref"}},
                 {MessageKey{u8"gallery.copy.sample.083",
                             String{u8"Radio · Dynamic options, group navigation, button edges, "
                                    u8"sizes, block, RTL, ref and component theme"}},
                  String{u8"Radio · 动态选项、键盘组导航、按钮连接边、三尺寸、block、RTL、ref 与独立主题"}},
                 {MessageKey{u8"gallery.copy.sample.084", String{u8"Dynamic outline · Identity follows value"}},
                  String{u8"按 value 保留 identity"}},
                 {MessageKey{u8"gallery.copy.sample.085", String{u8"Reorder / add or remove options"}},
                  String{u8"重排 / 增删"}},
                 {MessageKey{u8"gallery.copy.sample.086", String{u8"Solid large block / controlled"}},
                  String{u8"三类值 / 禁用"}},
                 {MessageKey{u8"gallery.copy.sample.087", String{u8"Vertical small / RTL"}},
                  String{u8"Vertical small / RTL"}},
                 {MessageKey{u8"gallery.copy.sample.088", String{u8"Focus radio button"}}, String{u8"聚焦单选按钮"}},
                 {MessageKey{u8"gallery.copy.sample.089",
                             String{u8"Flex · Actual baselines, compact sizes, RTL and default stretch"}},
                  String{u8"Flex · 真实文字基线、反向换行、RTL 与默认拉伸"}},
                 {MessageKey{u8"gallery.copy.sample.090", String{u8"Baseline · 14 / 28 dp and control labels"}},
                  String{u8"14 / 28 dp 与控件标签"}},
                 {MessageKey{u8"gallery.copy.sample.091", String{u8"Ag small"}}, String{u8"Ag 基线"}},
                 {MessageKey{u8"gallery.copy.sample.092", String{u8"Ag large"}}, String{u8"Ag 大字"}},
                 {MessageKey{u8"gallery.copy.sample.093", String{u8"Ag button"}}, String{u8"Ag 按钮"}},
                 {MessageKey{u8"gallery.copy.sample.094", String{u8"Wrap reverse · Stack from the bottom"}},
                  String{u8"从下边堆叠"}},
                 {MessageKey{u8"gallery.copy.sample.095", String{u8"Vertical RTL · From the right"}},
                  String{u8"纵向换列"}},
                 {MessageKey{u8"gallery.copy.sample.096", String{u8"Default stretch · Automatic height 64 dp"}},
                  String{u8"自动高度 64 dp"}},
                 {MessageKey{u8"gallery.copy.sample.097", String{u8"Default stretch"}}, String{u8"默认 Stretch"}},
                 {MessageKey{u8"gallery.copy.sample.098",
                             String{u8"Grid · 24 columns, window breakpoints and retained forms"}},
                  String{u8"Grid · 24 列、窗口断点与 retained 输入"}},
                 {MessageKey{u8"gallery.copy.sample.099",
                             String{u8"Responsive · xs=24 / sm=12 / md=8 / lg=6; breakpoints update form layout"}},
                  String{u8"xs=24 / sm=12 / md=8 / lg=6，窗口断点逐字段覆盖"}},
                 {MessageKey{u8"gallery.copy.sample.100",
                             String{u8"Offset / order / push-pull / gutters in both directions"}},
                  String{u8"offset / order / push-pull / 双轴 gutter"}},
                 {MessageKey{u8"gallery.copy.sample.101", String{u8"This column exceeds the row's 24 columns"}},
                  String{u8"换行后继续使用 24 列"}},
                 {MessageKey{u8"gallery.copy.sample.102",
                             String{u8"Flex · Fixed 120 dp + remaining space in a 1:2 ratio"}},
                  String{u8"固定 120 dp + 1:2 剩余空间"}},
                 {MessageKey{u8"gallery.copy.sample.103",
                             String{u8"RTL · A zero span hides the column and retains its content"}},
                  String{u8"方向与 span=0 隐藏保留内容"}},
                 {MessageKey{u8"gallery.copy.sample.104", String{u8"Toggle RTL"}}, String{u8"切换 RTL"}},
                 {MessageKey{u8"gallery.copy.sample.105", String{u8"Hide/restore column"}}, String{u8"隐藏 / 恢复列"}},
                 {MessageKey{u8"gallery.copy.sample.106",
                             String{u8"Masonry · Measured layout, window breakpoints and stable keyed content"}},
                  String{u8"Masonry · 真实测量、窗口断点与稳定 keyed 内容"}},
                 {MessageKey{u8"gallery.copy.sample.107",
                             String{u8"Measured · Place each card in the shortest column"}},
                  String{u8"按实际卡片高度进入最短列"}},
                 {MessageKey{u8"gallery.copy.sample.108",
                             String{u8"Dynamic · Size changes, additions, removals and reorder retain unchanged keys"}},
                  String{u8"内容尺寸变化、增删与重排只更新相关 key"}},
                 {MessageKey{u8"gallery.copy.sample.109",
                             String{u8"Toggle buttons to change the actual measured height"}},
                  String{u8"点击控制按钮改变实际测量高度"}},
                 {MessageKey{u8"gallery.copy.sample.110", String{u8"Change height"}}, String{u8"改变高度"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.111", String{u8"Add/remove"}}, String{u8"添加 / 移除"}},
                 {MessageKey{u8"gallery.copy.sample.112", String{u8"Reverse order"}}, String{u8"反转顺序"}},
                 {MessageKey{u8"gallery.copy.sample.113",
                             String{u8"Space · Separator content participates in layout, drawing and Tab order"}},
                  String{u8"富分隔随内容交错布局、绘制和 Tab"}},
                 {MessageKey{u8"gallery.copy.sample.114", String{u8"Separator action"}}, String{u8"分隔操作"}},
                 {MessageKey{u8"gallery.copy.sample.115",
                             String{u8"Baseline · Align mixed button sizes with input text"}},
                  String{u8"大尺寸按钮、输入与文字对齐"}},
                 {MessageKey{u8"gallery.copy.sample.116", String{u8"Ag text"}}, String{u8"Ag 基线"}},
                 {MessageKey{u8"gallery.copy.sample.117", String{u8"Ag button"}}, String{u8"Ag 大按钮"}},
                 {MessageKey{u8"gallery.copy.sample.118", String{u8"Search"}}, String{u8"虚线"}},
                 {MessageKey{u8"gallery.copy.sample.119", String{u8"Combine actions"}}, String{u8"连接操作"}},
                 {MessageKey{u8"gallery.copy.sample.120", String{u8"Vertical Compact · Block / vertical / RTL"}},
                  String{u8"block / 纵向 / RTL"}},
                 {MessageKey{u8"gallery.copy.sample.121", String{u8"Top"}}, String{u8"上方"}},
                 {MessageKey{u8"gallery.copy.sample.122", String{u8"Danger action below"}}, String{u8"下方危险操作"}},
                 {MessageKey{u8"gallery.copy.sample.123",
                             String{u8"Mixed Compact · Input / Password / Search / Radio / nested"}},
                  String{u8"Input / Password / Search / Radio / nested"}},
                 {MessageKey{u8"gallery.copy.sample.124", String{u8"Select"}}, String{u8"选择"}},
                 {MessageKey{u8"gallery.copy.sample.125", String{u8"Nested A"}}, String{u8"嵌套 A"}},
                 {MessageKey{u8"gallery.copy.sample.126", String{u8"Nested B"}}, String{u8"嵌套 B"}},
                 {MessageKey{u8"gallery.copy.sample.127",
                             String{u8"Addon variants · Text, error and warning, disabled"}},
                  String{u8"四变体 / 错误与警告 / 禁用"}},
                 {MessageKey{u8"gallery.copy.sample.128", String{u8"Warning"}}, String{u8"附加"}},
                 {MessageKey{u8"gallery.copy.sample.129", String{u8"Disabled"}}, String{u8"操作"}},
                 {MessageKey{u8"gallery.copy.sample.130",
                             String{u8"Icon · 848 built-in icons, clipping, two-tone, rotation and custom vectors"}},
                  String{u8"Icon：848 个离线图标 / 三类 / 双色 / 旋转 / 自定义向量"}},
                 {MessageKey{u8"gallery.copy.sample.131", String{u8"Start/stop spin"}}, String{u8"开始 / 停止 spin"}},
                 {MessageKey{u8"gallery.copy.sample.132", String{u8"Rotate 45°"}}, String{u8"旋转 45°"}},
                 {MessageKey{u8"gallery.copy.sample.133", String{u8"Switch built-in/custom"}},
                  String{u8"切换内置 / 自定义"}},
                 {MessageKey{u8"gallery.copy.sample.134",
                             String{u8"Tabs · Keyboard navigation, retained drafts and scoped theme"}},
                  String{u8"Tabs · 键盘导航 / 保留草稿 / 溢出滚动"}},
                 {MessageKey{u8"gallery.copy.sample.135", String{u8"Content"}}, String{u8"内容 / Content"}},
                 {MessageKey{u8"gallery.copy.sample.136", String{u8"Component theme · Scoped colors / 16 dp gap"}},
                  String{u8"局部颜色 / 16 dp 间距"}},
                 {MessageKey{u8"gallery.copy.sample.137", String{u8"Change label and order"}},
                  String{u8"切换标签 / 禁用"}},
                 {MessageKey{u8"gallery.copy.sample.138",
                             String{u8"Editable card · Add, remove, reorder, lazy and destroyOnHidden"}},
                  String{u8"Editable-card · 键控新增 / 删除 / 重排 / lazy / destroyOnHidden"}},
                 {MessageKey{u8"gallery.copy.sample.139", String{u8"Rich label · Draft"}}, String{u8"富标签 · Draft"}},
                 {MessageKey{u8"gallery.copy.sample.140", String{u8"Add tab"}}, String{u8"工作区"}},
                 {MessageKey{u8"gallery.copy.sample.141", String{u8"Reorder"}}, String{u8"重排"}},
                 {MessageKey{u8"gallery.copy.sample.142",
                             String{u8"Modal · Confirm/cancel, retained editing, document scrolling and nesting"}},
                  String{u8"Modal · 确认取消 / 保留编辑 / 正文滚动 / 嵌套"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.143",
                             String{u8"Distinguish confirmation, Cancel, close button, Escape and mask clicks."}},
                  String{u8"确认、取消、右上角关闭、Escape或点击遮罩。"}},
                 {MessageKey{u8"gallery.copy.sample.144",
                             String{u8"Closing returns focus to the original valid trigger."}},
                  String{u8"关闭后会返回原来的有效入口。"}},
                 {MessageKey{u8"gallery.copy.sample.145",
                             String{u8"Edit, close and reopen; the draft and selection remain."}},
                  String{u8"修改内容后关闭再打开，草稿和选项继续保留。"}},
                 {MessageKey{u8"gallery.copy.sample.146", String{u8"Retained content"}}, String{u8"输入帮助"}},
                 {MessageKey{
                      u8"gallery.copy.sample.147",
                      String{u8"Confirm and cancel only request changes. This example writes back the open state."}},
                  String{u8"确认与取消只发送请求。本例不会回写关闭状态。"}},
                 {MessageKey{u8"gallery.copy.sample.148", String{u8"Close example"}}, String{u8"关闭本例"}},
                 {MessageKey{u8"gallery.copy.sample.149",
                             String{u8"Confirmation is temporarily disabled while loading; Cancel remains available."}},
                  String{u8"确认在加载时禁用，取消仍可使用。"}},
                 {MessageKey{u8"gallery.copy.sample.150", String{u8"Toggle confirmation loading"}},
                  String{u8"切换确认加载状态"}},
                 {MessageKey{u8"gallery.copy.sample.151", String{u8"Title and footer use independent content slots."}},
                  String{u8"标题和页脚使用独立内容插槽。"}},
                 {MessageKey{u8"gallery.copy.sample.152", String{u8"Custom title"}}, String{u8"自定义标题"}},
                 {MessageKey{u8"gallery.copy.sample.153", String{u8"Help"}}, String{u8"返回"}},
                 {MessageKey{u8"gallery.copy.sample.154", String{u8"Save draft"}}, String{u8"保存草稿"}},
                 {MessageKey{u8"gallery.copy.sample.155", String{u8"Use the close button, Escape or Cancel to close."}},
                  String{u8"使用右上角关闭、Escape或点击遮罩取消。"}},
                 {MessageKey{u8"gallery.copy.sample.156",
                             String{u8"The background cannot be operated with the pointer while "
                                    u8"the dialog is open; outside tests can close it."}},
                  String{u8"背景仍不可操作。点击对话框外部可以关闭。"}},
                 {MessageKey{u8"gallery.copy.sample.157",
                             String{u8"Resize the window to observe centering in the usable viewport."}},
                  String{u8"调整窗口大小，观察对话框在有限视口内居中。"}},
                 {MessageKey{u8"gallery.copy.sample.158", String{u8"Long content"}}, String{u8"完成阅读"}},
                 {MessageKey{u8"gallery.copy.sample.159", String{u8"Scroll or Tab to the final editor."}},
                  String{u8"使用滚轮浏览，或按Tab移动到末尾编辑框。"}},
                 {MessageKey{u8"gallery.copy.sample.160", String{u8"First step"}}, String{u8"面板一"}},
                 {MessageKey{u8"gallery.copy.sample.161", String{u8"Review"}}, String{u8"面板二"}},
                 {MessageKey{u8"gallery.copy.sample.162", String{u8"Open another dialog"}}, String{u8"打开下一层"}},
                 {MessageKey{u8"gallery.copy.sample.163",
                             String{u8"Nested child · Closing returns to the valid parent trigger"}},
                  String{u8"关闭后返回父层有效入口"}},
                 {MessageKey{u8"gallery.copy.sample.164",
                             String{u8"The scoped theme uses a purple button and a 12 dp radius, "
                                    u8"independent of the outer default theme."}},
                  String{u8"局部主题使用紫色焦点和12 dp圆角，跟随外部明暗主题。"}},
                 {MessageKey{
                      u8"gallery.copy.sample.165",
                      String{
                          u8"RGBA image, derived fallback text, loading failure, breakpoints and stable member order"}},
                  String{u8"RGBA 图像 / 可取消加载 / 失败回退 / 窗口断点 / 稳定键分组"}},
                 {MessageKey{u8"gallery.copy.sample.166", String{u8"Complete simulated image request"}},
                  String{u8"完成模拟图像请求"}},
                 {MessageKey{u8"gallery.copy.sample.167", String{u8"Toggle Circle/Square image"}},
                  String{u8"切换 Circle / Square 图像"}},
                 {MessageKey{u8"gallery.copy.sample.168", String{u8"Simulate loading failure"}},
                  String{u8"模拟加载失败"}},
                 {MessageKey{u8"gallery.copy.sample.169", String{u8"Cancel and retry"}}, String{u8"取消并重试"}},
                 {MessageKey{u8"gallery.copy.sample.170", String{u8"Reorder members"}}, String{u8"重排分组"}},
                 {MessageKey{u8"gallery.copy.sample.171",
                             String{u8"Digit rolling, rich text, typed subcomponents, Processing and Ribbon"}},
                  String{u8"数字滚动 / 任意文本与 typed 富计数 / Processing / Ribbon"}},
                 {MessageKey{u8"gallery.copy.sample.172", String{u8"Independent button"}}, String{u8"收件箱"}},
                 {MessageKey{u8"gallery.copy.sample.173", String{u8"Count +1"}}, String{u8"数字 +1"}},
                 {MessageKey{u8"gallery.copy.sample.174", String{u8"Count -3"}}, String{u8"数字 -3"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.175", String{u8"Pause/resume Processing"}},
                  String{u8"暂停/恢复 Processing"}},
                 {MessageKey{u8"gallery.copy.sample.176", String{u8"Independent action inside Ribbon"}},
                  String{u8"保留内容与独立操作"}},
                 {MessageKey{u8"gallery.copy.sample.177",
                             String{u8"Cancelable closing, controlled reorder and selection; "
                                    u8"native dragging follows the right edge in vertical layout"}},
                  String{u8"可取消关闭 / 有限进退场 / 受控选择 / 原生拖排手柄（也可用左右键）"}},
                 {MessageKey{u8"gallery.copy.sample.178", String{u8"Cancelable close"}}, String{u8"可取消的关闭"}},
                 {MessageKey{u8"gallery.copy.sample.179", String{u8"Allow closing"}}, String{u8"允许关闭"}},
                 {MessageKey{u8"gallery.copy.sample.180", String{u8"Controlled CheckableTag"}},
                  String{u8"受控 CheckableTag"}},
                 {MessageKey{u8"gallery.copy.sample.181", String{u8"Allow/reject reorder"}}, String{u8"允许/拒绝重排"}},
                 {MessageKey{u8"gallery.copy.sample.182", String{u8"Tag / Badge · State and selection themes"}},
                  String{u8"Tag / Badge · 状态、选择与计数"}},
                 {MessageKey{u8"gallery.copy.sample.183", String{u8"Default tag"}}, String{u8"默认标签"}},
                 {MessageKey{u8"gallery.copy.sample.184", String{u8"Processing"}}, String{u8"运行正常"}},
                 {MessageKey{u8"gallery.copy.sample.185", String{u8"Close tag"}}, String{u8"关闭标签"}},
                 {MessageKey{u8"gallery.copy.sample.186", String{u8"Select tag"}}, String{u8"选择标签"}},
                 {MessageKey{u8"gallery.copy.sample.187", String{u8"Restore tag"}}, String{u8"恢复标签"}},
                 {MessageKey{u8"gallery.copy.sample.188", String{u8"Mark as read"}}, String{u8"已读"}},
                 {MessageKey{u8"gallery.copy.sample.189", String{u8"Round notification"}}, String{u8"圆点通知"}},
                 {MessageKey{u8"gallery.copy.sample.190", String{u8"Add notification"}}, String{u8"添加通知"}},
                 {MessageKey{u8"gallery.copy.sample.191",
                             String{u8"Drawer · Four placements, retained draft and document scrolling"}},
                  String{u8"Drawer · 四边停靠 / 保留草稿 / 正文滚动"}},
                 {MessageKey{u8"gallery.copy.sample.192", String{u8"Toggle keeping content on close"}},
                  String{u8"切换关闭时销毁正文"}},
                 {MessageKey{u8"gallery.copy.sample.193", String{u8"Open nested drawer"}}, String{u8"打开嵌套抽屉"}},
                 {MessageKey{u8"gallery.copy.sample.194", String{u8"Confirm save"}}, String{u8"确认保存"}},
                 {MessageKey{u8"gallery.copy.sample.195", String{u8"Finish and close"}}, String{u8"完成并关闭"}},
                 {MessageKey{u8"gallery.copy.sample.196", String{u8"Switch theme"}}, String{u8"切换加载"}},
                 {MessageKey{u8"gallery.copy.sample.197", String{u8"Popconfirm · Confirm, loading and cancel"}},
                  String{u8"Popconfirm · 确认 / 加载 / 取消"}},
                 {MessageKey{u8"gallery.copy.sample.198", String{u8"Perform action"}}, String{u8"执行操作"}},
                 {MessageKey{u8"gallery.copy.sample.199",
                             String{u8"Confirm to enter loading, then complete using the button below."}},
                  String{u8"点击确定进入加载状态，再点击下方按钮完成。"}},
                 {MessageKey{u8"gallery.copy.sample.200", String{u8"Complete action"}}, String{u8"完成操作"}},
                 {MessageKey{u8"gallery.copy.sample.201", String{u8"Standalone confirmation"}}, String{u8"快速确认"}},
                 {MessageKey{u8"gallery.copy.sample.202", String{u8"Alert / Spin / Progress · Feedback and themes"}},
                  String{u8"Alert / Spin / Progress · 反馈与进度"}},
                 {MessageKey{u8"gallery.copy.sample.203", String{u8"Retry"}}, String{u8"重试"}},
                 {MessageKey{u8"gallery.copy.sample.204", String{u8"Restore alert"}}, String{u8"恢复提示"}},
                 {MessageKey{u8"gallery.copy.sample.205", String{u8"Toggle loading"}}, String{u8"切换加载"}},
                 {MessageKey{u8"gallery.copy.sample.206", String{u8"Automatic progress"}}, String{u8"自动进度"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.207", String{u8"500 ms delay"}}, String{u8"延迟 500ms"}},
                 {MessageKey{u8"gallery.copy.sample.208", String{u8"Fullscreen loading"}}, String{u8"全屏加载"}},
                 {MessageKey{u8"gallery.copy.sample.209", String{u8"Content action"}}, String{u8"内容操作"}},
                 {MessageKey{u8"gallery.copy.sample.210", String{u8"Dismiss fullscreen loading"}},
                  String{u8"结束全屏加载"}},
                 {MessageKey{u8"gallery.copy.sample.211", String{u8"Advance progress"}}, String{u8"推进进度"}},
                 {MessageKey{u8"gallery.copy.sample.212", String{u8"Toggle exception state"}},
                  String{u8"切换异常状态"}},
                 {MessageKey{u8"gallery.copy.sample.213",
                             String{u8"Table / Pagination · Edit quantities, select records and change pages"}},
                  String{u8"Table / Pagination · 编辑、排序、选择与分页"}},
                 {MessageKey{u8"gallery.copy.sample.214", String{u8"Update data"}}, String{u8"更新首行"}},
                 {MessageKey{u8"gallery.copy.sample.215", String{u8"Reverse row order"}}, String{u8"反转数据顺序"}},
                 {MessageKey{u8"gallery.copy.sample.216", String{u8"Remove last row"}}, String{u8"删除末行"}},
                 {MessageKey{u8"gallery.copy.sample.217", String{u8"Restore data"}}, String{u8"恢复数据"}},
                 {MessageKey{u8"gallery.copy.sample.218", String{u8"Toggle loading"}}, String{u8"切换加载"}},
                 {MessageKey{u8"gallery.copy.sample.219",
                             String{u8"No records. Restore the data to continue the example."}},
                  String{u8"暂无记录，点击恢复数据继续体验。"}},
                 {MessageKey{u8"gallery.copy.sample.220",
                             String{u8"Page size, quick jump, compact and responsive layout"}},
                  String{u8"页大小 / 快速跳页 / 总数 / 自适应对齐"}},
                 {MessageKey{u8"gallery.copy.sample.221",
                             String{u8"Simple · Enter a page number and press Enter or leave the editor"}},
                  String{u8"Simple · 输入页码后 Enter 或离开编辑器"}},
                 {MessageKey{u8"gallery.copy.sample.222",
                             String{u8"Typed itemRender · Retained custom button content"}},
                  String{u8"Typed itemRender · 保留自定义按钮内容"}},
                 {MessageKey{u8"gallery.copy.sample.223", String{u8"Transfer · Two-panel filtering and pagination, "
                                                                 u8"controlled callbacks, dynamic data and RTL"}},
                  String{u8"Transfer Token / 双侧独立搜索和分页 / 受控回调 / 动态几何 / RTL"}},
                 {MessageKey{u8"gallery.copy.sample.224",
                             String{u8"Actual pagination / Item 2 disabled / Shift+Space range selection"}},
                  String{u8"真实分页 / Item 2 禁用 / Shift+Space 范围选择"}},
                 {MessageKey{u8"gallery.copy.sample.225", String{u8"Accept/reject controlled moves"}},
                  String{u8"接受 / 拒绝受控移动"}},
                 {MessageKey{u8"gallery.copy.sample.226", String{u8"Toggle oneWay / target deletion"}},
                  String{u8"切换 oneWay / 行内删除"}},
                 {MessageKey{u8"gallery.copy.sample.227", String{u8"Chinese / English locale"}},
                  String{u8"中文 / English locale"}},
                 {MessageKey{u8"gallery.copy.sample.228", String{u8"Dynamic list height and row gap"}},
                  String{u8"动态列表尺寸和行间距"}},
                 {MessageKey{u8"gallery.copy.sample.229", String{u8"Scoped Transfer tokens · Color / radius / size"}},
                  String{u8"局部 Transfer Token 颜色 / 字体 / 尺寸"}},
                 {MessageKey{u8"gallery.copy.sample.230",
                             String{u8"Retained custom list, footer TextArea, controlled keys and dynamic height"}},
                  String{u8"保留自定义列表 / footer TextArea / 受控动作和动态计数"}},
                 {MessageKey{u8"gallery.copy.sample.231", String{u8"Edit rich-item draft"}},
                  String{u8"编辑子浮层草稿"}},
                 {MessageKey{u8"gallery.copy.sample.232",
                             String{u8"Lazy children show loading and error states, with a real Retry action"}},
                  String{u8"本地加载器：首次明确演示错误，随后可点击 Retry"}},
                 {MessageKey{u8"gallery.copy.sample.233",
                             String{u8"Removable tags, responsive overflow and controlled selection"}},
                  String{u8"可删除标签、responsive 溢出与受控选择"}},
                 {MessageKey{u8"gallery.copy.sample.234",
                             String{u8"Scoped Cascader tokens, Unicode filtering and long-column scrolling"}},
                  String{u8"独立 Cascader Token、Unicode 搜索和长列滚动"}},
                 {MessageKey{u8"gallery.copy.sample.235",
                             String{u8"Retained custom tags · The editor draft belongs to each removable tag"}},
                  String{u8"保留自定义标签；编辑草稿后可单项移除"}},
                 {MessageKey{u8"gallery.copy.sample.236", String{u8"Remove"}}, String{u8"移除"}},
                 {MessageKey{u8"gallery.copy.sample.237",
                             String{u8"Custom trigger for an empty result with application-owned state"}},
                  String{u8"保留的自定义触发器与空状态"}},
                 {MessageKey{u8"gallery.copy.sample.238", String{u8"Open empty catalog"}}, String{u8"打开空目录"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.239", String{u8"Application-provided empty catalog content"}},
                  String{u8"应用提供的空目录内容"}},
                 {MessageKey{u8"gallery.copy.sample.240", String{u8"Menu / Dropdown · Navigation and commands"}},
                  String{u8"Menu / Dropdown · 导航与命令"}},
                 {MessageKey{u8"gallery.copy.sample.241", String{u8"Update menu label"}}, String{u8"更新菜单文字"}},
                 {MessageKey{u8"gallery.copy.sample.242", String{u8"Toggle menu disabled"}}, String{u8"切换菜单禁用"}},
                 {MessageKey{
                      u8"gallery.copy.sample.243",
                      String{u8"Horizontal overflow / vertical flyout / inline collapsed · RTL and stable keys"}},
                  String{u8"Horizontal overflow / Vertical flyout / Inline collapsed · RTL 与稳定 key"}},
                 {MessageKey{u8"gallery.copy.sample.244", String{u8"Switch navigation mode"}},
                  String{u8"切换三种模式"}},
                 {MessageKey{u8"gallery.copy.sample.245", String{u8"Reorder without removing keys"}},
                  String{u8"重排并增删 key"}},
                 {MessageKey{u8"gallery.copy.sample.246", String{u8"Collapse icon navigation"}},
                  String{u8"折叠图标导航"}},
                 {MessageKey{u8"gallery.copy.sample.247", String{u8"Commands from a trigger and context menu"}},
                  String{u8"点击或右键打开命令"}},
                 {MessageKey{u8"gallery.copy.sample.248", String{u8"Hover + click · Split button and native flyout"}},
                  String{u8"Hover + click · split button · 原生 flyout"}},
                 {MessageKey{u8"gallery.copy.sample.249", String{u8"Run"}}, String{u8"运行"}},
                 {MessageKey{u8"gallery.copy.sample.250",
                             String{u8"Retained popupRender · Menu and editor draft remain together"}},
                  String{u8"Retained popupRender · 菜单与编辑草稿共同保留"}},
                 {MessageKey{u8"gallery.copy.sample.251", String{u8"Custom context menu"}}, String{u8"自定义内容"}},
                 {MessageKey{u8"gallery.copy.sample.252", String{u8"Open custom popup"}}, String{u8"保留自定义 popup"}},
                 {MessageKey{u8"gallery.copy.sample.253",
                             String{u8"ColorPicker · Color, transparency and gradient stops"}},
                  String{u8"ColorPicker · 颜色、透明度和渐变色标"}},
                 {MessageKey{u8"gallery.copy.sample.254",
                             String{u8"Drag SV, hue and alpha; edit HEX / RGB / HSB fields"}},
                  String{u8"拖动 SV、色相、透明度；HEX / RGB / HSB 和四通道输入"}},
                 {MessageKey{u8"gallery.copy.sample.255", String{u8"Native custom expanded panel"}},
                  String{u8"原生自定义扩展区域"}},
                 {MessageKey{u8"gallery.copy.sample.256",
                             String{u8"Gradient stops: select, drag, add, remove and edit percentages"}},
                  String{u8"渐变色标：选择、拖动、添加、移除和百分比编辑"}},
                 {MessageKey{u8"gallery.copy.sample.257",
                             String{u8"stringMode · Exact decimal values beyond 2^53 and a 1e-17 step"}},
                  String{u8"stringMode · 超过 2^53 的精确十进制和 1e-17 步长"}},
                 {MessageKey{u8"gallery.copy.sample.258",
                             String{u8"formatter/parser · Custom grouping, keyboard and wheel stepping"}},
                  String{u8"formatter/parser · 自定义千分位；长按和 wheel 可取消"}},
                 {MessageKey{u8"gallery.copy.sample.259",
                             String{u8"InputNumber · Decimals, bounds, step and read-only"}},
                  String{u8"InputNumber · 小数、范围、禁用与只读"}},
                 {MessageKey{u8"gallery.copy.sample.260", String{u8"Controlled decimal 0..10"}},
                  String{u8"受控小数 0..10"}},
                 {MessageKey{u8"gallery.copy.sample.261", String{u8"Toggle disabled"}}, String{u8"禁用切换"}},
                 {MessageKey{u8"gallery.copy.sample.262", String{u8"Read-only"}}, String{u8"只读"}},
                 {MessageKey{u8"gallery.copy.sample.263", String{u8"Toggle number disabled"}},
                  String{u8"切换数值禁用"}},
                 {MessageKey{u8"gallery.copy.sample.264", String{u8"Focus decimal editor"}}, String{u8"聚焦小数编辑"}},
                 {MessageKey{u8"gallery.copy.sample.265",
                             String{u8"Dynamic panels · Synthetic sample data · Reorder retains "
                                    u8"drafts; remove and restore rebuilds retired content"}},
                  String{u8"动态面板 · 合成示例数据 · 重排保留草稿；懒挂载与关闭后销毁可独立控制"}},
                 {MessageKey{u8"gallery.copy.sample.266", String{u8"Save draft"}}, String{u8"保留草稿"}},
                 {MessageKey{u8"gallery.copy.sample.267", String{u8"Reorder panels"}}, String{u8"懒加载正文"}},
                 {MessageKey{u8"gallery.copy.sample.268", String{u8"Toggle accordion"}}, String{u8"重排面板"}},
                 {MessageKey{u8"gallery.copy.sample.269", String{u8"Ghost panels"}}, String{u8"Ghost 外观"}},
                 {MessageKey{u8"gallery.copy.sample.270", String{u8"Toggle draft panel"}}, String{u8"切换草稿面板"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{
                      u8"gallery.copy.sample.271",
                      String{
                          u8"Dynamic segmented options · Synthetic sample data · RTL / Round / actual pill animation"}},
                  String{u8"动态数值分段 · 合成示例数据 · RTL / Round / 有限 pill 过渡"}},
                 {MessageKey{u8"gallery.copy.sample.272", String{u8"Reorder options"}}, String{u8"重排选项"}},
                 {MessageKey{u8"gallery.copy.sample.273", String{u8"Toggle RTL"}}, String{u8"切换 RTL"}},
                 {MessageKey{u8"gallery.copy.sample.274",
                             String{u8"Dynamic descriptions · Synthetic sample data · Window "
                                    u8"breakpoints with 1/2/3 columns preserve editor drafts"}},
                  String{u8"动态资料 · 合成示例数据 · 窗口断点 1/2/3 列；重排保留真实编辑草稿"}},
                 {MessageKey{u8"gallery.copy.sample.275", String{u8"Responsive columns"}}, String{u8"响应式详情"}},
                 {MessageKey{u8"gallery.copy.sample.276", String{u8"Retained content"}}, String{u8"重排资料"}},
                 {MessageKey{u8"gallery.copy.sample.277", String{u8"Editable note"}}, String{u8"可编辑备注"}},
                 {MessageKey{u8"gallery.copy.sample.278", String{u8"Status"}}, String{u8"状态"}},
                 {MessageKey{u8"gallery.copy.sample.279", String{u8"Example added item"}}, String{u8"示例运行中"}},
                 {MessageKey{u8"gallery.copy.sample.280", String{u8"Dynamic description tail"}}, String{u8"填满行尾"}},
                 {MessageKey{u8"gallery.copy.sample.281",
                             String{u8"Logical window width reallocates columns without rebuilding content"}},
                  String{u8"随窗口 logical width 重新分配，内容不重挂载"}},
                 {MessageKey{u8"gallery.copy.sample.282", String{u8"Toggle layout RTL"}}, String{u8"切换详情 RTL"}},
                 {MessageKey{u8"gallery.copy.sample.283", String{u8"Dynamic timeline · Synthetic sample data · Content "
                                                                 u8"layout, titleSpan and actual loading icon"}},
                  String{u8"动态时间轴 · 合成示例数据 · 横纵布局、titleSpan 与真实 loading 图标"}},
                 {MessageKey{u8"gallery.copy.sample.284", String{u8"Release record"}}, String{u8"重排记录"}},
                 {MessageKey{u8"gallery.copy.sample.285", String{u8"Reorder / restore"}}, String{u8"横向 / 纵向"}},
                 {MessageKey{u8"gallery.copy.sample.286", String{u8"Toggle direction"}}, String{u8"切换加载"}},
                 {MessageKey{u8"gallery.copy.sample.287", String{u8"Switch status color"}}, String{u8"切换状态色"}},
                 {MessageKey{u8"gallery.copy.sample.288", String{u8"Title 25% / 180 dp"}},
                  String{u8"标题 25% / 180dp"}},
                 {MessageKey{u8"gallery.copy.sample.289", String{u8"Collapse · Retained editing and component theme"}},
                  String{u8"Collapse · 保留编辑与独立操作"}},
                 {MessageKey{u8"gallery.copy.sample.290", String{u8"Toggle accordion mode"}},
                  String{u8"切换手风琴模式"}},
                 {MessageKey{u8"gallery.copy.sample.291", String{u8"Toggle advanced panel disabled"}},
                  String{u8"切换高级面板禁用"}},
                 {MessageKey{u8"gallery.copy.sample.292", String{u8"Collapse all"}}, String{u8"收起全部"}},
                 {MessageKey{u8"gallery.copy.sample.293", String{u8"Show more options"}}, String{u8"展示面板内选项"}},
                 {MessageKey{u8"gallery.copy.sample.294", String{u8"Basic settings"}}, String{u8"基本设置"}},
                 {MessageKey{u8"gallery.copy.sample.295", String{u8"Help"}}, String{u8"帮助"}},
                 {MessageKey{u8"gallery.copy.sample.296", String{u8"Advanced settings"}}, String{u8"高级设置"}},
                 {MessageKey{u8"gallery.copy.sample.297", String{u8"Segmented · Workspace views"}},
                  String{u8"Segmented · 横向等宽视图"}},
                 {MessageKey{u8"gallery.copy.sample.298", String{u8"Vertical small · Shared controlled value"}},
                  String{u8"纵向小尺寸，共享受控值"}},
                 {MessageKey{u8"gallery.copy.sample.299", String{u8"Large rich labels"}}, String{u8"大尺寸富标签"}},
                 {MessageKey{u8"gallery.copy.sample.300", String{u8"List"}}, String{u8"列表"}},
                 {MessageKey{u8"gallery.copy.sample.301", String{u8"Update segmented label"}},
                  String{u8"更新分段标签"}},
                 {MessageKey{u8"gallery.copy.sample.302", String{u8"Toggle week view disabled"}},
                  String{u8"切换周视图禁用"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.303", String{u8"Empty · Empty state and creation action"}},
                  String{u8"Empty · 空状态与创建操作"}},
                 {MessageKey{u8"gallery.copy.sample.304", String{u8"Create example data"}}, String{u8"创建示例任务"}},
                 {MessageKey{u8"gallery.copy.sample.305", String{u8"Switch empty illustration"}},
                  String{u8"切换空状态图示"}},
                 {MessageKey{u8"gallery.copy.sample.306", String{u8"Compact empty state"}}, String{u8"紧凑空状态"}},
                 {MessageKey{u8"gallery.copy.sample.307", String{u8"Custom illustration and spacing"}},
                  String{u8"自定义图示与外边距"}},
                 {MessageKey{u8"gallery.copy.sample.308", String{u8"Result · Status, actions and retained draft"}},
                  String{u8"Result · 状态、详情与保留草稿"}},
                 {MessageKey{u8"gallery.copy.sample.309", String{u8"Result without an icon"}},
                  String{u8"带图标的结果标题"}},
                 {MessageKey{u8"gallery.copy.sample.310", String{u8"Toggle result sections"}}, String{u8"切换富标题"}},
                 {MessageKey{u8"gallery.copy.sample.311",
                             String{u8"Retained details: edit a draft, then change the result state above."}},
                  String{u8"任务详情：编辑草稿后可切换上方结果状态。"}},
                 {MessageKey{u8"gallery.copy.sample.312", String{u8"Switch result state"}}, String{u8"切换结果状态"}},
                 {MessageKey{u8"gallery.copy.sample.313", String{u8"Continue action"}}, String{u8"继续操作"}},
                 {MessageKey{u8"gallery.copy.sample.314", String{u8"404 exception result"}}, String{u8"404 异常结果"}},
                 {MessageKey{u8"gallery.copy.sample.315", String{u8"Custom result icon"}}, String{u8"自定义结果图标"}},
                 {MessageKey{
                      u8"gallery.copy.sample.316",
                      String{u8"Actual forms, custom typed controls, explicit bridging and native scroll targets"}},
                  String{u8"真实布局 / 自动 typed 控件绑定 / 精确数字 / 原生滚动目标"}},
                 {MessageKey{u8"gallery.copy.sample.317", String{u8"Switch horizontal / vertical / inline"}},
                  String{u8"切换横向 / 纵向 / Inline"}},
                 {MessageKey{u8"gallery.copy.sample.318", String{u8"Toggle RTL"}}, String{u8"切换 RTL"}},
                 {MessageKey{u8"gallery.copy.sample.319", String{u8"Validate and scroll to first error"}},
                  String{u8"校验并滚动到错误"}},
                 {MessageKey{u8"gallery.copy.sample.320", String{u8"Reveal and focus native field"}},
                  String{u8"显示并聚焦名称"}},
                 {MessageKey{u8"gallery.copy.sample.321", String{u8"Reveal and focus decimal editor"}},
                  String{u8"显示并聚焦精确金额"}},
                 {MessageKey{u8"gallery.copy.sample.322", String{u8"Reveal and focus custom control"}},
                  String{u8"显示并聚焦自定义控件"}},
                 {MessageKey{u8"gallery.copy.sample.323", String{u8"Reset typed-control drafts"}},
                  String{u8"重置 typed 控件与草稿"}},
                 {MessageKey{u8"gallery.copy.sample.324",
                             String{u8"Array paths, stable lists, nested lists and actual submitted values"}},
                  String{u8"深路径 / 稳定列表 / 嵌套列表 / 实际提交值"}},
                 {MessageKey{u8"gallery.copy.sample.325", String{u8"Copy group"}}, String{u8"上移本行"}},
                 {MessageKey{u8"gallery.copy.sample.326", String{u8"Remove group"}}, String{u8"删除本行"}},
                 {MessageKey{u8"gallery.copy.sample.327", String{u8"Add member"}}, String{u8"新增成员"}},
                 {MessageKey{u8"gallery.copy.sample.328", String{u8"Submit member list"}}, String{u8"提交深列表"}},
                 {MessageKey{u8"gallery.copy.sample.329", String{u8"Reset initial list"}}, String{u8"重置初始列表"}},
                 {MessageKey{u8"gallery.copy.sample.330",
                             String{u8"Async cancellation, dependent fields, debounce and owner-thread delivery"}},
                  String{u8"异步取消 / 依赖字段 / Debounce / owner-thread 交付"}},
                 {MessageKey{u8"gallery.copy.sample.331", String{u8"Submit current values"}}, String{u8"提交当前快照"}},
                 {MessageKey{u8"gallery.copy.sample.332", String{u8"Deliver synthetic async result"}},
                  String{u8"交付合成异步结果"}},
                 {MessageKey{u8"gallery.copy.sample.333", String{u8"Reset and cancel pending"}},
                  String{u8"重置并取消 pending"}},
                 {MessageKey{u8"gallery.copy.sample.334", String{u8"I agree to save this form"}},
                  String{u8"同意保存本次填写内容"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.335", String{u8"Save"}}, String{u8"保存"}},
                 {MessageKey{u8"gallery.copy.sample.336", String{u8"Validate"}}, String{u8"校验"}},
                 {MessageKey{u8"gallery.copy.sample.337", String{u8"Reset"}}, String{u8"重置"}},
                 {MessageKey{u8"gallery.copy.sample.338", String{u8"Toggle disabled"}}, String{u8"切换禁用"}},
                 {MessageKey{u8"gallery.copy.sample.339", String{u8"Toggle read-only"}}, String{u8"切换只读"}},
                 {MessageKey{u8"gallery.copy.sample.340", String{u8"Update help"}}, String{u8"更新帮助"}},
                 {MessageKey{u8"gallery.copy.sample.341", String{u8"Selection note"}}, String{u8"选择备注"}},
                 {MessageKey{u8"gallery.copy.sample.342", String{u8"Restore example date"}}, String{u8"恢复示例日期"}},
                 {MessageKey{u8"gallery.copy.sample.343", String{u8"Save date"}}, String{u8"保存日期"}},
                 {MessageKey{u8"gallery.copy.sample.344", String{u8"Reset date"}}, String{u8"重置日期"}},
                 {MessageKey{u8"gallery.copy.sample.345", String{u8"Toggle disabled"}}, String{u8"切换禁用"}},
                 {MessageKey{u8"gallery.copy.sample.346", String{u8"Toggle read-only"}}, String{u8"切换只读"}},
                 {MessageKey{u8"gallery.copy.sample.347", String{u8"Update date help"}}, String{u8"更新日期帮助"}},
                 {MessageKey{u8"gallery.copy.sample.348", String{u8"Grapheme count · Display-only source length"}},
                  String{u8"原文超限只提示"}},
                 {MessageKey{u8"gallery.copy.sample.349", String{u8"Exceed formatter · Byte count / remove spaces"}},
                  String{u8"字节统计 / 去除空格"}},
                 {MessageKey{u8"gallery.copy.sample.350", String{u8"Focus hints · Email / native hints"}},
                  String{u8"Email / 原生提示"}},
                 {MessageKey{u8"gallery.copy.sample.351", String{u8"Clear vector · Built-in two-tone icon"}},
                  String{u8"禁用动作保留图标"}},
                 {MessageKey{u8"gallery.copy.sample.352", String{u8"Focus and select all"}},
                  String{u8"Focus and select all"}},
                 {MessageKey{u8"gallery.copy.sample.353", String{u8"Toggle clear disabled"}},
                  String{u8"Toggle clear disabled"}},
                 {MessageKey{u8"gallery.copy.sample.354", String{u8"Password · Hover, prefix and count"}},
                  String{u8"悬停 / prefix / count"}},
                 {MessageKey{u8"gallery.copy.sample.355", String{u8"Controlled password · No trailing toggle"}},
                  String{u8"受控 / 显隐动作开关"}},
                 {MessageKey{u8"gallery.copy.sample.356", String{u8"Toggle visibility action"}},
                  String{u8"Toggle visibility action"}},
                 {MessageKey{u8"gallery.copy.sample.357", String{u8"Search · Dark Compact / clear source"}},
                  String{u8"Dark Compact / Clear 来源"}},
                 {MessageKey{u8"gallery.copy.sample.358",
                             String{u8"Small underlined · Compact height / typed content"}},
                  String{u8"连接高度 / typed content"}},
                 {MessageKey{u8"gallery.copy.sample.359", String{u8"Search"}}, String{u8"查询"}},
                 {MessageKey{u8"gallery.copy.sample.360",
                             String{u8"TextArea · Lines, selection counts, autoSize, scrolling and resize"}},
                  String{u8"TextArea / 多行输入：换行、选择、统计、autoSize、滚动与 resize"}},
                 {MessageKey{u8"gallery.copy.sample.361", String{u8"AutoSize · minRows=2 / maxRows=5"}},
                  String{u8"minRows=2 / maxRows=5"}},
                 {MessageKey{u8"gallery.copy.sample.362",
                             String{u8"Resize without wrapping · Drag in both directions / wheel"}},
                  String{u8"双向拖动 / wheel"}},
                 {MessageKey{u8"gallery.copy.sample.363", String{u8"Read-only · Dark Compact / selection preserved"}},
                  String{u8"Dark Compact / 可选择复制"}},
                 {MessageKey{u8"gallery.copy.sample.364", String{u8"Disabled · Warning"}}, String{u8"Warning / 禁用"}},
                 {MessageKey{u8"gallery.copy.sample.365",
                             String{u8"OTP · Graphemes, paste distribution, formatter, mask and dynamic length"}},
                  String{u8"OTP / 分格输入：grapheme、粘贴分发、formatter、mask 与动态长度"}},
                 {MessageKey{u8"gallery.copy.sample.366",
                             String{u8"Controlled · Completion and removal notifications"}},
                  String{u8"六格 / 填满通知"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.367", String{u8"Formatter and mask · Uppercase / bullet / Large"}},
                  String{u8"大写 / bullet / Large"}},
                 {MessageKey{u8"gallery.copy.sample.368", String{u8"RTL · Typed separator / RightToLeft"}},
                  String{u8"typed separator / RightToLeft"}},
                 {MessageKey{u8"gallery.copy.sample.369",
                             String{u8"Dynamic length · Add/remove cells and preserve the prefix"}},
                  String{u8"四/六格 / 前缀身份保留"}},
                 {MessageKey{u8"gallery.copy.sample.370",
                             String{u8"Read-only mask · Dark Compact / Small / custom mask"}},
                  String{u8"Dark Compact / Small / 自定义 mask"}},
                 {MessageKey{u8"gallery.copy.sample.371", String{u8"Disabled OTP · Warning"}},
                  String{u8"Warning / 禁用"}},
                 {MessageKey{u8"gallery.copy.sample.372", String{u8"Focus and select all"}},
                  String{u8"Focus and select all"}},
                 {MessageKey{u8"gallery.copy.sample.373", String{u8"Toggle OTP length"}},
                  String{u8"Toggle OTP length"}},
                 {MessageKey{u8"gallery.copy.sample.374",
                             String{u8"Bidirectional text · Arabic / Hebrew / numbers / Chinese · Auto / LTR / RTL"}},
                  String{u8"双向文字：Arabic / Hebrew / 数字 / 括号 · Auto / LTR / RTL"}},
                 {MessageKey{u8"gallery.copy.sample.375", String{u8"Bidirectional Input"}},
                  String{u8"Bidirectional Input"}},
                 {MessageKey{u8"gallery.copy.sample.376", String{u8"Bidirectional TextArea"}},
                  String{u8"Bidirectional TextArea"}},
                 {MessageKey{u8"gallery.copy.sample.377", String{u8"Bidirectional Password"}},
                  String{u8"Bidirectional Password"}},
                 {MessageKey{u8"gallery.copy.sample.378",
                             String{u8"Multiple / Tags / search / typed values / token separators / tag panel"}},
                  String{u8"Multiple / Tags / 分组 / typed 多值 / Token 分隔 / 标签溢出"}},
                 {MessageKey{u8"gallery.copy.sample.379", String{u8"Accept/reject multi-selection requests"}},
                  String{u8"接受/拒绝多选请求"}},
                 {MessageKey{u8"gallery.copy.sample.380",
                             String{u8"10,000 windowed items / RTL / custom options and popup / actionable prefix"}},
                  String{u8"10,000 项窗口列表 / RTL / 自定义 option 与 popup / 可操作前缀"}},
                 {MessageKey{
                      u8"gallery.copy.sample.381",
                      String{u8"Cancelable async options with deterministic synthetic data and no external network"}},
                  String{u8"可取消异步选项（确定性合成数据，无外部网络请求）"}},
                 {MessageKey{u8"gallery.copy.sample.382", String{u8"Deliver synthetic async options"}},
                  String{u8"交付合成异步选项"}},
                 {MessageKey{u8"gallery.copy.sample.383",
                             String{u8"Select · Single selection, Unicode search, clear, dynamic options and theme"}},
                  String{u8"Select · 单选 / Unicode 搜索 / 清空 / 动态选项 / 键盘"}},
                 {MessageKey{u8"gallery.copy.sample.384", String{u8"Select component theme · Purple / 36 dp rows"}},
                  String{u8"独立颜色 / 36 dp 行高"}},
                 {MessageKey{u8"gallery.copy.sample.385", String{u8"Focus Select"}}, String{u8"Focus Select"}},
                 {MessageKey{u8"gallery.copy.sample.386", String{u8"Toggle Select popup"}},
                  String{u8"Toggle Select popup"}},
                 {MessageKey{u8"gallery.copy.sample.387", String{u8"Update options"}}, String{u8"Update options"}},
                 {MessageKey{u8"gallery.copy.sample.388",
                             String{u8"FloatButton · Icons, badges, theme and explicit native scroll targets"}},
                  String{u8"FloatButton · 图标 / 徽标 / 分组 / 显式原生滚动目标"}},
                 {MessageKey{u8"gallery.copy.sample.389", String{u8"Basics · Default / primary / disabled / tooltip"}},
                  String{u8"默认 / 主色 / 内容 / 禁用"}},
                 {MessageKey{u8"gallery.copy.sample.390", String{u8"Support"}}, String{u8"客服"}},
                 {MessageKey{u8"gallery.copy.sample.391",
                             String{u8"Groups · Persistent / click / hover / four directions"}},
                  String{u8"常驻 / 点击 / 悬停 / 四方向"}},
                 {MessageKey{u8"gallery.copy.sample.392", String{u8"Controlled · Explicit open authority"}},
                  String{u8"显式 open authority"}},
                 {MessageKey{u8"gallery.copy.sample.393",
                             String{u8"BackTop · Actual native scroll domain / animated scroll to top"}},
                  String{u8"滑块滚动真实内容 / 进度环返回顶部"}},
                 {MessageKey{u8"gallery.copy.sample.394",
                             String{u8"Window · Actual lower-right window anchor / not draggable"}},
                  String{u8"窗口右下角真实浮动 / 可拖动"}},
                 {MessageKey{u8"gallery.copy.sample.395", String{u8"Reverse key order"}}, String{u8"反转 key 顺序"}},
                 {MessageKey{u8"gallery.copy.sample.396", String{u8"List/expandable layout"}},
                  String{u8"列表 / 两列网格"}},
                 {MessageKey{u8"gallery.copy.sample.397", String{u8"Clear/restore"}}, String{u8"清空 / 恢复"}},
                 {MessageKey{u8"gallery.copy.sample.398", String{u8"Loading/restore"}}, String{u8"禁用 / 恢复"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.399", String{u8"Pagination/windowing"}},
                  String{u8"分页 / 连续滚动"}},
                 {MessageKey{u8"gallery.copy.sample.400", String{u8"Toggle disabled"}}, String{u8"切换方向"}},
                 {MessageKey{u8"gallery.copy.sample.401", String{u8"Scroll to end"}}, String{u8"滚动到末尾"}},
                 {MessageKey{u8"gallery.copy.sample.402",
                             String{u8"Edit a draft, reorder, paginate and change layout; the "
                                    u8"wheel and final action use a real scroll domain."}},
                  String{u8"编辑草稿后重排、切页或切换网格；滚轮和末尾按钮控制真实视口。"}},
                 {MessageKey{u8"gallery.copy.sample.403", String{u8"Account list · Stable keys and retained drafts"}},
                  String{u8"账户列表 · 稳定 key 保留草稿"}},
                 {MessageKey{u8"gallery.copy.sample.404",
                             String{u8"Empty description · Can contain actual input controls"}},
                  String{u8"富说明 · 空数据也能保留真实操作控件"}},
                 {MessageKey{u8"gallery.copy.sample.405",
                             String{u8"Empty · Application image-loading service and dynamic native slots"}},
                  String{u8"Empty · 应用图片加载服务与局部动态 slots"}},
                 {MessageKey{u8"gallery.copy.sample.406", String{u8"Complete image load"}}, String{u8"完成图片加载"}},
                 {MessageKey{u8"gallery.copy.sample.407", String{u8"Fail image request"}}, String{u8"图片加载失败"}},
                 {MessageKey{u8"gallery.copy.sample.408", String{u8"Reload"}}, String{u8"重新加载"}},
                 {MessageKey{u8"gallery.copy.sample.409", String{u8"Empty/text description"}},
                  String{u8"富说明 / 文本说明"}},
                 {MessageKey{u8"gallery.copy.sample.410", String{u8"Remove/restore illustration"}},
                  String{u8"移除 / 恢复操作区"}},
                 {MessageKey{u8"gallery.copy.sample.411", String{u8"Account details"}}, String{u8"账户概览"}},
                 {MessageKey{u8"gallery.copy.sample.412",
                             String{u8"Card · Grid / Meta / keyed Tabs / native Image cover"}},
                  String{u8"Card · Grid / Meta / keyed Tabs / 原生 Image cover"}},
                 {MessageKey{u8"gallery.copy.sample.413", String{u8"Reorder/restore"}}, String{u8"两列 / 单列"}},
                 {MessageKey{u8"gallery.copy.sample.414", String{u8"Remove tab key"}}, String{u8"重排 tab keys"}},
                 {MessageKey{u8"gallery.copy.sample.415",
                             String{u8"Card · Title, retained slots, editing state and theme"}},
                  String{u8"Card · 标题 / 区域组合 / 保留编辑状态 / 加载"}},
                 {MessageKey{u8"gallery.copy.sample.416", String{u8"Basic Card · Borderless retained content"}},
                  String{u8"无标题内容容器"}},
                 {MessageKey{u8"gallery.copy.sample.417", String{u8"Business content"}},
                  String{u8"Business content / 业务内容"}},
                 {MessageKey{u8"gallery.copy.sample.418", String{u8"Account Card · Title / extra / editing state"}},
                  String{u8"标题 / extra / 编辑状态"}},
                 {MessageKey{u8"gallery.copy.sample.419", String{u8"Cover and actions · Vertical / cover / actions"}},
                  String{u8"富标题 / cover / actions"}},
                 {MessageKey{u8"gallery.copy.sample.420", String{u8"Retained content"}},
                  String{u8"Retained content / 保留内容"}},
                 {MessageKey{u8"gallery.copy.sample.421", String{u8"Cover slot / independent content subtree"}},
                  String{u8"Cover slot / 封面内容区域"}},
                 {MessageKey{u8"gallery.copy.sample.422", String{u8"Small Card"}}, String{u8"小号尺寸"}},
                 {MessageKey{u8"gallery.copy.sample.423", String{u8"Borderless / child hover"}},
                  String{u8"无边框 / 子控件 hover"}},
                 {MessageKey{u8"gallery.copy.sample.424", String{u8"Native loading placeholder"}},
                  String{u8"原生加载占位"}},
                 {MessageKey{u8"gallery.copy.sample.425", String{u8"Scoped theme / 16 dp padding"}},
                  String{u8"局部背景 / 16 dp padding"}},
                 {MessageKey{u8"gallery.copy.sample.426", String{u8"Scoped presentation"}},
                  String{u8"Scoped presentation / 局部主题"}},
                 {MessageKey{u8"gallery.copy.sample.427", String{u8"Toggle account card loading"}},
                  String{u8"切换账户卡片加载"}},
                 {MessageKey{u8"gallery.copy.sample.428",
                             String{u8"Anchor · Explicit scroll target and nested anchors"}},
                  String{u8"显式滚动目标与层级锚点"}},
                 {MessageKey{u8"gallery.copy.sample.429", String{u8"Public API"}}, String{u8"公开 API"}},
                 {MessageKey{u8"gallery.copy.sample.430", String{u8"Vertical/horizontal"}}, String{u8"横向 / 纵向"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.431", String{u8"Fixed/static"}}, String{u8"固定 / 静态"}},
                 {MessageKey{u8"gallery.copy.sample.432", String{u8"Remove/restore example anchor"}},
                  String{u8"删除 / 恢复示例锚点"}},
                 {MessageKey{u8"gallery.copy.sample.433", String{u8"Application draft"}}, String{u8"应用草稿"}},
                 {MessageKey{u8"gallery.copy.sample.434",
                             String{u8"Breadcrumb · Independent editing inside displayed hierarchy"}},
                  String{u8"展示层级中的独立编辑"}},
                 {MessageKey{u8"gallery.copy.sample.435", String{u8"Rename"}}, String{u8"改名"}},
                 {MessageKey{u8"gallery.copy.sample.436", String{u8"Hide project"}}, String{u8"显隐祖先"}},
                 {MessageKey{u8"gallery.copy.sample.437", String{u8"Hide draft"}}, String{u8"显隐草稿"}},
                 {MessageKey{u8"gallery.copy.sample.438", String{u8"Disable project level"}}, String{u8"锁定层级"}},
                 {MessageKey{u8"gallery.copy.sample.439", String{u8"Restore hierarchy"}}, String{u8"恢复层级"}},
                 {MessageKey{u8"gallery.copy.sample.440", String{u8"Edit draft"}}, String{u8"编辑草稿"}},
                 {MessageKey{u8"gallery.copy.sample.441",
                             String{u8"Dynamic Breadcrumb · Navigation, action menu and layout RTL"}},
                  String{u8"动态拓扑 / 内置菜单 / 换行与 RTL"}},
                 {MessageKey{u8"gallery.copy.sample.442", String{u8"Reorder without changing items"}},
                  String{u8"重排键控条目"}},
                 {MessageKey{u8"gallery.copy.sample.443", String{u8"Remove/restore project"}},
                  String{u8"删除 / 恢复条目"}},
                 {MessageKey{u8"gallery.copy.sample.444", String{u8"RTL / LTR"}}, String{u8"RTL / LTR"}},
                 {MessageKey{u8"gallery.copy.sample.445", String{u8"Save note"}}, String{u8"保存备注"}},
                 {MessageKey{u8"gallery.copy.sample.446", String{u8"Previous"}}, String{u8"上一步"}},
                 {MessageKey{u8"gallery.copy.sample.447", String{u8"Next"}}, String{u8"下一步"}},
                 {MessageKey{u8"gallery.copy.sample.448", String{u8"Reset flow"}}, String{u8"重置流程"}},
                 {MessageKey{u8"gallery.copy.sample.449", String{u8"Toggle direction"}}, String{u8"横纵切换"}},
                 {MessageKey{u8"gallery.copy.sample.450", String{u8"Switch size"}}, String{u8"切换尺寸"}},
                 {MessageKey{u8"gallery.copy.sample.451", String{u8"Switch runtime status"}}, String{u8"切换错误状态"}},
                 {MessageKey{u8"gallery.copy.sample.452", String{u8"Hide result"}}, String{u8"显隐审核"}},
                 {MessageKey{u8"gallery.copy.sample.453", String{u8"Disable deployment focus"}},
                  String{u8"锁定资料与审核"}},
                 {MessageKey{u8"gallery.copy.sample.454", String{u8"Edit note"}}, String{u8"编辑备注"}},
                 {MessageKey{u8"gallery.copy.sample.455",
                             String{u8"Dynamic lazy steps · Reorder preserves the editor area"}},
                  String{u8"动态键控步骤 · 重排后保留编辑内容"}},
                 {MessageKey{u8"gallery.copy.sample.456", String{u8"Reorder steps"}}, String{u8"重排步骤"}},
                 {MessageKey{u8"gallery.copy.sample.457",
                             String{u8"08:00–18:00, every 15 minutes; hour 12 unavailable; controlled read-only text"}},
                  String{u8"08:00–18:00，每 15 分钟，12 时不可选；滚轮只滚动列"}},
                 {MessageKey{u8"gallery.copy.sample.458", String{u8"Selection note"}}, String{u8"选择备注"}},
                 {MessageKey{u8"gallery.copy.sample.459", String{u8"Restore example time"}}, String{u8"恢复时间示例"}},
                 {MessageKey{u8"gallery.copy.sample.460", String{u8"Confirmation/immediate mode"}},
                  String{u8"确认/即时模式"}},
                 {MessageKey{u8"gallery.copy.sample.461", String{u8"Toggle accepted-value authority"}},
                  String{u8"切换锁定已保存值"}},
                 {MessageKey{u8"gallery.copy.sample.462", String{u8"Toggle input read-only"}},
                  String{u8"切换文本只读"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.463", String{u8"Save time"}}, String{u8"保存时间"}},
                 {MessageKey{u8"gallery.copy.sample.464", String{u8"Reset time"}}, String{u8"重置时间"}},
                 {MessageKey{u8"gallery.copy.sample.465", String{u8"Toggle disabled"}}, String{u8"切换禁用"}},
                 {MessageKey{u8"gallery.copy.sample.466", String{u8"Toggle read-only"}}, String{u8"切换只读"}},
                 {MessageKey{u8"gallery.copy.sample.467", String{u8"Update time help"}}, String{u8"更新时间帮助"}},
                 {MessageKey{u8"gallery.copy.sample.468", String{u8"Project details"}}, String{u8"项目资料"}},
                 {MessageKey{u8"gallery.copy.sample.469", String{u8"Name"}}, String{u8"名称"}},
                 {MessageKey{u8"gallery.copy.sample.470", String{u8"Role"}}, String{u8"角色"}},
                 {MessageKey{u8"gallery.copy.sample.471", String{u8"Status"}}, String{u8"启用"}},
                 {MessageKey{u8"gallery.copy.sample.472", String{u8"Note"}}, String{u8"操作"}},
                 {MessageKey{u8"gallery.copy.sample.473", String{u8"Profile panel"}}, String{u8"保存资料"}},
                 {MessageKey{u8"gallery.copy.sample.474", String{u8"Columns / dynamic"}}, String{u8"单列/多列"}},
                 {MessageKey{u8"gallery.copy.sample.475", String{u8"Set controlled span"}}, String{u8"名称跨列"}},
                 {MessageKey{u8"gallery.copy.sample.476", String{u8"Hide region"}}, String{u8"显隐资料"}},
                 {MessageKey{u8"gallery.copy.sample.477", String{u8"Content/label"}}, String{u8"横纵标签"}},
                 {MessageKey{u8"gallery.copy.sample.478", String{u8"Toggle border"}}, String{u8"切换边框"}},
                 {MessageKey{u8"gallery.copy.sample.479", String{u8"Switch size"}}, String{u8"切换尺寸"}},
                 {MessageKey{u8"gallery.copy.sample.480", String{u8"Edit name"}}, String{u8"编辑名称"}},
                 {MessageKey{u8"gallery.copy.sample.481", String{u8"Toggle motion"}}, String{u8"切换流光"}},
                 {MessageKey{u8"gallery.copy.sample.482", String{u8"Loading/show content"}}, String{u8"加载/显示正文"}},
                 {MessageKey{u8"gallery.copy.sample.483", String{u8"Title"}}, String{u8"标题"}},
                 {MessageKey{u8"gallery.copy.sample.484", String{u8"Paragraph rows"}}, String{u8"正文行数"}},
                 {MessageKey{u8"gallery.copy.sample.485", String{u8"Avatar"}}, String{u8"头像"}},
                 {MessageKey{u8"gallery.copy.sample.486", String{u8"Round corners"}}, String{u8"圆角"}},
                 {MessageKey{u8"gallery.copy.sample.487", String{u8"Edit content"}}, String{u8"编辑正文"}},
                 {MessageKey{u8"gallery.copy.sample.488", String{u8"Project update"}}, String{u8"项目动态"}},
                 {MessageKey{u8"gallery.copy.sample.489", String{u8"Share update"}}, String{u8"发布动态"}},
                 {MessageKey{u8"gallery.copy.sample.490", String{u8"Standalone placeholders"}}, String{u8"独立占位"}},
                 {MessageKey{u8"gallery.copy.sample.491", String{u8"Switch shape and size"}},
                  String{u8"切换形态与尺寸"}},
                 {MessageKey{u8"gallery.copy.sample.492", String{u8"Block layout"}}, String{u8"填满宽度"}},
                 {MessageKey{u8"gallery.copy.sample.493", String{u8"Image placeholder"}}, String{u8"图片占位"}},
                 {MessageKey{u8"gallery.copy.sample.494", String{u8"Custom retained content"}},
                  String{u8"自定义保留内容"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.495", String{u8"Share checklist / native context menu"}},
                  String{u8"滚动清单 / 点击或右键"}},
                 {MessageKey{u8"gallery.copy.sample.496", String{u8"Share checklist"}}, String{u8"保存清单"}},
                 {MessageKey{u8"gallery.copy.sample.497", String{u8"Toggle RTL"}}, String{u8"切换 RTL"}},
                 {MessageKey{u8"gallery.copy.sample.498", String{u8"Toggle boundary protection"}},
                  String{u8"切换线框主题"}},
                 {MessageKey{u8"gallery.copy.sample.499", String{u8"Edit note"}}, String{u8"编辑备注"}},
                 {MessageKey{u8"gallery.copy.sample.500", String{u8"Project note"}}, String{u8"项目备注"}},
                 {MessageKey{u8"gallery.copy.sample.501", String{u8"Save note"}}, String{u8"保存备注"}},
                 {MessageKey{u8"gallery.copy.sample.502", String{u8"Cancel editing"}}, String{u8"取消编辑"}},
                 {MessageKey{u8"gallery.copy.sample.503", String{u8"Change placement"}}, String{u8"切换方位"}},
                 {MessageKey{u8"gallery.copy.sample.504", String{u8"Toggle arrow"}}, String{u8"切换箭头"}},
                 {MessageKey{u8"gallery.copy.sample.505", String{u8"Disable/restore"}}, String{u8"禁用/恢复"}},
                 {MessageKey{u8"gallery.copy.sample.506", String{u8"Reopen"}}, String{u8"重新打开"}},
                 {MessageKey{u8"gallery.copy.sample.507", String{u8"Open notification"}}, String{u8"打开通知"}},
                 {MessageKey{u8"gallery.copy.sample.508", String{u8"Update to success"}}, String{u8"更新为成功"}},
                 {MessageKey{u8"gallery.copy.sample.509", String{u8"Chinese / English"}}, String{u8"中文 / English"}},
                 {MessageKey{u8"gallery.copy.sample.510", String{u8"Close notification"}}, String{u8"关闭通知"}},
                 {MessageKey{u8"gallery.copy.sample.511", String{u8"Read close result"}}, String{u8"读取关闭结果"}},
                 {MessageKey{u8"gallery.copy.sample.512", String{u8"Open message"}}, String{u8"处理提示"}},
                 {MessageKey{u8"gallery.copy.sample.513", String{u8"Update to success"}}, String{u8"更新成功"}},
                 {MessageKey{u8"gallery.copy.sample.514", String{u8"Close message"}}, String{u8"关闭提示"}},
                 {MessageKey{u8"gallery.copy.sample.515", String{u8"Open wide message"}}, String{u8"带进度的消息"}},
                 {MessageKey{u8"gallery.copy.sample.516", String{u8"Read close result"}}, String{u8"读取关闭结果"}},
                 {MessageKey{u8"gallery.copy.sample.517", String{u8"Confirm review"}}, String{u8"确认核对"}},
                 {MessageKey{u8"gallery.copy.sample.518", String{u8"Open notification"}}, String{u8"打开通知"}},
                 {MessageKey{u8"gallery.copy.sample.519", String{u8"Update title"}}, String{u8"更新标题"}},
                 {MessageKey{u8"gallery.copy.sample.520", String{u8"Close notification"}}, String{u8"关闭通知"}},
                 {MessageKey{u8"gallery.copy.sample.521", String{u8"Four-corner placement"}}, String{u8"四角位置"}},
                 {MessageKey{u8"gallery.copy.sample.522", String{u8"Switch status"}}, String{u8"切换状态"}},
                 {MessageKey{u8"gallery.copy.sample.523", String{u8"Clear notices"}}, String{u8"清空提示"}},
                 {MessageKey{u8"gallery.copy.sample.524", String{u8"Confirm receipt"}}, String{u8"确认审核"}},
                 {MessageKey{u8"gallery.copy.sample.525", String{u8"Create persistent notification"}},
                  String{u8"打开审核通知"}},
                 {MessageKey{u8"gallery.copy.sample.526", String{u8"Update title and retain draft"}},
                  String{u8"更新标题，保留草稿"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.527", String{u8"Close persistent notification"}},
                  String{u8"关闭审核通知"}},
                 {MessageKey{u8"gallery.copy.sample.528", String{u8"Four-corner placement"}}, String{u8"四角位置"}},
                 {MessageKey{u8"gallery.copy.sample.529", String{u8"Switch status"}}, String{u8"切换状态"}},
                 {MessageKey{u8"gallery.copy.sample.530", String{u8"Short timed notification"}},
                  String{u8"添加限时通知"}},
                 {MessageKey{u8"gallery.copy.sample.531", String{u8"Clear notifications"}}, String{u8"清空通知"}},
                 {MessageKey{u8"gallery.copy.sample.532", String{u8"Save follow-up note"}}, String{u8"保存处理备注"}},
                 {MessageKey{u8"gallery.copy.sample.533", String{u8"Switch placement"}}, String{u8"切换位置"}},
                 {MessageKey{u8"gallery.copy.sample.534", String{u8"Chronological/reverse"}}, String{u8"正序/反序"}},
                 {MessageKey{u8"gallery.copy.sample.535", String{u8"Toggle pending"}}, String{u8"切换处理中"}},
                 {MessageKey{u8"gallery.copy.sample.536", String{u8"Hide note"}}, String{u8"显隐备注"}},
                 {MessageKey{u8"gallery.copy.sample.537", String{u8"Edit note"}}, String{u8"编辑备注"}},
                 {MessageKey{u8"gallery.copy.sample.538", String{u8"Save member"}}, String{u8"保存成员"}},
                 {MessageKey{u8"gallery.copy.sample.539", String{u8"Text/icon"}}, String{u8"文字/图标"}},
                 {MessageKey{u8"gallery.copy.sample.540", String{u8"Circle/square"}}, String{u8"圆形/方形"}},
                 {MessageKey{u8"gallery.copy.sample.541", String{u8"Large/small avatar"}}, String{u8"大/小头像"}},
                 {MessageKey{u8"gallery.copy.sample.542", String{u8"Show 2/0 members"}}, String{u8"显示2/0人"}},
                 {MessageKey{u8"gallery.copy.sample.543", String{u8"Hide member"}}, String{u8"显隐成员"}},
                 {MessageKey{u8"gallery.copy.sample.544", String{u8"Restore editing"}}, String{u8"恢复编辑"}},
                 {MessageKey{u8"gallery.copy.sample.545", String{u8"5-second countdown"}}, String{u8"5 秒倒计时"}},
                 {MessageKey{u8"gallery.copy.sample.546", String{u8"Count up"}}, String{u8"正计时"}},
                 {MessageKey{u8"gallery.copy.sample.547", String{u8"Stop timer"}}, String{u8"停止计时"}},
                 {MessageKey{u8"gallery.copy.sample.548", String{u8"Change animated value"}}, String{u8"改变动画值"}},
                 {MessageKey{u8"gallery.copy.sample.549", String{u8"Switch locale / separators"}},
                  String{u8"切换 locale / 分组"}},
                 {MessageKey{u8"gallery.copy.sample.550", String{u8"Save metric note"}}, String{u8"保存指标备注"}},
                 {MessageKey{u8"gallery.copy.sample.551", String{u8"Switch business value"}}, String{u8"切换业务值"}},
                 {MessageKey{u8"gallery.copy.sample.552", String{u8"Precision: 2 / 8 / source"}},
                  String{u8"2/8/原精度"}},
                 {MessageKey{u8"gallery.copy.sample.553", String{u8"Switch separators"}}, String{u8"切换分隔符"}},
                 {MessageKey{u8"gallery.copy.sample.554", String{u8"Switch formatter"}}, String{u8"切换formatter"}},
                 {MessageKey{u8"gallery.copy.sample.555", String{u8"Loading/error"}}, String{u8"加载/完成"}},
                 {MessageKey{u8"gallery.copy.sample.556", String{u8"Restore metric editing"}},
                  String{u8"恢复指标编辑"}},
                 {MessageKey{u8"gallery.copy.sample.557", String{u8"Save service review"}}, String{u8"保存服务评价"}},
                 {MessageKey{u8"gallery.copy.sample.558", String{u8"Switch rating direction"}},
                  String{u8"切换评分方向"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.559", String{u8"Switch built-in rating character"}},
                  String{u8"切换逐项绘制字符"}},
                 {MessageKey{u8"gallery.copy.sample.560", String{u8"Focus rating"}}, String{u8"聚焦评分"}},
                 {MessageKey{u8"gallery.copy.sample.561", String{u8"Accept/reject changes"}},
                  String{u8"接受/拒绝修改"}},
                 {MessageKey{u8"gallery.copy.sample.562", String{u8"Enable/disable rating"}},
                  String{u8"禁用/启用评分"}},
                 {MessageKey{u8"gallery.copy.sample.563", String{u8"Read-only/edit rating"}},
                  String{u8"只读/编辑评分"}},
                 {MessageKey{u8"gallery.copy.sample.564", String{u8"Hide rating"}}, String{u8"显隐评分"}},
                 {MessageKey{u8"gallery.copy.sample.565", String{u8"Restore rating"}}, String{u8"恢复评分"}},
                 {MessageKey{u8"gallery.copy.sample.566",
                             String{u8"Mentions · Search, 1,000 windowed options, RTL and unbounded columns"}},
                  String{u8"分组 / 1,000 项窗口列表 / RTL / 有限开启动画"}},
                 {MessageKey{u8"gallery.copy.sample.567", String{u8"Custom one-line editor, retained popup notes and "
                                                                 u8"explicit Form value/onChange/ref bridging"}},
                  String{u8"自定义多行编辑器 / 保留 popup 备注；Form 仍须显式 value/onChange/ref 绑定"}},
                 {MessageKey{
                      u8"gallery.copy.sample.568",
                      String{
                          u8"Cancelable async suggestions with deterministic synthetic data and no external network"}},
                  String{u8"可取消异步建议（确定性合成数据，无外部网络请求）"}},
                 {MessageKey{u8"gallery.copy.sample.569", String{u8"Deliver synthetic async suggestions"}},
                  String{u8"交付合成异步建议"}},
                 {MessageKey{u8"gallery.copy.sample.570",
                             String{u8"Synthetic async suggestions: type @query, then deliver the "
                                    u8"example data; no external network is used."}},
                  String{u8"合成异步建议：输入 @query 后点击交付；此示例不访问外部网络。"}},
                 {MessageKey{u8"gallery.copy.sample.571", String{u8"Deliver synthetic async suggestions"}},
                  String{u8"交付合成异步建议"}},
                 {MessageKey{u8"gallery.copy.sample.572", String{u8"Update suggestions"}}, String{u8"面板详情"}},
                 {MessageKey{u8"gallery.copy.sample.573", String{u8"Accept/reject"}}, String{u8"接受 / 拒绝"}},
                 {MessageKey{u8"gallery.copy.sample.574", String{u8"Reorder by key"}}, String{u8"按 key 重排"}},
                 {MessageKey{u8"gallery.copy.sample.575", String{u8"Remove last item"}}, String{u8"删除末项"}},
                 {MessageKey{u8"gallery.copy.sample.576", String{u8"Restore source"}}, String{u8"恢复原项"}},
                 {MessageKey{u8"gallery.copy.sample.577", String{u8"Today"}}, String{u8"详情"}},
                 {MessageKey{u8"gallery.copy.sample.578", String{u8"Toggle mini/fullscreen"}},
                  String{u8"切换 mini/fullscreen"}},
                 {MessageKey{u8"gallery.copy.sample.579", String{u8"Switch week start"}}, String{u8"切换周首日"}},
                 {MessageKey{u8"gallery.copy.sample.580", String{u8"Toggle valid date range"}},
                  String{u8"切换有效日期范围"}},
                 {MessageKey{
                      u8"gallery.copy.sample.581",
                      String{
                          u8"Mentions · Suggestions at the multiline caret. @ / @@ mention contacts; # mentions tags; "
                          u8"a Unicode prefix is also supported."}},
                  String{
                      u8"Mentions · 多行编辑与 caret 建议。@ / @@ 提及联系人，# 提及标签，提及 使用 Unicode 前缀。"}},
                 {MessageKey{u8"gallery.copy.sample.582", String{u8"Accept/reject editing"}},
                  String{u8"接受/拒绝编辑"}},
                 {MessageKey{u8"gallery.copy.sample.583", String{u8"Disable/restore"}}, String{u8"禁用/恢复"}},
                 {MessageKey{u8"gallery.copy.sample.584", String{u8"Read-only/restore"}}, String{u8"只读/恢复"}},
                 {MessageKey{u8"gallery.copy.sample.585", String{u8"Hide/restore editor"}},
                  String{u8"隐藏/恢复编辑器"}},
                 {MessageKey{u8"gallery.copy.sample.586", String{u8"RTL/automatic direction"}},
                  String{u8"RTL/自动方向"}},
                 {MessageKey{u8"gallery.copy.sample.587", String{u8"Suggestions above/below"}},
                  String{u8"上方/下方建议"}},
                 {MessageKey{u8"gallery.copy.sample.588", String{u8"Reset Form"}}, String{u8"重置 Form"}},
                 {MessageKey{
                      u8"gallery.copy.sample.589",
                      String{u8"One-line suggestions: 10,000 ordinary items are realized within the visible window; "
                             u8"clear or disable preserves the draft and releases focus."}},
                  String{u8"长建议列表：10,000 个普通建议按可见窗口实现；富建议离屏时保存草稿并释放焦点。"}},
                 {MessageKey{u8"gallery.copy.sample.590", String{u8"Save contact note"}}, String{u8"保存联系人备注"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.591", String{u8"Accept/reject changes"}},
                  String{u8"接受/拒绝修改"}},
                 {MessageKey{u8"gallery.copy.sample.592", String{u8"Update suggestions"}}, String{u8"更新建议"}},
                 {MessageKey{u8"gallery.copy.sample.593", String{u8"Enable/disable"}}, String{u8"禁用/启用"}},
                 {MessageKey{u8"gallery.copy.sample.594", String{u8"Read-only/edit"}}, String{u8"只读/编辑"}},
                 {MessageKey{u8"gallery.copy.sample.595", String{u8"Restore contact input"}},
                  String{u8"恢复联系人输入"}},
                 {MessageKey{u8"gallery.copy.sample.596", String{u8"Save form contact"}}, String{u8"保存表单联系人"}},
                 {MessageKey{u8"gallery.copy.sample.597", String{u8"Reset contact"}}, String{u8"重置联系人"}},
                 {MessageKey{u8"gallery.copy.sample.598", String{u8"Reverse stable keys"}},
                  String{u8"动态反转稳定 key"}},
                 {MessageKey{u8"gallery.copy.sample.599", String{u8"Scroll to row 10,000"}},
                  String{u8"scrollTo 第 10000 行"}},
                 {MessageKey{u8"gallery.copy.sample.600", String{u8"Synthetic async source: expand a lazy node, then "
                                                                 u8"use the delivery button for its children."}},
                  String{u8"合成异步源：展开两个节点后，点击交付；不访问网络"}},
                 {MessageKey{u8"gallery.copy.sample.601", String{u8"Deliver synthetic child nodes out of order"}},
                  String{u8"逆序交付合成加载"}},
                 {MessageKey{u8"gallery.copy.sample.602",
                             String{u8"Native custom popup, retaining the Tree keyboard contract"}},
                  String{u8"原生自定义 popup：保留 Tree 键盘合同"}},
                 {MessageKey{u8"gallery.copy.sample.603", String{u8"Native popup note"}}, String{u8"原生 popup 操作"}},
                 {MessageKey{u8"gallery.copy.sample.604",
                             String{u8"TreeSelect · 10,000-row search, multiple selection and dynamic ordering"}},
                  String{u8"TreeSelect：10000 个可搜索虚拟选项与动态顺序"}},
                 {MessageKey{u8"gallery.copy.sample.605", String{u8"Scroll to option 10,000"}},
                  String{u8"scrollTo 第 10000 个选项"}},
                 {MessageKey{u8"gallery.copy.sample.606", String{u8"Reverse stable key order"}},
                  String{u8"反转稳定 key 顺序"}},
                 {MessageKey{u8"gallery.copy.sample.607",
                             String{u8"Synthetic async TreeSelect source: expand and deliver children out of order."}},
                  String{u8"TreeSelect 合成异步源：展开后点击交付；不访问网络"}},
                 {MessageKey{u8"gallery.copy.sample.608", String{u8"Deliver synthetic TreeSelect children"}},
                  String{u8"交付合成 TreeSelect 加载"}},
                 {MessageKey{u8"gallery.copy.sample.609", String{u8"Save project note"}}, String{u8"保存项目备注"}},
                 {MessageKey{u8"gallery.copy.sample.610", String{u8"Enable/disable project tree"}},
                  String{u8"禁用/启用项目树"}},
                 {MessageKey{u8"gallery.copy.sample.611", String{u8"Hide project tree"}}, String{u8"显隐项目树"}},
                 {MessageKey{u8"gallery.copy.sample.612", String{u8"Unlock/restore archive branch"}},
                  String{u8"归档分支禁用/恢复"}},
                 {MessageKey{u8"gallery.copy.sample.613", String{u8"Restore project note"}}, String{u8"恢复项目备注"}},
                 {MessageKey{u8"gallery.copy.sample.614", String{u8"Accept/reject selection"}},
                  String{u8"接受/拒绝选择"}},
                 {MessageKey{u8"gallery.copy.sample.615", String{u8"Accept/reject closing"}},
                  String{u8"接受/拒绝关闭"}},
                 {MessageKey{u8"gallery.copy.sample.616", String{u8"Switch guide title"}}, String{u8"切换指南标题"}},
                 {MessageKey{u8"gallery.copy.sample.617", String{u8"Enable/disable archive"}},
                  String{u8"禁用/启用归档"}},
                 {MessageKey{u8"gallery.copy.sample.618", String{u8"Enable/disable selector"}},
                  String{u8"禁用/启用选择器"}},
                 {MessageKey{u8"gallery.copy.sample.619", String{u8"Read-only/restore selection"}},
                  String{u8"只读/恢复选择"}},
                 {MessageKey{u8"gallery.copy.sample.620", String{u8"Hide selector"}}, String{u8"显隐选择器"}},
                 {MessageKey{u8"gallery.copy.sample.621", String{u8"Restore target selection"}},
                  String{u8"恢复目标选择"}},
                 {MessageKey{u8"gallery.copy.sample.622", String{u8"Save target note"}}, String{u8"保存目标备注"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.623", String{u8"Submit target"}}, String{u8"提交交付目标"}},
                 {MessageKey{u8"gallery.copy.sample.624", String{u8"Focus and reset target"}},
                  String{u8"聚焦后重置目标"}},
                 {MessageKey{u8"gallery.copy.sample.625",
                             String{u8"Date / week / month / quarter / year · Locale and RTL"}},
                  String{u8"日期／周／月／季度／年份 · locale 与 RTL"}},
                 {MessageKey{u8"gallery.copy.sample.626", String{u8"Edit rich-cell draft"}},
                  String{u8"编辑日期子浮层"}},
                 {MessageKey{u8"gallery.copy.sample.627", String{u8"Switch picker mode"}}, String{u8"切换日期模式"}},
                 {MessageKey{u8"gallery.copy.sample.628", String{u8"Sunday / ISO Monday"}},
                  String{u8"Sunday / ISO Monday"}},
                 {MessageKey{u8"gallery.copy.sample.629", String{u8"Switch direction"}}, String{u8"切换方向"}},
                 {MessageKey{u8"gallery.copy.sample.630",
                             String{u8"Optional confirmation and nullable range · A fixed accepted "
                                    u8"value demonstrates controlled rejection"}},
                  String{u8"多选确认与可空范围 · 锁定 accepted 值可观察受控拒绝"}},
                 {MessageKey{u8"gallery.copy.sample.631", String{u8"Toggle range authority"}},
                  String{u8"切换范围接受"}},
                 {MessageKey{u8"gallery.copy.sample.632",
                             String{u8"Native confirmation of date and exact time · CivilDateTime does not convert "
                                    u8"timezones implicitly"}},
                  String{u8"日期与精确时间原子确认 · CivilDateTime 没有隐式时区转换"}},
                 {MessageKey{u8"gallery.copy.sample.633",
                             String{u8"AM/PM, stepped seconds, cascading disabled values, Now and presets"}},
                  String{u8"AM/PM、整数毫秒、依赖禁用、Now 与预设"}},
                 {MessageKey{u8"gallery.copy.sample.634", String{u8"12 / 24 hours"}}, String{u8"12 / 24 小时"}},
                 {MessageKey{u8"gallery.copy.sample.635", String{u8"Select while scrolling"}}, String{u8"滚轮选择"}},
                 {MessageKey{
                      u8"gallery.copy.sample.636",
                      String{u8"Time range · Endpoint drafts, nullable values, order validation and submission"}},
                  String{u8"时间范围 · 端点草稿、可空、排序与整体提交"}},
                 {MessageKey{u8"gallery.copy.sample.637", String{u8"Toggle range authority"}},
                  String{u8"切换范围接受"}},
                 {MessageKey{u8"gallery.copy.sample.638",
                             String{u8"QR Model 2 · UTF-8/ECI · Four-module quiet zone · Native actions and theme"}},
                  String{u8"标准 QR Model 2 · UTF-8/ECI · 4 模块 quiet zone · 无网络请求"}},
                 {MessageKey{u8"gallery.copy.sample.639", String{u8"Hide/show actual refresh action"}},
                  String{u8"过期 / 显示真实刷新操作"}},
                 {MessageKey{u8"gallery.copy.sample.640", String{u8"Switch Active/Expired/Loading/Scanned"}},
                  String{u8"切换 Active/Expired/Loading/Scanned"}},
                 {MessageKey{u8"gallery.copy.sample.641", String{u8"Accept/reject refresh request"}},
                  String{u8"接受 / 拒绝刷新请求"}},
                 {MessageKey{u8"gallery.copy.sample.642", String{u8"Invalid error-correction level / restore"}},
                  String{u8"空内容错误 / 恢复"}},
                 {MessageKey{u8"gallery.copy.sample.643", String{u8"Owned RGBA logo / no logo"}},
                  String{u8"共享 RGBA logo / 无 logo"}},
                 {MessageKey{u8"gallery.copy.sample.644", String{u8"Export actual PNG + SVG"}},
                  String{u8"导出实际 PNG + SVG 数据"}},
                 {MessageKey{u8"gallery.copy.sample.645", String{u8"Text watermark"}}, String{u8"文字水印"}},
                 {MessageKey{u8"gallery.copy.sample.646", String{u8"Image watermark"}}, String{u8"图像水印"}},
                 {MessageKey{u8"gallery.copy.sample.647", String{u8"Start async load"}}, String{u8"开始异步加载"}},
                 {MessageKey{u8"gallery.copy.sample.648", String{u8"Complete load"}}, String{u8"完成加载"}},
                 {MessageKey{u8"gallery.copy.sample.649", String{u8"Fail load"}}, String{u8"返回失败"}},
                 {MessageKey{u8"gallery.copy.sample.650", String{u8"Rotate"}}, String{u8"旋转"}},
                 {MessageKey{u8"gallery.copy.sample.651", String{u8"Gap/offset"}}, String{u8"间距 / 偏移"}},
                 {MessageKey{u8"gallery.copy.sample.652", String{u8"Content height"}}, String{u8"内容高度"}},
                 {MessageKey{u8"gallery.copy.sample.653", String{u8"Content color"}}, String{u8"主题颜色"}},
                 {MessageKey{u8"gallery.copy.sample.654", String{u8"Watermark repeats over the actual content size; "
                                                                 u8"retained controls continue to edit normally."}},
                  String{u8"水印随实际内容尺寸重复，并保持子控件的原生命中。 "}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.sample.655", String{u8"Bind/release target"}},
                  String{u8"连接 / 释放目标"}},
                 {MessageKey{u8"gallery.copy.sample.656", String{u8"Switch target type"}}, String{u8"切换目标区域"}},
                 {MessageKey{u8"gallery.copy.sample.657", String{u8"Switch offset"}}, String{u8"切换偏移"}},
                 {MessageKey{u8"gallery.copy.sample.658", String{u8"Disable/enable"}}, String{u8"禁用 / 启用"}},
                 {MessageKey{
                      u8"gallery.copy.sample.659",
                      String{
                          u8"Affixed content retains natural document space. An explicit scroll container uses its "
                          u8"local placeholder and boundaries; releasing the target restores its original location."}},
                  String{u8"下面的内容保留自然文档空间。继续滚动可使顶部占位跨过固定边界；释放目标后恢复原位。 "}},
                 {MessageKey{
                      u8"gallery.copy.sample.660",
                      String{
                          u8"This example uses the Gallery's native document scroll domain, with no browser or DOM."}},
                  String{u8"本示例使用 Gallery 原生文档滚动，没有浏览器或 DOM。 "}},
                 {MessageKey{u8"gallery.copy.sample.661", String{u8"Bottom affixed button"}}, String{u8"底部固定按钮"}},
                 {MessageKey{u8"gallery.copy.sample.662", String{u8"Switch direction"}}, String{u8"切换方向"}},
                 {MessageKey{u8"gallery.copy.sample.663", String{u8"Cycle 2 s / 6 s"}}, String{u8"周期 2s / 6s"}},
                 {MessageKey{u8"gallery.copy.sample.664", String{u8"Theme colors / radius"}},
                  String{u8"主题颜色 / 圆角"}},
                 {MessageKey{u8"gallery.copy.sample.665", String{u8"Reduced motion"}}, String{u8"减少动态效果"}},
                 {MessageKey{u8"gallery.copy.sample.666", String{u8"Hide/show"}}, String{u8"隐藏 / 显示"}},
                 {MessageKey{u8"gallery.copy.sample.667",
                             String{u8"Reduced motion keeps a static border. Animation stops off "
                                    u8"page, while hidden, or when the window loses focus."}},
                  String{u8"减少动态效果时保留静态光束；隐藏、离页和窗口失活时停止运动。 "}},
                 {MessageKey{u8"gallery.copy.sample.668", String{u8"Leaving this group clears input; password and "
                                                                 u8"verification-code drafts are not retained."}},
                  String{u8"离开此分组会清空输入；不保留密码或验证码草稿。"}},
                 {MessageKey{u8"gallery.copy.sample.669", String{u8"Directory"}}, String{u8"目录"}},
                 {MessageKey{u8"gallery.copy.sample.670", String{u8"Theme"}}, String{u8"主题"}},
                 {MessageKey{u8"gallery.shell.directory", String{u8"Component directory"}}, String{u8"组件目录"}},
                 {MessageKey{u8"gallery.shell.components", String{u8"Components"}}, String{u8"组件"}},
                 {MessageKey{u8"gallery.shell.tokens", String{u8"Design Token"}}, String{u8"Design Token"}},
                 {MessageKey{u8"gallery.shell.theme", String{u8"Default / Dark"}}, String{u8"默认 / 深色"}},
                 {MessageKey{u8"gallery.shell.compact", String{u8"Compact"}}, String{u8"紧凑"}},
                 {MessageKey{u8"gallery.shell.settings", String{u8"Settings"}}, String{u8"设置"}},
                 {MessageKey{u8"gallery.shell.brand", String{u8"Brand color"}}, String{u8"品牌色"}},
                 {MessageKey{u8"gallery.shell.blue", String{u8"Blue"}}, String{u8"蓝色"}},
                 {MessageKey{u8"gallery.shell.purple", String{u8"Purple"}}, String{u8"紫色"}},
                 {MessageKey{u8"gallery.shell.green", String{u8"Green"}}, String{u8"绿色"}},
                 {MessageKey{u8"gallery.shell.motion", String{u8"Enable motion"}}, String{u8"启用动效"}},
                 {MessageKey{u8"gallery.shell.search", String{u8"Search components · Ctrl+K"}},
                  String{u8"搜索组件 · Ctrl+K"}},
                 {MessageKey{u8"gallery.shell.overview", String{u8"Component overview"}}, String{u8"组件总览"}},
                 {MessageKey{u8"gallery.shell.introduction", String{u8"Introduction"}}, String{u8"设计介绍"}},
                 {MessageKey{u8"gallery.shell.unmatched", String{u8"The current page is outside the filter results"}},
                  String{u8"当前页不在筛选结果中"}},
                 {MessageKey{u8"gallery.shell.empty", String{u8"No matching components"}},
                  String{u8"没有匹配的组件"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.shell.clear", String{u8"Clear filters"}}, String{u8"清除条件"}},
                 {MessageKey{u8"gallery.shell.back", String{u8"← Back"}}, String{u8"← 后退"}},
                 {MessageKey{u8"gallery.shell.forward", String{u8"Forward →"}}, String{u8"前进 →"}},
                 {MessageKey{u8"gallery.shell.native-scope", String{u8"Within the declared native scope"}},
                  String{u8"按声明的原生范围"}},
                 {MessageKey{
                      u8"gallery.shell.baseline",
                      String{u8"RynUI · Native C++ components and design conventions based on Ant Design 6.6.5"}},
                  String{u8"RynUI · 基于 Ant Design 6.6.5 的原生 C++ 组件与设计约定"}},
                 {MessageKey{u8"gallery.shell.enter", String{u8"Enter page content ↓"}}, String{u8"进入页面内容 ↓"}},
                 {MessageKey{u8"gallery.shell.status-note",
                             String{u8"Status follows the declared scope. Select a component for "
                                    u8"actual examples, public APIs and missing capabilities."}},
                  String{u8"状态来自声明范围，选择组件查看真实示例、公开 API 和缺失能力。"}},
                 {MessageKey{
                      u8"gallery.shell.theme-help",
                      String{u8"Default / Dark controls colors; Compact adds compact density. Use theme settings above "
                             u8"for brand color and motion."}},
                  String{u8"Default / Dark 决定颜色，Compact 可叠加为紧凑密度。品牌色与动效使用顶栏的主题设置。"}},
                 {MessageKey{u8"gallery.shell.api-note",
                             String{u8"Code cards show the actual example source. Public headers "
                                    u8"and component scope define the complete contract."}},
                  String{u8"代码卡展示实际样例源码；完整约束以公开头文件与组件支持范围为准。"}},
                 {MessageKey{u8"gallery.shell.supported", String{u8"Supported"}}, String{u8"已支持"}},
                 {MessageKey{u8"gallery.shell.unsupported", String{u8"Missing capabilities and scope boundaries"}},
                  String{u8"缺失与范围边界"}},
                 {MessageKey{u8"gallery.shell.no-scope", String{u8"No native capabilities have been declared."}},
                  String{u8"尚无已声明的原生能力。"}},
                 {MessageKey{u8"gallery.shell.no-missing",
                             String{u8"No additional entries; interpret the status within the declared scope."}},
                  String{u8"没有额外登记项；状态按声明范围解释。"}},
                 {MessageKey{u8"gallery.shell.component-tokens",
                             String{u8"Current effective component tokens (common fields)"}},
                  String{u8"当前有效组件 Token（常用字段）"}},
                 {MessageKey{u8"gallery.shell.global-tokens", String{u8"Current effective global tokens"}},
                  String{u8"当前有效全局 Token"}},
                 {MessageKey{u8"gallery.shell.view-tokens", String{u8"View global Design Token →"}},
                  String{u8"查看全局 Design Token →"}},
                 {MessageKey{u8"gallery.shell.sources", String{u8"Pinned version and reference sources"}},
                  String{u8"锁定版本与参考来源"}},
                 {MessageKey{u8"gallery.shell.source-note",
                             String{u8"Catalog declarations and source identities do not "
                                    u8"represent automatic acceptance on the current platform."}},
                  String{u8"目录声明与来源标识不代表当前平台的自动验收结果。"}},
                 {MessageKey{u8"gallery.shell.code", String{u8"C++ · View source"}}, String{u8"C++ · 查看源码"}},
                 {MessageKey{
                      u8"gallery.shell.code-note",
                      String{
                          u8"Actual example source; see the corresponding file for page state and helper functions."}},
                  String{u8"实际样例源码；页面状态与辅助函数见对应文件。"}},
                 {MessageKey{u8"gallery.shell.copy", String{u8"Copy code"}}, String{u8"复制代码"}},
                 {MessageKey{u8"gallery.shell.failed", String{u8"This page is temporarily unavailable"}},
                  String{u8"此页暂时无法显示"}},
                 {MessageKey{u8"gallery.shell.retry", String{u8"Retry"}}, String{u8"重试"}},
                 {MessageKey{u8"gallery.shell.return", String{u8"Return to component overview"}},
                  String{u8"返回组件总览"}},
                 {MessageKey{u8"gallery.shell.language", String{u8"Language: English / 中文"}},
                  String{u8"界面语言：中文 / English"}},
                 {MessageKey{u8"gallery.shell.format", String{u8"Format: English / 中文"}},
                  String{u8"格式语言：中文 / English"}},
                 {MessageKey{u8"gallery.shell.direction", String{u8"Layout direction: LTR / RTL"}},
                  String{u8"布局方向：LTR / RTL"}},
                 {MessageKey{u8"gallery.page.usage.title", String{u8"Usage"}}, String{u8"用途"}},
                 {MessageKey{u8"gallery.page.examples.title", String{u8"Examples"}}, String{u8"示例"}},
                 {MessageKey{u8"gallery.page.api.title", String{u8"C++ and API"}}, String{u8"C++ 与 API"}},
                 {MessageKey{u8"gallery.page.support.title", String{u8"Support scope"}}, String{u8"支持范围"}},
                 {MessageKey{u8"gallery.page.tokens.title", String{u8"Design Token"}}, String{u8"Design Token"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.page.sources.title", String{u8"Sources"}}, String{u8"来源"}},
                 {MessageKey{u8"gallery.status.all", String{u8"All statuses"}}, String{u8"全部状态"}},
                 {MessageKey{u8"gallery.status.implemented", String{u8"Implemented"}}, String{u8"已实现"}},
                 {MessageKey{u8"gallery.status.partial", String{u8"Partial"}}, String{u8"部分支持"}},
                 {MessageKey{u8"gallery.status.planned", String{u8"Planned"}}, String{u8"规划中"}},
                 {MessageKey{u8"gallery.status.web-only", String{u8"Web only"}}, String{u8"仅 Web"}},
                 {MessageKey{u8"gallery.status.deprecated", String{u8"Deprecated"}}, String{u8"已弃用"}},
                 {MessageKey{u8"gallery.status.out-of-scope", String{u8"Out of scope"}}, String{u8"不在范围"}},
                 {MessageKey{u8"gallery.family.input", String{u8"Input"}}, String{u8"单行输入"}},
                 {MessageKey{u8"gallery.family.search", String{u8"Search"}}, String{u8"搜索"}},
                 {MessageKey{u8"gallery.family.password", String{u8"Password"}}, String{u8"密码"}},
                 {MessageKey{u8"gallery.family.text-area", String{u8"TextArea"}}, String{u8"多行输入"}},
                 {MessageKey{u8"gallery.family.otp", String{u8"OTP"}}, String{u8"验证码"}},
                 {MessageKey{u8"gallery.api.methods",
                             String{u8"Common members: {value}"},
                             {{u8"value", MessageArgType::Text}}},
                  String{u8"常用成员：{value}"}},
                 {MessageKey{
                      u8"gallery.api.props", String{u8"Typed Props: {value}"}, {{u8"value", MessageArgType::Text}}},
                  String{u8"typed Props：{value}"}},
                 {MessageKey{
                      u8"gallery.api.slots", String{u8"Typed slots: {value}"}, {{u8"value", MessageArgType::Text}}},
                  String{u8"typed slots：{value}"}},
                 {MessageKey{u8"gallery.ant.component.button.name", String{u8"Button"}}, String{u8"Button 按钮"}},
                 {MessageKey{
                      u8"gallery.ant.component.button.summary",
                      String{u8"Native Button supports colors, variants, ghost/dashed borders, retained icons/loading "
                             u8"slots, shapes/block, focus refs, delayed loading and finite wave effects. Web APIs are "
                             u8"excluded; platform acceptance is recorded separately."}},
                  String{u8"原生 Button 已补齐颜色/变体/ghost、虚线、保留图标/加载 slots、形状/block、focus "
                         u8"ref/延迟加载与有限 "
                         u8"wave；Web 专用 API 不移植，平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.button.supported",
                      String{
                          u8"Default/Primary/Danger/Text types · Small/Middle/Large sizes · disabled and loading "
                          u8"priority · pointer/keyboard/focus-visible interaction · Theme component tokens and 16 "
                          u8"color / 6 variant composition · transparent ghost and rounded dashed borders · retained "
                          u8"start/end icon and custom loading slots / icon-only · Default/Circle/Round/Square shape "
                          u8"and block sizing · owner-thread ButtonRef, mount-only autoFocus, cancellable loading "
                          u8"delay · finite theme-driven wave with motion/window/destruction cancellation"}},
                  String{u8"Default/Primary/Danger/Text 类型与 Small/Middle/Large 尺寸；disabled/loading "
                         u8"优先级；指针、键盘及可见焦点；Theme Token、16 色与 6 变体组合；透明 "
                         u8"ghost、圆角虚线；保留首尾图标及自定义加载 slot、纯图标；Default/Circle/Round/Square 形状与 "
                         u8"block；所有者线程 ButtonRef、仅挂载时 autoFocus、可取消加载延迟；有限 wave "
                         u8"在减弱动画、窗口失活或销毁时取消。"}},
                 {MessageKey{
                      u8"gallery.ant.component.button.missing",
                      String{
                          u8"HTML href/target/htmlType, DOM refs, CSS classNames/styles and React compatibility "
                          u8"aliases are "
                          u8"out of native scope · Linux native GPU/font/input evidence remains independent pending"}},
                  String{u8"HTML href/target/htmlType、DOM ref、CSS classNames/styles 和 React "
                         u8"兼容别名不属原生范围；Linux 原生 "
                         u8"GPU/字体/输入证据仍独立待验。"}},
                 {MessageKey{u8"gallery.ant.component.float-button.name", String{u8"FloatButton"}},
                  String{u8"FloatButton 悬浮按钮"}},
                 {MessageKey{
                      u8"gallery.ant.component.float-button.summary",
                      String{u8"Native FloatButton, Group and BackTop provide typed Props/slots, window anchoring and "
                             u8"dragging, explicit scroll targets, theme and input lifetime."}},
                  String{u8"原生 FloatButton、Group 与 BackTop；typed "
                         u8"Props/slots、窗口定位与拖动、显式滚动目标、主题和输入生命周期。"}},
                 {MessageKey{u8"gallery.ant.component.float-button.supported",
                             String{u8"Default/primary, circle/square, icons/content, Tooltip configuration, Badge, "
                                    u8"disabled/click · Four "
                                    u8"window corners, optional dragging and PointerRouter capture cleanup · "
                                    u8"Persistent/click/hover "
                                    u8"groups, four directions and controlled authority, keyboard, hidden branches and "
                                    u8"retained identity "
                                    u8"· Explicit native scroll source/callbacks, threshold, progress ring, duration "
                                    u8"and reduced motion"}},
                  String{u8"默认/主色、圆形/方形、图标与内容、Tooltip 配置、Badge、禁用与点击 · "
                         u8"窗口四角浮动定位、可选拖动与 "
                         u8"PointerRouter 捕获清理 · 常驻、点击、悬停、四方向和受控 Group；键盘、隐藏分支与保留身份 · "
                         u8"显式原生滚动源与回调、阈值、进度环、时长和 reduced motion"}},
                 {MessageKey{
                      u8"gallery.ant.component.float-button.missing",
                      String{
                          u8"DOM/CSS/React, href/target and HTML form APIs are outside the native API · Windows/MSVC "
                          u8"and native GPU/window results are recorded separately and are not replaced by HEADLESS"}},
                  String{u8"DOM/CSS/React、href/target 和 HTML 表单接口不属于原生 API · Windows/MSVC 与原生 "
                         u8"GPU/窗口平台结果独立记录，不以 HEADLESS 代替"}},
                 {MessageKey{u8"gallery.ant.component.icon.name", String{u8"Icon"}}, String{u8"Icon 图标"}},
                 {MessageKey{
                      u8"gallery.ant.component.icon.summary",
                      String{
                          u8"All 848 offline icons are available in outlined, filled and two-tone styles, with typed "
                          u8"custom Bezier vectors, retained rotation and lifetime-aware spin. Platform acceptance is "
                          u8"recorded separately."}},
                  String{u8"RynUI 已提供完整 848 图标、outlined/filled/two-tone、typed "
                         u8"自定义贝塞尔向量、保留旋转与生命周期感知 "
                         u8"spin；原生桌面范围已实现，平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.icon.supported",
                      String{
                          u8"complete locked offline catalog: 447 outlined / 251 filled / 150 two-tone · Theme and "
                          u8"semantic foreground inheritance · reactive source/name and DPI-aware shared glyph raster "
                          u8"cache · retained color-role layers, explicit/derived two-tone colors · rotate/spin with "
                          u8"Theme/reduced motion/active-branch lifetime · immutable typed viewBox and "
                          u8"Move/Line/Quadratic/Cubic/Close vectors"}},
                  String{
                      u8"锁定的完整离线目录：447 outlined、251 filled、150 two-tone；Theme 与语义前景继承；响应式 "
                      u8"source/name 和感知 DPI 的共同字形光栅缓存；保留颜色角色图层、显式及派生双色；旋转与 spin 遵守 "
                      u8"Theme、减弱动画及活动分支生命周期；不可变 typed viewBox 与 Move/Line/Quadratic/Cubic/Close "
                      u8"向量。"}},
                 {MessageKey{
                      u8"gallery.ant.component.icon.missing",
                      String{u8"Web DOM/CSS, arbitrary SVG parser and network iconfont APIs are outside native scope"}},
                  String{u8"Web DOM/CSS、任意 SVG 解析器和网络 iconfont API 不属原生范围。"}},
                 {MessageKey{u8"gallery.ant.component.typography.name", String{u8"Typography"}},
                  String{u8"Typography 排版"}},
                 {MessageKey{
                      u8"gallery.ant.component.typography.summary",
                      String{u8"Typography provides five heading levels, Text, Paragraph, Link, immutable rich runs, "
                             u8"mixed-font Unicode layout, ellipsis, plain-text/HTML copy and multiline editing. Web "
                             u8"APIs are excluded; platform results remain separate."}},
                  String{u8"RynUI 提供五级 Title、Text、Paragraph、Link、不可变 typed rich runs、整段 Unicode "
                         u8"混合字体排版、省略展开、HTML/全文双格式复制及 TextArea 多行编辑；DOM/CSS/React 等 Web "
                         u8"专属接口不移植，平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.typography.supported",
                      String{
                          u8"typed Text props · CJK/Latin shaping and rendering · Theme typography tokens · retained "
                          u8"text scene · five heading levels and semantic colors · "
                          u8"strong/italic/code/keyboard/mark/underline/strikethrough · grapheme-safe ellipsis with "
                          u8"expand/collapse · full-content copy with independent late-bound clipboard · copy/edit "
                          u8"and actual-ellipsis Tooltip hints with reactive text · persistent TextArea editing with "
                          u8"autoSize, controlled draft and IME lifecycle · Link pointer/keyboard/focus-visible "
                          u8"interaction · immutable typed rich runs with combined semantic colors and decorations · "
                          u8"whole-source bidi, grapheme-safe mixed-font wrapping and ellipsis · escaped semantic "
                          u8"HTML plus original plain-text clipboard with platform-owned MIME lifetime"}},
                  String{u8"typed Text 属性、CJK/Latin 塑形渲染、Theme 排版 Token 与保留文本场景；五级标题、语义色及 "
                         u8"strong/italic/code/keyboard/mark/underline/"
                         u8"strikethrough；按字素安全省略及展开；独立晚绑定剪贴板复制全文；响应式复制/编辑及实际省略 "
                         u8"Tooltip；保留 "
                         u8"TextArea 的 autoSize、受控草稿和 IME；Link 指针/键盘/可见焦点；不可变富 runs "
                         u8"组合颜色与修饰；整段 "
                         u8"bidi、混合字体换行与省略；转义语义 HTML 和原始纯文本复制，MIME 生命周期归平台。"}},
                 {MessageKey{u8"gallery.ant.component.typography.missing",
                             String{u8"DOM/CSS/React props, arbitrary HTML input and URL navigation (Web-only)"}},
                  String{u8"DOM/CSS/React 属性、任意 HTML 输入和 URL 导航属于 Web 范围。"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.ant.component.divider.name", String{u8"Divider"}}, String{u8"Divider 分割线"}},
                 {MessageKey{
                      u8"gallery.ant.component.divider.summary",
                      String{u8"Native Divider supports horizontal/vertical Solid/Dashed/Dotted lines, sizes, "
                             u8"physical/logical title placement, length/fraction spacing and Theme. Web APIs and "
                             u8"platform acceptance are tracked separately."}},
                  String{u8"原生 Divider 已补齐水平/垂直 Solid/Dashed/Dotted、尺寸、物理/逻辑标题方位、长度/比例间距和 "
                         u8"Theme；Web 专用 API 不移植，平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.divider.supported",
                      String{
                          u8"horizontal/vertical and persistent typed text slot · left/center/right orientation and "
                          u8"explicit "
                          u8"margin precedence · dashed/plain variants and Theme component tokens · same-frame "
                          u8"label/rail "
                          u8"layout and material-only color updates · Solid/Dashed/Dotted with circular dots and "
                          u8"transparent "
                          u8"gaps · Small/Middle/Large Theme margins · physical Left/Right/Center and scoped LTR/RTL "
                          u8"Start/End · explicit logical length margins with bounded narrow constraints"}},
                  String{
                      u8"水平/垂直与保留 typed 文本 slot；左中右方位及显式边距优先；dashed/plain 和 Theme "
                      u8"Token；标签/线条同帧布局及仅材质颜色更新；Solid/Dashed/Dotted、圆点与透明间隙；三尺寸 Theme "
                      u8"边距；物理 Left/Right/Center 与作用域 LTR/RTL 的 Start/End；逻辑长度边距和有界窄屏约束。"}},
                 {MessageKey{
                      u8"gallery.ant.component.divider.missing",
                      String{u8"DOM refs, CSS styles/classNames and HTML/React compatibility aliases are out of native "
                             u8"scope · Linux native GPU/font/window evidence remains independent pending"}},
                  String{u8"DOM ref、CSS styles/classNames、HTML/React 兼容别名不属原生范围；Linux "
                         u8"GPU/字体/窗口仍独立待验。"}},
                 {MessageKey{u8"gallery.ant.component.flex.name", String{u8"Flex"}}, String{u8"Flex 弹性布局"}},
                 {MessageKey{
                      u8"gallery.ant.component.flex.summary",
                      String{u8"Native Flex supports default Stretch, real text baselines, WrapReverse, LTR/RTL, "
                             u8"physical alignment and flexible outer layout. Web-specific interfaces are excluded."}},
                  String{
                      u8"RynUI 已补齐原生 Flex 的默认 "
                      u8"Stretch、真实文字基线、WrapReverse、LTR/RTL、物理对齐与弹性外部布局；Web 专用接口不移植。"}},
                 {MessageKey{u8"gallery.ant.component.flex.supported",
                             String{u8"horizontal and vertical layout · align and justify · NoWrap/Wrap/WrapReverse · "
                                    u8"real text and "
                                    u8"control baselines with align-self · LTR/RTL and physical Left/Right alignment · "
                                    u8"default Stretch "
                                    u8"and native compatibility aliases · row/column gap · LayoutStyle integration"}},
                  String{u8"水平/垂直、align/justify、NoWrap/Wrap/WrapReverse；真实文本及控件基线与 "
                         u8"align-self；LTR/RTL 和物理 "
                         u8"Left/Right；默认 Stretch、原生兼容别名、行列 gap 与 LayoutStyle。"}},
                 {MessageKey{u8"gallery.ant.component.flex.missing",
                             String{u8"Web CSS strings, DOM/component and React attributes (out of native scope)"}},
                  String{u8"Web CSS 字符串、DOM/component 与 React 属性不属原生范围。"}},
                 {MessageKey{u8"gallery.ant.component.grid.name", String{u8"Grid"}}, String{u8"Grid 栅格"}},
                 {MessageKey{
                      u8"gallery.ant.component.grid.summary",
                      String{u8"Native Row/Col provides responsive 24-column layout, field-wise overrides, typed flex, "
                             u8"two-axis gutters, RTL and hidden-input lifetime."}},
                  String{u8"原生 Row/Col 支持窗口响应式二十四列、逐字段覆盖、typed flex、双轴 gutter、RTL "
                         u8"与隐藏输入生命周期。"}},
                 {MessageKey{
                      u8"gallery.ant.component.grid.supported",
                      String{u8"Typed RowProps/ColProps, RowContent/ColContent and reactive Prop<T> · "
                             u8"span/offset/order/push/pull, typed grow/shrink/basis and frozen constraints · Window "
                             u8"xs..xxxl field-wise overrides, responsive align/justify/gutter, wrap and RTL · span=0 "
                             u8"retains identity and blocks hits/focus/capture, with retained measurement/placement"}},
                  String{u8"typed RowProps/ColProps、RowContent/ColContent 与 reactive Prop<T> · "
                         u8"span/offset/order/push/pull、typed grow/shrink/basis 与约束冻结 · 窗口 xs..xxxl "
                         u8"逐字段覆盖，响应式 "
                         u8"align/justify/gutter、wrap、RTL · span=0 保留身份并阻断命中/焦点/捕获，稳定 retained "
                         u8"测量/放置"}},
                 {MessageKey{
                      u8"gallery.ant.component.grid.missing",
                      String{u8"Web DOM, CSS unit strings and React useBreakpoint are excluded; native window/GPU "
                             u8"acceptance remains separate"}},
                  String{
                      u8"DOM、CSS 单位字符串、React useBreakpoint 等 Web 专用接口不迁入；平台窗口/GPU 验收独立追踪"}},
                 {MessageKey{u8"gallery.ant.component.layout.name", String{u8"Layout"}}, String{u8"Layout 布局"}},
                 {MessageKey{
                      u8"gallery.ant.component.layout.summary",
                      String{u8"Layout/Header/Sider/ContentArea/Footer provide nested remaining-space allocation, "
                             u8"responsive sidebars, controlled collapse, accessible zero-width triggers and Theme "
                             u8"tokens. Platform integration is recorded separately."}},
                  String{u8"原生 Layout/Header/Sider/ContentArea/Footer "
                         u8"已提供嵌套剩余空间布局、窗口响应侧栏、受控折叠、零宽可访问触发器及 Theme "
                         u8"Token；平台集成独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.layout.supported",
                      String{
                          u8"typed Props/slots and reactive Prop<T>; retained nested shell and direct-Sider detection "
                          u8"· Header/Footer/content allocation and multiple light/dark Siders · window xs through "
                          u8"xxxl breakpoints with onBreakpoint/onCollapse · controlled/default collapsed, "
                          u8"width/collapsedWidth, reverse/custom/hidden/zero-width triggers · pointer/keyboard "
                          u8"focus, hidden input/capture/session cleanup and retained drafts · "
                          u8"ThemeConfig.layout.tokens and independent canonical Gallery examples"}},
                  String{u8"typed Props/slots 与 reactive Prop<T>；保留嵌套结构并检测直属 "
                         u8"Sider；Header/Footer/正文分配及多个浅/深色侧栏；窗口 xs..xxxl "
                         u8"断点、onBreakpoint/onCollapse；受控/默认折叠、width/collapsedWidth、反向/自定义/隐藏/"
                         u8"零宽触发器；指针、键盘焦点及隐藏输入/捕获/会话清理、草稿保留；ThemeConfig.layout.tokens "
                         u8"与独立 "
                         u8"Gallery 样例。"}},
                 {MessageKey{
                      u8"gallery.ant.component.layout.missing",
                      String{
                          u8"DOM/CSS/SSR and Web-only fixed positioning are excluded from the native API · native GPU "
                          u8"and Windows integration are tracked separately in change106"}},
                  String{u8"DOM/CSS/SSR 和 Web 固定定位不属原生 API；原生 GPU 与 Windows 集成按 change106 独立追踪。"}},
                 {MessageKey{u8"gallery.ant.component.masonry.name", String{u8"Masonry"}}, String{u8"Masonry 瀑布流"}},
                 {MessageKey{u8"gallery.ant.component.masonry.summary",
                             String{u8"Native Masonry provides typed keyed data, shortest-column placement based on "
                                    u8"actual measured "
                                    u8"height, responsive columns/gutters, explicit columns/heights and safe "
                                    u8"incremental lifetime."}},
                  String{u8"原生 Masonry 支持 typed keyed 数据、真实高度最短列、响应式 "
                         u8"columns/gutter、指定列/高度及安全的增量生命周期。"}},
                 {MessageKey{u8"gallery.ant.component.masonry.supported",
                             String{u8"MasonryProps<Data>/MasonryItem<Data>, typed itemRender and higher-priority "
                                    u8"children slot · "
                                    u8"Stable String/integer keys, add/remove/reorder and retained data/index/column "
                                    u8"bindings · Shared "
                                    u8"responsive window breakpoints, columns/two-axis gutters, RTL, explicit "
                                    u8"column/logical height · "
                                    u8"Measured shortest-column placement, fresh/explicit refresh, onLayoutChange only "
                                    u8"when allocation "
                                    u8"changes · Invalid keys/columns/sizes rejected, destruction and failed "
                                    u8"new-branch rollback"}},
                  String{u8"MasonryProps<Data>/MasonryItem<Data>、typed itemRender 与优先 children slot · 稳定 "
                         u8"String/integer "
                         u8"key、增删/排序与 retained 数据/index/column bindings · 共享窗口断点响应式 columns/双轴 "
                         u8"gutter、RTL、指定列与逻辑高度 · 实际尺寸最短列重排、fresh/显式刷新、仅分配变化 "
                         u8"onLayoutChange · "
                         u8"无效键/列数/尺寸显式拒绝、销毁及新增分支失败回滚"}},
                 {MessageKey{
                      u8"gallery.ant.component.masonry.missing",
                      String{
                          u8"Web DOM, CSS semantic styles/classNames and React nodes are excluded; native window/GPU "
                          u8"acceptance remains separate"}},
                  String{u8"DOM、CSS semantic styles/classNames、React nodes 等 Web 专用接口不迁入；平台窗口/GPU "
                         u8"验收独立追踪"}},
                 {MessageKey{u8"gallery.ant.component.space.name", String{u8"Space"}}, String{u8"Space 间距"}},
                 {MessageKey{
                      u8"gallery.ant.component.space.summary",
                      String{u8"Space provides native desktop layout, rich separators, connected Compact controls and "
                             u8"four "
                             u8"Addon variants. Common and Windows/Linux native acceptance are recorded separately."}},
                  String{u8"RynUI 已补齐 Space 原生桌面布局、富分隔、Compact 连接控件与四变体 Addon；平台通用与 "
                         u8"Windows/Linux "
                         u8"原生验收分别记录。"}},
                 {MessageKey{u8"gallery.ant.component.space.supported",
                             String{u8"horizontal and vertical spacing · typed size and custom logical gap · wrap · "
                                    u8"typed content "
                                    u8"composition, native Auto/Baseline and orientation/RTL · retained rich "
                                    u8"separator/split with "
                                    u8"interleaved layout/scene/focus order · Compact H/V/RTL, three sizes, block, "
                                    u8"nested groups and "
                                    u8"explicit-size priority · Button/Input/Password/Search/RadioButton connected "
                                    u8"exterior corners "
                                    u8"and state seams · retained editing/IME/selection/ref identity and native "
                                    u8"clipboard · SpaceAddon "
                                    u8"typed content, four variants, disabled/status and Theme"}},
                  String{
                      u8"水平/垂直间距、typed size 与逻辑 gap、换行；typed 内容组合、Auto/Baseline、方向与 RTL；保留富 "
                      u8"separator/split，布局/场景/焦点顺序交错；Compact "
                      u8"横纵/RTL、三尺寸、block、嵌套及显式尺寸优先；Button/Input/Password/Search/RadioButton "
                      u8"连接外圆角和状态接缝；保留编辑、IME、选区、ref 与原生剪贴板；SpaceAddon 的 typed "
                      u8"内容、四变体、禁用/状态与 Theme。"}},
                 {MessageKey{u8"gallery.ant.component.space.missing",
                             String{u8"Web DOM, CSS strings, React component/HTML attributes (out of native scope)"}},
                  String{u8"Web DOM、CSS 字符串、React component/HTML 属性不属原生范围。"}},
                 {MessageKey{u8"gallery.ant.component.splitter.name", String{u8"Splitter"}},
                  String{u8"Splitter 分隔面板"}},
                 {MessageKey{
                      u8"gallery.ant.component.splitter.summary",
                      String{u8"Native Splitter provides multiple panels, logical/percentage sizes, controlled/default "
                             u8"values, min/max, pointer/keyboard input, two-sided collapse, actual lazy preview, "
                             u8"hidden-content policies and motion. Platform integration is recorded separately."}},
                  String{u8"原生 Splitter "
                         u8"已提供多面板、逻辑/百分比尺寸、受控与默认值、min/max、指针/键盘、双侧折叠、真实 lazy "
                         u8"预览、隐藏内容策略和动画；平台集成独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.splitter.supported",
                      String{
                          u8"typed SplitterProps/Panel/size/slots and reactive properties; horizontal/vertical/RTL and "
                          u8"nested panels · bounded finite multi-panel solver; controlled/default logical and "
                          u8"percentage "
                          u8"sizes · PointerRouter capture, keyboard arrows/Home/End, resize start/change/end and "
                          u8"double-click callbacks · two-side collapse/restore, icon visibility and custom "
                          u8"dragger/collapse "
                          u8"icons · lazy logical-scene preview with release-only commit; cancellation and "
                          u8"generation-safe "
                          u8"callbacks · global/per-panel destroyOnHidden with retained drafts or new content "
                          u8"generation · "
                          u8"ThemeConfig.splitter.tokens, optional collapse motion and reduced-motion behavior · "
                          u8"independent "
                          u8"canonical Gallery samples and real page/input/teardown contracts"}},
                  String{u8"typed SplitterProps/Panel/size/slots "
                         u8"与响应属性；横纵/RTL、嵌套；有限多面板求解、受控/默认逻辑及百分比尺寸；PointerRouter "
                         u8"捕获、方向键/Home/End、调整开始/变化/结束及双击回调；双侧折叠恢复、图标可见性和自定义拖动/"
                         u8"折叠图标；lazy 逻辑场景预览仅释放时提交、取消和代际守卫；全局/逐面板 "
                         u8"destroyOnHidden，保留草稿或创建新一代内容；Theme Token、有限折叠和减弱动画；独立 Gallery "
                         u8"与真实页面/输入/退役合同。"}},
                 {MessageKey{
                      u8"gallery.ant.component.splitter.missing",
                      String{u8"DOM/CSS/React and deprecated Web aliases are excluded from the native API · native GPU "
                             u8"and Windows integration are tracked separately in change106"}},
                  String{u8"DOM/CSS/React 和废弃 Web 别名不属原生 API；GPU 与 Windows 集成按 change106 独立追踪。"}},
                 {MessageKey{u8"gallery.ant.component.anchor.name", String{u8"Anchor"}}, String{u8"Anchor 锚点"}},
                 {MessageKey{u8"gallery.ant.component.anchor.summary",
                             String{u8"Anchor provides explicit scroll state/requests, dynamic keyed hierarchy, "
                                    u8"horizontal/vertical "
                                    u8"layout, active tracking, offsets/bounds, affixed/static modes and cancelable "
                                    u8"smooth scrolling."}},
                  String{u8"原生 Anchor "
                         u8"提供显式滚动状态/请求、动态键控层级、横纵方向、活动跟踪、偏移/"
                         u8"边界、固定及静态模式和可取消平滑滚动。"}},
                 {MessageKey{u8"gallery.ant.component.anchor.supported",
                             String{u8"typed reactive keyed hierarchy and retained rich title slots; "
                                    u8"append/remove/reorder/update without "
                                    u8"unrelated remounts · explicit native ScrollState compatible with BackTop, host "
                                    u8"scroll requests, "
                                    u8"tracking bounds and global/per-item target offsets · vertical/horizontal "
                                    u8"projection, affix/static "
                                    u8"placeholder and active ink; controlled active key and click/change callbacks · "
                                    u8"finite monotonic "
                                    u8"smooth scrolling, bottom clamping, reduced motion, "
                                    u8"user/visibility/input-boundary cancellation and "
                                    u8"callback destruction/exception safety · independent interactive Gallery with "
                                    u8"real clipped content, "
                                    u8"scroll slider, direction and topology controls; dedicated Theme family"}},
                  String{u8"typed 响应式键控层级，保留富标题，增删重排更新不重挂无关内容；兼容 BackTop 的原生 "
                         u8"ScrollState、宿主滚动请求、跟踪边界与全局/逐项偏移；横纵投影、固定/"
                         u8"静态占位与活动标记，受控 key "
                         u8"和点击/变化回调；单调时钟平滑滚动、底部钳制、减弱动画及用户/可见性/"
                         u8"输入边界取消，回调销毁和异常安全；独立 Gallery 的裁剪正文、滚动滑条、方向/拓扑操作及专属 "
                         u8"Theme。"}},
                 {MessageKey{
                      u8"gallery.ant.component.anchor.missing",
                      String{u8"URL/hash/history, DOM/CSS/React and browser routing APIs are outside native scope · "
                             u8"native GPU/window, Linux platform, Windows MSVC, system IME and assistive technology "
                             u8"evidence remain independent"}},
                  String{u8"URL/hash/history、DOM/CSS/React 和浏览器路由不属原生范围；GPU/窗口、Linux、Windows "
                         u8"MSVC、系统 IME "
                         u8"和辅助技术分别记录。"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.ant.component.breadcrumb.name", String{u8"Breadcrumb"}},
                  String{u8"Breadcrumb 面包屑"}},
                 {MessageKey{
                      u8"gallery.ant.component.breadcrumb.summary",
                      String{
                          u8"Breadcrumb supports reactive keyed topology, retained rich titles/icons, per-item "
                          u8"separators, "
                          u8"built-in Dropdown/Menu, wrapping and RTL. Activation reports native key/index events."}},
                  String{
                      u8"原生 Breadcrumb 支持响应式键控拓扑、保留富标题/图标、每项分隔符、内置 Dropdown/Menu、换行与 "
                      u8"RTL；激活使用原生 key/index 事件。"}},
                 {MessageKey{
                      u8"gallery.ant.component.breadcrumb.supported",
                      String{
                          u8"typed reactive items with unique keys, current last-visible item and original-index "
                          u8"activation "
                          u8"· retained rich title/icon/separator slots with independent native child input · pointer "
                          u8"and "
                          u8"Enter/Space navigation, disabled/hidden/Modal/Spin/window input gates · hidden-item flow "
                          u8"removal and identity restoration, finite glyph/surface/hit clipping and external focus "
                          u8"outlines "
                          u8"· component Theme inheritance/algorithm/overrides, final validation and owned input-block "
                          u8"teardown · interactive Gallery trail and production catalog anchor, original key/index "
                          u8"feedback, reactive rename/visibility/disabled/restore, retained current and no-handler "
                          u8"native "
                          u8"child editing; Recording Default/Dark/Compact at 1200/560 x 900 · reactive keyed topology "
                          u8"append/remove/reorder/update; preserved native editor drafts and cleanup of "
                          u8"removed/replaced "
                          u8"popup/focus/capture · built-in current MenuProps/DropdownProps composition and native "
                          u8"menu "
                          u8"callbacks, per-item separator/activation/dropdown icon · container-width wrapping and "
                          u8"LTR/RTL "
                          u8"mirroring without remounts; existing vector API remains callable · independent dynamic "
                          u8"Gallery "
                          u8"trail with menu, retained editor, keyed reorder/remove/restore and RTL controls"}},
                  String{u8"typed 响应式唯一键条目、最后可见项为当前项、回调保留原索引；标题/图标/分隔富 slot "
                         u8"与独立原生子输入；指针及 "
                         u8"Enter/Space、禁用/隐藏/Modal/Spin/窗口门控；隐藏移出流与身份恢复、有限字形/表面/"
                         u8"命中裁剪及外侧焦点线；完整 Theme 和输入块退役；Gallery "
                         u8"导航、改名/显隐/禁用/恢复、保留编辑与原始 "
                         u8"key/index 反馈；Recording "
                         u8"三主题、1200/560×900；键控增删重排、草稿保留及被移除弹层/焦点/捕获清理；原生 "
                         u8"Menu/Dropdown 组合和逐项分隔/激活图标；容器换行、LTR/RTL 镜像及旧 vector API。"}},
                 {MessageKey{
                      u8"gallery.ant.component.breadcrumb.missing",
                      String{
                          u8"router/href/params, DOM/CSS/React and browser routing are outside native scope · native "
                          u8"GPU/window, Linux platform, Windows MSVC, system IME and assistive technology evidence "
                          u8"remain independent"}},
                  String{u8"router/href/params、DOM/CSS/React 和浏览器路由不属原生范围；GPU/窗口、Linux、Windows "
                         u8"MSVC、系统 IME "
                         u8"及辅助技术分别记录。"}},
                 {MessageKey{u8"gallery.ant.component.dropdown.name", String{u8"Dropdown"}},
                  String{u8"Dropdown 下拉菜单"}},
                 {MessageKey{
                      u8"gallery.ant.component.dropdown.summary",
                      String{u8"Dropdown provides hover/click/context-menu combinations, typed split buttons, retained "
                             u8"custom popups, arrows/window placement, explicit controlled request causes, child "
                             u8"ownership and focus restoration. Platform results remain separate."}},
                  String{u8"原生 Dropdown 已实现 hover/点击/右键组合、typed 分离按钮、保留自定义 "
                         u8"popup、箭头与窗口定位、来源明确的受控请求、子浮层所有权和焦点恢复；平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.dropdown.supported",
                      String{u8"hover/click/context-menu combinations with bounded delays, native activation gates and "
                             u8"controlled source/cause events · typed split Button props/slots and retained popup "
                             u8"renderer with optional native Menu content · twelve-direction placement/overflow, "
                             u8"token-driven unified arrow fill, child popup/input ownership and focus restoration"}},
                  String{
                      u8"悬停/点击/右键组合、有界延迟、原生激活门控与来源明确的受控事件；typed 分离 Button "
                      u8"属性/slot、保留 "
                      u8"popup renderer 与可选原生 Menu；十二方位/溢出、Token 箭头填充、子弹层与输入归属及焦点恢复。"}},
                 {MessageKey{
                      u8"gallery.ant.component.dropdown.missing",
                      String{
                          u8"native Windows/Linux GPU, driver, system-font/DPI and input acceptance remain "
                          u8"independent · Web DOM/CSS/React APIs, cross-window OS context menus and OS accessibility "
                          u8"adapter are outside this native API"}},
                  String{u8"Windows/Linux 原生 GPU、驱动、系统字体/DPI 和输入验收独立；Web DOM/CSS/React、跨窗口 OS "
                         u8"上下文菜单及 OS 辅助技术适配器不属本 API。"}},
                 {MessageKey{u8"gallery.ant.component.menu.name", String{u8"Menu"}}, String{u8"Menu 导航菜单"}},
                 {MessageKey{
                      u8"gallery.ant.component.menu.summary",
                      String{u8"Menu provides dynamic stable keys, three modes, horizontal overflow, vertical flyouts, "
                             u8"collapse/RTL, themes, multiple selection and keyboard/scroll interaction. Gallery has "
                             u8"actual interactions; platform results remain separate."}},
                  String{u8"原生 Menu 已实现动态稳定 key、三种模式、横向溢出、纵向 "
                         u8"flyout、折叠/RTL、主题、多选及完整键盘与滚动；Gallery 提供真实交互，平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.menu.supported",
                      String{u8"reactive keyed topology with retained rich labels/icons/extra content and predictable "
                             u8"removal cleanup · Horizontal overflow popup, Vertical flyouts, Inline/collapsed "
                             u8"navigation, typed theme/RTL · controlled selection/open authority, multiple selection, "
                             u8"pointer/keyboard/scroll, finite hover delays and lifecycle cancellation"}},
                  String{u8"响应式键控拓扑，保留富标签/图标/extra，删除时明确清理；Horizontal 溢出 popup、Vertical "
                         u8"flyout、Inline/折叠、typed 主题与 "
                         u8"RTL；受控选择/展开、多选、指针/键盘/滚动、有界悬停延迟及生命周期取消。"}},
                 {MessageKey{u8"gallery.ant.component.menu.missing",
                             String{u8"native Windows/Linux GPU, driver, system-font/DPI and input acceptance remain "
                                    u8"independent · Web "
                                    u8"DOM/CSS/React APIs and OS accessibility adapter are outside this native API"}},
                  String{u8"Windows/Linux 原生 GPU、驱动、系统字体/DPI 和输入验收独立；Web DOM/CSS/React 与 OS "
                         u8"辅助技术适配器不属本 API。"}},
                 {MessageKey{u8"gallery.ant.component.pagination.name", String{u8"Pagination"}},
                  String{u8"Pagination 分页"}},
                 {MessageKey{
                      u8"gallery.ant.component.pagination.summary",
                      String{
                          u8"Pagination supports page size, quick jump, simple mode, responsive alignment, typed item "
                          u8"content and totals. Controlled requests, invalid input and shrinking bounds have Linux "
                          u8"HEADLESS contracts; native results remain separate."}},
                  String{u8"原生 Pagination 已补齐页大小、快速跳页、simple、响应式对齐、typed item "
                         u8"内容与总数；受控请求、非法输入和收缩边界通过 Linux HEADLESS 合同，平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.pagination.supported",
                      String{
                          u8"controlled/default current page, total and page size, page/jump buttons · reactive "
                          u8"updates, "
                          u8"disabled state, hide-on-single-page layout and Table composition · controlled/default "
                          u8"page and "
                          u8"pageSize with independent authority and size/change events · native Select size changer, "
                          u8"validated quick jump and read-only/simple mode · shared window responsive size, "
                          u8"alignment, "
                          u8"total ranges and retained typed item rendering · disabled/hide-on-single-page, "
                          u8"clamp/empty/overflow arithmetic and callback destruction cleanup"}},
                  String{u8"受控/默认当前页、总量、页大小与页码/跳转按钮；响应更新、禁用、单页隐藏及 Table "
                         u8"组合；page/pageSize "
                         u8"独立受控权威与尺寸/变化事件；原生 Select "
                         u8"页大小、验证快速跳页和只读/simple；共享窗口响应尺寸、对齐、范围总数与保留 typed "
                         u8"item；钳制/空值/溢出算术及回调销毁清理。"}},
                 {MessageKey{u8"gallery.ant.component.pagination.missing",
                             String{u8"native Linux GPU/window/input/system IME/accessibility and actual Windows MSVC "
                                    u8"acceptance remain independent category integration tasks · Web DOM/CSS/React "
                                    u8"compatibility APIs are outside native scope"}},
                  String{u8"Linux 原生 GPU/窗口/输入/系统 IME/辅助技术及真实 Windows MSVC 验收独立追踪；Web "
                         u8"DOM/CSS/React 兼容 "
                         u8"API 不属原生范围。"}},
                 {MessageKey{u8"gallery.ant.component.steps.name", String{u8"Steps"}}, String{u8"Steps 步骤条"}},
                 {MessageKey{
                      u8"gallery.ant.component.steps.summary",
                      String{u8"Steps supports dynamic keyed topology, Dot/Inline/Navigation/Panel, percent/maxCount, "
                             u8"responsive/RTL layout and finite theme motion. Rich content retains identity; platform "
                             u8"results remain separate."}},
                  String{u8"原生 Steps 已补齐动态键控结构、Dot/Inline/Navigation/Panel、percent/maxCount、响应式/RTL "
                         u8"与有限主题动画；富内容身份保留，平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.steps.supported",
                      String{
                          u8"typed reactive keyed items, controlled/default current and original-index callbacks · "
                          u8"Wait/Process/Finish/Error state projection, item overrides, native number/check/error and "
                          u8"connectors · horizontal/vertical layouts and Default/Small sizes with retained rich "
                          u8"title/subtitle/description/icon slots · roving Tab and directional/Home/End focus, "
                          u8"explicit "
                          u8"Enter/Space/pointer activation and controlled rejection/retry · independent native child "
                          u8"input, "
                          u8"disabled/hidden/Modal/Spin/window gates and owned input-block teardown · finite "
                          u8"glyph/surface/hit clipping, visible external focus outlines and complete component Theme "
                          u8"resolution · interactive controlled Gallery workflow and production catalog anchor, "
                          u8"next/previous/reset, direction/size/status/visibility/disabled controls, native retained "
                          u8"description editor and keyboard requests; Recording Default/Dark/Compact at 1200/560 x "
                          u8"900 · "
                          u8"dynamic keyed append/remove/reorder with retained rich slots, current-key preservation "
                          u8"and "
                          u8"exception rollback · Default/Dot/Inline/Navigation/Panel logical visuals, retained inline "
                          u8"detail popup and finite percent ring · maxCount disabled ellipses, responsive "
                          u8"orientation, RTL "
                          u8"layout and directional keyboard navigation · finite Theme activation motion, "
                          u8"reduced/inactive/hidden cancellation and native input cleanup"}},
                  String{u8"typed 响应式键控条目、受控/默认 current "
                         u8"与原始索引回调；Wait/Process/Finish/Error、逐项覆盖及原生数字/对勾/错误/"
                         u8"连线；横纵、两尺寸及保留富标题/副标题/说明/图标；游走 "
                         u8"Tab、方向/Home/End、Enter/Space/指针，受控拒绝重试；子输入及禁用/隐藏/Modal/Spin/"
                         u8"窗口门控和退役；有限裁剪、外侧焦点线及完整 Theme；Gallery 实际流程及 Recording 三主题 "
                         u8"1200/560×900；键控增删重排、当前 key "
                         u8"保留、失败回滚；Default/Dot/Inline/Navigation/Panel、保留详情 "
                         u8"popup、有限百分比环；maxCount 禁用省略、响应方向/RTL 键盘；有限 Theme "
                         u8"动画与减弱/失活/隐藏取消。"}},
                 {MessageKey{u8"gallery.ant.component.steps.missing",
                             String{u8"native Linux GPU/window/input/system IME/accessibility and actual Windows MSVC "
                                    u8"acceptance remain independent category integration tasks · Web DOM/CSS/React "
                                    u8"compatibility APIs are outside native scope"}},
                  String{u8"Linux 原生 GPU/窗口/输入/系统 IME/辅助技术及真实 Windows MSVC 验收独立追踪；Web "
                         u8"DOM/CSS/React 兼容 "
                         u8"API 不属原生范围。"}},
                 {MessageKey{u8"gallery.ant.component.tabs.name", String{u8"Tabs"}}, String{u8"Tabs 标签页"}},
                 {MessageKey{
                      u8"gallery.ant.component.tabs.summary",
                      String{u8"Tabs supports dynamic keyed editing, lazy/destroy policies, four placements/RTL, rich "
                             u8"labels/extra content, overflow menus, custom indicators and finite motion. Unchanged "
                             u8"panel drafts remain; platform results remain separate."}},
                  String{u8"原生 Tabs 已补齐动态键控编辑、lazy/destroy、四方向/RTL、富标签/extra、溢出菜单与定制 "
                         u8"indicator/有限动画；保留未变面板草稿，平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.tabs.supported",
                      String{
                          u8"typed reactive Props/TabItem labels/disabled and retained TabContent panels · "
                          u8"controlled/default String keys, disabled navigation, single Tab entry and "
                          u8"pointer/keyboard "
                          u8"activation · Line/Card, three sizes, Top/Bottom, centered and clipped arrow/wheel "
                          u8"overflow "
                          u8"navigation · retained Input/Select state, hidden panel focus/IME/popup cleanup and Tabs "
                          u8"Component Token · dynamic keyed add/remove/reorder, controlled rejection and "
                          u8"deterministic "
                          u8"active removal fallback · editable-card add/close/Delete edit requests, closable/hideAdd "
                          u8"and "
                          u8"retained rich labels/extra slots · explicit lazy/forceRender/destroyOnHidden with hidden "
                          u8"focus/capture/IME/popup cleanup and factory rollback · Top/Bottom/Left/Right, RTL "
                          u8"keyboard/wheel/layout, clipped arrows and native Menu/Dropdown overflow · custom "
                          u8"indicator "
                          u8"size/alignment, finite Theme activation motion and reduced/inactive cleanup"}},
                  String{
                      u8"typed 响应属性/标签/禁用与保留面板；受控/默认 String key、禁用导航、单一 Tab "
                      u8"入口与指针/键盘激活；Line/Card、三尺寸、上下方位、居中、裁剪箭头/滚轮溢出；保留 "
                      u8"Input/Select、隐藏焦点/IME/弹层清理与 "
                      u8"Token；键控增删重排、拒绝及活动项移除回退；editable-card "
                      u8"新增/关闭/Delete、closable/hideAdd、富标签/extra；lazy/forceRender/"
                      u8"destroyOnHidden、隐藏捕获清理与工厂回滚；四方位、RTL、原生 Menu/Dropdown 溢出；定制 indicator "
                      u8"尺寸/对齐及有限 Theme 动画和减弱/失活清理。"}},
                 {MessageKey{u8"gallery.ant.component.tabs.missing",
                             String{u8"native Linux GPU/window/input/system IME/accessibility and actual Windows MSVC "
                                    u8"acceptance remain independent category integration tasks · Web DOM/CSS/React "
                                    u8"compatibility APIs are outside native scope"}},
                  String{u8"Linux 原生 GPU/窗口/输入/系统 IME/辅助技术及真实 Windows MSVC 验收独立追踪；Web "
                         u8"DOM/CSS/React 兼容 "
                         u8"API 不属原生范围。"}},
                 {MessageKey{u8"gallery.ant.component.auto-complete.name", String{u8"AutoComplete"}},
                  String{u8"AutoComplete 自动完成"}},
                 {MessageKey{
                      u8"gallery.ant.component.auto-complete.summary",
                      String{
                          u8"AutoComplete provides free editing, stable grouped/rich suggestions, windowing, "
                          u8"cancelable application async sources, custom Input/TextArea/popup, RTL/finite motion and "
                          u8"explicit Form reset. New platform acceptance remains separate."}},
                  String{u8"原生自由输入与完整建议合同：稳定分组/富内容、窗口列表、可取消应用异步源、自定义 "
                         u8"Input/TextArea 和 "
                         u8"popup、RTL/有限动画及显式 Form reset；新增平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.auto-complete.supported",
                      String{u8"Typed/reactive String, dynamic keyed/grouped/rich suggestions, "
                             u8"filtering/backfill/clear/native "
                             u8"keyboard · Controlled value/open, windowed ordinary rows, retained rich controls, "
                             u8"focus/capture/IME and child-popup boundaries · Application provider/owner-thread "
                             u8"dispatcher, "
                             u8"last-query/lifetime/generation cancellation, errors and reset · Retained "
                             u8"Input/TextArea and "
                             u8"popup renderer, RTL, finite Theme motion and width · Existing contact/note/form "
                             u8"examples plus "
                             u8"grouped long lists, custom editors and explicitly synthetic async examples · Linux "
                             u8"private "
                             u8"Wayland/Vulkan/SPIR-V/system-font bounded input, input-area/cursor services and "
                             u8"retirement; "
                             u8"async Gallery uses a synthetic provider"}},
                  String{u8"typed/reactive String、动态 keyed/grouped/rich 建议、过滤、backfill、清空与原生键盘 · 受控 "
                         u8"value/open、窗口化普通行、保留富子控件、focus/capture/IME 与子 popup 边界 · 应用 "
                         u8"provider/owner-thread dispatcher、last-query/lifetime/generation 取消、错误处理和 reset · "
                         u8"retained "
                         u8"Input/TextArea editor 与 popup renderer、RTL、Theme 有限动画和宽度 · "
                         u8"保留联系人/备注/表单示例，新增分组长列表、自定义编辑器和明确标注的合成异步 Gallery · Linux "
                         u8"私有 "
                         u8"Wayland/Vulkan/SPIR-V/system fonts 有限自动输入、宿主 input area/cursor 与离页清理；异步 "
                         u8"Gallery "
                         u8"使用合成 provider"}},
                 {MessageKey{
                      u8"gallery.ant.component.auto-complete.missing",
                      String{
                          u8"Physical IME candidates, actual cross-monitor/DPI, assistive technology and Windows MSVC "
                          u8"native windows remain unverified; the data-entry family is not fully closed · Web "
                          u8"DOM/CSS/React/portal and browser-event compatibility aliases are outside typed Core"}},
                  String{u8"系统 IME 候选 UI、跨实际显示器/DPI、辅助技术和 Windows MSVC "
                         u8"原生窗口仍未验证；数据录入分类尚未整体收口 "
                         u8"· Web DOM/CSS/React/portal 与浏览器事件兼容别名不属于 typed Core"}},
                 {MessageKey{u8"gallery.ant.component.cascader.name", String{u8"Cascader"}},
                  String{u8"Cascader 级联选择"}},
                 {MessageKey{
                      u8"gallery.ant.component.cascader.summary",
                      String{u8"Cascader provides native multi-column single/multiple selection, retained tags, "
                             u8"independent "
                             u8"Theme, Unicode search, application loaders, Panel and input-lifetime contracts."}},
                  String{u8"原生 Cascader 完成多列单选/多选、保留标签、独立 Theme、Unicode 搜索、应用加载器、Panel "
                         u8"和输入生命周期合同。"}},
                 {MessageKey{
                      u8"gallery.ant.component.cascader.supported",
                      String{
                          u8"String/number typed paths, controlled/default value/open/expandedPath/searchValue and "
                          u8"current "
                          u8"authority · Actual removable/custom retained tags, responsive maxTagCount, grapheme "
                          u8"truncation, "
                          u8"custom placeholder and actionable overflow · Stable keys/content_revision, retained Input "
                          u8"drafts, dynamic validation and full retirement on failure/reentry · Independently clipped "
                          u8"columns/search/status, precise wheel boundary handoff, RTL, Home/End/arrows/Page and "
                          u8"rich-child "
                          u8"reveal · Unicode-normalized search, filter/sort/render/limit, selector/search Core IME, "
                          u8"nested "
                          u8"popup and focus-leave cleanup · Explicit application loader, owner-thread completion, "
                          u8"atomic "
                          u8"cancellation tickets, errors/Retry and rejection of old/duplicate/foreign-thread results "
                          u8"· "
                          u8"Independent Cascader tokens, seed/algorithm/inherit, four invalidation domains, "
                          u8"size/status/variant/placement and typed refs/events/slots · Actual Gallery "
                          u8"tag/Theme/scroll/search/loader examples and bounded Linux private Wayland/Vulkan "
                          u8"acceptance"}},
                  String{u8"String/number typed paths，受控/default value、open、expandedPath、searchValue "
                         u8"及当前受控权威 · "
                         u8"真实可移除/自定义保留 tags，responsive maxTagCount、grapheme "
                         u8"截断、自定义占位符和可操作溢出浮层 · "
                         u8"稳定 key/content_revision、富 Input 草稿保留、动态校验、异常与重入的完整组件退场 · "
                         u8"列/搜索/状态区独立裁剪，精确滚轮边缘交接、RTL、键盘 Home/End/arrows/Page 和富子控件显露 · "
                         u8"完整 "
                         u8"Unicode 搜索归一化，filter/sort/render/limit、selector/search Core "
                         u8"IME、嵌套浮层和焦点离开清理 · "
                         u8"显式应用 loader、owner-thread completion、原子取消票据、错误/Retry "
                         u8"和旧/重复/外线程完成拒绝 · 独立 "
                         u8"Cascader Component "
                         u8"Token、seed/algorithm/inherit、四个失效域、size/status/variant/placement 和 "
                         u8"typed ref/events/slots · 实际 Gallery tags/Theme/scroll/search/loader 示例及 Linux 私有 "
                         u8"Wayland "
                         u8"Vulkan 有限自动验收"}},
                 {MessageKey{
                      u8"gallery.ant.component.cascader.missing",
                      String{
                          u8"Windows MSVC, physical IME candidate windows and assistive technology need independent "
                          u8"platform evidence · DOM/CSS/React, browser URLs and portals are outside the native API"}},
                  String{
                      u8"Windows MSVC、系统 IME 候选窗口和辅助技术的独立平台实测 · DOM/CSS/React、浏览器 URL 和 portal "
                      u8"接口不属于原生 API"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.ant.component.checkbox.name", String{u8"Checkbox"}},
                  String{u8"Checkbox 多选框"}},
                 {MessageKey{
                      u8"gallery.ant.component.checkbox.summary",
                      String{u8"Checkbox provides three typed value kinds, groups, retained dynamic options, "
                             u8"nearest-group/skipGroup semantics, RTL/ref/autoFocus/onClick, independent theme and "
                             u8"finite wave. Platform results remain separate."}},
                  String{u8"RynUI 已补齐 Checkbox 原生桌面功能：三类 typed 值、多选 Group、动态保留选项、最近组与 "
                         u8"skipGroup、RTL/ref/autoFocus/onClick、独立主题和有限 wave；平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.checkbox.supported",
                      String{
                          u8"checked/defaultChecked and indeterminate · disabled and onChange · typed label slot with "
                          u8"CJK/Latin text · pointer/Space/focus-visible interaction · typed String/double/bool Group "
                          u8"values and reactive controlled/default selection · retained dynamic options, "
                          u8"deletion/capture "
                          u8"cleanup and layout/Tab ordering · nearest Group, skipGroup, disabled composition and "
                          u8"callback "
                          u8"destruction safety · CheckboxRef, autoFocus, ordered onClick and inherited reactive RTL · "
                          u8"isolated component Theme algorithm, indicator/label/focus tokens and finite wave "
                          u8"lifecycle · "
                          u8"native onFocus/onBlur actual focus transitions with callback retirement safety"}},
                  String{u8"checked/defaultChecked、半选、禁用与 onChange；typed CJK/Latin 标签 "
                         u8"slot、指针/Space/可见焦点；String/double/bool "
                         u8"Group、响应式受控/默认选择；动态选项保留、删除/捕获清理、布局/Tab 顺序；最近 "
                         u8"Group、skipGroup、禁用组合及回调销毁安全；CheckboxRef、autoFocus、有序 onClick 与继承 "
                         u8"RTL；独立 "
                         u8"Theme、指示器/标签/焦点 Token、有限 wave；真实 onFocus/onBlur 转移和退役安全。"}},
                 {MessageKey{
                      u8"gallery.ant.component.checkbox.missing",
                      String{
                          u8"DOM/HTML/CSS, form name/required and React-specific interfaces · Linux native results are "
                          u8"recorded independently in change 107; new Windows/MSVC behavior, physical IME candidates, "
                          u8"OS "
                          u8"assistive technology and actual cross-monitor checks are not replaced by HEADLESS"}},
                  String{
                      u8"DOM/HTML/CSS、表单 name/required 和 React 专用接口不属原生范围；Linux 结果按 107 记录，新增 "
                      u8"Windows/MSVC、物理 IME、OS 辅助技术和实际跨显示器结果不能由 HEADLESS 替代。"}},
                 {MessageKey{u8"gallery.ant.component.color-picker.name", String{u8"ColorPicker"}},
                  String{u8"ColorPicker 颜色选择器"}},
                 {MessageKey{
                      u8"gallery.ant.component.color-picker.summary",
                      String{u8"ColorPicker provides typed RGB/HSB/HEX and alpha, actual SV/rails/channel editing, "
                             u8"gradient stops, presets and retained popups. Platform acceptance remains separate."}},
                  String{u8"原生 ColorPicker 提供 typed RGB/HSB/HEX 与 alpha、真实 "
                         u8"SV/色带/通道编辑、渐变色标、预设和保留浮层；平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.color-picker.supported",
                      String{
                          u8"Typed finite color conversion and validated keyed gradient stops · Actual logical-scene "
                          u8"SV/hue/alpha/gradient controls, pointer capture and keyboard · Controlled/default value, "
                          u8"format/open authority and change/changeComplete/clear callbacks · Retained "
                          u8"panel/trigger/text/extra slots, reactive presets, disabled/direction and component Theme "
                          u8"· "
                          u8"Independent interactive Gallery · Linux private Wayland/Vulkan/SPIR-V/system-font bounded "
                          u8"input, "
                          u8"actual callbacks, editor/preedit/capture retirement and input-area/cursor service calls"}},
                  String{
                      u8"有限 typed 颜色转换与有效键控渐变色标；真实逻辑场景 "
                      u8"SV/色相/透明度/渐变控件，指针捕获和键盘；受控/默认值、format/open "
                      u8"权威和变化/完成/清空回调；保留面板/触发器/文字/extra、响应预设、禁用/方向及独立 Theme；独立 "
                      u8"Gallery；Linux 私有 "
                      u8"Wayland/Vulkan/SPIR-V/系统字体有限输入、真实回调及编辑器/preedit/捕获退役、input-area/cursor "
                      u8"服务调用。"}},
                 {MessageKey{
                      u8"gallery.ant.component.color-picker.missing",
                      String{
                          u8"Windows/MSVC, physical IME candidates, actual cross-monitor/DPI and assistive technology "
                          u8"remain unverified; the data-entry family is not fully closed · Web DOM/CSS/React/portal "
                          u8"APIs are outside native scope"}},
                  String{u8"Windows/MSVC、物理 IME 候选 UI、实际跨显示器/DPI "
                         u8"和辅助技术尚未验证，数据录入族未整体收口；Web "
                         u8"DOM/CSS/React/portal API 不属原生范围。"}},
                 {MessageKey{u8"gallery.ant.component.date-picker.name", String{u8"DatePicker"}},
                  String{u8"DatePicker 日期选择框"}},
                 {MessageKey{u8"gallery.ant.component.date-picker.summary",
                             String{u8"DatePicker provides date/week/month/quarter/year panels, typed multiple "
                                    u8"selection/date-time/ranges, locale/format, clock/presets and retained rich "
                                    u8"slots. Authority, Form "
                                    u8"reset, RTL, finite motion and cleanup have focused contracts; platform results "
                                    u8"remain separate."}},
                  String{u8"原生 DatePicker 完成日期/周/月/季度/年真实面板、typed "
                         u8"多选/日期与日期时间范围、locale/格式、时钟/预设和保留富 slots；受控 authority、Form "
                         u8"重置、RTL、有限动画及失败清理有定向合同，平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.date-picker.supported",
                      String{
                          u8"source-compatible Gregorian CivilDate and optional single-value ISO API · real "
                          u8"Date/Week/Month/Quarter/Year/Decade panels, controlled panel date and mode, locale "
                          u8"week-year "
                          u8"and validated formats · typed multiple selection, CivilDateTime integration and "
                          u8"DateRangePicker "
                          u8"date/date-time values with atomic pending/accepted authority · allow-empty/order/disabled "
                          u8"endpoint policies, disabledDate info/from and dependent disabledTime validation · "
                          u8"explicit "
                          u8"clock Today/Now, lazy keyed single/multiple/date-time/range presets · retained typed rich "
                          u8"cells/panel/header/footer/icon slots and actual input refs, keyboard/preedit/inner-popup "
                          u8"ownership · Theme, responsive width, RTL, placement fallback and finite lifetime-aware "
                          u8"animation · same-value Form reset without fake change, controlled refusal/event ordering "
                          u8"and "
                          u8"throwing/self-destroying callback cleanup · independent actual Gallery pages with "
                          u8"modes/locale/RTL/multiple/date-time/range authority controls · Linux private "
                          u8"Wayland/Vulkan "
                          u8"finite DatePicker/TimePicker/Button sequence with actual input, retained draft, Core "
                          u8"preedit "
                          u8"and off-page cleanup"}},
                  String{u8"兼容 Gregorian CivilDate 和 optional 单值 ISO API；Date/Week/Month/Quarter/Year/Decade "
                         u8"真实面板、受控日期/模式、locale 周年及格式校验；typed "
                         u8"多选/CivilDateTime、日期及日期时间范围、候选/已接受值原子权威；空端点/顺序/"
                         u8"禁用策略、disabledDate "
                         u8"info/from 和依赖 disabledTime；显式时钟 "
                         u8"Today/Now、延迟键控预设；保留富单元格/面板/页头/页尾/图标与实际输入 "
                         u8"ref、键盘/preedit/内层弹层；Theme、响应宽度/RTL/方位回退/有限动画；同值 Form "
                         u8"reset、受控拒绝和回调顺序/抛错/自销毁清理；真实 Gallery 模式/格式/范围操作；Linux 私有 "
                         u8"Wayland/Vulkan 的有限 DatePicker/TimePicker/Button、草稿/Core preedit/离页清理。"}},
                 {MessageKey{
                      u8"gallery.ant.component.date-picker.missing",
                      String{
                          u8"Web DOM/CSS/React interfaces are outside native scope · Windows MSVC, physical system "
                          u8"IME candidate UI, assistive technology, real desktop and cross-monitor acceptance remain "
                          u8"independent platform boundaries"}},
                  String{u8"Web DOM/CSS/React 不属原生范围；Windows MSVC、物理系统 IME 候选 "
                         u8"UI、辅助技术、真实桌面及跨显示器验收是独立边界。"}},
                 {MessageKey{u8"gallery.ant.component.form.name", String{u8"Form"}}, String{u8"Form 表单"}},
                 {MessageKey{
                      u8"gallery.ant.component.form.summary",
                      String{u8"Form/Item/List/Provider provide three layouts, responsive columns, deep structured "
                             u8"stores, stable nested lists, typed automatic/custom binding, sync/cancelable async "
                             u8"rules, fine-grained state, Enter submission and native scroll/focus. Legacy "
                             u8"binding/results and same-value reset remain; Gallery has three complete workflows."}},
                  String{u8"原生 Form/Item/List/Provider 提供真实三布局与响应式列、深路径和结构化 "
                         u8"store、稳定键控嵌套列表、typed 自动/自定义绑定、同步/可取消异步规则和细粒度状态、Enter "
                         u8"提交与原生滚动焦点；保留旧绑定、结果和同值 reset，Gallery 提供三个独立完整流程。"}},
                 {MessageKey{
                      u8"gallery.ant.component.form.supported",
                      String{
                          u8"typed String, optional<double>, bool, SelectSelection, SelectValues and optional<Decimal> "
                          u8"with "
                          u8"exact decimal authority · literal legacy String identities, explicit string/index "
                          u8"NamePath, "
                          u8"structured mounted/unmounted values, initial/set/reset APIs and fine-grained watch · "
                          u8"retained "
                          u8"keyed nested FormList add/remove/move, rich drafts/preedit/focus, shifted unmounted child "
                          u8"values and generation-bound callback authority · actual horizontal/inline/vertical "
                          u8"layout, "
                          u8"responsive label/wrapper columns, alignment/wrap/colon/required marks, "
                          u8"RTL/locale/size/variant "
                          u8"and independent nested scopes · Item noStyle/hidden/help/extra/status/real feedback "
                          u8"icons/tooltips, retained rich label/help/extra/feedback slots and native Theme tokens · "
                          u8"first "
                          u8"compatible typed built-in automatic binding, preserved explicit bindings and explicit "
                          u8"FormControlAdapter for arbitrary custom controls · synchronous rich rules/warnings/exact "
                          u8"ranges, cross-field snapshots/dependencies, triggers/debounce, owner-thread async "
                          u8"cancellation/timeouts and stale submit protection · FormRef "
                          u8"submit/validate/reset/setFields/getFields/error/meta/typed native instance APIs, "
                          u8"FormProvider "
                          u8"events and readable watch/status Bindings · native single-line Enter with "
                          u8"TextArea/IME/nested "
                          u8"scope isolation, real control geometry scroll reveal/focus and "
                          u8"disabled/hidden/zero/inactive "
                          u8"gates · atomic invalid list operations or deliberate complete Form/List retirement on "
                          u8"unsafe "
                          u8"user failures, preserving unrelated controls · legacy four-field Gallery plus independent "
                          u8"layout/exact/scroll, deep-list and async/dependency examples with actual submitted "
                          u8"values"}},
                  String{
                      u8"typed "
                      u8"String、optional<double>、bool、SelectSelection/"
                      u8"Values、optional<Decimal>，保持精确小数权威；旧 "
                      u8"String 身份、显式 NamePath、结构化已挂/未挂值、初始/设置/重置及细粒度 watch；稳定嵌套 "
                      u8"FormList "
                      u8"增删移动、富草稿/preedit/焦点、未挂子值移位及代际回调；三布局、响应列、对齐/换行/冒号/"
                      u8"必填、RTL/locale/"
                      u8"尺寸/变体及独立嵌套；noStyle/hidden/help/extra/status、反馈图标/Tooltip、富 slot 和 "
                      u8"Theme；兼容内置自动绑定、显式绑定及 "
                      u8"FormControlAdapter；同步规则/警告/精确范围、跨字段快照/依赖/触发/去抖、所有者线程异步取消/"
                      u8"超时和过期提交防护；FormRef 完整提交/查询/重置/实例 API、Provider 事件和 Bindings；Enter 与 "
                      u8"TextArea/IME/"
                      u8"嵌套隔离、真实几何滚动聚焦及输入门控；无效列表操作原子拒绝，不安全用户失败时完整退役自有范围；"
                      u8"四字段旧"
                      u8"例与精确布局/滚动、深列表、异步依赖真实流程。"}},
                 {MessageKey{
                      u8"gallery.ant.component.form.missing",
                      String{u8"Web DOM/CSS/HTML/React aliases are excluded from the native API; arbitrary custom "
                             u8"controls require the explicit typed adapter · native GPU/window, physical system IME "
                             u8"candidate UI, assistive technology and Windows/MSVC evidence remain independent"}},
                  String{u8"Web DOM/CSS/HTML/React 别名不属原生 API；任意自定义控件需显式 typed "
                         u8"adapter；GPU/窗口、物理系统 IME "
                         u8"候选 UI、辅助技术和 Windows/MSVC 证据独立。"}},
                 {MessageKey{u8"gallery.ant.component.input.name", String{u8"Input"}}, String{u8"Input 输入框"}},
                 {MessageKey{
                      u8"gallery.ant.component.input.summary",
                      String{u8"Input, Password, Search, TextArea and OTP provide native desktop editing. Unicode 17 "
                             u8"bidirectional layout, visual caret/disjoint selection and logical clipboard/history "
                             u8"share reactive paragraph direction. Platform results remain separate."}},
                  String{u8"Input、Password、Search、TextArea 和 OTP 已完成原生桌面功能；Unicode 17 双向排版、视觉 "
                         u8"caret/不连续选择与逻辑 clipboard/history、reactive "
                         u8"段落方向共享编辑基础，分平台证据独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.input.supported",
                      String{u8"typed Props and prefix/suffix slots · controlled/uncontrolled single-line Unicode "
                             u8"editing and "
                             u8"scalar maxLength · selection, clipboard, undo/redo and composition event bridge · "
                             u8"Theme/status/disabled/readOnly and retained caret deadline · Search Input/Button "
                             u8"composition with "
                             u8"Enter/button submission and loading gate · Password grapheme mask and visibility "
                             u8"action · Input "
                             u8"allowClear action with editor/session reuse · Outlined/Filled/Borderless/Underlined "
                             u8"with Theme and "
                             u8"Compact composition · scalar/grapheme/custom statistics, soft count max and atomic "
                             u8"exceedFormatter "
                             u8"· InputRef focus/blur/select, mount-only autoFocus and native input hints · "
                             u8"clearDisabled/custom "
                             u8"vector clearIcon/onClear · reactive Password visibility/toggle focusability, Hover and "
                             u8"custom icon "
                             u8"renderer · Search custom icon/content, Clear source and connected variant appearance · "
                             u8"TextArea LF "
                             u8"normalization, rows/wrap/autoSize and common Input props · two-dimensional selection, "
                             u8"visual "
                             u8"line/Page navigation, multiline IME coverage · retained wheel/caret reveal, native "
                             u8"resize capture "
                             u8"and safe onResize · typed OTP, grapheme cells, partial/completion callbacks and "
                             u8"transactional "
                             u8"formatter · OTP clipboard paste, custom mask, indexed separator and RTL group "
                             u8"navigation · "
                             u8"retained OTP dynamic length, focus refs and IME owner retirement · Unicode 17 bidi "
                             u8"Arabic/Hebrew, "
                             u8"visual navigation and disjoint selection/preedit · reactive TextDirection Auto/LTR/RTL "
                             u8"with "
                             u8"retained editor/ref/scene identity · canonical Gallery routes for Input / Search / "
                             u8"Password / "
                             u8"TextArea / OTP retain the existing native editing contracts"}},
                  String{
                      u8"typed 属性及前后缀 slot；受控/非受控 Unicode 单行、标量 "
                      u8"maxLength、选择/剪贴板/撤销重做/composition；Theme/状态/禁用/只读、保留 caret "
                      u8"deadline；Search "
                      u8"组合、Enter/按钮及加载门控；Password 字素掩码/显隐；allowClear 复用编辑器/会话；四变体与 "
                      u8"Compact；标量/字素/自定义统计、软上限及原子 "
                      u8"exceedFormatter；InputRef/autoFocus/原生提示、自定义清空与 "
                      u8"onClear；Password 响应显隐/Hover/图标；Search 图标/内容/Clear 来源与连接外观；TextArea "
                      u8"LF、rows/wrap/autoSize、二维选区/视觉行/Page、多行 IME、滚轮/显露/resize；OTP "
                      u8"字素单元/部分与完成回调/事务 formatter、粘贴/掩码/分隔/RTL、动态长度/ref/IME 退役；Unicode 17 "
                      u8"阿拉伯/希伯来 bidi、视觉导航/不连续选区/preedit；Auto/LTR/RTL 保留编辑器/ref/scene；五种规范 "
                      u8"Gallery "
                      u8"路由保留原生编辑合同。"}},
                 {MessageKey{
                      u8"gallery.ant.component.input.missing",
                      String{u8"DOM/CSS/React refs/events and HTML autofill are outside native scope · Linux native "
                             u8"results are "
                             u8"recorded independently in change 107; new Windows/MSVC behavior, physical IME "
                             u8"candidates, OS "
                             u8"assistive technology and actual cross-monitor checks are not replaced by HEADLESS"}},
                  String{u8"DOM/CSS/React ref/事件和 HTML 自动填充不属原生范围；Linux 结果按 107 记录，新增 "
                         u8"Windows/MSVC、物理 "
                         u8"IME、OS 辅助技术及实际跨显示器不能由 HEADLESS 替代。"}},
                 {MessageKey{u8"gallery.ant.component.input-number.name", String{u8"InputNumber"}},
                  String{u8"InputNumber 数字输入框"}},
                 {MessageKey{
                      u8"gallery.ant.component.input-number.summary",
                      String{u8"InputNumber retains the double API and provides exact stringMode, formatter/parser, "
                             u8"cancelable hold repeat, optional wheel, spinner and typed control icons. Platform "
                             u8"acceptance remains separate."}},
                  String{
                      u8"原生 InputNumber 保留 double API，并提供精确 stringMode、formatter/parser、可取消长按、可选 "
                      u8"wheel、spinner 与 typed 控制图标；平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.input-number.supported",
                      String{
                          u8"Nullable double, retained drafts, controlled/default values and submit · "
                          u8"Bounds/precision/step, "
                          u8"step buttons/keyboard, InputRef, disabled/readOnly and IME gates · Exact typed Decimal "
                          u8"values/ranges/steps without double conversion · Formatter/parser typing context, decimal "
                          u8"separator and changeOnBlur · Pointer-captured monotonic hold repeat, optional fractional "
                          u8"wheel "
                          u8"stepping and cancellation · Retained spinner/up/down controls and typed icons · Linux "
                          u8"private "
                          u8"Wayland/Vulkan/SPIR-V/system-font bounded input, actual callbacks, editor/preedit/capture "
                          u8"retirement and input-area/cursor services"}},
                  String{
                      u8"可空 "
                      u8"double、保留草稿、受控/默认与提交；范围/精度/步长、按钮/键盘、InputRef、禁用/只读/IME；typed "
                      u8"Decimal 值/范围/步长不经 double；formatter/parser 输入上下文、小数分隔和 "
                      u8"changeOnBlur；捕获指针的单调时钟长按、可选分数滚轮及取消；保留 spinner/上下按钮和 typed "
                      u8"图标；Linux "
                      u8"私有 Wayland/Vulkan/SPIR-V/系统字体有限输入、真实回调、编辑器/preedit/捕获退役及 "
                      u8"input-area/cursor "
                      u8"服务。"}},
                 {MessageKey{
                      u8"gallery.ant.component.input-number.missing",
                      String{
                          u8"Windows/MSVC, physical IME candidates, actual cross-monitor/DPI and assistive technology "
                          u8"remain unverified; the data-entry family is not fully closed · Web DOM/CSS/React aliases "
                          u8"are outside native scope"}},
                  String{u8"Windows/MSVC、物理 IME、实际跨显示器/DPI 及辅助技术尚未验证，数据录入族未整体收口；Web "
                         u8"DOM/CSS/React 别名不属原生范围。"}},
                 {MessageKey{u8"gallery.ant.component.mentions.name", String{u8"Mentions"}}, String{u8"Mentions 提及"}},
                 {MessageKey{
                      u8"gallery.ant.component.mentions.summary",
                      String{u8"Mentions provides retained multiline editing, actual caret suggestions, Unicode "
                             u8"replacement, stable rich rows, cancelable async loading, independent theme and Form "
                             u8"reset. Platform results remain separate."}},
                  String{u8"原生 Mentions 已实现保留多行编辑、实际 caret 建议、Unicode "
                         u8"替换、稳定富行、可取消异步、独立主题与 "
                         u8"Form reset；平台结果独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.mentions.supported",
                      String{
                          u8"typed reactive Props and stable native TextArea value/ref/selection; Unicode, bidi, "
                          u8"clipboard, "
                          u8"composition and maxLength · multiple and overlapping Unicode prefixes; actual caret "
                          u8"query, "
                          u8"validated search, exact grapheme-safe replacement and pure getMentions · caret-anchored "
                          u8"window-layer popup following wrapping, scrolling, resize and flat ancestor translation; "
                          u8"bounded "
                          u8"placement · keyboard/pointer selection, disabled skipping, parent/child popup ownership, "
                          u8"bounded ordinary rows and retained rich drafts · application-owned cancellable async "
                          u8"source "
                          u8"with owner-thread post/wakeup, generation, lifetime and duplicate-result authority · "
                          u8"independent Mentions input/suggestion Theme, scoped seed/algorithm/inherit, "
                          u8"snapshot/hash/invalidation and finite motion · explicit Form String/ref binding and "
                          u8"same-value "
                          u8"reset; independent editing, long-list and synthetic async Gallery samples · Linux private "
                          u8"Wayland/Vulkan/SPIR-V/system-font Mentions to Button acceptance: 2 cases, 185 checks, "
                          u8"native "
                          u8"input-area/cursor services and editor/capture retirement"}},
                  String{u8"typed 响应属性和稳定 "
                         u8"TextArea/ref/选区，Unicode/bidi/剪贴板/composition/maxLength；多种重叠 Unicode "
                         u8"前缀、真实 caret 查询、验证搜索、按字素精确替换及纯 getMentions；窗口层 caret "
                         u8"锚定跟随换行/滚动/resize/祖先平移，有界方位；键盘/"
                         u8"指针、跳过禁用、父子弹层、有界普通行与保留富草稿；应用异步来源、所有者线程 "
                         u8"post/唤醒、代际/生命周期/重复完成权威；独立输入/建议 "
                         u8"Theme、seed/algorithm/inherit、快照/hash/失效及有限动画；显式 Form String/ref 和同值 "
                         u8"reset、独立编辑/长列表/合成异步 Gallery；Linux 私有 Wayland/Vulkan/SPIR-V/系统字体 "
                         u8"Mentions→Button "
                         u8"记录为 2 cases、185 checks，含原生 input-area/cursor 和编辑器/捕获退役。"}},
                 {MessageKey{
                      u8"gallery.ant.component.mentions.missing",
                      String{
                          u8"Windows/MSVC, physical system IME candidate UI, assistive technology and cross-monitor "
                          u8"acceptance remain independent · Web DOM/CSS/React/HTML and browser portal/transport APIs "
                          u8"are outside native scope"}},
                  String{u8"Windows/MSVC、物理系统 IME 候选 UI、辅助技术及跨显示器验收独立；Web DOM/CSS/React/HTML "
                         u8"和浏览器 "
                         u8"portal/传输 API 不属原生范围。"}},
                 {MessageKey{u8"gallery.ant.component.radio.name", String{u8"Radio"}}, String{u8"Radio 单选框"}},
                 {MessageKey{
                      u8"gallery.ant.component.radio.summary",
                      String{u8"Native Radio/RadioButton/RadioGroup provide dynamic typed values, group keyboard "
                             u8"navigation, "
                             u8"connected buttons, sizes, block, RTL, refs, independent theme and finite feedback."}},
                  String{
                      u8"原生 Radio / RadioButton / "
                      u8"RadioGroup：动态类型值、键盘组导航、连接按钮、尺寸、block、RTL、ref、独立主题与有限反馈。"}},
                 {MessageKey{
                      u8"gallery.ant.component.radio.supported",
                      String{
                          u8"Three controlled/uncontrolled value kinds, nearest typed Group, reactive options, stable "
                          u8"identity "
                          u8"and dynamic cleanup · One Tab stop, directional navigation, refs/autoFocus/onClick and "
                          u8"finite wave "
                          u8"· RadioButton, outline/solid, three sizes, block, horizontal/vertical/RTL adjacency and "
                          u8"independent Theme · Native onFocus/onBlur actual transitions with callback retirement "
                          u8"safety"}},
                  String{u8"受控/非受控三类值、最近 typed Group、响应选项、稳定身份及动态清理；单一 "
                         u8"Tab、方向键、ref/autoFocus/onClick 与有限 "
                         u8"wave；RadioButton、outline/solid、三尺寸、block、横纵/RTL "
                         u8"相邻边界和独立 Theme；真实 onFocus/onBlur 转移及回调退役安全。"}},
                 {MessageKey{
                      u8"gallery.ant.component.radio.missing",
                      String{u8"Web DOM/form/name/required, React events and CSS entry points are excluded · Linux "
                             u8"native results "
                             u8"are recorded independently in change 107; new Windows/MSVC behavior, physical IME "
                             u8"candidates, OS "
                             u8"assistive technology and actual cross-monitor checks are not replaced by HEADLESS"}},
                  String{u8"Web 专用 DOM/form/name/required、React 事件和 CSS 入口不移植 · Linux 后台原生结果按 change "
                         u8"107 "
                         u8"精简证据独立记录；本轮新增行为的 Windows/MSVC、物理 IME 候选 UI、OS "
                         u8"辅助技术与跨显示器实测不由 "
                         u8"HEADLESS 代替"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.ant.component.rate.name", String{u8"Rate"}}, String{u8"Rate 评分"}},
                 {MessageKey{
                      u8"gallery.ant.component.rate.summary",
                      String{u8"Rate provides half ratings/clear/controlled writeback, dynamic count, per-item "
                             u8"paint-only characters, explicit RTL, refs/focus/key events and retained rich Tooltips. "
                             u8"Platform results remain separate."}},
                  String{u8"原生 Rate：半星/清空/受控回写、动态数量、响应逐项纯绘制字符、显式 "
                         u8"RTL、ref/焦点按键事件及保留富 "
                         u8"Tooltip；平台结果独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.rate.supported",
                      String{u8"Public typed Props, native stars, arrows/Home/End/Enter/Space and disabled/readOnly · "
                             u8"Catalog-linked service-rating Gallery, accept/reject feedback, rich Tooltip Input and "
                             u8"retained "
                             u8"draft · Native onFocus/onBlur with callback retirement safety · Reactive RateDirection "
                             u8"mirrors "
                             u8"geometry, actual half-glyph clipping, pointer fractions and keyboard direction · "
                             u8"Owner-thread "
                             u8"RateRef focus/blur, mount-only autoFocus and typed RateKeyEvent · characterFor binds "
                             u8"per-index "
                             u8"Prop<RateCharacter> once, preserving identity and rolling back failed growth"}},
                  String{
                      u8"公开 typed 属性、原生星项、方向/Home/End/Enter/Space "
                      u8"与禁用/只读；目录关联的服务评分、接受/拒绝反馈、富 Tooltip Input "
                      u8"与草稿；真实焦点回调退役安全；RateDirection "
                      u8"镜像几何、实际半字形裁剪、指针比例及键盘方向；所有者线程 RateRef、仅挂载 autoFocus 和 typed "
                      u8"按键事件；characterFor 按索引只绑定一次 Prop<RateCharacter>，保留身份，增长失败回滚。"}},
                 {MessageKey{
                      u8"gallery.ant.component.rate.missing",
                      String{
                          u8"Linux native results are recorded independently in change 107; new Windows/MSVC "
                          u8"behavior, physical IME candidates, OS assistive technology and cross-monitor checks are "
                          u8"not replaced by HEADLESS · Characters are paint-only String/IconSource mappings; "
                          u8"arbitrary stateful Component/ReactNode cloning and Web TooltipProps/DOM/CSS/React "
                          u8"aliases are not forwarded; global ConfigProvider is separate"}},
                  String{u8"Linux 后台原生结果按 change 107 精简证据独立记录；本轮新增行为的 Windows/MSVC、物理 IME "
                         u8"候选 UI、OS "
                         u8"辅助技术与跨显示器实测不由 HEADLESS 代替 · character 为 String/IconSource "
                         u8"纯绘制映射；任意有状态 "
                         u8"Component/ReactNode 的双份克隆、Web TooltipProps/DOM/CSS/React 别名不透传；全局 "
                         u8"ConfigProvider "
                         u8"属于独立能力"}},
                 {MessageKey{u8"gallery.ant.component.select.name", String{u8"Select"}}, String{u8"Select 选择器"}},
                 {MessageKey{
                      u8"gallery.ant.component.select.summary",
                      String{
                          u8"Select retains its single-selection API and provides Multiple/Tags editing, grouped/rich "
                          u8"rendering, tag overflow, windowing, cancelable application async sources, interactive "
                          u8"affixes, RTL and finite motion. New platform results remain separate."}},
                  String{u8"原生 Select 保留单选兼容 API，补齐 Multiple/Tags "
                         u8"实际输入、分组/富渲染、标签溢出、窗口列表、可取消应用异步源、交互 affix、RTL "
                         u8"和有限动画；新增平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.select.supported",
                      String{u8"Typed/reactive single/multiple/labeled values, controlled requests and explicit Form "
                             u8"reset · "
                             u8"Multiple/Tags selection, input/paste tokenization, Enter/Backspace, tag removal, "
                             u8"maxCount/truncation/overflow · Stable keyed grouped/rich options, retained "
                             u8"option/label/tag/popup "
                             u8"renderers and interactive affixes · Unicode filtering, stable within-group sorting, "
                             u8"controlled "
                             u8"search and active/select/deselect/clear/key/scroll events · Windowed ordinary rows, "
                             u8"retained rich "
                             u8"drafts, keyboard reveal, nearest-popup wheel and failure/lifetime cleanup · "
                             u8"Application async "
                             u8"provider/owner-thread dispatcher, cancellation/errors/old-generation protection · "
                             u8"Theme, three "
                             u8"sizes/four variants/status, typed icons, RTL/placement/width, finite motion and actual "
                             u8"Gallery · "
                             u8"Linux private Wayland/Vulkan/SPIR-V/system-font bounded input, input-area/cursor and "
                             u8"retirement; "
                             u8"async Gallery uses a synthetic provider"}},
                  String{u8"typed/reactive single/multiple/labeled values，受控请求和显式 Form reset · Multiple/Tags "
                         u8"选择、输入/粘贴 tokenization、Enter/Backspace、标签移除、maxCount/标签截断和 overflow · "
                         u8"稳定 key 的 "
                         u8"grouped/rich options、保留 option/label/tag/popup renderers 和交互式前后缀 · Unicode "
                         u8"过滤、稳定组内排序、受控 search、active/select/deselect/clear/key/scroll events · "
                         u8"窗口化普通行、保留富草稿、keyboard reveal、最近 popup wheel、失败/生命周期清理 · 应用异步 "
                         u8"provider/owner-thread dispatcher、取消/错误/旧 generation 防护 · "
                         u8"Theme、三尺寸/四变体/status、typed "
                         u8"icons、RTL/placement/宽度、有限 motion 与新增真实 Gallery · Linux 私有 "
                         u8"Wayland/Vulkan/SPIR-V/system "
                         u8"fonts 有限自动输入、宿主 input area/cursor 与离页清理；异步 Gallery 使用合成 provider"}},
                 {MessageKey{u8"gallery.ant.component.select.missing",
                             String{u8"Physical IME candidates, actual cross-monitor/DPI, assistive technology and "
                                    u8"Windows MSVC native "
                                    u8"windows remain unverified; data-entry is not fully closed · Web "
                                    u8"DOM/CSS/React/portal, dynamic "
                                    u8"object fieldNames and deprecated aliases are outside typed Core"}},
                  String{u8"系统 IME 候选 UI、跨实际显示器/DPI、辅助技术和 Windows MSVC "
                         u8"原生窗口仍未验证；数据录入分类尚未整体收口 "
                         u8"· Web DOM/CSS/React/portal、动态对象 fieldNames 和废弃兼容别名不属于 typed Core"}},
                 {MessageKey{u8"gallery.ant.component.slider.name", String{u8"Slider"}}, String{u8"Slider 滑动输入条"}},
                 {MessageKey{
                      u8"gallery.ant.component.slider.summary",
                      String{
                          u8"Slider supports single/range/multiple handles, discrete selection, marks/dots, tooltips, "
                          u8"whole-track dragging, dynamic endpoints, per-handle disabled state and safe "
                          u8"focus/blur/autoFocus. Platform results remain separate; Web APIs are excluded."}},
                  String{u8"RynUI 已补齐原生单值、双端和多端 "
                         u8"Slider：离散选择/marks/dots/值提示、整段拖动、动态端点编辑与逐端点禁用、安全 "
                         u8"focus/blur/autoFocus；平台验收证据独立记录，Web API 不移植。"}},
                 {MessageKey{
                      u8"gallery.ant.component.slider.supported",
                      String{
                          u8"Slider / RangeSlider typed values and atomic reactive limits · mouse/touch capture, "
                          u8"keyboard, independent thumb focus, disabled · reactive marks / label selection / "
                          u8"marksOnly / dots / included · persistent thumb Tooltip: Auto / Always / Hidden, "
                          u8"formatter and placement · MultiSlider retained topology, draggable track, editable "
                          u8"min/max count and per-handle disabled · native SliderRef focus/blur, autoFocus, vertical "
                          u8"hint default and overflow control · Theme Slider tokens and retained logical scene · "
                          u8"native onFocus/onBlur actual focus transitions with callback retirement safety"}},
                  String{u8"Slider/RangeSlider typed 值及原子响应范围；鼠标/触摸捕获、键盘、独立滑块焦点/禁用；响应 "
                         u8"marks/label/marksOnly/dots/included；保留 Tooltip 的 "
                         u8"Auto/Always/Hidden、formatter/方位；MultiSlider "
                         u8"保留拓扑、整轨拖动、动态端点数量与逐项禁用；SliderRef/"
                         u8"autoFocus、纵向提示默认与溢出控制；Theme Token "
                         u8"和保留逻辑场景；真实 onFocus/onBlur 与回调退役安全。"}},
                 {MessageKey{u8"gallery.ant.component.slider.missing",
                             String{u8"Web DOM/CSS/portal and compatibility aliases are excluded · Linux native "
                                    u8"results are recorded "
                                    u8"independently in change 107; new Windows/MSVC behavior, physical IME "
                                    u8"candidates, OS assistive "
                                    u8"technology and actual cross-monitor checks are not replaced by HEADLESS"}},
                  String{
                      u8"Web-only DOM/CSS/portal and compatibility aliases are excluded from native scope · Linux "
                      u8"后台原生结果按 change 107 精简证据独立记录；本轮新增行为的 Windows/MSVC、物理 IME 候选 UI、OS "
                      u8"辅助技术与跨显示器实测不由 HEADLESS 代替"}},
                 {MessageKey{u8"gallery.ant.component.switch.name", String{u8"Switch"}}, String{u8"Switch 开关"}},
                 {MessageKey{
                      u8"gallery.ant.component.switch.summary",
                      String{
                          u8"Switch provides retained text/icon content, RTL, refs/autoFocus/onClick, component "
                          u8"theme, pressed handle expansion and finite wave. Platform acceptance remains separate."}},
                  String{u8"RynUI 已补齐 Switch "
                         u8"原生桌面功能：保留文字/图标内容、RTL、ref/autoFocus/onClick、组件主题、按压伸展与有限 "
                         u8"wave；平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.switch.supported",
                      String{
                          u8"checked/defaultChecked and onChange · Middle/Small sizes · disabled and loading spinner "
                          u8"· pointer/Space/focus-visible interaction · retained checked/unchecked Text/Icon slots "
                          u8"and clipping · RTL, SwitchRef, autoFocus and ordered onClick candidate · Theme inner "
                          u8"margins, handle shadow, component algorithm and opacityLoading · pressed handle "
                          u8"extension, finite wave and cancellation/idle lifecycle · native onFocus/onBlur actual "
                          u8"focus transitions with callback retirement safety"}},
                  String{u8"checked/defaultChecked/onChange、两尺寸、禁用及加载 "
                         u8"spinner；指针/Space/可见焦点；保留选中/未选 "
                         u8"Text/Icon slot 和裁剪；RTL、SwitchRef、autoFocus、有序 onClick 候选；Theme "
                         u8"内边距/手柄阴影/组件算法/加载透明度；按压伸展、有限 wave、取消与 "
                         u8"idle；真实焦点转移及回调退役安全。"}},
                 {MessageKey{u8"gallery.ant.component.switch.missing",
                             String{u8"DOM/HTML/CSS, React refs and Web value/defaultValue aliases · Linux native "
                                    u8"results are recorded "
                                    u8"independently in change 107; new Windows/MSVC behavior, physical IME "
                                    u8"candidates, OS assistive "
                                    u8"technology and actual cross-monitor checks are not replaced by HEADLESS"}},
                  String{
                      u8"DOM/HTML/CSS and React ref · Web value/defaultValue aliases · Linux 后台原生结果按 change 107 "
                      u8"精简证据独立记录；本轮新增行为的 Windows/MSVC、物理 IME 候选 UI、OS "
                      u8"辅助技术与跨显示器实测不由 "
                      u8"HEADLESS 代替"}},
                 {MessageKey{u8"gallery.ant.component.time-picker.name", String{u8"TimePicker"}},
                  String{u8"TimePicker 时间选择框"}},
                 {MessageKey{u8"gallery.ant.component.time-picker.summary",
                             String{u8"TimePicker provides integer milliseconds, 12/24-hour mode, typed nullable "
                                    u8"ranges, dependent "
                                    u8"disabled rules, locale/format, explicit Now/presets and wheel selection. Rich "
                                    u8"slots, authority, "
                                    u8"Form reset, finite motion and cleanup have focused contracts; platform results "
                                    u8"remain separate."}},
                  String{u8"原生 TimePicker 完成整数毫秒与 12/24 小时、typed "
                         u8"可空时间范围、依赖禁用规则、locale/格式、显式 "
                         u8"Now/预设和滚轮选择；保留富 slots、受控 authority、Form "
                         u8"重置及有限动画有定向合同，平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.time-picker.supported",
                      String{
                          u8"source-compatible TimeOfDay and optional single-value API with exact integer milliseconds "
                          u8"· "
                          u8"real 12/24 hour and AM/PM columns, minute/second/millisecond precision with all step "
                          u8"boundaries "
                          u8"· validated locale display/input formats and hierarchical dependent disabled-time "
                          u8"predicates · "
                          u8"TimeRangePicker typed nullable endpoints, allow-empty/order/disabled policies, "
                          u8"pending/accepted "
                          u8"and controlled open/endpoint authority · explicit clock Now, lazy keyed presets, "
                          u8"needConfirm "
                          u8"and geometry-bounded changeOnScroll without remounting rows · retained typed reactive "
                          u8"rich "
                          u8"cells/panel/footer/icon slots and real input refs · Theme, RTL, placement fallback and "
                          u8"finite "
                          u8"lifetime-aware animation · same-value Form reset, preedit/nested popup/Tab ownership and "
                          u8"throwing/self-destroying callback cleanup · independent actual Gallery page with precise "
                          u8"12/24 "
                          u8"hour, Now/presets, scroll-select and range authority controls · Linux private "
                          u8"Wayland/Vulkan "
                          u8"finite DatePicker/TimePicker/Button sequence with actual input, retained draft, Core "
                          u8"preedit "
                          u8"and off-page cleanup"}},
                  String{u8"兼容 TimeOfDay/optional 单值及精确整数毫秒；真实 12/24 "
                         u8"小时、AM/PM、分秒毫秒精度及步长边界；验证 "
                         u8"locale 显示/输入格式和层级依赖禁用；typed 可空范围、空值/顺序/端点禁用、候选/已接受及受控 "
                         u8"open "
                         u8"权威；显式 Now、延迟键控预设、needConfirm、有界 changeOnScroll 不重挂行；保留 typed "
                         u8"富单元格/面板/页尾/图标/ref；Theme/RTL/方位回退/有限动画；同值 Form "
                         u8"reset、preedit/嵌套弹层/Tab、抛错及自销毁清理；独立精确时间/预设/滚轮/范围 Gallery；Linux "
                         u8"私有 "
                         u8"Wayland/Vulkan 的有限 Picker/Button、真实输入/草稿/Core preedit/离页清理。"}},
                 {MessageKey{
                      u8"gallery.ant.component.time-picker.missing",
                      String{
                          u8"Web DOM/CSS/React interfaces are outside native scope · Windows MSVC, physical system "
                          u8"IME candidate UI, assistive technology, real desktop and cross-monitor acceptance remain "
                          u8"independent platform boundaries"}},
                  String{u8"Web DOM/CSS/React 不属原生范围；Windows MSVC、物理系统 IME 候选 "
                         u8"UI、辅助技术、真实桌面及跨显示器验收是独立边界。"}},
                 {MessageKey{u8"gallery.ant.component.transfer.name", String{u8"Transfer"}},
                  String{u8"Transfer 穿梭框"}},
                 {MessageKey{
                      u8"gallery.ant.component.transfer.summary",
                      String{
                          u8"Transfer provides typed data/controlled requests, independent scoped tokens, dynamic "
                          u8"geometry, locale/RTL, retained extension slots and full input/failure cleanup. Platform "
                          u8"boundaries remain separate."}},
                  String{u8"原生 Transfer 已完成 typed 数据/受控请求、独立 scoped "
                         u8"Token、动态几何、本地化/RTL、保留扩展 slots "
                         u8"及完整输入/失败清理合同；平台边界分别记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.transfer.supported",
                      String{
                          u8"Stable String/number keys, rowKey, controlled/default targetKeys/selectedKeys, "
                          u8"synchronous "
                          u8"reentry and batch authority validation · Actual Checkbox/Input/Button/Pagination, two-way "
                          u8"moves, oneWay deletion, select-all/page/range/invert/clear · Per-side search, fractional "
                          u8"wheel/boundary handoff and independent controlled pagination/page size · 17 colors/13 "
                          u8"metrics/typography/effects/order, scoped seed/algorithm/inherit and complete Theme "
                          u8"dirty/hash/golden · Dynamic listWidth/listHeight/size/rowSpacing/pagination, final "
                          u8"geometry "
                          u8"validation and rollback/full retirement · Typed locale, reactive "
                          u8"operations/selectAllLabels, "
                          u8"retained item/title/list/footer/action/icon slots and actual Table composition · RTL "
                          u8"controls/search/pagination/footer, stable rich-row drafts, reorder/move · "
                          u8"Input/TextArea/nested "
                          u8"popup, composition/Tab/Escape, focus reveal, capture/session and "
                          u8"disabled/readOnly/loading/visible/modal/window/zero-area cleanup · "
                          u8"Binding/render/filter/factory/handler exceptions and self-destruction, "
                          u8"owner-thread/generation/side checks and bounded idle/no-op wakeups"}},
                  String{u8"String/number stable keys、rowKey、受控/default "
                         u8"targetKeys/selectedKeys、同步重入和批处理权威校验 · "
                         u8"真实 Checkbox/Input/Button/Pagination、双向移动、oneWay "
                         u8"删除、全选/当前页/范围选择/反选/清空 · "
                         u8"按侧搜索、fractional wheel/boundary handoff、独立分页及可配置页大小的受控请求 · 17 色/13 "
                         u8"metrics/typography/effects/order、scoped seed/algorithm/inherit 与完整 Theme "
                         u8"dirty/hash/golden · "
                         u8"动态 listWidth/listHeight/size/rowSpacing/pagination、有限最终几何校验与事务回滚/完整 "
                         u8"retirement · "
                         u8"完整 typed locale、可响应 operations/selectAllLabels、保留 "
                         u8"item/title/list/footer/action/icon slots "
                         u8"与真实 Table 组合 · RTL Checkbox/列表/动作/搜索/分页/footer；稳定 keyed "
                         u8"富行草稿、重排和移动 · "
                         u8"Input/TextArea/嵌套 popup、composition/Tab/Escape、焦点 reveal、capture/session 及 "
                         u8"disabled/readOnly/loading/visible/modal/window/zero-area 清理 · "
                         u8"绑定/render/filter/factory/handler "
                         u8"异常与自销毁、owner-thread/generation/side 校验、idle/no-op 有限唤醒"}},
                 {MessageKey{u8"gallery.ant.component.transfer.missing",
                             String{u8"Windows MSVC, physical IME candidates and OS assistive technology require "
                                    u8"independent evidence "
                                    u8"· DOM/CSS/React, HTML attributes, browser events and Web refs are outside the "
                                    u8"native API"}},
                  String{u8"Windows MSVC、物理系统 IME 候选窗及 OS 辅助技术平台证据独立验证 · DOM/CSS/React、HTML "
                         u8"属性、浏览器事件及 Web refs 不属于原生接口"}},
                 {MessageKey{u8"gallery.ant.component.tree-select.name", String{u8"TreeSelect"}},
                  String{u8"TreeSelect 树选择"}},
                 {MessageKey{
                      u8"gallery.ant.component.tree-select.summary",
                      String{u8"TreeSelect provides compatible single values, typed multiple/checkbox/tag selection, "
                             u8"search, dynamic rich nodes, custom popup, async/virtual trees, RTL/finite motion and "
                             u8"same-value Form reset. Actual Input/Popover/Tree composition remains."}},
                  String{
                      u8"原生 TreeSelect 已完成兼容单值与 typed 多值/checkbox/标签、搜索、动态富节点、自定义 "
                      u8"popup、异步/虚拟树、RTL/有限 motion 和同值 Form reset；保留真实 Input/Popover/Tree 组合。"}},
                 {MessageKey{
                      u8"gallery.ant.component.tree-select.supported",
                      String{u8"Compatible String and typed TreeKeys/labeled values, strict/cascaded checkboxes, "
                             u8"showCheckedStrategy and retained tags · Removable/custom Tag, fixed/responsive "
                             u8"maxTagCount, "
                             u8"Unicode lengths and interactive prefix/suffix/popup · Controlled "
                             u8"value/search/open/expanded/loaded "
                             u8"requests, stable dynamic rich nodes and independent per-node async Tree · Actual "
                             u8"virtual rows, "
                             u8"keyboard/scrollTo, composition/inner Escape/Tab, popup ownership and concurrent "
                             u8"instances · Actual "
                             u8"Input/Popover/Tree, independent Theme, size/status/variant/placement/RTL and finite "
                             u8"motion · "
                             u8"Same-value Form String reset cancels search/async/pending focus/popup while retaining "
                             u8"editor; "
                             u8"failure cleanup and retry after controlled rejection · Independent "
                             u8"multiple/tag/search/strict/RTL/rich-popup Gallery and Linux private Wayland/Vulkan"}},
                  String{u8"兼容 String 与 typed TreeKeys/labeled values、strict/级联 "
                         u8"checkbox、showCheckedStrategy、保留标签 · "
                         u8"真实可移除/自定义 Tag、固定/响应式 maxTagCount、Unicode 文本长度、交互式 "
                         u8"prefix/suffix/popup · 受控 "
                         u8"value/search/open/expanded/loaded 请求、稳定 key 动态富节点与每节点独立异步 Tree · "
                         u8"实际虚拟行、键盘/scrollTo、composition/内层 Escape/Tab、弹层归属与多实例并存 · 真实 "
                         u8"Input/Popover/Tree、独立 Theme、size/status/variant/placement/RTL 和有限 motion · 同值 "
                         u8"Form String "
                         u8"reset 取消查询/异步/待聚焦/弹层而保留编辑器；失败清理和旧拒绝重试合同 · 独立 Gallery "
                         u8"多选/标签/搜索/strict/RTL/富 popup 回调；Linux 私有 Wayland/Vulkan"}},
                 {MessageKey{
                      u8"gallery.ant.component.tree-select.missing",
                      String{
                          u8"Windows MSVC/D3D12, physical IME candidates, actual cross-monitor and OS assistive "
                          u8"technology require independent evidence · DOM/CSS/React/portal, browser DataTransfer and "
                          u8"external OS DND are outside native scope"}},
                  String{u8"Windows MSVC/D3D12、物理系统 IME 候选窗、跨实际显示器和 OS 辅助技术平台证据独立验证 · "
                         u8"DOM/CSS/React/portal、浏览器 DataTransfer 与外部 OS DND 不属于原生接口"}},
                 {MessageKey{u8"gallery.ant.component.upload.name", String{u8"Upload"}}, String{u8"Upload 上传"}},
                 {MessageKey{
                      u8"gallery.ant.component.upload.summary",
                      String{u8"Upload provides a stable controlled file model, actual Image list/preview, native "
                             u8"chooser/drop/paste boundaries and cancelable application transport. No default HTTP "
                             u8"transport is provided."}},
                  String{u8"原生 Upload 已实现稳定受控文件模型、真实 Image 列表/预览、原生宿主选择与 drop/paste "
                         u8"边界及可取消应用传输；不提供默认 HTTP。"}},
                 {MessageKey{
                      u8"gallery.ant.component.upload.supported",
                      String{u8"Typed stable UID/fileList/status/progress/error, controlled authority, "
                             u8"accept/multiple/directory/maxCount and beforeUpload/transform/Keep/Ignore · Explicit "
                             u8"application customRequest, independent progress/success/error/retry/cancel, async "
                             u8"onRemove and "
                             u8"rejection of old/duplicate/foreign-thread results · Native Button/drag area, keyboard, "
                             u8"typed "
                             u8"NativeFileServices, SDL chooser adapter, owned file-drop paths and native file MIME "
                             u8"paste · "
                             u8"Text/Picture/PictureCard/PictureCircle, actual PNG/JPEG/BMP thumbnails/circular clips, "
                             u8"shared "
                             u8"PreviewGroup and preview/download/remove/actions callbacks · Stable UID retains "
                             u8"drafts/preview "
                             u8"transforms, local item/icon renderer changes, typed trigger/action-icon slots and weak "
                             u8"action "
                             u8"handles · disabled/readOnly/visible/size/status, five-domain Upload tokens, "
                             u8"RTL/locale/image_preview, ref and typed Form adapters · Actual Gallery with explicitly "
                             u8"synthetic manual transport; Linux HEADLESS and private Wayland/Vulkan "
                             u8"pixels/input/retirement"}},
                  String{
                      u8"typed stable "
                      u8"UID/fileList/status/progress/error、受控权威、accept/multiple/directory/"
                      u8"maxCount、beforeUpload/"
                      u8"transform/Keep/Ignore · 显式应用 customRequest、独立进度/成功/失败/重试/取消、异步 onRemove "
                      u8"与旧/重复/外线程结果拒绝 · 原生 Button/Drag area、键盘、typed NativeFileServices、SDL chooser "
                      u8"SDK "
                      u8"adapter、拥有路径的 file-drop 与原生文件 MIME paste · "
                      u8"Text/Picture/PictureCard/PictureCircle，实际 "
                      u8"PNG/JPEG/BMP 缩略图/圆形裁剪、共享 PreviewGroup、preview/download/remove/actions callbacks · "
                      u8"稳定 "
                      u8"UID 保留文件草稿/预览变换，局部 item/icon renderer 替换、typed trigger/action-icon slots "
                      u8"与弱生命周期 action handles · disabled/readOnly/visible/size/status、独立五域 Upload "
                      u8"Token、RTL/locale/image_preview、ref 与 typed Form decode/encode/focus adapter · 真实 Gallery "
                      u8"控件，明确标注的手动合成传输；Linux HEADLESS 与私有 Wayland Vulkan 像素/交互/退休验收"}},
                 {MessageKey{u8"gallery.ant.component.upload.missing",
                             String{u8"Actual OS chooser/portal, cross-app clipboard, Windows/MSVC/D3D12, system IME "
                                    u8"and OS AT lack "
                                    u8"platform evidence; SDK mocks do not prove chooser operation · SDL cancellation "
                                    u8"rejects results "
                                    u8"without an SDK OS-request close handle; default host recognizes PNG/JPEG/BMP "
                                    u8"MIME/image bytes, "
                                    u8"with other MIME supplied by applications · HTTP/URL/server response/download IO "
                                    u8"belongs to the "
                                    u8"app; DOM/CSS/React/browser input/portal semantics stay outside Core"}},
                  String{u8"真实 OS chooser/portal、跨应用 clipboard、Windows/MSVC/D3D12、系统 IME 与 OS AT "
                         u8"尚无平台实测；SDK "
                         u8"mock 不代表 OS chooser 通过 · SDL chooser 取消只拒绝结果，SDK 没有 OS request "
                         u8"关闭句柄；默认宿主仅识别 PNG/JPEG/BMP MIME/图片 bytes，其他 MIME 由应用服务提供 · "
                         u8"HTTP/URL/服务器响应/下载 IO 属于应用，DOM/CSS/React/浏览器 input 与 portal 语义不进入 "
                         u8"Core"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.ant.component.avatar.name", String{u8"Avatar"}}, String{u8"Avatar 头像"}},
                 {MessageKey{
                      u8"gallery.ant.component.avatar.summary",
                      String{
                          u8"Avatar/AvatarGroup provide shared Image, explicit cancelable async sources, responsive "
                          u8"logical breakpoints, stable keyed dynamic groups and retained overflow content. Platform "
                          u8"acceptance remains separate."}},
                  String{u8"原生 Avatar/AvatarGroup 已补齐共享 "
                         u8"Image、显式可取消异步、响应式逻辑断点、稳定键动态分组与保留溢出内容；平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.avatar.supported",
                      String{
                          u8"Old text/icon/shape/gap/actual auto-fit and vector Group API · Shared "
                          u8"ImageSource/ImagePixels/EncodedImage/explicit ImageLoader, success/error/cancel/retry and "
                          u8"onError(false) fallback control · Actual circle/rounded-square image shapes and Theme "
                          u8"radius "
                          u8"mapping; closed preview does not take input · Logical-window Breakpoints sizes, "
                          u8"zero/narrow "
                          u8"recovery, RTL and Theme font geometry · Stable keyed add/remove/reorder, inheritance/item "
                          u8"overrides, overlap/draw order and max.count · Native Popover overflow, retained custom "
                          u8"rich "
                          u8"content, actual open callbacks and private Form binding scope · Independent actual "
                          u8"Gallery "
                          u8"image success/error/cancel/retry, responsive grouping and overflow actions"}},
                  String{
                      u8"保留旧文字/图标/shape/gap/真实 auto-fit 与 vector Group API · 共享 "
                      u8"ImageSource/ImagePixels/EncodedImage/显式 ImageLoader、成功/错误/取消/重试及 onError(false) "
                      u8"回退控制 · 真实 circle/rounded-square 图像形状与 Avatar Theme 圆角映射；预览关闭不抢输入 · "
                      u8"窗口逻辑宽度 Breakpoints 尺寸、零/窄空间恢复、RTL、Theme 字体几何 · 稳定 key "
                      u8"add/remove/reorder、继承与逐项覆盖、overlap/绘制顺序、max.count · 原生 Popover "
                      u8"溢出、保留自定义富内容和实际开合通知、Form 私有绑定范围 · 独立实际 Gallery "
                      u8"图像成功/失败/取消重试、响应分组及溢出操作"}},
                 {MessageKey{
                      u8"gallery.ant.component.avatar.missing",
                      String{u8"DOM/CSS/React/portal and Web ref/href/target browser interfaces are outside the native "
                             u8"contract · GPU/actual windows, Windows/MSVC/D3D12, system IME/AT and actual "
                             u8"cross-monitor results remain independent; HEADLESS/Recording do not replace them"}},
                  String{u8"DOM/CSS/React/portal、Web ref/href/target 等浏览器接口不属于原生合同 · "
                         u8"GPU/真实窗口、Windows/MSVC/D3D12、系统 IME/AT 与实际跨显示器结果独立记录，不以 "
                         u8"HEADLESS/Recording "
                         u8"代替"}},
                 {MessageKey{u8"gallery.ant.component.badge.name", String{u8"Badge"}}, String{u8"Badge 徽标数"}},
                 {MessageKey{
                      u8"gallery.ant.component.badge.summary",
                      String{
                          u8"Badge provides arbitrary String/typed rich counts, actual digit rolling, active/visible "
                          u8"Processing and retained Ribbon. The pinned upstream version has no BadgeGroup."}},
                  String{u8"原生 Badge 已补齐任意 String/typed 富计数、实际数字滚动、活动可见 Processing 与 retained "
                         u8"Ribbon；锁定上游无 BadgeGroup。"}},
                 {MessageKey{
                      u8"gallery.ant.component.badge.supported",
                      String{u8"Compatible int count/overflowCount/showZero/dot/status/offset/size, standalone/wrapped "
                             u8"modes · Arbitrary String, finite double and retained typed rich count, RTL and local "
                             u8"factory replacement · Finite directional two-row digit geometry, reverse/new "
                             u8"target/zero-range recovery · Actual Processing phase rendering only requests frames "
                             u8"while visible/active; reduced motion/hidden/inactive/retirement stops it · BadgeRibbon "
                             u8"text/rich content/Start-End/RTL/actual folded corner and independent retained child "
                             u8"actions · Actual Gallery count/rich count/Processing/Ribbon actions and callbacks"}},
                  String{u8"兼容原 int count/overflowCount/showZero/dot/status/offset/size/独立与包裹模式 · 任意 "
                         u8"String、有限 "
                         u8"double 与 retained typed 富 count，RTL 与局部工厂替换 · "
                         u8"数字前后两行的有限有方向中间几何，反向/新目标/零范围恢复 · Processing "
                         u8"实际相位绘制，仅可见活动分支请求帧；reduced motion/隐藏/失活/退役停止 · BadgeRibbon "
                         u8"的文本/富文本/Start-End/RTL/真实折角和独立保留子操作 · 真实 Gallery "
                         u8"数字操作、任意/富计数、Processing 开关、Ribbon 与回调结果"}},
                 {MessageKey{
                      u8"gallery.ant.component.badge.missing",
                      String{u8"DOM/CSS/React/portal and Web ref/href/target browser interfaces are outside the native "
                             u8"contract · GPU/actual windows, Windows/MSVC/D3D12, system IME/AT and actual "
                             u8"cross-monitor results remain independent; HEADLESS/Recording do not replace them"}},
                  String{u8"DOM/CSS/React/portal、Web ref/href/target 等浏览器接口不属于原生合同 · "
                         u8"GPU/真实窗口、Windows/MSVC/D3D12、系统 IME/AT 与实际跨显示器结果独立记录，不以 "
                         u8"HEADLESS/Recording "
                         u8"代替"}},
                 {MessageKey{u8"gallery.ant.component.calendar.name", String{u8"Calendar"}}, String{u8"Calendar 日历"}},
                 {MessageKey{
                      u8"gallery.ant.component.calendar.summary",
                      String{
                          u8"Calendar provides Gregorian date/month views, controlled values/modes, range/locale/week "
                          u8"numbers, retained rich cells, replaceable header, independent Theme and input lifetime."}},
                  String{u8"原生 Calendar：公历日期/月视图、受控值/模式、范围/locale/周编号、保留富单元格和可替换 "
                         u8"header、独立 "
                         u8"Theme 与输入生命周期。"}},
                 {MessageKey{
                      u8"gallery.ant.component.calendar.supported",
                      String{u8"CivilDate years 1..9999, 42-day/12-month grids, mini/fullscreen, closed ranges, "
                             u8"disabledDate, locale/week-year and RTL · Controlled requests, select/panel events, "
                             u8"keyboard and owner/thread-checked refs, explicit clock/Today · Stable date/month keys, "
                             u8"reactive cell/full/date/month/header factories, actual retained Input/Popover, local "
                             u8"replacement and failed-factory rollback · Independent Calendar tokens, scoped "
                             u8"seed/algorithm/inherit, actual header/schedule Gallery and public compilable example"}},
                  String{u8"CivilDate 年 1..9999；42 日期/12 "
                         u8"月份网格、mini/fullscreen、闭区间范围、disabledDate、locale/week "
                         u8"year 与 RTL · 受控请求、选择/面板事件、键盘和 owner/thread checked ref；显式 clock/Today · "
                         u8"稳定日期/月 key、响应 cell/full/date/month/header 工厂；真实 Input/Popover "
                         u8"保留与局部替换/失败回滚 "
                         u8"· 独立 Calendar token、scoped seed/algorithm/inherit；实际 header/日程 Gallery "
                         u8"与公开可编译例子"}},
                 {MessageKey{
                      u8"gallery.ant.component.calendar.missing",
                      String{u8"DOM/CSS/React, browser portal and OS assistive APIs are excluded · HEADLESS "
                             u8"Core/logical-scene evidence does not prove Windows/MSVC, GPU, actual desktop or system "
                             u8"IME operation; platform integration is recorded independently in 63.3/64.3"}},
                  String{u8"DOM/CSS/React、浏览器 portal 与 OS 辅助技术接口不属于原生 API · HEADLESS "
                         u8"Core/逻辑场景证据不代表 "
                         u8"Windows/MSVC、GPU、实际桌面或系统 IME 实测；平台集成由 63.3/64.3 单独记录"}},
                 {MessageKey{u8"gallery.ant.component.card.name", String{u8"Card"}}, String{u8"Card 卡片"}},
                 {MessageKey{u8"gallery.ant.component.card.summary",
                             String{u8"Native Card supports Grid, Meta, retained Tabs, Image covers, bounded loading "
                                    u8"animation and shared rounded composition of ordinary descendants."}},
                  String{u8"原生 Card 支持 Grid、Meta、保留 Tabs、Image cover、有限 loading "
                         u8"动画与任意普通后代的共同圆角合成。"}},
                 {MessageKey{
                      u8"gallery.ant.component.card.supported",
                      String{
                          u8"Typed reactive content, Grid/Meta/keyed Tabs and shared Image covers; retained editors "
                          u8"and bounded loading motion; optional shared rounded composition of text, images, effects, "
                          u8"child shadows and focus rings, with matching hit geometry and atomic resource retry."}},
                  String{u8"typed reactive Props and body/title/extra/cover/actions slots；default/small, "
                         u8"outlined/borderless and hover across nested controls；retained loading placeholder with "
                         u8"body focus/input/popup cleanup and editor state restoration；Card Component Token, precise "
                         u8"invalidation, logical translation/clipping and idle cleanup；Grid/Meta/keyed Tabs, shared "
                         u8"Image cover and bounded loading pulse；optional rounded descendant composition across "
                         u8"Quad/Glyph/Image/Effect, child shadows and focus rings; retained editors and rounded hit "
                         u8"geometry；ancestor hard clip updates, nested mask coverage and atomic resource "
                         u8"failure/retry; cold path allocates no composition resources"}},
                 {MessageKey{u8"gallery.ant.component.card.missing",
                             String{u8"Web DOM/CSS interfaces are outside the native API. System IME and OS "
                                    u8"accessibility adapters require their own platform evidence."}},
                  String{u8"Web/DOM/CSS interfaces are outside the native API; system IME and OS accessibility "
                         u8"adapters require their own platform evidence"}},
                 {MessageKey{u8"gallery.ant.component.carousel.name", String{u8"Carousel"}},
                  String{u8"Carousel 走马灯"}},
                 {MessageKey{
                      u8"gallery.ant.component.carousel.summary",
                      String{u8"Carousel provides stable keyed retained panels, controlled transitions, actual "
                             u8"ScrollX/Fade, dragging/keyboard/direction, native controls and finite autoplay."}},
                  String{u8"原生 Carousel：稳定键保留面板、受控切换、实际 "
                         u8"ScrollX/Fade、拖动/键盘/方向、原生控件与有限自动播放。"}},
                 {MessageKey{
                      u8"gallery.ant.component.carousel.supported",
                      String{u8"Typed Props/slots/Ref, controlled rejection, exact before/after callbacks, dynamic "
                             u8"keys and "
                             u8"local content replacement/rollback; infinite boundaries never clone stateful subtrees "
                             u8"· Actual "
                             u8"translation/opacity within logical clips, adaptive height, arrows/dots/custom paging, "
                             u8"RTL/vertical, background dragging and native-editor isolation · Monotonic deadlines, "
                             u8"explicit "
                             u8"dot progress, hover/focus/hidden/inactive/reduced-motion pauses · Independent "
                             u8"Theme/scoped "
                             u8"seed/algorithm/inherit and two separate Gallery examples"}},
                  String{u8"typed Props/slots/Ref；受控拒绝、准确 before/after、动态 key "
                         u8"与局部内容替换/回滚；无限边界不克隆状态子树 · 逻辑裁剪内实际平移/透明度、adaptive "
                         u8"height、箭头/dot/custom paging、RTL/纵向、背景拖动与原生编辑器输入隔离 · 单调时钟 "
                         u8"deadline、显式 "
                         u8"dot progress、hover/focus/隐藏/失活/reduced motion 暂停；独立 Theme/scoped "
                         u8"seed/algorithm/inherit；两个独立 Gallery 样例"}},
                 {MessageKey{
                      u8"gallery.ant.component.carousel.missing",
                      String{
                          u8"React/DOM/CSS/nativeElement/innerSlider, browser portals and react-slick Web settings "
                          u8"are outside the single-viewport native API; see docs/carousel.md · HEADLESS/CPU "
                          u8"input/scene evidence does not prove GPU, Windows/MSVC, actual desktop, system IME or OS "
                          u8"assistive technology; parent accepts 63.3/64.3 independently"}},
                  String{u8"React/DOM/CSS/nativeElement/innerSlider、浏览器 portal 及 react-slick Web settings "
                         u8"插件不属于单 "
                         u8"viewport 原生 API；详见 docs/carousel.md · HEADLESS/CPU 场景与输入证据不代表 "
                         u8"GPU、Windows/MSVC、实际桌面、系统 IME 或 OS 辅助技术实测；63.3/64.3 由父线程单独验收"}},
                 {MessageKey{u8"gallery.ant.component.collapse.name", String{u8"Collapse"}},
                  String{u8"Collapse 折叠面板"}},
                 {MessageKey{
                      u8"gallery.ant.component.collapse.summary",
                      String{
                          u8"Collapse provides stable dynamic panels, local slot replacement, "
                          u8"lazy/forceRender/destroy policies, finite height/arrow motion, ghost/gates/RTL. The old "
                          u8"vector API and drafts remain; platform results remain separate."}},
                  String{u8"原生 Collapse 已补齐稳定 keyed 动态面板、局部 slot 替换、lazy/forceRender/destroy "
                         u8"策略、实际有限高度/箭头 motion、ghost/门控/RTL；保留旧 vector "
                         u8"与草稿行为，平台证据独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.collapse.supported",
                      String{
                          u8"stable keyed reactive panel add/remove/reorder and explicit title/body/extra factory "
                          u8"revisions · controlled activeKeys/accordion, uncontrolled defaults and independent rich "
                          u8"extra actions · explicit lazy with eager source compatibility, forceRender precedence "
                          u8"and destroyOnHidden at settled boundary · finite clipped height and rotating arrow "
                          u8"motion with reverse, immediate input/popup retirement and idle cancellation · "
                          u8"bordered/borderless/ghost, three sizes, custom expand icon and Start/End placement, "
                          u8"Header/Icon/Disabled gates and RTL · retained FormItem/Input/Select/Popover composition "
                          u8"and actual interactive Linux Wayland/Vulkan Gallery sequence"}},
                  String{u8"稳定键控响应面板增删重排，明确标题/正文/extra 工厂 revision；受控 "
                         u8"activeKeys/accordion、默认值及独立富 extra；显式 lazy 保留 eager 兼容，forceRender "
                         u8"优先、稳定边界 "
                         u8"destroyOnHidden；有限裁剪高度/旋转箭头、反向、中止输入/弹层及 "
                         u8"idle；边框/无边框/ghost、三尺寸、图标/Start-End、Header/Icon/Disabled 门控和 RTL；保留 "
                         u8"FormItem/Input/Select/Popover 与实际 Linux Wayland/Vulkan Gallery。"}},
                 {MessageKey{
                      u8"gallery.ant.component.collapse.missing",
                      String{
                          u8"HTML/DOM/CSS/React compatibility aliases and OS accessibility/screen-reader platform "
                          u8"bridges are outside this native component contract · Windows/MSVC, physical system IME, "
                          u8"assistive technology and cross-monitor/manual desktop acceptance are not claimed by "
                          u8"Linux HEADLESS/private Wayland evidence"}},
                  String{
                      u8"HTML/DOM/CSS/React 别名及 OS 辅助技术/屏幕阅读器桥不属本组件合同；Linux HEADLESS/私有 Wayland "
                      u8"不代表 Windows/MSVC、物理 IME、辅助技术或实际桌面/跨显示器验收。"}},
                 {MessageKey{u8"gallery.ant.component.descriptions.name", String{u8"Descriptions"}},
                  String{u8"Descriptions 描述列表"}},
                 {MessageKey{
                      u8"gallery.ant.component.descriptions.summary",
                      String{u8"Descriptions provides stable dynamic items, rich headers, local factory replacement, "
                             u8"responsive columns/span/fill, locale/RTL and native measured equal-column layout. "
                             u8"Platform results remain separate."}},
                  String{u8"原生 Descriptions 已补齐稳定 keyed 动态条目与富 "
                         u8"header、局部工厂替换、窗口断点列/span/fill、locale/"
                         u8"RTL；等分列与真实测量提供原生表格布局，平台证据独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.descriptions.supported",
                      String{
                          u8"stable keyed dynamic items with retained label/content/title/extra and local factory "
                          u8"replacement · responsive columns and spans from shared logical window width, custom "
                          u8"Breakpoints, fill-row remainder and wrapping · whole-update key/span/column/metric "
                          u8"validation "
                          u8"and failed-factory owner retirement · horizontal/vertical labels, bordered/unbordered, "
                          u8"three "
                          u8"sizes, locale separator/colon and RTL geometry · partition paint/hit/focus clipping, "
                          u8"actual "
                          u8"retained rich Input/FormItem/Select/Popover and independent header actions · interactive "
                          u8"breakpoint/rich-cell Gallery and Linux Wayland/Vulkan retirement sequence"}},
                  String{u8"稳定键控动态条目、保留 label/content/title/extra "
                         u8"和局部工厂替换；共享窗口逻辑宽度的响应列/span、自定义 Breakpoints、填满剩余行与换行；整次 "
                         u8"key/span/column/metric 验证、失败工厂退役；横纵标签、边框/三尺寸、locale 分隔/冒号及 "
                         u8"RTL；分区绘制/命中/焦点裁剪、保留富 "
                         u8"Input/FormItem/Select/Popover、独立页头操作；响应断点/富单元格 "
                         u8"Gallery 与 Linux Wayland/Vulkan 退役序列。"}},
                 {MessageKey{
                      u8"gallery.ant.component.descriptions.missing",
                      String{
                          u8"HTML/DOM/CSS/React compatibility aliases and OS accessibility/screen-reader platform "
                          u8"bridges are outside this native component contract · Windows/MSVC, physical system IME, "
                          u8"assistive technology and cross-monitor/manual desktop acceptance are not claimed by "
                          u8"Linux HEADLESS/private Wayland evidence · HTML auto-table/CSS sizing is mapped to "
                          u8"documented native measured equal columns/span/fill allocation"}},
                  String{u8"HTML/DOM/CSS/React 别名及 OS 辅助技术/屏幕阅读器桥不属本合同；Linux HEADLESS/私有 Wayland "
                         u8"不代表 "
                         u8"Windows/MSVC、物理 IME、辅助技术、实际桌面/跨显示器；HTML 自动表格/CSS "
                         u8"尺寸对应已说明的原生等分列/span/fill 分配。"}},
                 {MessageKey{u8"gallery.ant.component.empty.name", String{u8"Empty"}}, String{u8"Empty 空状态"}},
                 {MessageKey{
                      u8"gallery.ant.component.empty.summary",
                      String{u8"Empty's optional native vector illustration, shared ImageSource and region-specific "
                             u8"dynamic slots passed MSVC HEADLESS Debug/Release and focused Windows hidden D3D12 "
                             u8"checks. Other platforms and Web boundaries remain separate."}},
                  String{u8"Empty 的可选原生矢量插画、共同 ImageSource 与区域级动态 slots 已通过 MSVC HEADLESS "
                         u8"Debug/Release 和 "
                         u8"Windows hidden D3D12 定向检查；其他平台及 Web 边界独立记录。"}},
                 {MessageKey{u8"gallery.ant.component.empty.supported",
                             String{u8"typed EmptyProps, reactive description/image and retained image/extra slots · "
                                    u8"Default/Compact/None "
                                    u8"native Icon presentation, empty-description collapse and finite glyph clipping "
                                    u8"· Theme component "
                                    u8"colors/metrics, zero/subpixel built-in image sizes and custom visual margins · "
                                    u8"retained native "
                                    u8"Button/Input identities across presentation and Theme updates · 076 Gallery "
                                    u8"source: default create "
                                    u8"action, image hide/recovery, Compact and custom 100 dp Icon with 50 dp side "
                                    u8"margins · 107 native "
                                    u8"illustration, shared ImageSource completion/failure/retry and retained dynamic "
                                    u8"regional slots"}},
                  String{u8"typed EmptyProps、响应 description/image、保留 image/extra；Default/Compact/None "
                         u8"原生图标、空说明收起及有限字形裁剪；Theme "
                         u8"颜色/尺寸、零和亚像素内置图像、自定义视觉边距；显示/Theme "
                         u8"更新保留 Button/Input；076 创建操作、图像显隐恢复、Compact 和 100dp 图标/50dp 侧边距；107 "
                         u8"原生插画、共享图像完成/失败/重试及保留区域动态 slot。"}},
                 {MessageKey{
                      u8"gallery.ant.component.empty.missing",
                      String{
                          u8"browser pixel-exact SVG/URL behavior and automatic network/state services; upstream "
                          u8"Empty has no standalone animation API · Web DOM/CSS/React aliases and screen-reader "
                          u8"semantic bridge · 076 Gallery runtime acceptance, component GPU/manual visual, Linux and "
                          u8"system IME evidence are recorded independently"}},
                  String{u8"浏览器像素精确 SVG/URL 及自动网络/状态服务不属原生范围；上游 Empty 无独立动画 API；Web "
                         u8"DOM/CSS/React "
                         u8"别名与屏幕阅读器桥不提供；076 Gallery 运行、GPU、人工视觉、Linux 和系统 IME "
                         u8"证据独立记录。"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.ant.component.image.name", String{u8"Image"}}, String{u8"Image 图片"}},
                 {MessageKey{u8"gallery.ant.component.image.summary",
                             String{u8"Image/PreviewGroup provides actual RGBA decoding/shared GPU resources, "
                                    u8"application loaders, "
                                    u8"controlled preview, gestures and retained toolbars. Platform boundaries remain "
                                    u8"separate."}},
                  String{u8"原生 Image/PreviewGroup 已实现真实 RGBA 解码/共享 GPU 资源、应用 "
                         u8"loader、受控预览、手势与保留工具栏；平台边界独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.image.supported",
                      String{u8"Immutable encoded/decoded ImageSource and explicit app loaders, actual PNG/JPEG/BMP "
                             u8"pixels, "
                             u8"loading/placeholder/error/fallback, alt/native labels · Independent RGBA8 Image ABI "
                             u8"2/resource/sampling, logical CPU scene v3, shared transactions/owner/epoch/retry; "
                             u8"Quad/Glyph/RoundedEffect ABI v2 unchanged · Contain/Cover/Fill/None/ScaleDown, "
                             u8"size/ratio, actual "
                             u8"circular clip and bounds/stride/bytes/overflow/input-limit checks · Stable "
                             u8"PreviewGroup keys, "
                             u8"controlled current/open, atomic items/current updates, empty-list recovery and "
                             u8"retained transforms "
                             u8"· Keyboard/pointer zoom/rotate/flip/drag/clamp/reset/navigation, RTL/locale, retained "
                             u8"action "
                             u8"renderer and focus/IME/capture cleanup · Owner-thread completion, "
                             u8"stale/duplicate/canceled "
                             u8"rejection, loader reentry/throw/self-destruction and external-handle lifetime · "
                             u8"Five-domain Image "
                             u8"tokens, complete Theme golden, actual Gallery and Linux private Wayland/Vulkan/SPIR-V "
                             u8"readback"}},
                  String{
                      u8"不可变 encoded/decoded ImageSource 与显式应用 loader，PNG/JPEG/BMP "
                      u8"实际像素、loading/placeholder/error/fallback、alt 与原生标签 · 独立 RGBA8 Image ABI "
                      u8"2/资源/采样路径，logical CPU scene "
                      u8"v3、共同事务/owner/epoch/失败重试；Quad/Glyph/RoundedEffect ABI "
                      u8"v2 保持不变 · Contain/Cover/Fill/None/ScaleDown、尺寸/比例、真实圆形 "
                      u8"clip、边界/stride/bytes/溢出与输入上限校验 · PreviewGroup 稳定 keys、受控 current/open、原子 "
                      u8"items/current 更新、动态空列表恢复与保留当前变换 · "
                      u8"键盘/指针预览、zoom/rotate/flip/drag/clamp/reset/navigation、RTL、locale、retained actions "
                      u8"renderer "
                      u8"与 focus/IME/capture 清理 · owner-thread completion、旧/重复/取消结果拒绝、loader "
                      u8"重入/throw/self-destroy 和外部 action 生命周期保护 · 独立五域 Image Component Token、完整 "
                      u8"Theme "
                      u8"golden、真实 Gallery 控件及 Linux 私有 Wayland Vulkan/SPIR-V 读回"}},
                 {MessageKey{
                      u8"gallery.ant.component.image.missing",
                      String{
                          u8"Windows/MSVC/D3D12, system IME and OS assistive technology require independent platform "
                          u8"evidence · File/URL IO belongs to host/app; Core does not fetch networks, JPEG EXIF "
                          u8"orientation is app-normalized and SVG/animation returns unsupported_format · "
                          u8"DOM/CSS/HTML/React and browser img/portal semantics are excluded"}},
                  String{
                      u8"Windows/MSVC/D3D12、系统 IME 与 OS 辅助技术平台证据独立验证 · 文件/URL IO 属于宿主/应用；Core "
                      u8"不抓取网络，JPEG EXIF 方向由应用规范化；SVG/动画格式返回 unsupported_format · "
                      u8"DOM/CSS/HTML/React "
                      u8"与浏览器 img/portal 语义不属于原生接口"}},
                 {MessageKey{u8"gallery.ant.component.list.name", String{u8"List"}}, String{u8"List 列表"}},
                 {MessageKey{
                      u8"gallery.ant.component.list.summary",
                      String{
                          u8"List remains deprecated. The separate migration page points to Listy's typed keys, item "
                          u8"slots, grid, pagination and scrolling; it does not revive the old List control."}},
                  String{u8"List 保持 deprecated；107 新增独立迁移说明页，指向 Listy 的 typed keys、item "
                         u8"slots、网格、分页及滚动，不复活旧 List 控件。"}},
                 {MessageKey{
                      u8"gallery.ant.component.list.supported",
                      String{u8"Pinned offline metadata and the native migration path in docs/list-migration.md; the "
                             u8"separate explanation page passed catalog/page contracts"}},
                  String{
                      u8"锁定版本离线元数据与 docs/list-migration.md 的原生替代路径；独立说明页已通过目录与页面合同"}},
                 {MessageKey{u8"gallery.ant.component.list.missing",
                             String{u8"No new List control is provided; see the separate Listy page for the native "
                                    u8"replacement scope"}},
                  String{u8"不提供新的 List 控件；原生替代范围见 Listy 独立页"}},
                 {MessageKey{u8"gallery.ant.component.listy.name", String{u8"Listy"}}, String{u8"Listy 高性能列表"}},
                 {MessageKey{
                      u8"gallery.ant.component.listy.summary",
                      String{
                          u8"Listy stable keys, retained rows, direction/grid, pagination, scrolling and empty states "
                          u8"passed MSVC HEADLESS Debug/Release and focused Windows hidden D3D12 checks. Other "
                          u8"platforms remain separate."}},
                  String{
                      u8"Listy 的稳定 key、保留行、方向/网格、分页、滚动及空状态已通过 MSVC HEADLESS Debug/Release 和 "
                      u8"Windows hidden D3D12 定向检查；其他平台独立待验。"}},
                 {MessageKey{u8"gallery.ant.component.listy.supported",
                             String{u8"Typed owned data, stable keys and retained item slots · Vertical/horizontal, "
                                    u8"RTL, equal-width "
                                    u8"grid, actual pagination, bounded viewport scrolling and empty state · Windows "
                                    u8"MSVC HEADLESS "
                                    u8"Debug/Release contracts for retained editing, data reorder, disabled state, "
                                    u8"controlled paging "
                                    u8"and failure cleanup · Windows hidden D3D12 captures in four configurations for "
                                    u8"retained drafts, "
                                    u8"reorder, grid, direction, scrolling and empty state"}},
                  String{
                      u8"typed owned data、稳定 key 与 retained item slots · "
                      u8"纵横方向、RTL、等宽网格、真实分页、有限视口滚动和空状态 · Windows MSVC HEADLESS Debug/Release "
                      u8"的保留编辑、数据重排、禁用、受控分页、失败清理合同 · Windows hidden D3D12 "
                      u8"四配置的保留草稿、重排、网格、方向、滚动及空态捕获"}},
                 {MessageKey{u8"gallery.ant.component.listy.missing",
                             String{u8"Linux native GPU/window, system IME and AT evidence remain independent · "
                                    u8"Group/sticky/virtual and Web DOM/CSS/React compatibility are not provided"}},
                  String{
                      u8"Linux 原生 GPU/窗口、系统 IME 和辅助技术独立待验；group/sticky/virtual 及 Web DOM/CSS/React "
                      u8"兼容未提供。"}},
                 {MessageKey{u8"gallery.ant.component.popover.name", String{u8"Popover"}},
                  String{u8"Popover 气泡卡片"}},
                 {MessageKey{
                      u8"gallery.ant.component.popover.summary",
                      String{u8"Popover supports retained rich content, combined/context-menu triggers, RTL, finite "
                             u8"motion and content scrolling. Focused Windows and native acceptance are recorded."}},
                  String{u8"原生 Popover 支持保留富内容、组合/右键触发、RTL、有限动画及内容滚动，Windows "
                         u8"定向与原生验收已记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.popover.supported",
                      String{
                          u8"typed trigger/title/content slots and reactive string title/content with retained native "
                          u8"editors and controls · controlled/default visibility, hover/focus/click combinations, "
                          u8"finite "
                          u8"delays and keyboard/outside closing · twelve placements, arrows, viewport clipping and "
                          u8"independent Theme inheritance · Modal/Spin/hidden suppression, child-popup priority, "
                          u8"content-first wheel routing and callback lifetime checks · interactive Gallery: real note "
                          u8"editing/save, placements, arrows and disabled recovery from the catalog · "
                          u8"contextMenu/composed "
                          u8"triggers, RTL/wireframe, finite motion and retained scrolling content"}},
                  String{u8"typed 触发器/标题/正文 slot 与响应式 "
                         u8"String，保留原生编辑器/控件；受控/默认显隐、悬停/焦点/点击组合、有限延迟及键盘/"
                         u8"外部关闭；十二方位、箭头、视口裁剪及独立 "
                         u8"Theme；Modal/Spin/隐藏抑制、子弹层优先、内容优先滚轮及生命周期检查；目录关联的编辑/"
                         u8"保存、方位/箭头/"
                         u8"禁用恢复；右键/组合、RTL/wireframe、有限动画和保留滚动正文。"}},
                 {MessageKey{
                      u8"gallery.ant.component.popover.missing",
                      String{
                          u8"Linux/Wayland/Vulkan, system IME and platform accessibility-adapter coverage need their "
                          u8"own "
                          u8"acceptance · Complete Ant Design Web/DOM/CSS APIs are outside the current native subset"}},
                  String{u8"Linux/Wayland/Vulkan、系统 IME 与平台 accessibility adapter 的本阶段覆盖仍待各自验收 · "
                         u8"完整 Ant "
                         u8"Design Web/DOM/CSS API 不在当前原生子集内"}},
                 {MessageKey{u8"gallery.ant.component.qr-code.name", String{u8"QRCode"}}, String{u8"QRCode 二维码"}},
                 {MessageKey{
                      u8"gallery.ant.component.qr-code.summary",
                      String{u8"QRCode supports Model 2, typed reactive APIs, Theme, four status actions, shared logos "
                             u8"and deterministic PNG/SVG/RGBA export. Actual Gallery, independent decoding and Linux "
                             u8"private Wayland/Vulkan pixel checks passed."}},
                  String{
                      u8"原生 QRCode 已实现标准 Model 2 编码、typed 响应 API、Theme、四类状态操作、共享 logo 与确定性 "
                      u8"PNG/SVG/RGBA 导出；实际 Gallery、独立解码及 Linux 私有 Wayland/Vulkan 像素验证通过。"}},
                 {MessageKey{u8"gallery.ant.component.qr-code.supported",
                             String{u8"QR Model 2 versions 1–40, numeric/alphanumeric/UTF8 ECI/binary/segmented "
                                    u8"payloads, L/M/Q/H, boost "
                                    u8"and standard masks · reactive value/size/margin/colors/status, typed status "
                                    u8"slot/context and "
                                    u8"generation-safe ref · Active/Expired/Loading/Scanned overlays and controlled "
                                    u8"application refresh "
                                    u8"requests · shared ImageSource/ImagePixels logo loading, owner-thread "
                                    u8"completion/cancellation and "
                                    u8"clipping · deterministic actual PNG/SVG/RGBA data export with colors, quiet "
                                    u8"zone and logo alpha · "
                                    u8"owned QRCode Theme tokens and real interactive Gallery controls"}},
                  String{u8"QR Model 2 版本 1–40，数字/字母数字/UTF8 ECI/二进制/分段 payload，L/M/Q/H、boost 和标准 "
                         u8"mask；响应值/尺寸/边距/颜色/状态、typed 状态 slot/context 及代际安全 "
                         u8"ref；Active/Expired/Loading/Scanned 遮罩与受控刷新请求；共享 ImageSource/ImagePixels "
                         u8"logo、所有者线程完成/取消及裁剪；确定性真实 PNG/SVG/RGBA 导出，含颜色/静区/logo "
                         u8"alpha；独立 Token "
                         u8"与真实 Gallery 操作。"}},
                 {MessageKey{
                      u8"gallery.ant.component.qr-code.missing",
                      String{
                          u8"Web DOM/CSS/React/HTML/canvas refs, URL icon fetching and compatibility aliases are "
                          u8"excluded; canvas/svg map to native data export · Windows/MSVC/D3D12, physical IME/AT and "
                          u8"actual cross-monitor checks remain unverified"}},
                  String{u8"Web DOM/CSS/React/HTML/canvas refs、URL icon fetching 与兼容别名不属于原生合同；canvas/svg "
                         u8"类型映射为原生数据导出 API · Windows/MSVC/D3D12、物理 IME/AT 与真实跨显示器未验证"}},
                 {MessageKey{u8"gallery.ant.component.segmented.name", String{u8"Segmented"}},
                  String{u8"Segmented 分段控制器"}},
                 {MessageKey{
                      u8"gallery.ant.component.segmented.summary",
                      String{
                          u8"Segmented provides dynamic stable keyed/value options, String/number authority, finite "
                          u8"motion of a measured pill, RTL/Round and independent rich child controls. The old String "
                          u8"API, shadows and focus theme remain; platform results remain separate."}},
                  String{
                      u8"原生 Segmented 已补齐稳定 key/value 动态选项、String/number typed authority、实际测量 pill "
                      u8"有限过渡、RTL/Round 与独立富子控件；保留旧 String 调用、阴影及焦点主题，平台证据独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.segmented.supported",
                      String{u8"stable keyed reactive option topology with strict String/number values and "
                             u8"controlled/default "
                             u8"authority · item visibility/disabled, uncontrolled removed-value fallback and "
                             u8"controlled refusal "
                             u8"with roving keyboard focus · horizontal/vertical/block, three sizes, Default/Round "
                             u8"shape, RTL "
                             u8"geometry and keyboard direction · finite pill motion between measured selected "
                             u8"rectangles with "
                             u8"reverse, resize/hidden/inactive/reduced-motion cancellation · local label/icon factory "
                             u8"revisions, "
                             u8"retained rich controls, selected shadow/focus Theme and pure-color stability · actual "
                             u8"nested "
                             u8"FormItem/Input/Select/Popover and interactive Linux Wayland/Vulkan typed RTL Gallery"}},
                  String{u8"稳定 key 的响应选项拓扑、严格 String/number "
                         u8"及受控/默认权威；逐项显隐/禁用、非受控移除回退、受控拒绝与游走焦点；横纵/"
                         u8"block、三尺寸、Default/"
                         u8"Round、RTL 几何/键盘；实测选中矩形之间有限 pill "
                         u8"动画，反向/resize/隐藏/失活/减弱取消；标签/图标局部工厂 "
                         u8"revision、保留富子控件、选中阴影/焦点 Theme "
                         u8"及纯颜色稳定；真实 FormItem/Input/Select/Popover 与 Linux Wayland/Vulkan Gallery。"}},
                 {MessageKey{
                      u8"gallery.ant.component.segmented.missing",
                      String{
                          u8"HTML/DOM/CSS/React compatibility aliases and OS accessibility/screen-reader platform "
                          u8"bridges are outside this native component contract · Windows/MSVC, physical system IME, "
                          u8"assistive technology and cross-monitor/manual desktop acceptance are not claimed by "
                          u8"Linux HEADLESS/private Wayland evidence"}},
                  String{u8"HTML/DOM/CSS/React 别名及 OS 辅助技术/屏幕阅读器桥不属本合同；Linux HEADLESS/私有 Wayland "
                         u8"不代表 "
                         u8"Windows/MSVC、物理 IME、辅助技术或实际桌面/跨显示器验收。"}},
                 {MessageKey{u8"gallery.ant.component.statistic.name", String{u8"Statistic"}},
                  String{u8"Statistic 统计数值"}},
                 {MessageKey{
                      u8"gallery.ant.component.statistic.summary",
                      String{u8"Statistic supports Countdown/Countup Timer, scoped locale and finite value animation. "
                             u8"Focused Windows and native acceptance are recorded."}},
                  String{u8"原生 Statistic 支持 Timer 倒计时/正计时、局部 locale 与有限数值动画，Windows "
                         u8"定向及原生验收已记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.statistic.supported",
                      String{u8"Typed numeric or string values, exact Decimal formatting, precision and separators, "
                             u8"formatters, retained rich content and loading; Countdown/Countup, explicit locale, "
                             u8"inherited ConfigProvider formatLocale and finite value animation."}},
                  String{u8"int64/uint64/double/String、precision、分隔符、formatter、富内容和 loading；Gallery "
                         u8"目录直达真实统计值及富 Input、精确业务值反馈和加载恢复；Countdown/Countup, explicit "
                         u8"reactive StatisticLocale and finite value animation；inherited ConfigProvider formatLocale "
                         u8"and shared exact Decimal number formatting"}},
                 {MessageKey{
                      u8"gallery.ant.component.statistic.missing",
                      String{u8"Linux/Wayland/Vulkan, system IME and OS accessibility adapters require their own "
                             u8"platform evidence. Full Ant Design Web DOM/CSS APIs are outside the native subset."}},
                  String{u8"Linux/Wayland/Vulkan、系统 IME 与平台 accessibility adapter 的本阶段覆盖仍待各自验收；完整 "
                         u8"Ant Design Web/DOM/CSS API 不在当前原生子集内"}},
                 {MessageKey{u8"gallery.ant.component.table.name", String{u8"Table"}}, String{u8"Table 表格"}},
                 {MessageKey{
                      u8"gallery.ant.component.table.summary",
                      String{u8"Native Table supports retained virtual rows, tree/details, filters, fixed columns, RTL "
                             u8"and draggable scrollbars. Windows validation is recorded separately."}},
                  String{u8"原生 Table 支持保留虚拟行、树形与详情、筛选、固定列、RTL 和可拖动滚动条；Windows "
                         u8"验证单独记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.table.supported",
                      String{
                          u8"Typed retained rows/cells/headers; controlled sorting and selection; bounded scroll and "
                          u8"Pagination; virtual rows with visited rich-row caching; tree/detail expansion, "
                          u8"filtering, fixed columns and responsive retained editing."}},
                  String{u8"typed "
                         u8"保留行、单元格与表头；受控排序和选择；有界滚动与分页；虚拟行和已访问富行缓存；树形与详情展"
                         u8"开、筛选、固定列及响应式保留编辑。"}},
                 {MessageKey{
                      u8"gallery.ant.component.table.missing",
                      String{u8"Visited virtual-row cache memory grows with access. Dynamic column schemas, complex "
                             u8"merged cells and server data pipelines remain application concerns. Linux, system IME, "
                             u8"accessibility and Web APIs are recorded separately."}},
                  String{u8"已访问虚拟行缓存随访问增长；动态列模式、复杂合并单元格和服务端数据管线由应用负责；Linux、系"
                         u8"统 IME、辅助功能及 Web API 单独记录。"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.ant.component.tag.name", String{u8"Tag"}}, String{u8"Tag 标签"}},
                 {MessageKey{
                      u8"gallery.ant.component.tag.summary",
                      String{
                          u8"Tag/CheckableTag provide finite entry/exit, cancelable closing, controlled/default "
                          u8"selection "
                          u8"and stable keyed drag reorder. Actual rich editors retain their state when reordered."}},
                  String{u8"原生 Tag/CheckableTag "
                         u8"已补齐有限进退场、可取消关闭、受控/默认选择及稳定键拖排组；真实富编辑器在重排时保留。"}},
                 {MessageKey{
                      u8"gallery.ant.component.tag.supported",
                      String{
                          u8"Compatible color/border/icon/disabled API, typed Outlined/Filled/Solid, RTL and close "
                          u8"slot · "
                          u8"Finite entry/exit and reduced motion; closing immediately blocks "
                          u8"focus/capture/input/owned "
                          u8"popups, then retains or destroys under destroyOnClose · "
                          u8"Cancelable/reentrant/self-destroying/throwing close callbacks and controlled "
                          u8"visible/checked "
                          u8"rejection · Actual upstream CheckableTagGroup, TagGroup maps to the draggable demo "
                          u8"without a "
                          u8"Web alias · Stable keyed items, retained Input drafts/selection/refs; removal/factory "
                          u8"revisions "
                          u8"affect only the owned key · Independent handle capture/5 dp threshold/visible "
                          u8"geometry/cancel/inactive cleanup, left/right reorder and roving focus · Controlled "
                          u8"onReorder "
                          u8"proposes order accepted only after app writeback; private Form scope and explicit nested "
                          u8"binding · Actual cancellation/selection/drag accept/reject/rich-draft Gallery"}},
                  String{u8"兼容原颜色/边框/icon/disabled API；typed Outlined/Filled/Solid、RTL 与 close slot · 有限 "
                         u8"enter/exit "
                         u8"与 reduced motion，close 立即屏蔽焦点/捕获/输入会话/owned popup，按保留或 destroyOnClose "
                         u8"边界清理 · "
                         u8"可取消、重入、自销毁和抛异常关闭回调；显式 visible/checked 受控拒绝保持已接受值 · "
                         u8"CheckableTagGroup "
                         u8"真实上游对应；TagGroup 映射 draggable demo，无 Web 组件别名 · 稳定 key 动态 items、保留富 "
                         u8"Input "
                         u8"草稿/选择/ref，删除或工厂 revision 只影响自有 key · 独立手柄捕获/5dp "
                         u8"阈值/精确可见几何/取消与失活清理、左右键重排及 roving focus · 受控 onReorder "
                         u8"只发候选顺序，数据写回后接受；Form 私有范围和显式嵌套绑定 · 真实 Gallery "
                         u8"关闭取消、受控选择、拖排接受/拒绝和富草稿"}},
                 {MessageKey{
                      u8"gallery.ant.component.tag.missing",
                      String{u8"DOM/CSS/React/portal and Web ref/href/target browser interfaces are outside the native "
                             u8"contract · GPU/actual windows, Windows/MSVC/D3D12, system IME/AT and actual "
                             u8"cross-monitor results remain independent; HEADLESS/Recording do not replace them"}},
                  String{u8"DOM/CSS/React/portal、Web ref/href/target 等浏览器接口不属于原生合同 · "
                         u8"GPU/真实窗口、Windows/MSVC/D3D12、系统 IME/AT 与实际跨显示器结果独立记录，不以 "
                         u8"HEADLESS/Recording "
                         u8"代替"}},
                 {MessageKey{u8"gallery.ant.component.timeline.name", String{u8"Timeline"}},
                  String{u8"Timeline 时间轴"}},
                 {MessageKey{
                      u8"gallery.ant.component.timeline.summary",
                      String{
                          u8"Timeline provides horizontal/vertical layouts, stable keyed dynamic items, "
                          u8"titleSpan/RTL, rich nodes, typed semantic colors, actual loading spin and finite color "
                          u8"transitions. The old vertical API and drafts remain; platform results remain separate."}},
                  String{u8"原生 Timeline 已补齐实际横纵布局、稳定 keyed 动态条目、titleSpan/RTL、富节点与 typed "
                         u8"语义色、真实 "
                         u8"loading spin 及有限状态色过渡；保留旧竖向调用与草稿，平台证据独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.timeline.supported",
                      String{u8"real Horizontal/Vertical Start/End/Alternate placements, reverse, titleSpan "
                             u8"fraction/logical/24-column allocation and RTL · stable keyed reactive item "
                             u8"add/remove/reorder with local title/content/icon revisions and retained drafts · rich "
                             u8"dots/titles/content, typed colors/status, Outlined/Filled and clipped/gapped tails "
                             u8"without last-item tail · actual LoadingOutlined spin via existing motion policy and "
                             u8"native loading item for upstream pending replacement · finite retained "
                             u8"status/variant/Theme color transition, hidden/inactive/reduced-motion cancellation and "
                             u8"idle behavior · actual nested FormItem/Input/Select/Popover and interactive "
                             u8"horizontal/dynamic/titleSpan/loading Linux Wayland/Vulkan Gallery"}},
                  String{u8"真实横纵 Start/End/Alternate、反序、titleSpan 比例/逻辑/24 列及 "
                         u8"RTL；稳定键控响应增删重排、局部标题/正文/图标 revision 与草稿；富节点、typed "
                         u8"颜色/状态、Outlined/Filled 和有间隙裁剪尾线、末项无尾；复用动画策略的 LoadingOutlined spin "
                         u8"和原生 "
                         u8"pending 替代项；有限状态/变体/Theme 颜色过渡、隐藏/失活/减弱取消及 idle；真实 "
                         u8"FormItem/Input/Select/Popover 与横向/动态/titleSpan/loading Linux Gallery。"}},
                 {MessageKey{
                      u8"gallery.ant.component.timeline.missing",
                      String{
                          u8"HTML/DOM/CSS/React compatibility aliases and OS accessibility/screen-reader platform "
                          u8"bridges are outside this native component contract · Windows/MSVC, physical system IME, "
                          u8"assistive technology and cross-monitor/manual desktop acceptance are not claimed by "
                          u8"Linux HEADLESS/private Wayland evidence"}},
                  String{u8"HTML/DOM/CSS/React 别名及 OS 辅助技术/屏幕阅读器桥不属本合同；Linux HEADLESS/私有 Wayland "
                         u8"不代表 "
                         u8"Windows/MSVC、物理 IME、辅助技术或实际桌面/跨显示器验收。"}},
                 {MessageKey{u8"gallery.ant.component.tooltip.name", String{u8"Tooltip"}},
                  String{u8"Tooltip 文字提示"}},
                 {MessageKey{
                      u8"gallery.ant.component.tooltip.summary",
                      String{u8"Tooltip provides String/typed rich titles, "
                             u8"hover/focus/click/context-menu/combined/manual/controlled visibility, "
                             u8"delays/Escape/outside closing, twelve placements, pointAtCenter, vector arrows and "
                             u8"themes. Platform results remain separate."}},
                  String{
                      u8"RynUI 已实现原生 Tooltip 的 String/typed "
                      u8"富标题、hover/focus/click/context-menu/组合/手动与受控显示、延迟/Escape/外部关闭、十二种位置/"
                      u8"pointAtCenter、向量箭头及主题；平台证据独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.tooltip.supported",
                      String{
                          u8"Reactive plain or retained rich titles, controlled visibility, "
                          u8"hover/focus/manual/click/context-menu and combined triggers, delays and dismissal, twelve "
                          u8"placements, overflow adjustment, vector arrows, Theme, RTL and optional finite motion."}},
                  String{u8"typed reactive title, open and trigger slot；hover/focus/manual with delay, Escape and "
                         u8"disabled-child hover；twelve placements, flip/shift, arrow and window clipping；retained "
                         u8"scene and Theme component tokens/algorithms；persistent rich title slot and reactive title "
                         u8"availability；click/context-menu/composed triggers and outside dismissal；pointAtCenter "
                         u8"and continuous vector glyph arrows；explicit RTL, finite optional motion and "
                         u8"generation-safe right-button press/release"}},
                 {MessageKey{u8"gallery.ant.component.tooltip.missing",
                             String{u8"Web-only DOM/CSS/portal APIs are excluded from native scope"}},
                  String{u8"Web 专用 DOM/CSS/portal API 不属原生范围。"}},
                 {MessageKey{u8"gallery.ant.component.tour.name", String{u8"Tour"}}, String{u8"Tour 漫游式引导"}},
                 {MessageKey{
                      u8"gallery.ant.component.tour.summary",
                      String{u8"Native Tour provides window-owned targets, highlights, retained rich steps, controlled "
                             u8"navigation and nested overlay confinement."}},
                  String{u8"原生 Tour 提供窗口所属目标、高亮、保留富步骤、受控导航及嵌套浮层约束。"}},
                 {MessageKey{
                      u8"gallery.ant.component.tour.supported",
                      String{u8"Typed Tour/Target/Ref with generation-bound geometry, mask/highlight, retained editor "
                             u8"drafts, controlled requests, RTL, live locale and focus/input cleanup. Windows "
                             u8"validation is recorded separately."}},
                  String{u8"typed "
                         u8"Tour/Target/"
                         u8"Ref、代际绑定几何、遮罩与高亮、保留编辑草稿、受控请求、RTL、实时语言及焦点和输入清理；Windo"
                         u8"ws 验证单独记录。"}},
                 {MessageKey{u8"gallery.ant.component.tour.missing",
                             String{u8"Browser DOM/CSS/portal and scrollIntoView APIs are outside the native scope. "
                                    u8"Linux/Wayland/Vulkan, system IME and platform accessibility acceptance remain "
                                    u8"independent."}},
                  String{u8"浏览器 DOM/CSS/portal 和 scrollIntoView API 不属原生范围；Linux/Wayland/Vulkan、系统 IME "
                         u8"及平台辅助功能验收保持独立。"}},
                 {MessageKey{u8"gallery.ant.component.tree.name", String{u8"Tree"}}, String{u8"Tree 树形控件"}},
                 {MessageKey{
                      u8"gallery.ant.component.tree.summary",
                      String{
                          u8"Tree/DirectoryTree provide stable dynamic keys, multiple/strict checkbox selection, "
                          u8"independent async loading, actual virtual rows, in-window drag/drop, vector lines/icons, "
                          u8"RTL and finite motion. Platform boundaries remain separate."}},
                  String{u8"原生 Tree/DirectoryTree 已完成稳定键动态拓扑、多选/strict "
                         u8"勾选、独立异步加载、真实虚拟行、窗口内拖放、向量连接线/图标、RTL 和有限 "
                         u8"motion；平台边界独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.tree.supported",
                      String{
                          u8"Whole-tree validation up to 100000 nodes/64 levels, stable keyed add/remove/reorder/move, "
                          u8"versioned rich factories, retained drafts and failure retirement · Controlled "
                          u8"expanded/selected/checked/half/loaded, default expansion, Ctrl/Meta/Shift "
                          u8"multi-selection/range "
                          u8"and disabled-checkbox isolation · Per-node cancellation/generation/owner dispatcher, "
                          u8"loading/error/retry, independent completions and stale rejection · Bounded actual virtual "
                          u8"rows, "
                          u8"variable height, scrollTo/focus reveal, filter highlight and rich-cache input/IME/popup "
                          u8"cleanup "
                          u8"· Actual PointerRouter capture drag/drop, before/inside/after, cycle/disabled rejection, "
                          u8"edge "
                          u8"autoscroll and app writeback · Independent Tree Theme, actual lines/vector "
                          u8"icons/switchers, "
                          u8"blockNode/RTL, DirectoryTree and finite/reduced motion · Separate "
                          u8"dynamic/10000-row/synthetic-async Gallery, Linux private Wayland/Vulkan and complete old "
                          u8"regressions"}},
                  String{
                      u8"100000 节点/64 层整体验证；稳定 key 增删/重排/移动与版本化富工厂、草稿保留和失败退役 · 受控 "
                      u8"expanded/selected/checked/half/loaded、默认展开、Ctrl/Meta/Shift 多选与键盘范围、禁用勾选隔离 "
                      u8"· "
                      u8"每节点取消/generation/owner-thread dispatcher、loading/error/retry、独立异步完成及过期拒绝 · "
                      u8"实际有界普通虚拟行、可变行高、scrollTo/焦点显露、过滤强调、rich 缓存输入/IME/子浮层清理 · "
                      u8"真实 "
                      u8"PointerRouter capture 拖放、before/inside/after、循环/disabled 拒绝、edge autoscroll "
                      u8"和应用回写 · "
                      u8"独立 Tree Theme、真实线段/向量 icons/switcher、blockNode/RTL、DirectoryTree 与有限/减弱动画 · "
                      u8"独立 "
                      u8"Gallery 动态回调、10000 行、合成异步；Linux 私有 Wayland/Vulkan 和完整旧回归"}},
                 {MessageKey{
                      u8"gallery.ant.component.tree.missing",
                      String{
                          u8"Windows MSVC/D3D12, physical IME candidates, actual cross-monitor and OS assistive "
                          u8"technology require independent evidence · DOM/CSS/React/portal, browser DataTransfer and "
                          u8"external OS DND are outside native scope"}},
                  String{u8"Windows MSVC/D3D12、物理系统 IME 候选窗、跨实际显示器和 OS 辅助技术平台证据独立验证 · "
                         u8"DOM/CSS/React/portal、浏览器 DataTransfer 与外部 OS DND 不属于原生接口"}},
                 {MessageKey{u8"gallery.ant.component.alert.name", String{u8"Alert"}}, String{u8"Alert 警告提示"}},
                 {MessageKey{
                      u8"gallery.ant.component.alert.summary",
                      String{
                          u8"Alert supports banners, marquee, finite exit and afterClose. Focused Windows and native "
                          u8"acceptance are recorded."}},
                  String{u8"原生 Alert 支持 banner、跑马灯、有限退出及 afterClose，Windows 定向与原生验收已记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.alert.supported",
                      String{u8"reactive Status/message/description/visible/closable and onClose requests · retained "
                             u8"typed message/description/icon/action/close slots and Alert Component Token · banner, "
                             u8"pausable marquee, finite exit and afterClose"}},
                  String{u8"响应 Status/message/description/visible/closable 与 onClose 请求；保留 typed "
                         u8"内容/说明/图标/操作/关闭 slot 和 Token；banner、可暂停跑马灯、有限退出和 afterClose。"}},
                 {MessageKey{
                      u8"gallery.ant.component.alert.missing",
                      String{
                          u8"Linux/Wayland/Vulkan, system IME and platform accessibility-adapter coverage need their "
                          u8"own "
                          u8"acceptance · Complete Ant Design Web/DOM/CSS APIs are outside the current native subset"}},
                  String{u8"Linux/Wayland/Vulkan、系统 IME 与平台 accessibility adapter 的本阶段覆盖仍待各自验收 · "
                         u8"完整 Ant "
                         u8"Design Web/DOM/CSS API 不在当前原生子集内"}},
                 {MessageKey{u8"gallery.ant.component.drawer.name", String{u8"Drawer"}}, String{u8"Drawer 抽屉"}},
                 {MessageKey{u8"gallery.ant.component.drawer.summary",
                             String{u8"Native Drawer supports four-edge transitions, continuous nested push, retained "
                                    u8"editing, loading, extra actions, RTL, resizing, lazy content, destroy-on-hidden "
                                    u8"and App services."}},
                  String{u8"原生 Drawer 支持四向开合、连续嵌套 "
                         u8"push、保留编辑、loading、extra、RTL、resize、lazy/destroy 与 App 动态服务。"}},
                 {MessageKey{u8"gallery.ant.component.drawer.supported",
                             String{u8"Controlled four-edge panels; fixed header/footer, scrolling and focus "
                                    u8"restoration; loading and retained typed content; resizing, RTL, lazy and "
                                    u8"destroy-on-hidden; continuous nested push and reversible transitions; real "
                                    u8"captured App services with completion-aligned futures."}},
                  String{u8"controlled open, four placements, finite width/height and retained typed "
                         u8"title/content/footer；fixed header/footer, scrolling body, focus reveal/restoration and "
                         u8"shared Modal nesting；mask/keyboard/close controls, lifecycle callbacks and independent "
                         u8"Drawer Component Token；loading/Skeleton, extra, RTL, drag resize, push, "
                         u8"lazy/destroyOnHidden and directional shadows；finite four-edge transitions and continuous "
                         u8"nested push; input stops immediately on close；completion callbacks, destroyOnHidden and "
                         u8"Drawer service futures follow terminal scene synchronization；captured App context with "
                         u8"real update/close/destroy services and retained content"}},
                 {MessageKey{u8"gallery.ant.component.drawer.missing",
                             String{u8"Linux/Wayland/Vulkan, system IME and OS accessibility adapters require their "
                                    u8"own platform evidence. Web DOM/CSS APIs and arbitrary floating windows are "
                                    u8"outside this docked native contract."}},
                  String{u8"Linux/Wayland/Vulkan、系统 IME 与平台 accessibility adapter 的本阶段覆盖仍待各自验收；完整 "
                         u8"Ant Design Web/DOM/CSS API 不在当前原生子集内"}},
                 {MessageKey{u8"gallery.ant.component.message.name", String{u8"Message"}},
                  String{u8"Message 全局提示"}},
                 {MessageKey{
                      u8"gallery.ant.component.message.summary",
                      String{u8"Message supports hover pause, progress, finite motion, close futures, collapsed stacks "
                             u8"and App live-context services."}},
                  String{u8"原生 Message 支持 hover 暂停、进度、有限动画、关闭 future、折叠栈及 App 活上下文服务。"}},
                 {MessageKey{u8"gallery.ant.component.message.supported",
                             String{u8"Keyed window-scoped messages with reactive content/status, persistent or timed "
                                    u8"entries, retained native controls, pause/progress/transitions/futures/stacking, "
                                    u8"captured App services and a live Loading indicator."}},
                  String{u8"window-scoped typed holder/ref with keyed open, partial update, close and clear；five "
                         u8"status icons, string/rich content and retained editor identities；top-centered stacking, "
                         u8"maxCount eviction, persistent or monotonic timed entries；Theme, input boundaries, "
                         u8"reentrant callbacks and actual clipped scene rendering；interactive Gallery: "
                         u8"catalog-linked native open/update/close/clear workflow；pauseOnHover, progress, finite "
                         u8"motion, close future, collapsed stack and captured App services；default Loading icon uses "
                         u8"shared Icon spin and motion/visibility lifecycle"}},
                 {MessageKey{
                      u8"gallery.ant.component.message.missing",
                      String{u8"Linux/Wayland/Vulkan, system IME and OS accessibility adapters require their own "
                             u8"platform evidence. Full Ant Design Web DOM/CSS APIs are outside the native subset."}},
                  String{u8"Linux/Wayland/Vulkan、系统 IME 与平台 accessibility adapter 的本阶段覆盖仍待各自验收；完整 "
                         u8"Ant Design Web/DOM/CSS API 不在当前原生子集内"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.ant.component.modal.name", String{u8"Modal"}}, String{u8"Modal 对话框"}},
                 {MessageKey{u8"gallery.ant.component.modal.summary",
                             String{u8"Native Modal supports retained editing, real backdrop blur, finite "
                                    u8"presentation, lazy content, dragging, resizing, RTL, responsive widths and "
                                    u8"captured App asynchronous services."}},
                  String{u8"原生 Modal 支持保留编辑、真实背景模糊、有限开合、lazy/destroy、拖动缩放、RTL、响应断点及 "
                         u8"App 异步服务。"}},
                 {MessageKey{u8"gallery.ant.component.modal.supported",
                             String{u8"Controlled retained dialogs and nested input boundaries; real Gaussian backdrop "
                                    u8"filtering; finite reversible motion, lazy content and terminal cleanup; title "
                                    u8"dragging, pointer and keyboard resizing, inherited RTL and responsive widths; "
                                    u8"captured App services whose close futures wait for presentation completion."}},
                  String{u8"typed reactive Props, retained ModalContent/title/footer slots and controlled OK/cancel "
                         u8"requests；mask/keyboard/close/footer/loading states, lifecycle notifications and Modal "
                         u8"Component Token；topmost logical input boundary, nested layers, focus trap/restoration and "
                         u8"IME/popup Escape priority；window clipping, fixed header/footer, pixel-aligned body scroll "
                         u8"and focused editor/caret reveal；captured App dynamic services and asynchronous "
                         u8"confirmation with retained content；opt-in real framebuffer-prefix Gaussian backdrop with "
                         u8"independent mask activation and PM alpha correctness；lazy/destroyOnHidden, finite "
                         u8"reversible presentation and terminal callbacks/service futures；actual title drag, "
                         u8"pointer/keyboard resize, RTL inheritance and live responsive breakpoint widths"}},
                 {MessageKey{
                      u8"gallery.ant.component.modal.missing",
                      String{u8"OS accessibility adapters and Web DOM/CSS compatibility aliases remain separate."}},
                  String{u8"native accessibility bridge and Web/DOM/CSS compatibility aliases"}},
                 {MessageKey{u8"gallery.ant.component.notification.name", String{u8"Notification"}},
                  String{u8"Notification 通知提醒框"}},
                 {MessageKey{
                      u8"gallery.ant.component.notification.summary",
                      String{
                          u8"Notification has an independent action page, hover pause, progress, finite motion, close "
                          u8"futures, collapsed stacks and App live-context services."}},
                  String{u8"原生 Notification 提供独立操作页、hover 暂停、进度、有限动画、关闭 future、折叠栈与 App "
                         u8"活上下文服务。"}},
                 {MessageKey{
                      u8"gallery.ant.component.notification.supported",
                      String{u8"Keyed notifications with title, description, actions, four placements, retained native "
                             u8"controls, persistent or timed entries, pause/progress/transitions/futures/stacking, "
                             u8"captured App services and a live Loading indicator."}},
                  String{
                      u8"typed window-scoped holder/ref with keyed partial updates and safe close/clear；four-corner "
                      u8"stacking, title/description/actions and real close Button；persistent/timed entries, "
                      u8"maxCount, retained rich controls and reentrant lifecycle；independent Theme, clipping, "
                      u8"focus/input boundaries and child-popup priority；interactive Gallery: retained native editor, "
                      u8"keyed title updates, placement/status changes and close/clear；independent actionable "
                      u8"Gallery, pause/progress/motion/future/stack and captured App services；default Loading icon "
                      u8"uses shared Icon spin and motion/visibility lifecycle"}},
                 {MessageKey{
                      u8"gallery.ant.component.notification.missing",
                      String{u8"Linux/Wayland/Vulkan, system IME and OS accessibility adapters require their own "
                             u8"platform evidence. Full Ant Design Web DOM/CSS APIs are outside the native subset."}},
                  String{u8"Linux/Wayland/Vulkan、系统 IME 与平台 accessibility adapter 的本阶段覆盖仍待各自验收；完整 "
                         u8"Ant Design Web/DOM/CSS API 不在当前原生子集内"}},
                 {MessageKey{u8"gallery.ant.component.popconfirm.name", String{u8"Popconfirm"}},
                  String{u8"Popconfirm 气泡确认框"}},
                 {MessageKey{
                      u8"gallery.ant.component.popconfirm.summary",
                      String{u8"Popconfirm provides retained pointer/keyboard-triggered confirmation, actual "
                             u8"confirm/cancel buttons, loading and focus restoration. Gallery includes controlled "
                             u8"completion and Drawer composition; native results remain separate."}},
                  String{
                      u8"已实现点击/键盘触发的保留式气泡确认、真实确认取消按钮、加载与焦点恢复；Gallery 包含受控完成和 "
                      u8"Drawer 内组合，原生平台验收独立记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.popconfirm.supported",
                      String{u8"controlled/default visibility, disabled, confirmLoading/showCancel and typed "
                             u8"trigger/title/description slots · real small Buttons, native trigger activation, "
                             u8"outside/Escape closing and focus restoration · twelve placements, arrow/flip/clamp, "
                             u8"Modal and child popup ownership with Component Token"}},
                  String{u8"受控/默认显隐、禁用、confirmLoading/showCancel 与 typed "
                         u8"触发器/标题/说明；真实小号按钮、原生激活、外部/Escape "
                         u8"关闭与焦点恢复；十二方位、箭头/翻转/钳制、Modal/子弹层归属及 Token。353 源码的 App "
                         u8"动态服务已接入 "
                         u8"Gallery，等待父端运行验证。"}},
                 {MessageKey{
                      u8"gallery.ant.component.popconfirm.missing",
                      String{
                          u8"Promise/static service, hover/contextMenu triggers and arbitrary ButtonProps forwarding "
                          u8"· activation animation, RTL and native accessibility bridge"}},
                  String{u8"353 的动态服务源码待父端本次组合验证；hover/contextMenu 触发、任意 ButtonProps "
                         u8"透传、激活动画/RTL "
                         u8"和原生辅助技术桥按独立合同追踪。"}},
                 {MessageKey{u8"gallery.ant.component.progress.name", String{u8"Progress"}},
                  String{u8"Progress 进度条"}},
                 {MessageKey{u8"gallery.ant.component.progress.summary",
                             String{u8"Progress supports deterministic line/circle/dashboard progress, three sizes and "
                                    u8"complete/error "
                                    u8"states. Arcs use bounded sampling; Gallery updates all three forms together."}},
                  String{u8"已实现 line/circle/dashboard 确定进度、三尺寸与完成/错误状态；圆弧使用有限采样，Gallery "
                         u8"可同步更新三种进度。"}},
                 {MessageKey{
                      u8"gallery.ant.component.progress.supported",
                      String{u8"reactive finite percent/status/showInfo, three sizes and Progress Component Token · "
                             u8"retained line fill, sampled circle/dashboard arcs and fixed dashboard gap"}},
                  String{u8"响应有限百分比/状态/信息、三尺寸与 Token；保留线形填充、有限采样 circle/dashboard "
                         u8"及固定仪表缺口。固定 Progress 包的源码功能和产品默认关闭的 paint/cap "
                         u8"能力按依赖交接边界处理。"}},
                 {MessageKey{
                      u8"gallery.ant.component.progress.missing",
                      String{u8"active shimmer, steps/gradient/success segment and arbitrary gap · exact vector arc, "
                             u8"translucent sample overlap and dashboard rail endpoint refinements · native GPU/DPI "
                             u8"visual and accessibility acceptance"}},
                  String{u8"active 流光、steps/gradient/success "
                         u8"段与任意缺口、精确向量弧、半透明采样重叠和仪表轨端点按固定 "
                         u8"Progress 源码包边界追踪；产品 paint/cap 默认关闭，未用源码集成冒充 native GPU/DPI "
                         u8"视觉或辅助技术验收。"}},
                 {MessageKey{u8"gallery.ant.component.result.name", String{u8"Result"}}, String{u8"Result 结果"}},
                 {MessageKey{
                      u8"gallery.ant.component.result.summary",
                      String{u8"Result supports pinned upstream exception illustrations, retained dynamic regions and "
                             u8"finite state motion. Focused Windows and native acceptance are recorded."}},
                  String{u8"原生 Result 支持固定上游异常插画、保留式动态区域与有限状态动画，Windows "
                         u8"定向及原生验收已记录。"}},
                 {MessageKey{u8"gallery.ant.component.result.supported",
                             String{u8"Seven statuses with native Icons or pinned 403/404/500 illustrations through "
                                    u8"shared ImageSource, reactive text and retained rich slots, native "
                                    u8"actions/editors, Theme, clipping and finite state transitions."}},
                  String{u8"typed ResultProps/ResultStatus, reactive title/subtitle/status and retained "
                         u8"icon/content/extra slots；Success/Warning/Error/info use native Icons; "
                         u8"Forbidden/NotFound/ServerError use pinned shared Image illustrations；Theme component "
                         u8"colors/metrics, finite text/Icon clipping, wrapped native actions and content "
                         u8"surface；retained Button interaction, Input editor/draft and Text scene identity across "
                         u8"status/Theme updates；076 Gallery source: seven-state cycle, native continue action/draft, "
                         u8"static 404 and custom Icon/Text Flex；pinned 403/404/500 illustrations through shared "
                         u8"ImageSource, retained dynamic slots and finite state motion"}},
                 {MessageKey{
                      u8"gallery.ant.component.result.missing",
                      String{
                          u8"Generic HTTP/network-state services belong to the app · Linux/Wayland/Vulkan, system IME "
                          u8"and platform accessibility-adapter coverage need independent acceptance · Complete "
                          u8"Web/DOM/CSS APIs are outside the native subset"}},
                  String{u8"通用 HTTP/网络状态服务由应用提供 · Linux/Wayland/Vulkan、系统 IME 与平台 accessibility "
                         u8"adapter "
                         u8"的本阶段覆盖仍待各自验收 · 完整 Ant Design Web/DOM/CSS API 不在当前原生子集内"}},
                 {MessageKey{u8"gallery.ant.component.skeleton.name", String{u8"Skeleton"}},
                  String{u8"Skeleton 骨架屏"}},
                 {MessageKey{
                      u8"gallery.ant.component.skeleton.summary",
                      String{u8"Skeleton supports active gradients, Image/Node and retained loading content. Focused "
                             u8"Windows and native acceptance are recorded."}},
                  String{
                      u8"原生 Skeleton 支持 active 渐变、Image/Node 与保留式加载正文，Windows 定向及原生验收已记录。"}},
                 {MessageKey{
                      u8"gallery.ant.component.skeleton.supported",
                      String{
                          u8"typed reactive static title/paragraph/avatar composition and proportional row widths · "
                          u8"standalone SkeletonAvatar/SkeletonButton/SkeletonInput with size, shape and block "
                          u8"semantics · retained loading content with input, focus, popup and preedit cleanup plus "
                          u8"identity/draft restoration · finite and zero-size recovery, logical scene clipping, "
                          u8"Theme inheritance and idle behavior · 086 interactive Gallery: "
                          u8"loading/title/paragraph/avatar/round, standalone shapes and native retained publication "
                          u8"flow · active shimmer, SkeletonImage/SkeletonNode and themed image glyph sizing/color"}},
                  String{u8"typed 响应标题/段落/头像和比例行宽；独立 Avatar/Button/Input "
                         u8"的尺寸/形状/block；保留加载正文，清理输入/焦点/弹层/preedit "
                         u8"后恢复身份/草稿；有限/零尺寸恢复、逻辑裁剪、Theme/idle；086 "
                         u8"交互页的加载/标题/段落/头像/圆角、独立形状与真实发布流程；active "
                         u8"渐变、Image/Node、主题图像字形尺寸/颜色。"}},
                 {MessageKey{
                      u8"gallery.ant.component.skeleton.missing",
                      String{u8"Network-loading services belong to the app · Linux/Wayland/Vulkan, system IME and "
                             u8"platform accessibility-adapter coverage need independent acceptance · Complete "
                             u8"Web/DOM/CSS APIs are outside the native subset"}},
                  String{u8"网络加载服务由应用提供 · Linux/Wayland/Vulkan、系统 IME 与平台 accessibility adapter "
                         u8"的本阶段覆盖仍待各自验收 · 完整 Ant Design Web/DOM/CSS API 不在当前原生子集内"}},
                 {MessageKey{u8"gallery.ant.component.spin.name", String{u8"Spin"}}, String{u8"Spin 加载中"}},
                 {MessageKey{
                      u8"gallery.ant.component.spin.summary",
                      String{u8"Spin supports fullscreen, delay, numeric/automatic percentages and retained content. "
                             u8"Focused Windows and native acceptance are recorded."}},
                  String{u8"原生 Spin 支持 fullscreen、delay、数值/自动百分比及保留式正文，Windows "
                         u8"定向及原生验收已记录。"}},
                 {MessageKey{u8"gallery.ant.component.spin.supported",
                             String{u8"reactive spinning/size/description, retained content/custom indicator and Spin "
                                    u8"Component Token "
                                    u8"· dim shield, nested input/capture/focus/session cleanup and recovery without "
                                    u8"disabled mutation "
                                    u8"· finite LoadingOutlined motion with reduced/offscreen/inactive/hidden lifetime "
                                    u8"handling · "
                                    u8"fullscreen input domain, cancellable delay, numeric/automatic percent and "
                                    u8"custom indicator"}},
                  String{u8"响应 spinning/尺寸/说明、保留正文/自定义指示器与 "
                         u8"Token；暗色屏障、嵌套输入/捕获/焦点/会话清理，恢复不修改 disabled；有限 LoadingOutlined "
                         u8"动画遵守减弱/离屏/失活/隐藏；全屏输入域、可取消延迟、数值/自动百分比及自定义指示器。"}},
                 {MessageKey{
                      u8"gallery.ant.component.spin.missing",
                      String{
                          u8"Linux/Wayland/Vulkan, system IME and platform accessibility-adapter coverage need their "
                          u8"own "
                          u8"acceptance · Complete Ant Design Web/DOM/CSS APIs are outside the current native subset"}},
                  String{u8"Linux/Wayland/Vulkan、系统 IME 与平台 accessibility adapter 的本阶段覆盖仍待各自验收 · "
                         u8"完整 Ant "
                         u8"Design Web/DOM/CSS API 不在当前原生子集内"}},
                 {MessageKey{u8"gallery.ant.component.watermark.name", String{u8"Watermark"}},
                  String{u8"Watermark 水印"}},
                 {MessageKey{
                      u8"gallery.ant.component.watermark.summary",
                      String{
                          u8"Watermark supports repeated text/shared images, rotation, gaps/offsets, Theme, retained "
                          u8"content and cancelable async sources. Focused MSVC and D3D12 Gallery checks passed."}},
                  String{u8"原生 Watermark "
                         u8"已实现文字/共享图像重复、旋转、间距/偏移、Theme、保留内容及可取消异步来源；Windows/MSVC "
                         u8"定向与 D3D12 Gallery 已验证。"}},
                 {MessageKey{
                      u8"gallery.ant.component.watermark.supported",
                      String{
                          u8"typed reactive content/image/size/gap/offset/rotate and Theme · shared rotated glyph "
                          u8"views / ImageSource, ancestor clipping and retained child input · owner-thread "
                          u8"cancellation, generation checks, observable failure and retry · real Gallery actions and "
                          u8"Windows D3D12 system-font readback"}},
                  String{u8"typed 响应 content/image/尺寸/gap/offset/rotate 与 "
                         u8"Theme；共享旋转字形视图/"
                         u8"ImageSource、祖先裁剪和保留子输入；所有者线程取消、代际检查、可观察失败/"
                         u8"重试；真实 Gallery 操作与已记录 Windows D3D12 系统字体读回。"}},
                 {MessageKey{
                      u8"gallery.ant.component.watermark.missing",
                      String{
                          u8"Web DOM/CSS anti-tamper APIs are outside the native contract · Linux, other actual DPI, "
                          u8"system IME and AT evidence remain independent"}},
                  String{
                      u8"Web DOM/CSS 防篡改 API 不属原生合同；Linux、其他实际 DPI、系统 IME 和辅助技术证据独立。"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.ant.component.affix.name", String{u8"Affix"}}, String{u8"Affix 固钉"}},
                 {MessageKey{u8"gallery.ant.component.affix.summary",
                             String{u8"Affix supports explicit targets, top/bottom offsets, retained "
                                    u8"placeholder/content, actual hit "
                                    u8"clipping, onChange and release restoration. Focused MSVC and D3D12 Gallery "
                                    u8"checks passed."}},
                  String{u8"原生 Affix 已实现显式目标、顶部/底部偏移、保留占位/子树、真实命中裁剪、onChange "
                         u8"与释放恢复；Windows/MSVC 定向与 D3D12 Gallery 已验证。"}},
                 {MessageKey{
                      u8"gallery.ant.component.affix.supported",
                      String{u8"explicit window-logical AffixTarget identity/viewport/offset/attachment · top/bottom "
                             u8"offsets, "
                             u8"Theme layer order and retained placeholder/content · native input/clip, target "
                             u8"replacement, "
                             u8"onChange and cleanup · real Gallery document scrolling and Windows D3D12 readback"}},
                  String{
                      u8"显式窗口逻辑 AffixTarget 身份/视口/偏移/附着；顶部/底部偏移、Theme "
                      u8"层级及保留占位/正文；原生输入/裁剪、目标替换、onChange 与清理；真实 Gallery 文档滚动及已记录 "
                      u8"Windows D3D12 读回。"}},
                 {MessageKey{
                      u8"gallery.ant.component.affix.missing",
                      String{u8"Applications publish native scroll target geometry; browser DOM target APIs are not "
                             u8"emulated · Linux, other actual DPI, system IME and AT evidence remain independent"}},
                  String{u8"应用发布原生滚动目标几何，不模拟浏览器 DOM target；Linux、其他实际 DPI、系统 IME "
                         u8"和辅助技术证据独立。"}},
                 {MessageKey{u8"gallery.ant.component.app.name", String{u8"App"}}, String{u8"App 包裹组件"}},
                 {MessageKey{
                      u8"gallery.ant.component.app.summary",
                      String{
                          u8"App supplies live-context Message/Notification services. The 353 source dependency adds "
                          u8"retained Modal/Drawer/Popconfirm panels and async confirmation; this Gallery integration "
                          u8"awaits parent runtime validation."}},
                  String{u8"App 提供活上下文 Message/Notification 服务；353 源码依赖新增保留式 Modal/Drawer/Popconfirm "
                         u8"与异步确认，本次 Gallery 组合等待父端运行验证。"}},
                 {MessageKey{
                      u8"gallery.ant.component.app.supported",
                      String{u8"Retained App/AppRef/useApp, captured live theme/config, real Message/Notification "
                             u8"holders and generation-safe handles. The 353 dependency adds retained "
                             u8"Modal/Drawer/Popconfirm services and async confirmation; Gallery actions and bilingual "
                             u8"feedback are integrated in source, pending parent runtime validation."}},
                  String{u8"保留 App/AppRef/useApp、捕获活 theme/config、真实 Message/Notification holder "
                         u8"及代际安全句柄。353 "
                         u8"源码新增 Modal/Drawer/Popconfirm 动态面板、富内容保留与异步确认；本次 Gallery "
                         u8"已接入操作和双语状态，实际验收由父端运行。"}},
                 {MessageKey{
                      u8"gallery.ant.component.app.missing",
                      String{u8"The 353 Modal/Drawer/Popconfirm services are present in the source baseline; parent "
                             u8"combination/platform validation remains required. Linux/Wayland/Vulkan, system IME and "
                             u8"accessibility remain independent; full Web/DOM/CSS APIs are excluded."}},
                  String{u8"353 的 Modal/Drawer/Popconfirm "
                         u8"服务已是本次源码依赖，组合及平台结果由父端实际验证；Linux/Wayland/Vulkan、系统 IME "
                         u8"和辅助技术独立；完整 Web/DOM/CSS API 不属原生子集。"}},
                 {MessageKey{u8"gallery.ant.component.border-beam.name", String{u8"BorderBeam"}},
                  String{u8"BorderBeam 边框流光"}},
                 {MessageKey{
                      u8"gallery.ant.component.border-beam.summary",
                      String{
                          u8"BorderBeam provides a single-color rounded-perimeter segment, duration/direction, Theme "
                          u8"and deadline cancellation for reduced motion, hiding and retirement. Focused MSVC and "
                          u8"fixed-clock D3D12 checks passed."}},
                  String{u8"原生 BorderBeam 已实现单色圆角周长运动、时长/方向、Theme 与 reduced "
                         u8"motion/隐藏/离页停帧；Windows/MSVC 定向和固定时钟 D3D12 帧已验证。"}},
                 {MessageKey{u8"gallery.ant.component.border-beam.supported",
                             String{u8"retained children and one rounded-perimeter moving segment · reactive "
                                    u8"duration/direction and "
                                    u8"Theme color/width/radius/length · hidden/offscreen/offpage/inactive/reduced "
                                    u8"motion deadline "
                                    u8"cancellation · deterministic native motion frames, real Gallery actions and "
                                    u8"D3D12 readback"}},
                  String{u8"保留子树与单个圆角周长运动片段；响应时长/方向与 Theme "
                         u8"颜色/宽度/圆角/长度；隐藏/离屏/离页/失活/减弱动画取消 deadline；确定时钟原生动画帧、真实 "
                         u8"Gallery "
                         u8"及已记录 D3D12 读回。"}},
                 {MessageKey{
                      u8"gallery.ant.component.border-beam.missing",
                      String{
                          u8"This native contract uses a single color segment; no gradient trail is claimed · Linux, "
                          u8"other actual DPI, system IME and AT evidence remain independent"}},
                  String{u8"当前原生合同使用单色片段，不宣称渐变拖尾；Linux、其他实际 DPI、系统 IME "
                         u8"和辅助技术证据独立。"}},
                 {MessageKey{u8"gallery.ant.component.config-provider.name", String{u8"ConfigProvider"}},
                  String{u8"ConfigProvider 全局化配置"}},
                 {MessageKey{
                      u8"gallery.ant.component.config-provider.summary",
                      String{u8"A single window-isolated ConfigProvider supplies field-wise locale, format locale, "
                             u8"messages, direction, size, disabled and theme inheritance. The public example "
                             u8"demonstrates precise values and retained rich content."}},
                  String{u8"唯一窗口隔离 ConfigProvider 提供 "
                         u8"locale、格式语言、消息、方向、尺寸、禁用及主题的逐字段继承；公开应用展示精确值与保留富内容"
                         u8"。"}},
                 {MessageKey{
                      u8"gallery.ant.component.config-provider.supported",
                      String{u8"Window-isolated field-wise config inheritance; public "
                             u8"MessageKey/MessageCatalog/I18n.text/useI18n, exact numbers and explicit fallback; "
                             u8"Default/Dark/Compact, nested Theme and component tokens; a standalone public app with "
                             u8"parameters, plurals, independent formatting/direction and retained rich Modal."}},
                  String{u8"唯一窗口隔离 ConfigProvider，逐字段继承 "
                         u8"locale/formatLocale/messages/direction/size/disabled/theme；公开 "
                         u8"MessageKey/MessageCatalog/I18n.text/useI18n、精确数值和明确 "
                         u8"fallback；Default/Dark/Compact、嵌套 "
                         u8"Theme 及组件 Token；独立公开应用展示参数/复数/格式语言/方向和保留富 Modal。"}},
                 {MessageKey{
                      u8"gallery.ant.component.config-provider.missing",
                      String{u8"DOM prefix/CSP/renderEmpty, complete Web configuration and browser wave APIs are "
                             u8"excluded. Full ICU/CLDR, currency, complex plurals and timezone conversion are not "
                             u8"supplied; see the declared format/fallback limits in docs/i18n.md."}},
                  String{u8"DOM prefix/CSP/renderEmpty、完整 Web 配置兼容与浏览器 wave API 不属原生合同；不提供完整 "
                         u8"ICU/CLDR、货币、复杂复数或时区转换，格式与 fallback 能力按 docs/i18n.md 声明。"}},
                 {MessageKey{u8"gallery.ant.component.util.name", String{u8"Util"}}, String{u8"Util 工具类"}},
                 {MessageKey{
                      u8"gallery.ant.component.util.summary",
                      String{
                          u8"Pinned upstream Util contains the TypeScript compile-time GetRef, GetProps and GetProp "
                          u8"utilities and has no independent visual component. RynUI uses public typed Props, refs, "
                          u8"value types and C++ type deduction."}},
                  String{u8"锁定上游 Util 是 GetRef、GetProps、GetProp 三种 TypeScript "
                         u8"编译期类型工具，没有独立视觉组件；RynUI "
                         u8"使用公开 typed Props、ref、值类型及 C++ 类型推导。"}},
                 {MessageKey{
                      u8"gallery.ant.component.util.supported",
                      String{
                          u8"Separate explanation page retains the pinned public type-utility sources and creates no "
                          u8"nonfunctional visual samples · Public SelectProps/InputProps, SelectRef/InputRef and "
                          u8"SelectOption/SelectValue express native types · Local expressions use decltype and "
                          u8"callable return types use std::invoke_result_t"}},
                  String{
                      u8"独立说明页保留锁定版本的公开类型工具来源，不注册无功能控件样例 · 公开 "
                      u8"SelectProps/InputProps、SelectRef/InputRef 和 SelectOption/SelectValue 直接表达相应原生类型 · "
                      u8"局部表达式使用 decltype；已有可调用对象的返回类型使用 std::invoke_result_t"}},
                 {MessageKey{
                      u8"gallery.ant.component.util.missing",
                      String{
                          u8"TypeScript GetRef/GetProps/GetProp, React Context extraction and NonNullable are outside "
                          u8"the C++ native API; no same-named runtime component is supplied"}},
                  String{u8"TypeScript GetRef/GetProps/GetProp、React Context 类型提取与 NonNullable 不属于 C++ 原生 "
                         u8"API；不提供同名运行时组件"}},
                 {MessageKey{u8"gallery.sample.watermark.native.title",
                             String{u8"Text/shared images, rotation, repetition and async cancellation"}},
                  String{u8"文字与共享图像、旋转、重复和异步取消"}},
                 {MessageKey{u8"gallery.sample.affix.native.title",
                             String{u8"Native scroll targets, top/bottom affixing and release restoration"}},
                  String{u8"原生滚动目标、顶部 / 底部固定与释放恢复"}},
                 {MessageKey{u8"gallery.sample.border-beam.native.title",
                             String{u8"Rounded-perimeter motion, direction, theme and reduced motion"}},
                  String{u8"圆角周长运动、方向、主题与减少动态效果"}},
                 {MessageKey{u8"gallery.sample.qr-code.native.title",
                             String{u8"Standard encoding, status requests, shared logo and PNG/SVG export"}},
                  String{u8"标准编码、状态请求、共享图标与 PNG/SVG 数据导出"}},
                 {MessageKey{u8"gallery.sample.carousel.panels.title",
                             String{u8"Retained panels, dragging, effects and autoplay"}},
                  String{u8"保留面板、拖动、效果与自动播放"}},
                 {MessageKey{u8"gallery.sample.carousel.controlled.title",
                             String{u8"Controlled rejection, custom indicators and dynamic keys"}},
                  String{u8"受控拒绝、自定义指示器与动态 key"}},
                 {MessageKey{u8"gallery.sample.calendar.custom.title",
                             String{u8"Custom header, full cells and retained popup"}},
                  String{u8"自定义 header、fullCell 与保留浮层"}},
                 {MessageKey{u8"gallery.sample.calendar.schedule.title",
                             String{u8"Gregorian calendar, year/month selection and retained schedule"}},
                  String{u8"公历、年月选择与保留日程"}},
                 {MessageKey{u8"gallery.sample.float-button.actions.title",
                             String{u8"Floating actions, groups and BackTop"}},
                  String{u8"浮动操作、分组与返回顶部"}},
                 {MessageKey{u8"gallery.sample.button.basics.title", String{u8"Types and interaction states"}},
                  String{u8"类型与交互状态"}},
                 {MessageKey{u8"gallery.sample.button.variants.title", String{u8"Colors, variants and icons"}},
                  String{u8"颜色、变体与图标"}},
                 {MessageKey{u8"gallery.sample.input.basics.title", String{u8"Controlled and uncontrolled input"}},
                  String{u8"受控与非受控输入"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.sample.input.features.title", String{u8"Variants, counts and focus"}},
                  String{u8"变体、统计与焦点"}},
                 {MessageKey{u8"gallery.sample.input.bidi.title", String{u8"Bidirectional text editing"}},
                  String{u8"双向文字编辑"}},
                 {MessageKey{u8"gallery.sample.search.basics.title", String{u8"Search and submission source"}},
                  String{u8"搜索与提交来源"}},
                 {MessageKey{u8"gallery.sample.search.features.title", String{u8"Connected appearance and icons"}},
                  String{u8"连接外观与图标"}},
                 {MessageKey{u8"gallery.sample.password.basics.title", String{u8"Password input"}},
                  String{u8"密码输入"}},
                 {MessageKey{u8"gallery.sample.password.features.title",
                             String{u8"Visibility action and controlled state"}},
                  String{u8"显隐动作与受控状态"}},
                 {MessageKey{u8"gallery.sample.password.bidi.title", String{u8"Bidirectional password input"}},
                  String{u8"双向密码输入"}},
                 {MessageKey{u8"gallery.sample.text-area.features.title",
                             String{u8"Multiple lines, autoSize and scrolling"}},
                  String{u8"多行、自动尺寸与滚动"}},
                 {MessageKey{u8"gallery.sample.text-area.bidi.title", String{u8"Bidirectional multiline editing"}},
                  String{u8"双向多行编辑"}},
                 {MessageKey{u8"gallery.sample.otp.features.title", String{u8"Segmented input and paste"}},
                  String{u8"分格输入与粘贴"}},
                 {MessageKey{u8"gallery.sample.tree-select.destination.title",
                             String{u8"Delivery target, note and form"}},
                  String{u8"交付目标、备注与表单"}},
                 {MessageKey{u8"gallery.sample.typography.levels.title", String{u8"Headings and inline styles"}},
                  String{u8"标题与行内样式"}},
                 {MessageKey{u8"gallery.sample.typography.features.title",
                             String{u8"Ellipsis, copy and editing in place"}},
                  String{u8"省略、复制与原地编辑"}},
                 {MessageKey{u8"gallery.sample.typography.bidi.title", String{u8"Bidirectional typography"}},
                  String{u8"双向文字排版"}},
                 {MessageKey{u8"gallery.sample.divider.basics.title", String{u8"Horizontal, labels and dashed lines"}},
                  String{u8"水平、标签与虚线"}},
                 {MessageKey{u8"gallery.sample.divider.orientation.title",
                             String{u8"Orientation and vertical dividers"}},
                  String{u8"朝向与垂直分隔"}},
                 {MessageKey{u8"gallery.sample.divider.variants.title", String{u8"Sizes, line styles and RTL"}},
                  String{u8"尺寸、线型与 RTL"}},
                 {MessageKey{u8"gallery.sample.flex.layouts.title", String{u8"Baselines, wrapping and stretch"}},
                  String{u8"基线、换行与拉伸"}},
                 {MessageKey{u8"gallery.sample.grid.layouts.title",
                             String{u8"24 columns, responsive breakpoints and flex"}},
                  String{u8"二十四列、响应断点与弹性"}},
                 {MessageKey{u8"gallery.sample.masonry.layouts.title",
                             String{u8"Measured heights, responsive columns and keyed content"}},
                  String{u8"真实高度、响应列数与 keyed 内容"}},
                 {MessageKey{u8"gallery.sample.splitter.multiple.title",
                             String{u8"Multiple panels, constraints and continuous resizing"}},
                  String{u8"多面板、约束与连续调整"}},
                 {MessageKey{u8"gallery.sample.splitter.controlled.title",
                             String{u8"Controlled percentages, custom drag icon and double-click"}},
                  String{u8"受控百分比、自定义拖动图标与双击"}},
                 {MessageKey{u8"gallery.sample.splitter.lazy-collapse.title",
                             String{u8"Lazy mode, two-sided collapse, motion and hidden lifetime"}},
                  String{u8"Lazy、双侧折叠、动画与隐藏生命周期"}},
                 {MessageKey{u8"gallery.sample.splitter.nested.title", String{u8"Nested horizontal/vertical panels"}},
                  String{u8"嵌套水平/垂直面板"}},
                 {MessageKey{u8"gallery.sample.layout.structure.title",
                             String{u8"Header, two sidebars, content and footer"}},
                  String{u8"页头、双侧栏、正文与页尾"}},
                 {MessageKey{u8"gallery.sample.layout.responsive.title",
                             String{u8"Responsive breakpoints and zero-width sidebar"}},
                  String{u8"响应断点与零宽侧栏"}},
                 {MessageKey{u8"gallery.sample.layout.controlled.title",
                             String{u8"Controlled state, custom trigger and theme"}},
                  String{u8"受控、自定义触发器与主题"}},
                 {MessageKey{u8"gallery.sample.space.composition.title",
                             String{u8"Spacing, separators and Compact composition"}},
                  String{u8"间距、分隔与紧凑组合"}},
                 {MessageKey{u8"gallery.sample.icon.catalog.title", String{u8"Offline icons, rotation and vectors"}},
                  String{u8"离线图标、旋转与向量"}},
                 {MessageKey{u8"gallery.sample.anchor.navigation.title",
                             String{u8"Hierarchy, direction, scrolling and dynamic anchors"}},
                  String{u8"层级、方向、滚动与动态锚点"}},
                 {MessageKey{u8"gallery.sample.breadcrumb.dynamic.title", String{u8"Dynamic keys, menu and RTL"}},
                  String{u8"动态键控、菜单与 RTL"}},
                 {MessageKey{u8"gallery.sample.breadcrumb.trail.title", String{u8"Hierarchy navigation and editing"}},
                  String{u8"层级导航与编辑"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.sample.steps.workflow.title", String{u8"Release workflow and note"}},
                  String{u8"发布流程与备注"}},
                 {MessageKey{u8"gallery.sample.image.native.title",
                             String{u8"Actual pixels, async source and native preview"}},
                  String{u8"真实像素、异步源与原生预览"}},
                 {MessageKey{u8"gallery.sample.upload.native.title",
                             String{u8"Native file selection and synthetic application transfer"}},
                  String{u8"原生文件选择与合成应用传输"}},
                 {MessageKey{u8"gallery.sample.transfer.lists.title",
                             String{u8"Two lists, search, pagination and retained draft"}},
                  String{u8"双列表、搜索、分页与保留草稿"}},
                 {MessageKey{u8"gallery.sample.cascader.paths.title",
                             String{u8"Hierarchical paths, multiple selection and application loader"}},
                  String{u8"层级路径、多选与应用加载器"}},
                 {MessageKey{u8"gallery.sample.menu.inline.title", String{u8"Selection, submenus and reactive text"}},
                  String{u8"选择、子菜单与响应文字"}},
                 {MessageKey{u8"gallery.sample.dropdown.actions.title", String{u8"Click, context menu and commands"}},
                  String{u8"点击、右键与菜单命令"}},
                 {MessageKey{u8"gallery.sample.pagination.controlled.title",
                             String{u8"Controlled pagination and page size"}},
                  String{u8"受控分页与页大小"}},
                 {MessageKey{u8"gallery.sample.tabs.panels.title", String{u8"Labels, retained content and overflow"}},
                  String{u8"标签、保留内容与溢出"}},
                 {MessageKey{u8"gallery.sample.switch.states.title", String{u8"Sizes, states and ref"}},
                  String{u8"尺寸、状态与引用"}},
                 {MessageKey{u8"gallery.sample.checkbox.selection.title", String{u8"Single and group selection"}},
                  String{u8"单项与多项选择"}},
                 {MessageKey{u8"gallery.sample.radio.selection.title",
                             String{u8"Mutually exclusive selection and dynamic options"}},
                  String{u8"互斥选择与动态选项"}},
                 {MessageKey{u8"gallery.sample.slider.values.title", String{u8"Marks, range and dragging"}},
                  String{u8"刻度、范围与拖动"}},
                 {MessageKey{u8"gallery.sample.tooltip.triggers.title", String{u8"Hover, focus and combined triggers"}},
                  String{u8"悬停、焦点与组合触发"}},
                 {MessageKey{u8"gallery.sample.modal.dialogs.title", String{u8"Confirm, cancel and retained editing"}},
                  String{u8"确认、取消与保留编辑"}},
                 {MessageKey{u8"gallery.sample.avatar.images.title",
                             String{u8"Image requests, breakpoints and dynamic groups"}},
                  String{u8"图片请求、响应式与动态分组"}},
                 {MessageKey{u8"gallery.sample.badge.native.title", String{u8"Digit animation, rich count and Ribbon"}},
                  String{u8"数字动画、富计数与 Ribbon"}},
                 {MessageKey{u8"gallery.sample.tag.native.title",
                             String{u8"Cancellation, controlled selection and retained draft reorder"}},
                  String{u8"取消、受控选择与保留草稿拖排"}},
                 {MessageKey{u8"gallery.sample.tag.states.title", String{u8"Tags, closing and selection"}},
                  String{u8"标签、关闭与选择"}},
                 {MessageKey{u8"gallery.sample.badge.counts.title", String{u8"Counts, dots and status"}},
                  String{u8"计数、圆点与状态"}},
                 {MessageKey{u8"gallery.sample.drawer.account.title", String{u8"Four-sided drawer and account draft"}},
                  String{u8"四边抽屉与账户草稿"}},
                 {MessageKey{u8"gallery.sample.popconfirm.confirm.title", String{u8"Confirm, cancel and loading"}},
                  String{u8"确认、取消与加载"}},
                 {MessageKey{u8"gallery.sample.alert.feedback.title", String{u8"Status feedback and recovery"}},
                  String{u8"状态提示与恢复"}},
                 {MessageKey{u8"gallery.sample.spin.loading.title", String{u8"Loading and retained content"}},
                  String{u8"加载与保留内容"}},
                 {MessageKey{u8"gallery.sample.progress.values.title", String{u8"Line, circle and status"}},
                  String{u8"线形、环形与状态"}},
                 {MessageKey{u8"gallery.sample.table.workflow.title",
                             String{u8"Editing, selection, pagination and commands"}},
                  String{u8"编辑、选择、分页与命令"}},
                 {MessageKey{u8"gallery.sample.color-picker.controls.title",
                             String{u8"Color, alpha, channels and gradient stops"}},
                  String{u8"颜色、透明度、通道与渐变色标"}},
                 {MessageKey{u8"gallery.sample.input-number.values.title", String{u8"Decimals, bounds and state"}},
                  String{u8"小数、范围与状态"}},
                 {MessageKey{u8"gallery.sample.collapse.panels.title",
                             String{u8"Expansion, actions and retained content"}},
                  String{u8"展开、操作与保留内容"}},
                 {MessageKey{u8"gallery.sample.segmented.views.title", String{u8"View selection and rich labels"}},
                  String{u8"视图选择与富标签"}},
                 {MessageKey{u8"gallery.sample.empty.states.title", String{u8"Empty state and creation action"}},
                  String{u8"空状态与创建操作"}},
                 {MessageKey{u8"gallery.sample.result.states.title", String{u8"Result and details"}},
                  String{u8"处理结果与详情"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.sample.form.fields.title", String{u8"Fields, validation and submission"}},
                  String{u8"字段、校验与提交"}},
                 {MessageKey{u8"gallery.sample.form.layouts.title",
                             String{u8"Layouts, automatic binding and scrolling"}},
                  String{u8"布局、自动绑定与滚动"}},
                 {MessageKey{u8"gallery.sample.form.lists.title", String{u8"Deep paths and stable nested lists"}},
                  String{u8"深路径与稳定嵌套列表"}},
                 {MessageKey{u8"gallery.sample.form.async.title",
                             String{u8"Async validation, dependencies and cancellation"}},
                  String{u8"异步、依赖与取消"}},
                 {MessageKey{u8"gallery.sample.date-picker.dates.title", String{u8"Date, rules and form"}},
                  String{u8"日期、规则与表单"}},
                 {MessageKey{u8"gallery.sample.select.options.title",
                             String{u8"Selection, search and dynamic options"}},
                  String{u8"选择、搜索与动态选项"}},
                 {MessageKey{u8"gallery.sample.card.completion.title",
                             String{u8"Grid, Meta, retained Tabs and Image cover"}},
                  String{u8"Grid、Meta、保留 Tabs 与图片封面"}},
                 {MessageKey{u8"gallery.sample.empty.dynamic.title",
                             String{u8"Image service and dynamic rich content"}},
                  String{u8"图片服务与动态富内容"}},
                 {MessageKey{u8"gallery.sample.listy.retained.title",
                             String{u8"Stable keys, grid, pagination and scrolling"}},
                  String{u8"稳定 key、网格、分页与滚动"}},
                 {MessageKey{u8"gallery.sample.card.content.title", String{u8"Content, actions and loading"}},
                  String{u8"内容、操作与加载"}},
                 {MessageKey{u8"gallery.sample.time-picker.times.title", String{u8"Time, rules and form"}},
                  String{u8"时间、规则与表单"}},
                 {MessageKey{u8"gallery.sample.descriptions.fields.title", String{u8"Profile, columns and editing"}},
                  String{u8"资料、列数与编辑"}},
                 {MessageKey{u8"gallery.sample.skeleton.loading.title", String{u8"Skeleton and retained editing"}},
                  String{u8"骨架与保留编辑"}},
                 {MessageKey{u8"gallery.sample.popover.notes.title",
                             String{u8"Popover content and controlled interaction"}},
                  String{u8"气泡内容与受控交互"}},
                 {MessageKey{u8"gallery.sample.app.services.title",
                             String{u8"App notices, retained editing and language switching"}},
                  String{u8"App 通知、保留编辑与语言切换"}},
                 {MessageKey{u8"gallery.sample.message.workflow.title", String{u8"Message and Notification workflow"}},
                  String{u8"Message 与 Notification 处理流程"}},
                 {MessageKey{u8"gallery.sample.notification.center.title",
                             String{u8"Notification center and timed progress"}},
                  String{u8"独立通知中心与限时进度"}},
                 {MessageKey{u8"gallery.sample.timeline.events.title", String{u8"Event stream and follow-up note"}},
                  String{u8"事件流与处理备注"}},
                 {MessageKey{u8"gallery.sample.avatar.members.title", String{u8"Members, groups and editing"}},
                  String{u8"成员、分组与编辑"}},
                 {MessageKey{u8"gallery.sample.statistic.metrics.title",
                             String{u8"Precision, formatting and metric note"}},
                  String{u8"精度、格式与指标备注"}},
                 {MessageKey{u8"gallery.sample.rate.service.title", String{u8"Service rating and note"}},
                  String{u8"服务评价与备注"}},
                 {MessageKey{u8"gallery.sample.mentions.editing.title",
                             String{u8"Multiline mentions, long suggestions and cancelable async source"}},
                  String{u8"多行提及、长建议和可取消异步"}},
                 {MessageKey{u8"gallery.sample.auto-complete.contacts.title", String{u8"Contact, note and form"}},
                  String{u8"联系人、备注与表单"}},
                 {MessageKey{u8"gallery.sample.tree.projects.title", String{u8"Project tree and follow-up note"}},
                  String{u8"项目树与处理备注"}},
                 {MessageKey{u8"gallery.sample.tokens.theme.title", String{u8"Theme algorithms and brand"}},
                  String{u8"主题算法与品牌"}},
                 {MessageKey{u8"gallery.sample.tokens.palette.title", String{u8"Semantic colors and palette"}},
                  String{u8"语义色与色板"}},
                 {MessageKey{u8"gallery.sample.tokens.scales.title", String{u8"Typography, sizes and radii"}},
                  String{u8"字号、尺寸与圆角"}},
                 {MessageKey{u8"gallery.sample.tokens.shadows.title", String{u8"Shadows and effects"}},
                  String{u8"阴影与效果"}},
                 {MessageKey{u8"gallery.sample.date-picker.modes.title", String{u8"Picker modes, locale and RTL"}},
                  String{u8"日期模式、locale 与 RTL"}},
                 {MessageKey{u8"gallery.sample.date-picker.range-multiple.title",
                             String{u8"Range and multiple-selection confirmation"}},
                  String{u8"范围与多选确认"}},
                 {MessageKey{u8"gallery.sample.date-picker.date-time.title", String{u8"Combined date and time values"}},
                  String{u8"日期时间组合值"}},
                 {MessageKey{u8"gallery.sample.time-picker.precision.title",
                             String{u8"12-hour mode and exact milliseconds"}},
                  String{u8"12 小时与精确毫秒"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.sample.time-picker.range.title",
                             String{u8"Time range and controlled rejection"}},
                  String{u8"时间范围与受控拒绝"}},
                 {MessageKey{u8"gallery.ant.document.introduction.title", String{u8"Introduction"}}, String{u8"介绍"}},
                 {MessageKey{u8"gallery.ant.document.design-values.title", String{u8"Design Values"}},
                  String{u8"设计价值观"}},
                 {MessageKey{u8"gallery.ant.document.resources.title", String{u8"Resources"}}, String{u8"设计资源"}},
                 {MessageKey{u8"gallery.ant.document.components-overview.title", String{u8"Components Overview"}},
                  String{u8"组件总览"}},
                 {MessageKey{u8"gallery.ant.category.general.name", String{u8"General"}}, String{u8"通用"}},
                 {MessageKey{u8"gallery.ant.category.layout.name", String{u8"Layout"}}, String{u8"布局"}},
                 {MessageKey{u8"gallery.ant.category.navigation.name", String{u8"Navigation"}}, String{u8"导航"}},
                 {MessageKey{u8"gallery.ant.category.data-entry.name", String{u8"Data Entry"}}, String{u8"数据录入"}},
                 {MessageKey{u8"gallery.ant.category.data-display.name", String{u8"Data Display"}},
                  String{u8"数据展示"}},
                 {MessageKey{u8"gallery.ant.category.feedback.name", String{u8"Feedback"}}, String{u8"反馈"}},
                 {MessageKey{u8"gallery.ant.category.other.name", String{u8"Other"}}, String{u8"其他"}},
                 {MessageKey{u8"gallery.value.natural.name", String{u8"Natural"}}, String{u8"自然"}},
                 {MessageKey{u8"gallery.value.natural.summary",
                             String{u8"Follow familiar user expectations so results are predictable."}},
                  String{u8"界面顺应用户已有认知，让操作结果容易预期。"}},
                 {MessageKey{u8"gallery.value.certain.name", String{u8"Certain"}}, String{u8"确定"}},
                 {MessageKey{u8"gallery.value.certain.summary",
                             String{u8"Consistent states, hierarchy and feedback reduce ambiguity and guesswork."}},
                  String{u8"状态、层级和反馈保持一致，减少模糊与猜测。"}},
                 {MessageKey{u8"gallery.value.meaningful.name", String{u8"Meaningful"}}, String{u8"有意义"}},
                 {MessageKey{u8"gallery.value.meaningful.summary",
                             String{u8"Visual emphasis serves tasks and information, without purposeless decoration."}},
                  String{u8"视觉强调服务于任务与信息，不增加无目的装饰。"}},
                 {MessageKey{u8"gallery.value.growing.name", String{u8"Growing"}}, String{u8"生长"}},
                 {MessageKey{u8"gallery.value.growing.summary",
                             String{u8"Components, tokens and acceptance evidence scale with the product."}},
                  String{u8"组件、Token 与验收证据能够随产品规模稳定扩展。"}},
                 {MessageKey{u8"gallery.status.count",
                             String{u8"{label} · {count}"},
                             {{u8"label", MessageArgType::Text}, {u8"count", MessageArgType::UInt}}},
                  String{u8"{label} · {count}"}},
                 {MessageKey{u8"gallery.copy.property.000", String{u8"RynUI heading levels"}},
                  String{u8"RynUI 标题层级"}},
                 {MessageKey{u8"gallery.copy.property.001", String{u8"Strong / Italic"}},
                  String{u8"Strong / Italic · 强调与斜体"}},
                 {MessageKey{u8"gallery.copy.property.003", String{u8"Ctrl + C / Keyboard"}},
                  String{u8"Ctrl + C / 键帽"}},
                 {MessageKey{u8"gallery.copy.property.004", String{u8"Highlight, underline and strikethrough"}},
                  String{u8"高亮、下划线、删除线"}},
                 {MessageKey{u8"gallery.copy.property.005", String{u8"Link · Pointer and keyboard activation"}},
                  String{u8"Link · 指针与键盘激活"}},
                 {MessageKey{u8"gallery.copy.property.006", String{u8"Divider label"}}, String{u8"Divider 标签"}},
                 {MessageKey{u8"gallery.copy.property.007", String{u8"Controlled: type Chinese / Latin / emoji"}},
                  String{u8"受控：输入中文 / Latin / emoji"}},
                 {MessageKey{u8"gallery.copy.property.008", String{u8"Clear to show the placeholder"}},
                  String{u8"清空后显示 placeholder"}},
                 {MessageKey{u8"gallery.copy.property.009", String{u8"Enter a search query"}},
                  String{u8"输入搜索词 / Search"}},
                 {MessageKey{u8"gallery.copy.property.010", String{u8"Enter password"}},
                  String{u8"输入密码 / Password"}},
                 {MessageKey{u8"gallery.copy.property.011", String{u8"None / Explicit zero orientation margin"}},
                  String{u8"None / 显式无朝向间距"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.property.012",
                             String{u8"RTL tooltip · Hover, focus, click or context menu"}},
                  String{u8"RTL 提示 · 悬停、焦点、点击或右键"}},
                 {MessageKey{u8"gallery.copy.property.013", String{u8"Show on hover or Tab focus; Escape closes"}},
                  String{u8"悬停或 Tab 焦点显示，Escape 关闭"}},
                 {MessageKey{u8"gallery.copy.property.014", String{u8"Disabled button retains its explanation"}},
                  String{u8"禁用按钮保留说明"}},
                 {MessageKey{u8"gallery.copy.property.015", String{u8"Pointer anchor"}},
                  String{u8"指针位置 / Pointer anchor"}},
                 {MessageKey{u8"gallery.copy.property.016", String{u8"Centered corner arrow"}},
                  String{u8"居中角箭头 / Centered corner arrow"}},
                 {MessageKey{u8"gallery.copy.property.017", String{u8"Small / Start / RTL"}},
                  String{u8"Small / Start / RTL"}},
                 {MessageKey{u8"gallery.copy.property.018", String{u8"Middle / End / 20 dp"}},
                  String{u8"Middle / End / 20 dp"}},
                 {MessageKey{u8"gallery.copy.property.019", String{u8"Fixed 22 dp"}}, String{u8"固定 22 dp"}},
                 {MessageKey{u8"gallery.copy.property.020", String{u8"Dynamic height"}}, String{u8"动态高度"}},
                 {MessageKey{u8"gallery.copy.property.021", String{u8"Explicit column=1 / height=108"}},
                  String{u8"指定 column=1 / height=108"}},
                 {MessageKey{u8"gallery.copy.property.022", String{u8"New key"}}, String{u8"新增 key"}},
                 {MessageKey{u8"gallery.copy.property.023",
                             String{u8"Press Escape to close the tooltip or menu, then again to cancel the dialog"}},
                  String{u8"先按Escape关闭提示或菜单，再按一次取消对话框"}},
                 {MessageKey{u8"gallery.copy.property.024", String{u8"Nested confirmation"}}, String{u8"二级确认"}},
                 {MessageKey{u8"gallery.copy.property.025", String{u8"Edit account"}},
                  String{u8"编辑账户 / Edit account"}},
                 {MessageKey{u8"gallery.copy.property.026", String{u8"Nested drawer"}}, String{u8"嵌套抽屉"}},
                 {MessageKey{u8"gallery.copy.property.027", String{u8"Save the current draft?"}},
                  String{u8"保存当前草稿？"}},
                 {MessageKey{u8"gallery.copy.property.028", String{u8"Perform this action?"}},
                  String{u8"执行这项操作？"}},
                 {MessageKey{u8"gallery.copy.property.029", String{u8"Only show the confirm button"}},
                  String{u8"只显示确认按钮"}},
                 {MessageKey{u8"gallery.copy.property.030", String{u8"Unstable connection"}}, String{u8"连接不稳定"}},
                 {MessageKey{u8"gallery.copy.property.031",
                             String{u8"Check the connection and retry, or dismiss this message."}},
                  String{u8"请检查连接后重试，也可以关闭此提示。"}},
                 {MessageKey{u8"gallery.copy.property.032", String{u8"Data saved"}}, String{u8"数据已保存"}},
                 {MessageKey{u8"gallery.copy.property.033", String{u8"Action failed; retry later"}},
                  String{u8"操作失败，请稍后重试"}},
                 {MessageKey{
                      u8"gallery.copy.property.034",
                      String{u8"Service notice: window-local notices retain their theme and content; hover to pause "
                             u8"scrolling and leave to resume."}},
                  String{u8"服务维护公告：窗口内的提示保留主题与内容；将鼠标移入暂停滚动，移出继续。"}},
                 {MessageKey{u8"gallery.copy.property.035", String{u8"Reading"}}, String{u8"正在读取"}},
                 {MessageKey{u8"gallery.copy.property.036", String{u8"Select region / Search paths"}},
                  String{u8"选择地区 / Search paths"}},
                 {MessageKey{u8"gallery.copy.property.037", String{u8"Retained child-popup draft"}},
                  String{u8"保留子浮层草稿"}},
                 {MessageKey{u8"gallery.copy.property.038", String{u8"Retained note inside a custom panel"}},
                  String{u8"自定义 panel 中保留的备注"}},
                 {MessageKey{u8"gallery.copy.property.039", String{u8"No filter results; adjust the query"}},
                  String{u8"暂无筛选结果，请调整查询条件"}},
                 {MessageKey{u8"gallery.copy.property.040", String{u8"Custom illustration uses a native Icon"}},
                  String{u8"自定义图示使用原生 Icon"}},
                 {MessageKey{u8"gallery.copy.property.041",
                             String{u8"Continue editing the same draft after changing status"}},
                  String{u8"切换状态后继续编辑同一份草稿"}},
                 {MessageKey{u8"gallery.copy.property.042", String{u8"404 · Page not found"}},
                  String{u8"404 · 页面未找到"}},
                 {MessageKey{u8"gallery.copy.property.043", String{u8"Check the task address or return to the list"}},
                  String{u8"请检查任务地址或返回列表"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.property.044", String{u8"Custom result presentation"}},
                  String{u8"自定义结果展示"}},
                 {MessageKey{u8"gallery.copy.property.045",
                             String{u8"Compose the illustration from existing controls using typed slots"}},
                  String{u8"图示可通过 typed slot 组合现有控件"}},
                 {MessageKey{u8"gallery.copy.property.046", String{u8"Retained content before the scroll target"}},
                  String{u8"滚动目标前的保留内容"}},
                 {MessageKey{u8"gallery.copy.property.047", String{u8"Name"}}, String{u8"名称"}},
                 {MessageKey{u8"gallery.copy.property.048",
                             String{u8"The first compatible control binds to the field automatically"}},
                  String{u8"首个兼容控件自动绑定字段"}},
                 {MessageKey{u8"gallery.copy.property.049", String{u8"Enter a name"}}, String{u8"填写名称"}},
                 {MessageKey{u8"gallery.copy.property.050", String{u8"Exact amount"}}, String{u8"精确金额"}},
                 {MessageKey{u8"gallery.copy.property.051", String{u8"Retained multiline note"}},
                  String{u8"保留多行备注"}},
                 {MessageKey{u8"gallery.copy.property.052", String{u8"Name (taken simulates an existing name)"}},
                  String{u8"名称（taken 模拟占用）"}},
                 {MessageKey{u8"gallery.copy.property.053", String{u8"Enter again"}}, String{u8"再次输入"}},
                 {MessageKey{u8"gallery.copy.property.054", String{u8"Form · Fill, validate and save"}},
                  String{u8"Form · 填写、校验与保存"}},
                 {MessageKey{u8"gallery.copy.property.055", String{u8"Name"}}, String{u8"名称"}},
                 {MessageKey{u8"gallery.copy.property.056", String{u8"Role"}}, String{u8"角色"}},
                 {MessageKey{u8"gallery.copy.property.057", String{u8"Quantity"}}, String{u8"数量"}},
                 {MessageKey{u8"gallery.copy.property.058",
                             String{u8"Required and at least 1; reset clears unfinished numeric drafts"}},
                  String{u8"必填，且至少为 1；重置会清除未完成数字草稿"}},
                 {MessageKey{u8"gallery.copy.property.059", String{u8"Consent"}}, String{u8"确认"}},
                 {MessageKey{u8"gallery.copy.property.060", String{u8"DatePicker · Date selection"}},
                  String{u8"DatePicker · 日期选择"}},
                 {MessageKey{u8"gallery.copy.property.061", String{u8"Form String date adapter"}},
                  String{u8"Form String 日期适配"}},
                 {MessageKey{u8"gallery.copy.property.062", String{u8"Date"}}, String{u8"日期"}},
                 {MessageKey{
                      u8"gallery.copy.property.065",
                      String{u8"Multiple selection; close tags or use Backspace with an empty search to remove"}},
                  String{u8"多选；标签关闭和空搜索 Backspace 可移除"}},
                 {MessageKey{u8"gallery.copy.property.066",
                             String{u8"Tags: enter Chinese; use comma or Enter to create a tag"}},
                  String{u8"Tags：输入中文；英文, 或 Enter 创建标签"}},
                 {MessageKey{u8"gallery.copy.property.067", String{u8"Independent draft inside popup"}},
                  String{u8"popup 内独立草稿"}},
                 {MessageKey{u8"gallery.copy.property.068", String{u8"Synthetic long list; Home/End/arrows/wheel"}},
                  String{u8"合成长列表；Home/End/上下键/滚轮"}},
                 {MessageKey{u8"gallery.copy.property.069", String{u8"Synthetic options await delivery"}},
                  String{u8"合成选项待交付"}},
                 {MessageKey{u8"gallery.copy.property.070",
                             String{u8"Enter a query, then deliver results inside the popup"}},
                  String{u8"输入查询后，在 popup 内点击交付"}},
                 {MessageKey{u8"gallery.copy.property.071", String{u8"Select / Search"}}, String{u8"请选择 / Search"}},
                 {MessageKey{u8"gallery.copy.property.072", String{u8"No matching items"}}, String{u8"没有匹配项"}},
                 {MessageKey{u8"gallery.copy.property.073", String{u8"No options"}}, String{u8"暂无选项"}},
                 {MessageKey{u8"gallery.copy.property.074", String{u8"Help"}}, String{u8"帮助"}},
                 {MessageKey{u8"gallery.copy.property.075", String{u8"Draggable window-floating entry"}},
                  String{u8"窗口浮动入口，可拖动"}},
                 {MessageKey{u8"gallery.copy.property.076", String{u8"No accounts; restore to continue"}},
                  String{u8"没有账户，点击恢复继续"}},
                 {MessageKey{u8"gallery.copy.property.077",
                             String{u8"Text description; image and action identities remain"}},
                  String{u8"文本说明，图片与操作区身份保持"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.property.078",
                             String{u8"Native vector illustration from the pinned upstream path"}},
                  String{u8"锁定上游路径的原生矢量插画"}},
                 {MessageKey{u8"gallery.copy.property.079", String{u8"Team"}}, String{u8"团队"}},
                 {MessageKey{u8"gallery.copy.property.080", String{u8"Meta description and native interaction"}},
                  String{u8"Meta 描述与原生交互"}},
                 {MessageKey{u8"gallery.copy.property.081", String{u8"Account"}}, String{u8"Account / 账户"}},
                 {MessageKey{u8"gallery.copy.property.082", String{u8"Native composition"}},
                  String{u8"Native composition / 原生组合"}},
                 {MessageKey{u8"gallery.copy.property.083", String{u8"Small"}}, String{u8"Small / 小号"}},
                 {MessageKey{u8"gallery.copy.property.084", String{u8"Hover"}}, String{u8"Hover / 悬停"}},
                 {MessageKey{u8"gallery.copy.property.085", String{u8"Loading"}}, String{u8"Loading / 加载"}},
                 {MessageKey{u8"gallery.copy.property.086", String{u8"Breadcrumb · Project hierarchy"}},
                  String{u8"Breadcrumb · 项目层级"}},
                 {MessageKey{u8"gallery.copy.property.087", String{u8"Breadcrumb draft"}},
                  String{u8"Breadcrumb draft"}},
                 {MessageKey{u8"gallery.copy.property.088", String{u8"Steps · Release workflow"}},
                  String{u8"Steps · 发布流程"}},
                 {MessageKey{u8"gallery.copy.property.089", String{u8"Edit note"}}, String{u8"编辑备注"}},
                 {MessageKey{u8"gallery.copy.property.090", String{u8"Steps draft"}}, String{u8"Steps draft"}},
                 {MessageKey{u8"gallery.copy.property.091", String{u8"Review details"}}, String{u8"核对资料"}},
                 {MessageKey{u8"gallery.copy.property.092", String{u8"Publish after confirmation"}},
                  String{u8"确认后发布"}},
                 {MessageKey{u8"gallery.copy.property.093", String{u8"TimePicker · Time selection"}},
                  String{u8"TimePicker · 时间选择"}},
                 {MessageKey{u8"gallery.copy.property.094", String{u8"Form String time adapter"}},
                  String{u8"Form String 时间适配"}},
                 {MessageKey{u8"gallery.copy.property.095", String{u8"Time"}}, String{u8"时间"}},
                 {MessageKey{u8"gallery.copy.property.096", String{u8"Descriptions · Project details"}},
                  String{u8"Descriptions · 项目资料"}},
                 {MessageKey{u8"gallery.copy.property.097", String{u8"Name"}}, String{u8"Name"}},
                 {MessageKey{u8"gallery.copy.property.098", String{u8"Skeleton · Content loading"}},
                  String{u8"Skeleton · 正文加载"}},
                 {MessageKey{u8"gallery.copy.property.099", String{u8"Content draft"}}, String{u8"Content draft"}},
                 {MessageKey{u8"gallery.copy.property.100", String{u8"Scrollable project checklist"}},
                  String{u8"可滚动的项目清单"}},
                 {MessageKey{u8"gallery.copy.property.101", String{u8"Popover · Project note"}},
                  String{u8"Popover · 项目备注"}},
                 {MessageKey{u8"gallery.copy.property.102", String{u8"Project note"}}, String{u8"Project note"}},
                 {MessageKey{u8"gallery.copy.property.103", String{u8"Timeline · Project activity"}},
                  String{u8"Timeline · 项目处理记录"}},
                 {MessageKey{u8"gallery.copy.property.104", String{u8"Follow-up note"}}, String{u8"Follow-up note"}},
                 {MessageKey{u8"gallery.copy.property.105", String{u8"Avatar · Members"}},
                  String{u8"Avatar · 成员头像"}},
                 {MessageKey{u8"gallery.copy.property.106", String{u8"Member name"}}, String{u8"Member name"}},
                 {MessageKey{u8"gallery.copy.property.107", String{u8"Active timer"}}, String{u8"活动计时器"}},
                 {MessageKey{u8"gallery.copy.property.108", String{u8"Localized numbers and finite animation"}},
                  String{u8"本地化数值与有限动画"}},
                 {MessageKey{u8"gallery.copy.property.109", String{u8"Statistic · Metrics"}},
                  String{u8"Statistic · 统计指标"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.copy.property.110", String{u8"Current value"}}, String{u8"本期数值"}},
                 {MessageKey{u8"gallery.copy.property.111", String{u8"Metric note"}}, String{u8"指标备注"}},
                 {MessageKey{u8"gallery.copy.property.112", String{u8"Metric note"}}, String{u8"Metric note"}},
                 {MessageKey{u8"gallery.copy.property.113", String{u8"Rate · Service rating"}},
                  String{u8"Rate · 服务评分"}},
                 {MessageKey{u8"gallery.copy.property.114", String{u8"Rating note"}}, String{u8"Rating note"}},
                 {MessageKey{u8"gallery.copy.property.115",
                             String{u8"Grouped suggestions; use arrows and wheel to browse"}},
                  String{u8"分组建议；用上下键和滚轮浏览"}},
                 {MessageKey{u8"gallery.copy.property.116", String{u8"Independent popup note"}},
                  String{u8"popup 内的独立备注"}},
                 {MessageKey{u8"gallery.copy.property.117",
                             String{u8"Custom TextArea; native editor continues to handle IME and clipboard"}},
                  String{u8"自定义 TextArea，IME/剪贴板继续由原生编辑器处理"}},
                 {MessageKey{u8"gallery.copy.property.118",
                             String{u8"Enter a query, then deliver results inside the popup"}},
                  String{u8"输入查询后，在 popup 内点击交付"}},
                 {MessageKey{u8"gallery.copy.property.119",
                             String{u8"@query · Explicit application delivery of synthetic results"}},
                  String{u8"@query · 应用显式交付合成结果"}},
                 {MessageKey{u8"gallery.copy.property.120",
                             String{u8"Type @A or #U; multiline, selection, undo and clipboard are supported"}},
                  String{u8"输入 @A 或 #U，支持多行、选区、撤销和剪贴板"}},
                 {MessageKey{u8"gallery.copy.property.121",
                             String{u8"Type @ and use arrows or wheel to browse long suggestions"}},
                  String{u8"输入 @，使用上下键或滚轮浏览长建议"}},
                 {MessageKey{u8"gallery.copy.property.122", String{u8"AutoComplete · Contact suggestions"}},
                  String{u8"AutoComplete · 联系人建议"}},
                 {MessageKey{u8"gallery.copy.property.123", String{u8"Contact note"}}, String{u8"Contact note"}},
                 {MessageKey{u8"gallery.copy.property.124", String{u8"Enter or select a contact"}},
                  String{u8"输入或选择联系人"}},
                 {MessageKey{u8"gallery.copy.property.125", String{u8"Form contact"}}, String{u8"表单联系人"}},
                 {MessageKey{u8"gallery.copy.property.126", String{u8"Form contact"}}, String{u8"Form contact"}},
                 {MessageKey{u8"gallery.copy.property.127", String{u8"Filter native tree"}}, String{u8"过滤原生树"}},
                 {MessageKey{u8"gallery.copy.property.128", String{u8"Tree · Project directory"}},
                  String{u8"Tree · 项目目录"}},
                 {MessageKey{u8"gallery.copy.property.129", String{u8"Project note"}}, String{u8"Project note"}},
                 {MessageKey{u8"gallery.copy.property.130", String{u8"TreeSelect · Hierarchical target"}},
                  String{u8"TreeSelect · 层级目标"}},
                 {MessageKey{u8"gallery.copy.property.131", String{u8"Select delivery target"}},
                  String{u8"选择交付目标"}},
                 {MessageKey{u8"gallery.copy.property.132", String{u8"Delivery note"}}, String{u8"交付备注"}},
                 {MessageKey{u8"gallery.copy.property.133", String{u8"Form · Delivery target"}},
                  String{u8"Form · 交付目标"}},
                 {MessageKey{u8"gallery.copy.property.134", String{u8"Delivery target"}}, String{u8"交付目标"}},
                 {MessageKey{u8"gallery.copy.property.135", String{u8"Select a form target"}},
                  String{u8"请选择表单目标"}},
                 {MessageKey{u8"gallery.copy.property.137", String{u8"Editable content with watermark"}},
                  String{u8"带水印的可编辑内容"}},
                 {MessageKey{u8"gallery.copy.property.138", String{u8"Affixed at top: actual retained input"}},
                  String{u8"顶部固定：真实保留输入"}},
                 {MessageKey{u8"gallery.copy.property.139", String{u8"Scrollable content"}}, String{u8"滚动内容"}},
                 {MessageKey{u8"gallery.copy.property.140", String{u8"Border beam along a rounded perimeter"}},
                  String{u8"沿圆角周长运动的边框光束"}},
                 {MessageKey{u8"gallery.feedback.000",
                             String{u8"Drag or focus the divider; use arrows/Home/End to resize"}},
                  String{u8"拖动或聚焦分隔条，用箭头/Home/End 调整"}},
                 {MessageKey{u8"gallery.feedback.001", String{u8"Sidebar state and window-breakpoint callbacks"}},
                  String{u8"侧栏状态与窗口断点回调"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.feedback.002", String{u8"Focus callback: waiting for interaction"}},
                  String{u8"焦点回调：等待交互"}},
                 {MessageKey{u8"gallery.feedback.003",
                             String{u8"Enter submits; selection, clipboard and undo/redo are supported"}},
                  String{u8"Enter 提交；支持选择、剪贴板、撤销/重做"}},
                 {MessageKey{u8"gallery.feedback.004", String{u8"Submit search with Enter or the button"}},
                  String{u8"Enter 或按钮提交搜索"}},
                 {MessageKey{u8"gallery.feedback.005",
                             String{u8"Open an example to inspect confirm and cancel sources"}},
                  String{u8"打开示例后查看确认与取消来源"}},
                 {MessageKey{u8"gallery.feedback.006", String{u8"No dialog is open"}},
                  String{u8"当前没有打开的对话框"}},
                 {MessageKey{u8"gallery.feedback.007", String{u8"Operate the components below to see state changes"}},
                  String{u8"操作下面的组件查看状态变化"}},
                 {MessageKey{u8"gallery.feedback.008",
                             String{u8"Edit quantities, select records or run menu commands"}},
                  String{u8"编辑数量、选择记录或执行菜单命令"}},
                 {MessageKey{u8"gallery.feedback.009", String{u8"Expand a panel or select a view"}},
                  String{u8"展开面板或选择视图"}},
                 {MessageKey{u8"gallery.feedback.010", String{u8"Status updates retain the editor draft in details"}},
                  String{u8"状态更新保留详情中的编辑草稿"}},
                 {MessageKey{u8"gallery.feedback.011", String{u8"Not submitted"}}, String{u8"尚未提交"}},
                 {MessageKey{u8"gallery.feedback.012", String{u8"Date not saved"}}, String{u8"尚未保存日期"}},
                 {MessageKey{u8"gallery.feedback.013",
                             String{u8"Select an anchor or use the slider to scroll the document"}},
                  String{u8"选择锚点或拖动滑块滚动正文"}},
                 {MessageKey{u8"gallery.feedback.014", String{u8"Select an ancestor or edit the current draft"}},
                  String{u8"选择祖先或编辑当前草稿"}},
                 {MessageKey{u8"gallery.feedback.015", String{u8"Fill in the details to continue"}},
                  String{u8"填写资料后继续"}},
                 {MessageKey{u8"gallery.feedback.016", String{u8"Nothing selected"}}, String{u8"尚未选择"}},
                 {MessageKey{u8"gallery.feedback.017", String{u8"Time not saved"}}, String{u8"尚未保存时间"}},
                 {MessageKey{u8"gallery.feedback.018", String{u8"Edit the profile, then save"}},
                  String{u8"编辑资料后保存"}},
                 {MessageKey{u8"gallery.feedback.019", String{u8"Edit the content and select its audience"}},
                  String{u8"编辑正文并选择分享范围"}},
                 {MessageKey{u8"gallery.feedback.020", String{u8"Open the popover to edit the note"}},
                  String{u8"打开气泡编辑备注"}},
                 {MessageKey{u8"gallery.feedback.021",
                             String{u8"Notices persist; close or clear them using the buttons"}},
                  String{u8"提示会保留，可用按钮关闭或清空"}},
                 {MessageKey{u8"gallery.feedback.022", String{u8"Edit and save the follow-up note"}},
                  String{u8"编辑并保存处理备注"}},
                 {MessageKey{u8"gallery.feedback.023", String{u8"Edit a member name; open +N to browse all members"}},
                  String{u8"编辑成员名，打开 +N 浏览全部成员"}},
                 {MessageKey{u8"gallery.feedback.024", String{u8"Note not saved"}}, String{u8"尚未保存备注"}},
                 {MessageKey{u8"gallery.feedback.025", String{u8"Focus and key events: waiting for interaction"}},
                  String{u8"焦点与按键事件：等待交互"}},
                 {MessageKey{u8"gallery.feedback.026", String{u8"Review not saved"}}, String{u8"尚未保存评价"}},
                 {MessageKey{u8"gallery.feedback.027", String{u8"Multi-selection unchanged"}},
                  String{u8"尚未修改多选"}},
                 {MessageKey{u8"gallery.feedback.028",
                             String{u8"Synthetic data; type and deliver results to simulate async completion"}},
                  String{u8"合成数据；输入后点击交付以模拟异步完成"}},
                 {MessageKey{u8"gallery.feedback.029", String{u8"No suggestion selected"}}, String{u8"尚未选择建议"}},
                 {MessageKey{u8"gallery.feedback.030", String{u8"Synthetic async source ready"}},
                  String{u8"合成异步源就绪"}},
                 {MessageKey{u8"gallery.feedback.031", String{u8"No contact selected"}}, String{u8"尚未选择联系人"}},
                 {MessageKey{u8"gallery.feedback.032", String{u8"Form not saved"}}, String{u8"表单尚未保存"}},
                 {MessageKey{u8"gallery.feedback.033", String{u8"Unchanged"}}, String{u8"尚未修改"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{
                      u8"gallery.feedback.034",
                      String{u8"Explicit simulated loading service: use buttons to complete or reject this request"}},
                  String{u8"显式模拟加载服务：使用按钮完成或拒绝本次请求"}},
                 {MessageKey{u8"gallery.feedback.035",
                             String{u8"Use buttons to inspect actual callbacks and digit rolling; "
                                    u8"Processing requests frames only while visible and active"}},
                  String{u8"点击按钮检查真实回调与数字滚动；Processing 仅在可见和活动时请求帧"}},
                 {MessageKey{u8"gallery.feedback.036", String{u8"Closing is canceled and reorder is rejected by "
                                                              u8"default; allow them using the buttons, then retry"}},
                  String{u8"关闭默认被取消，重排默认拒绝；通过按钮允许后重试"}},
                 {MessageKey{u8"gallery.feedback.037",
                             String{u8"Synthetic application transport: no HTTP request is performed"}},
                  String{u8"Synthetic application transport: no HTTP request is performed"}},
                 {MessageKey{u8"gallery.feedback.038",
                             String{u8"Right-click or click to open; scroll to the bottom to save"}},
                  String{u8"右键或点击打开，滚动到底部保存"}},
                 {MessageKey{u8"gallery.feedback.039", String{u8"Start a countdown or count up"}},
                  String{u8"启动倒计时或正计时"}},
                 {MessageKey{u8"gallery.feedback.040",
                             String{u8"Switch with arrows, indicators, keyboard or background "
                                    u8"dragging; native child editors retain selection and drafts."}},
                  String{u8"箭头、指示器、键盘和背景拖动切换；编辑器的选择与草稿由原生子控件保留。"}},
                 {MessageKey{u8"gallery.feedback.041", String{u8"Controlled requests are rejected by default; enable "
                                                              u8"acceptance, then use a custom dot or Ref."}},
                  String{u8"默认拒绝受控请求；打开接受后再使用自定义 dot 或 Ref。"}},
                 {MessageKey{u8"gallery.feedback.042", String{u8"Select a date or use the year/month header; cell "
                                                              u8"notes remain until their date leaves the grid."}},
                  String{u8"选择日期或通过年月 header 切换；单元格备注保留到日期离开网格。"}},
                 {MessageKey{u8"gallery.feedback.043",
                             String{u8"Ctrl/Shift multi-selection; application writes back row order after dragging"}},
                  String{u8"Ctrl/Shift 多选；拖动行后由应用回写顺序"}},
                 {MessageKey{u8"gallery.feedback.044", String{u8"Multiple selection / tags / search / rich popup"}},
                  String{u8"多选 / 标签 / 搜索 / 富 popup"}},
                 {MessageKey{u8"gallery.feedback.045",
                             String{u8"Editing re-encodes immediately; PNG/SVG exports are application-owned data"}},
                  String{u8"编辑内容可立即重新编码；PNG/SVG 导出为应用持有的数据"}},
                 {MessageKey{u8"gallery.feedback.046",
                             String{u8"Scroll this page to observe top/bottom affixing; the input draft remains."}},
                  String{u8"滚动当前页面观察顶部 / 底部固定；输入草稿保留。"}},
                 {MessageKey{u8"gallery.feedback.047", String{u8"Resize started: pointer captured"}},
                  String{u8"调整开始：指针已捕获"}},
                 {MessageKey{u8"gallery.feedback.048", String{u8"Updating panel layout"}},
                  String{u8"持续更新面板布局"}},
                 {MessageKey{u8"gallery.feedback.049", String{u8"Resize ended: capture released"}},
                  String{u8"调整结束：捕获已释放"}},
                 {MessageKey{u8"gallery.feedback.050",
                             String{u8"Window below lg: zero-width trigger remains focusable"}},
                  String{u8"窗口小于 lg：零宽度触发器仍可聚焦"}},
                 {MessageKey{u8"gallery.feedback.051", String{u8"Window at lg: sidebar expanded"}},
                  String{u8"窗口达到 lg：侧栏展开"}},
                 {MessageKey{u8"gallery.feedback.052", String{u8"Slider focus"}}, String{u8"Slider focus"}},
                 {MessageKey{u8"gallery.feedback.053", String{u8"Slider blur"}}, String{u8"Slider blur"}},
                 {MessageKey{u8"gallery.feedback.054", String{u8"Switch focus"}}, String{u8"Switch focus"}},
                 {MessageKey{u8"gallery.feedback.055", String{u8"Switch blur"}}, String{u8"Switch blur"}},
                 {MessageKey{u8"gallery.feedback.056", String{u8"Checkbox focus"}}, String{u8"Checkbox focus"}},
                 {MessageKey{u8"gallery.feedback.057", String{u8"Checkbox blur"}}, String{u8"Checkbox blur"}},
                 {MessageKey{u8"gallery.feedback.058", String{u8"Radio focus"}}, String{u8"Radio focus"}},
                 {MessageKey{u8"gallery.feedback.059", String{u8"Radio blur"}}, String{u8"Radio blur"}},
                 {MessageKey{u8"gallery.feedback.060",
                             String{u8"Confirmation requested; this example keeps the dialog open"}},
                  String{u8"已收到确认请求，本例继续保持打开"}},
                 {MessageKey{u8"gallery.feedback.061", String{u8"Confirmed"}}, String{u8"已确认"}},
                 {MessageKey{u8"gallery.feedback.062", String{u8"Dialog shown"}}, String{u8"对话框已显示"}},
                 {MessageKey{u8"gallery.feedback.063", String{u8"Dialog hidden"}}, String{u8"对话框已隐藏"}},
                 {MessageKey{u8"gallery.feedback.064", String{u8"Dialog closed; input and popups cleaned up"}},
                  String{u8"对话框已关闭，输入和浮层已清理"}},
                 {MessageKey{u8"gallery.feedback.065", String{u8"Custom footer: saved"}},
                  String{u8"自定义页脚：已保存"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.feedback.066",
                             String{u8"Actual ImagePixels loaded; four colored pixels use the shared RGBA path"}},
                  String{u8"真实 ImagePixels 已加载，四个彩色像素由共享 RGBA 路径呈现"}},
                 {MessageKey{u8"gallery.feedback.067", String{u8"Old request canceled; retry started"}},
                  String{u8"已取消旧请求并重试"}},
                 {MessageKey{u8"gallery.feedback.068",
                             String{u8"Reordered by stable key; open +N to inspect overflow members"}},
                  String{u8"已按稳定键重排，打开 +N 查看溢出成员"}},
                 {MessageKey{u8"gallery.feedback.069", String{u8"Independent child button callback executed"}},
                  String{u8"独立子按钮回调已执行"}},
                 {MessageKey{u8"gallery.feedback.070", String{u8"Independent Ribbon child button callback executed"}},
                  String{u8"Ribbon 内部独立按钮回调已执行"}},
                 {MessageKey{u8"gallery.feedback.071", String{u8"Application canceled closing"}},
                  String{u8"关闭被应用取消"}},
                 {MessageKey{u8"gallery.feedback.072",
                             String{u8"Close accepted; input blocked immediately and content hidden after motion"}},
                  String{u8"关闭已接受，输入立即屏蔽，动画结束后隐藏"}},
                 {MessageKey{u8"gallery.feedback.073", String{u8"Closing allowed; click the tag close button again"}},
                  String{u8"允许关闭，请再次点击标签关闭按钮"}},
                 {MessageKey{u8"gallery.feedback.074", String{u8"Selected"}}, String{u8"已选中"}},
                 {MessageKey{u8"gallery.feedback.075", String{u8"Deselected"}}, String{u8"已取消选择"}},
                 {MessageKey{u8"gallery.feedback.076", String{u8"Controlled reorder rejected; accepted order remains"}},
                  String{u8"受控重排请求被拒绝，已接受顺序保持"}},
                 {MessageKey{u8"gallery.feedback.077",
                             String{u8"Stable-key reorder accepted; drafts and selection retained"}},
                  String{u8"已接受稳定键重排，草稿和选择保留"}},
                 {MessageKey{u8"gallery.feedback.078", String{u8"Reorder allowed"}}, String{u8"允许重排"}},
                 {MessageKey{u8"gallery.feedback.079", String{u8"Reorder rejected"}}, String{u8"拒绝重排"}},
                 {MessageKey{u8"gallery.feedback.080", String{u8"Drawer draft saved"}}, String{u8"抽屉草稿已确认保存"}},
                 {MessageKey{u8"gallery.feedback.081", String{u8"Action canceled"}}, String{u8"已取消操作"}},
                 {MessageKey{u8"gallery.feedback.082", String{u8"Action completed"}}, String{u8"操作已完成"}},
                 {MessageKey{u8"gallery.feedback.083", String{u8"Reconnection requested"}}, String{u8"已请求重新连接"}},
                 {MessageKey{u8"gallery.feedback.084", String{u8"Loading-region action executed"}},
                  String{u8"加载区域中的操作已执行"}},
                 {MessageKey{u8"gallery.feedback.085", String{u8"Actual beforeUpload decision: Ignore"}},
                  String{u8"Actual beforeUpload decision: Ignore"}},
                 {MessageKey{u8"gallery.feedback.086",
                             String{u8"Synthetic request pending; deliver manually or cancel"}},
                  String{u8"Synthetic request pending; deliver manually or cancel"}},
                 {MessageKey{u8"gallery.feedback.087", String{u8"Synthetic request canceled"}},
                  String{u8"Synthetic request cancelled"}},
                 {MessageKey{u8"gallery.feedback.088", String{u8"Removal awaits application approval"}},
                  String{u8"Removal pending application approval"}},
                 {MessageKey{u8"gallery.feedback.089", String{u8"Transfer move refused"}},
                  String{u8"Transfer move refused"}},
                 {MessageKey{u8"gallery.feedback.090", String{u8"First local demo error; click Retry"}},
                  String{u8"本地演示的首次错误；点击 Retry 重试"}},
                 {MessageKey{u8"gallery.feedback.091",
                             String{u8"Application-owned loader; this example supplies local data synchronously"}},
                  String{u8"加载器由应用提供；本例同步提供本地数据"}},
                 {MessageKey{u8"gallery.feedback.092", String{u8"Split button: default action executed"}},
                  String{u8"分离按钮：已运行默认操作"}},
                 {MessageKey{u8"gallery.feedback.093", String{u8"Format callback"}}, String{u8"Format callback"}},
                 {MessageKey{u8"gallery.feedback.094", String{u8"Clear callback"}}, String{u8"Clear callback"}},
                 {MessageKey{u8"gallery.feedback.095", String{u8"Empty"}}, String{u8"empty"}},
                 {MessageKey{u8"gallery.feedback.096", String{u8"Independent help action executed"}},
                  String{u8"已执行独立帮助操作"}},
                 {MessageKey{u8"gallery.feedback.097", String{u8"Result continue action executed"}},
                  String{u8"已执行结果页继续操作"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.feedback.098", String{u8"Disabled; submission skipped"}},
                  String{u8"当前禁用，提交未执行"}},
                 {MessageKey{u8"gallery.feedback.099", String{u8"Validation passed; not saved yet"}},
                  String{u8"校验通过，尚未保存"}},
                 {MessageKey{u8"gallery.feedback.100", String{u8"Validation failed; inspect field errors"}},
                  String{u8"校验失败，请查看字段错误"}},
                 {MessageKey{u8"gallery.feedback.101", String{u8"Reset"}}, String{u8"已重置"}},
                 {MessageKey{u8"gallery.feedback.102", String{u8"Date validation failed"}}, String{u8"日期校验失败"}},
                 {MessageKey{u8"gallery.feedback.103", String{u8"Disabled; date submission skipped"}},
                  String{u8"当前禁用，日期提交未执行"}},
                 {MessageKey{u8"gallery.feedback.104", String{u8"Date reset"}}, String{u8"日期已重置"}},
                 {MessageKey{u8"gallery.feedback.105", String{u8"Referenced input focused"}},
                  String{u8"引用输入已聚焦"}},
                 {MessageKey{u8"gallery.feedback.106", String{u8"onClear delivered"}}, String{u8"onClear 已通知"}},
                 {MessageKey{u8"gallery.feedback.107", String{u8"Clear"}}, String{u8"Clear"}},
                 {MessageKey{u8"gallery.feedback.108", String{u8"Multi-selection accepted"}}, String{u8"已接受多选"}},
                 {MessageKey{u8"gallery.feedback.109", String{u8"Multi-selection rejected"}}, String{u8"已拒绝多选"}},
                 {MessageKey{u8"gallery.feedback.110", String{u8"Prefix action executed"}}, String{u8"已执行前缀操作"}},
                 {MessageKey{u8"gallery.feedback.111",
                             String{u8"Synthetic options processed; canceled requests cannot update the selector"}},
                  String{u8"已处理合成选项；已取消的请求不会更新选择器"}},
                 {MessageKey{u8"gallery.feedback.112", String{u8"Floating action executed"}},
                  String{u8"已执行浮动操作"}},
                 {MessageKey{u8"gallery.feedback.113", String{u8"Window-floating entry clicked"}},
                  String{u8"窗口浮动入口已点击"}},
                 {MessageKey{u8"gallery.feedback.114", String{u8"Previous step"}}, String{u8"上一步"}},
                 {MessageKey{u8"gallery.feedback.115", String{u8"Release completed"}}, String{u8"发布完成"}},
                 {MessageKey{u8"gallery.feedback.116", String{u8"Next step"}}, String{u8"下一步"}},
                 {MessageKey{u8"gallery.feedback.117", String{u8"Flow reset; note retained"}},
                  String{u8"流程已重置，备注保留"}},
                 {MessageKey{u8"gallery.feedback.118", String{u8"Example restored"}}, String{u8"已恢复示例"}},
                 {MessageKey{u8"gallery.feedback.119", String{u8"Time validation failed"}}, String{u8"时间校验失败"}},
                 {MessageKey{u8"gallery.feedback.120", String{u8"Disabled; time submission skipped"}},
                  String{u8"当前禁用，时间提交未执行"}},
                 {MessageKey{u8"gallery.feedback.121", String{u8"Time reset"}}, String{u8"时间已重置"}},
                 {MessageKey{u8"gallery.feedback.122", String{u8"Saved at the end of the checklist"}},
                  String{u8"清单末尾保存成功"}},
                 {MessageKey{u8"gallery.feedback.123", String{u8"Message closed"}}, String{u8"消息已关闭"}},
                 {MessageKey{u8"gallery.feedback.124", String{u8"Processing message opened"}},
                  String{u8"处理提示已打开"}},
                 {MessageKey{u8"gallery.feedback.125", String{u8"The same message was updated to success"}},
                  String{u8"同一提示已更新为处理成功"}},
                 {MessageKey{u8"gallery.feedback.126", String{u8"Message closing completed"}},
                  String{u8"消息关闭已完成"}},
                 {MessageKey{u8"gallery.feedback.127", String{u8"Message canceled"}}, String{u8"消息已取消"}},
                 {MessageKey{u8"gallery.feedback.128", String{u8"Message remains open"}}, String{u8"消息仍打开"}},
                 {MessageKey{u8"gallery.feedback.129", String{u8"Notification closed; it can be reopened"}},
                  String{u8"通知已关闭，可重新打开"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.feedback.130", String{u8"Notification opened; edit the review note"}},
                  String{u8"通知已打开，可编辑核对备注"}},
                 {MessageKey{u8"gallery.feedback.131", String{u8"Notification title updated; edited content retained"}},
                  String{u8"已更新通知标题，编辑内容保持"}},
                 {MessageKey{u8"gallery.feedback.132", String{u8"Messages and notifications cleared"}},
                  String{u8"已清空消息与通知"}},
                 {MessageKey{u8"gallery.feedback.133", String{u8"Review notification closed"}},
                  String{u8"审核通知已关闭"}},
                 {MessageKey{u8"gallery.feedback.134", String{u8"Countdown completed"}}, String{u8"倒计时已完成"}},
                 {MessageKey{u8"gallery.feedback.135", String{u8"Countdown running"}}, String{u8"倒计时进行中"}},
                 {MessageKey{u8"gallery.feedback.136", String{u8"Rate focus"}}, String{u8"Rate focus"}},
                 {MessageKey{u8"gallery.feedback.137", String{u8"Rate blur"}}, String{u8"Rate blur"}},
                 {MessageKey{u8"gallery.feedback.138", String{u8"Rate keydown"}}, String{u8"Rate keydown"}},
                 {MessageKey{
                      u8"gallery.feedback.139",
                      String{u8"Synthetic requests processed; only the current live query may update suggestions"}},
                  String{u8"已处理合成请求；仅当前存活查询可更新建议"}},
                 {MessageKey{u8"gallery.feedback.140", String{u8"Application rejected this edit request"}},
                  String{u8"本次编辑请求被应用拒绝"}},
                 {MessageKey{
                      u8"gallery.feedback.141",
                      String{u8"Form reset; same-value reset also cancels preedit, queries and async suggestions"}},
                  String{u8"Form 已重置；同值 reset 也取消 preedit、查询和异步建议"}},
                 {MessageKey{u8"gallery.feedback.142", String{u8"Contact reset"}}, String{u8"已重置联系人"}},
                 {MessageKey{u8"gallery.feedback.143", String{u8"This example accepts only before/after row moves"}},
                  String{u8"此示例只接受行前/行后移动"}},
                 {MessageKey{u8"gallery.feedback.144", String{u8"onDrop: application wrote back topology"}},
                  String{u8"onDrop：应用已回写拓扑"}},
                 {MessageKey{u8"gallery.feedback.145", String{u8"TreeSelect prefix action executed"}},
                  String{u8"已执行 TreeSelect 前缀"}},
                 {MessageKey{u8"gallery.feedback.146", String{u8"TreeSelect popup footer executed"}},
                  String{u8"已执行 TreeSelect popup footer"}},
                 {MessageKey{u8"gallery.feedback.147", String{u8"TreeSelect: dynamic topology reversed"}},
                  String{u8"TreeSelect：已反转动态拓扑"}},
                 {MessageKey{u8"gallery.feedback.148", String{u8"Select a delivery target"}},
                  String{u8"请选择交付目标"}},
                 {MessageKey{u8"gallery.feedback.149", String{u8"Target reset"}}, String{u8"已重置目标"}},
                 {MessageKey{u8"gallery.feedback.150", String{u8"onRefresh delivered"}}, String{u8"onRefresh 已触发"}},
                 {MessageKey{u8"gallery.feedback.151", String{u8"Error-correction level changed; boost disabled"}},
                  String{u8"纠错级别已改变，boost 关闭"}},
                 {MessageKey{u8"gallery.feedback.152", String{u8"Application accepts refresh requests"}},
                  String{u8"刷新请求会被应用接受"}},
                 {MessageKey{u8"gallery.feedback.153", String{u8"Application rejects refresh requests"}},
                  String{u8"刷新请求会被应用拒绝"}},
                 {MessageKey{u8"gallery.feedback.154", String{u8"Top onChange(true)"}},
                  String{u8"顶部 onChange(true)"}},
                 {MessageKey{u8"gallery.feedback.155", String{u8"Top onChange(false); original position restored"}},
                  String{u8"顶部 onChange(false)，恢复原位"}},
                 {MessageKey{u8"gallery.feedback.156", String{u8"Bottom affixed button received an actual click"}},
                  String{u8"底部固定按钮收到真实点击"}},
                 {MessageKey{u8"gallery.app.notice.caption", String{u8"Notice"}}, String{u8"Notice"}},
                 {MessageKey{u8"gallery.collapse.advanced.caption", String{u8"Advanced"}}, String{u8"高级草稿"}},
                 {MessageKey{u8"gallery.collapse.draft.caption", String{u8"Draft"}}, String{u8"保留的设置草稿"}},
                 {MessageKey{u8"gallery.drawer.bottom.caption", String{u8"Bottom"}}, String{u8"从下打开"}},
                 {MessageKey{u8"gallery.drawer.draft.caption", String{u8"Draft"}}, String{u8"保留的抽屉草稿"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.drawer.end.caption", String{u8"End"}}, String{u8"末尾编辑"}},
                 {MessageKey{u8"gallery.drawer.left.caption", String{u8"Left"}}, String{u8"从左打开"}},
                 {MessageKey{u8"gallery.drawer.right.caption", String{u8"Right"}}, String{u8"从右打开"}},
                 {MessageKey{u8"gallery.drawer.top.caption", String{u8"Top"}}, String{u8"从上打开"}},
                 {MessageKey{u8"gallery.input.borderless.caption", String{u8"input · borderless"}},
                  String{u8"input · borderless"}},
                 {MessageKey{u8"gallery.input.filled.caption", String{u8"input · filled"}}, String{u8"input · filled"}},
                 {MessageKey{u8"gallery.input.outlined.caption", String{u8"input · outlined"}},
                  String{u8"input · outlined"}},
                 {MessageKey{u8"gallery.input.underlined.caption", String{u8"input · underlined"}},
                  String{u8"input · underlined"}},
                 {MessageKey{u8"gallery.modal.basic.caption", String{u8"Basic confirmation"}}, String{u8"基础确认"}},
                 {MessageKey{u8"gallery.modal.motion.caption", String{u8"Animated, movable and resizable dialog"}},
                  String{u8"动画、拖动与缩放"}},
                 {MessageKey{u8"gallery.modal.centered.caption", String{u8"Vertically centered"}},
                  String{u8"垂直居中"}},
                 {MessageKey{u8"gallery.modal.component-theme.caption", String{u8"Scoped dialog theme"}},
                  String{u8"局部对话框主题"}},
                 {MessageKey{u8"gallery.modal.confirm-loading.caption", String{u8"Confirmation loading"}},
                  String{u8"确认加载中"}},
                 {MessageKey{u8"gallery.modal.controlled-refusal.caption", String{u8"Parent keeps the dialog open"}},
                  String{u8"父级保留打开状态"}},
                 {MessageKey{u8"gallery.modal.custom-footer.caption", String{u8"Custom title and footer"}},
                  String{u8"自定义标题与页脚"}},
                 {MessageKey{u8"gallery.modal.long-body.caption", String{u8"Long body and focus reveal"}},
                  String{u8"长正文与焦点显露"}},
                 {MessageKey{u8"gallery.modal.maskless.caption", String{u8"No mask color"}}, String{u8"无遮罩颜色"}},
                 {MessageKey{u8"gallery.modal.nested.caption", String{u8"Nested dialog"}}, String{u8"嵌套对话框"}},
                 {MessageKey{u8"gallery.modal.no-footer.caption", String{u8"No footer"}}, String{u8"无页脚"}},
                 {MessageKey{u8"gallery.modal.retained-edit.caption", String{u8"Retained editing draft"}},
                  String{u8"保留编辑草稿"}},
                 {MessageKey{u8"gallery.notices.workflow.caption", String{u8"Workflow"}},
                  String{u8"Message / Notification · 处理提示"}},
                 {MessageKey{u8"gallery.notification.workflow.caption", String{u8"Workflow"}}, String{u8"通知中心"}},
                 {MessageKey{u8"gallery.otp.borderless.caption", String{u8"otp · borderless"}},
                  String{u8"otp · borderless"}},
                 {MessageKey{u8"gallery.otp.filled.caption", String{u8"otp · filled"}}, String{u8"otp · filled"}},
                 {MessageKey{u8"gallery.otp.outlined.caption", String{u8"otp · outlined"}}, String{u8"otp · outlined"}},
                 {MessageKey{u8"gallery.otp.underlined.caption", String{u8"otp · underlined"}},
                  String{u8"otp · underlined"}},
                 {MessageKey{u8"gallery.pagination.controlled.caption", String{u8"Controlled"}}, String{u8"每页四项"}},
                 {MessageKey{u8"gallery.select.borderless.caption", String{u8"Borderless"}}, String{u8"Borderless"}},
                 {MessageKey{u8"gallery.select.controlled-open.caption", String{u8"Controlled open"}},
                  String{u8"Controlled open"}},
                 {MessageKey{u8"gallery.select.controlled-search.caption", String{u8"Controlled search"}},
                  String{u8"受控 / 清空 / 动态 options"}},
                 {MessageKey{u8"gallery.select.disabled.caption", String{u8"Disabled"}}, String{u8"Disabled"}},
                 {MessageKey{u8"gallery.select.empty.caption", String{u8"Empty"}}, String{u8"Empty"}},
                 {MessageKey{u8"gallery.select.error.caption", String{u8"Error"}}, String{u8"Error"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.select.filled.caption", String{u8"Filled"}}, String{u8"Filled"}},
                 {MessageKey{u8"gallery.select.large.caption", String{u8"Large"}}, String{u8"Large"}},
                 {MessageKey{u8"gallery.select.loading.caption", String{u8"Loading"}}, String{u8"保留搜索，阻止提交"}},
                 {MessageKey{u8"gallery.select.middle.caption", String{u8"Middle"}}, String{u8"Middle"}},
                 {MessageKey{u8"gallery.select.numeric.caption", String{u8"Numeric"}}, String{u8"int64 value"}},
                 {MessageKey{u8"gallery.select.outlined.caption", String{u8"Outlined"}}, String{u8"Outlined"}},
                 {MessageKey{u8"gallery.select.small.caption", String{u8"Small"}}, String{u8"Small"}},
                 {MessageKey{u8"gallery.select.underlined.caption", String{u8"Underlined"}}, String{u8"Underlined"}},
                 {MessageKey{u8"gallery.select.warning.caption", String{u8"Warning"}}, String{u8"Warning"}},
                 {MessageKey{u8"gallery.space.compact.large.caption", String{u8"Large"}}, String{u8"Large"}},
                 {MessageKey{u8"gallery.space.compact.middle.caption", String{u8"Middle"}}, String{u8"Middle"}},
                 {MessageKey{u8"gallery.space.compact.small.caption", String{u8"Small"}}, String{u8"Small"}},
                 {MessageKey{u8"gallery.tabs.bottom.caption", String{u8"Bottom"}}, String{u8"Bottom"}},
                 {MessageKey{u8"gallery.tabs.bottom-card.caption", String{u8"Bottom card"}}, String{u8"Bottom card"}},
                 {MessageKey{u8"gallery.tabs.card.caption", String{u8"Card"}}, String{u8"Card"}},
                 {MessageKey{u8"gallery.tabs.centered.caption", String{u8"Centered"}}, String{u8"Centered"}},
                 {MessageKey{u8"gallery.tabs.controlled-retained.caption", String{u8"Controlled retained"}},
                  String{u8"受控 / 切换保留编辑"}},
                 {MessageKey{u8"gallery.tabs.disabled.caption", String{u8"Disabled"}}, String{u8"Disabled"}},
                 {MessageKey{u8"gallery.tabs.dynamic-label.caption", String{u8"Dynamic label"}},
                  String{u8"响应标签 / 禁用"}},
                 {MessageKey{u8"gallery.tabs.empty.caption", String{u8"Empty"}}, String{u8"Empty"}},
                 {MessageKey{u8"gallery.tabs.large.caption", String{u8"Large"}}, String{u8"Large"}},
                 {MessageKey{u8"gallery.tabs.overflow.caption", String{u8"Overflow"}}, String{u8"箭头 / 滚轮"}},
                 {MessageKey{u8"gallery.tabs.small.caption", String{u8"Small"}}, String{u8"Small"}},
                 {MessageKey{u8"gallery.tabs.unknown-key.caption", String{u8"Unknown key"}},
                  String{u8"未知 key 不显示面板"}},
                 {MessageKey{u8"gallery.text-area.borderless.caption", String{u8"text-area · borderless"}},
                  String{u8"text-area · borderless"}},
                 {MessageKey{u8"gallery.text-area.filled.caption", String{u8"text-area · filled"}},
                  String{u8"text-area · filled"}},
                 {MessageKey{u8"gallery.text-area.outlined.caption", String{u8"text-area · outlined"}},
                  String{u8"text-area · outlined"}},
                 {MessageKey{u8"gallery.text-area.underlined.caption", String{u8"text-area · underlined"}},
                  String{u8"text-area · underlined"}},
                 {MessageKey{u8"gallery.event.000", String{u8"Load failed: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"加载失败：{v0}"}},
                 {MessageKey{
                      u8"gallery.event.001", String{u8"Count updated to {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"计数更新为 {v0}"}},
                 {MessageKey{
                      u8"gallery.event.002", String{u8"Group selection count: {v0}"}, {{u8"v0", MessageArgType::UInt}}},
                  String{u8"组选择数量：{v0}"}},
                 {MessageKey{u8"gallery.event.003",
                             String{u8"Pagination / {v0} / {v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"分页 / {v0} / {v1}"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{
                      u8"gallery.event.004.yes", String{u8"Moved right: {v1}"}, {{u8"v1", MessageArgType::UInt}}},
                  String{u8"向右移动：{v1}"}},
                 {MessageKey{u8"gallery.event.004.no", String{u8"Moved left: {v1}"}, {{u8"v1", MessageArgType::UInt}}},
                  String{u8"向左移动：{v1}"}},
                 {MessageKey{
                      u8"gallery.event.005.yes", String{u8"Target search: {v1}"}, {{u8"v1", MessageArgType::Text}}},
                  String{u8"目标搜索：{v1}"}},
                 {MessageKey{
                      u8"gallery.event.005.no", String{u8"Source search: {v1}"}, {{u8"v1", MessageArgType::Text}}},
                  String{u8"来源搜索：{v1}"}},
                 {MessageKey{u8"gallery.event.006", String{u8"List scroll: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"List scroll: {v0}"}},
                 {MessageKey{
                      u8"gallery.event.007", String{u8"Rich rows moved: {v0}"}, {{u8"v0", MessageArgType::UInt}}},
                  String{u8"Rich row moved: {v0}"}},
                 {MessageKey{u8"gallery.event.008",
                             String{u8"Custom list rows moved: {v0}"},
                             {{u8"v0", MessageArgType::UInt}}},
                  String{u8"Custom list moved: {v0}"}},
                 {MessageKey{u8"gallery.event.009", String{u8"Cascader paths: {v0}"}, {{u8"v0", MessageArgType::UInt}}},
                  String{u8"Cascader paths: {v0}"}},
                 {MessageKey{u8"gallery.event.010", String{u8"Checked leaves: {v0}"}, {{u8"v0", MessageArgType::UInt}}},
                  String{u8"Checked leaves: {v0}"}},
                 {MessageKey{u8"gallery.event.011", String{u8"Tags: {v0}"}, {{u8"v0", MessageArgType::UInt}}},
                  String{u8"Tags: {v0}"}},
                 {MessageKey{u8"gallery.event.012",
                             String{u8"Theme search selections: {v0}"},
                             {{u8"v0", MessageArgType::UInt}}},
                  String{u8"Theme search selections: {v0}"}},
                 {MessageKey{u8"gallery.event.013", String{u8"Selected menu: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已选择菜单：{v0}"}},
                 {MessageKey{
                      u8"gallery.event.014", String{u8"Navigation command: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"导航命令：{v0}"}},
                 {MessageKey{
                      u8"gallery.event.015", String{u8"Command executed: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已执行命令：{v0}"}},
                 {MessageKey{u8"gallery.event.016", String{u8"Split menu: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"分离菜单：{v0}"}},
                 {MessageKey{u8"gallery.event.017", String{u8"Complete: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"Complete: {v0}"}},
                 {MessageKey{u8"gallery.event.018",
                             String{u8"Validation failed · {v0} errors"},
                             {{u8"v0", MessageArgType::UInt}}},
                  String{u8"校验失败 · {v0} 项错误"}},
                 {MessageKey{u8"gallery.event.019", String{u8"Date saved · {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"日期保存成功 · {v0}"}},
                 {MessageKey{u8"gallery.event.020",
                             String{u8"OTP partial: {v0}/{v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::UInt}}},
                  String{u8"OTP partial: {v0}/{v1}"}},
                 {MessageKey{u8"gallery.event.021", String{u8"Tag count: {v0}"}, {{u8"v0", MessageArgType::UInt}}},
                  String{u8"标签数量={v0}"}},
                 {MessageKey{
                      u8"gallery.event.022", String{u8"Current anchor / {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"当前锚点 / {v0}"}},
                 {MessageKey{u8"gallery.event.023",
                             String{u8"Open {v0} / {v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"打开 {v0} / {v1}"}},
                 {MessageKey{u8"gallery.event.024", String{u8"Apply {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"应用 {v0}"}},
                 {MessageKey{u8"gallery.event.025", String{u8"Menu / {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"菜单 / {v0}"}},
                 {MessageKey{u8"gallery.event.026",
                             String{u8"{v0} / {v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"{v0} / {v1}"}},
                 {MessageKey{u8"gallery.event.027", String{u8"Switch / {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"切换 / {v0}"}},
                 {MessageKey{u8"gallery.event.028", String{u8"Save {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"保存 {v0}"}},
                 {MessageKey{u8"gallery.event.029", String{u8"Dynamic step / {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"动态步骤 / {v0}"}},
                 {MessageKey{
                      u8"gallery.event.030.yes", String{u8"Time accepted: {v1}"}, {{u8"v1", MessageArgType::Text}}},
                  String{u8"已接受时间：{v1}"}},
                 {MessageKey{
                      u8"gallery.event.030.no", String{u8"Time rejected: {v1}"}, {{u8"v1", MessageArgType::Text}}},
                  String{u8"已拒绝时间：{v1}"}},
                 {MessageKey{u8"gallery.event.030.clear.yes", String{u8"Clear accepted"}, {}}, String{u8"已接受清空"}},
                 {MessageKey{u8"gallery.event.030.clear.no", String{u8"Clear rejected"}, {}}, String{u8"已拒绝清空"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.event.031", String{u8"Time saved · {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"时间保存成功 · {v0}"}},
                 {MessageKey{u8"gallery.event.032.reviewer.enabled",
                             String{u8"Saved {v0} · reviewer · enabled"},
                             {{u8"v0", MessageArgType::Text}}},
                  String{u8"已保存 {v0} · 审核者 · 已启用"}},
                 {MessageKey{u8"gallery.event.032.reviewer.disabled",
                             String{u8"Saved {v0} · reviewer · disabled"},
                             {{u8"v0", MessageArgType::Text}}},
                  String{u8"已保存 {v0} · 审核者 · 已停用"}},
                 {MessageKey{u8"gallery.event.032.editor.enabled",
                             String{u8"Saved {v0} · editor · enabled"},
                             {{u8"v0", MessageArgType::Text}}},
                  String{u8"已保存 {v0} · 编辑者 · 已启用"}},
                 {MessageKey{u8"gallery.event.032.editor.disabled",
                             String{u8"Saved {v0} · editor · disabled"},
                             {{u8"v0", MessageArgType::Text}}},
                  String{u8"已保存 {v0} · 编辑者 · 已停用"}},
                 {MessageKey{u8"gallery.event.033.yes",
                             String{u8"Published {v0} · private"},
                             {{u8"v0", MessageArgType::Text}}},
                  String{u8"已发布 {v0} · 仅自己"}},
                 {MessageKey{
                      u8"gallery.event.033.no", String{u8"Published {v0} · team"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已发布 {v0} · 团队可见"}},
                 {MessageKey{u8"gallery.event.034", String{u8"Saved {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已保存 {v0}"}},
                 {MessageKey{u8"gallery.event.035", String{u8"Confirmed {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已确认 {v0}"}},
                 {MessageKey{u8"gallery.event.036", String{u8"Confirmed: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已确认：{v0}"}},
                 {MessageKey{u8"gallery.event.037", String{u8"Recorded {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已记录 {v0}"}},
                 {MessageKey{u8"gallery.event.038", String{u8"Member saved: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已保存成员 {v0}"}},
                 {MessageKey{u8"gallery.event.039", String{u8"Saved {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已保存 {v0}"}},
                 {MessageKey{u8"gallery.event.040.yes",
                             String{u8"Candidate accepted: {v1}"},
                             {{u8"v1", MessageArgType::Text}}},
                  String{u8"已接受候选：{v1}"}},
                 {MessageKey{
                      u8"gallery.event.040.no", String{u8"Candidate rejected: {v1}"}, {{u8"v1", MessageArgType::Text}}},
                  String{u8"已拒绝候选：{v1}"}},
                 {MessageKey{u8"gallery.event.041", String{u8"Saved {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已保存 {v0}"}},
                 {MessageKey{u8"gallery.event.042",
                             String{u8"Waiting for synthetic suggestions: {v0}{v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"等待合成建议：{v0}{v1}"}},
                 {MessageKey{u8"gallery.event.043",
                             String{u8"Selected: {v0}{v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"已选择：{v0}{v1}"}},
                 {MessageKey{u8"gallery.event.044",
                             String{u8"{v0} draft={v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"{v0} draft={v1}"}},
                 {MessageKey{u8"gallery.event.045",
                             String{u8"before {v0} → {v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"before {v0} → {v1}"}},
                 {MessageKey{u8"gallery.event.046", String{u8"after {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"after {v0}"}},
                 {MessageKey{u8"gallery.event.047.yes",
                             String{u8"Accepted index={v1} source={v2}"},
                             {{u8"v1", MessageArgType::Text}, {u8"v2", MessageArgType::Text}}},
                  String{u8"接受 index={v1} source={v2}"}},
                 {MessageKey{u8"gallery.event.047.no",
                             String{u8"Rejected index={v1} source={v2}"},
                             {{u8"v1", MessageArgType::Text}, {u8"v2", MessageArgType::Text}}},
                  String{u8"拒绝 index={v1} source={v2}"}},
                 {MessageKey{
                      u8"gallery.event.048.yes", String{u8"Panel: {v0} · month"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"面板：{v0} · 月"}},
                 {MessageKey{
                      u8"gallery.event.048.no", String{u8"Panel: {v0} · year"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"面板：{v0} · 年"}},
                 {MessageKey{u8"gallery.event.049",
                             String{u8"Selection: {v0} source={v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"选择：{v0} source={v1}"}},
                 {MessageKey{u8"gallery.event.050",
                             String{u8"Schedule {v0}: {v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"日程 {v0}：{v1}"}},
                 {MessageKey{u8"gallery.event.051",
                             String{u8"Search: prefix={v0} query={v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"搜索：prefix={v0} query={v1}"}},
                 {MessageKey{u8"gallery.event.052",
                             String{u8"Selected: {v0}{v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"选择：{v0}{v1}"}},
                 {MessageKey{u8"gallery.event.053",
                             String{u8"Long-list selection: {v0}{v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"长列表选择：{v0}{v1}"}},
                 {MessageKey{u8"gallery.event.054.yes",
                             String{u8"Candidate accepted: {v1}"},
                             {{u8"v1", MessageArgType::Text}}},
                  String{u8"接受候选：{v1}"}},
                 {MessageKey{
                      u8"gallery.event.054.no", String{u8"Candidate rejected: {v1}"}, {{u8"v1", MessageArgType::Text}}},
                  String{u8"拒绝候选：{v1}"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{
                      u8"gallery.event.055", String{u8"Candidate selected: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"选择候选={v0}"}},
                 {MessageKey{u8"gallery.event.056", String{u8"Saved {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已保存 {v0}"}},
                 {MessageKey{u8"gallery.event.057", String{u8"selected={v0}"}, {{u8"v0", MessageArgType::UInt}}},
                  String{u8"selected={v0}"}},
                 {MessageKey{u8"gallery.event.058",
                             String{u8"checked={v0}, half={v1}"},
                             {{u8"v0", MessageArgType::UInt}, {u8"v1", MessageArgType::UInt}}},
                  String{u8"checked={v0}, half={v1}"}},
                 {MessageKey{u8"gallery.event.059", String{u8"onValuesChange={v0}"}, {{u8"v0", MessageArgType::UInt}}},
                  String{u8"onValuesChange={v0}"}},
                 {MessageKey{
                      u8"gallery.event.060", String{u8"strict labeled values={v0}"}, {{u8"v0", MessageArgType::UInt}}},
                  String{u8"strict labeled values={v0}"}},
                 {MessageKey{
                      u8"gallery.event.061", String{u8"virtual onChange={v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"virtual onChange={v0}"}},
                 {MessageKey{u8"gallery.event.062", String{u8"async onChange={v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"async onChange={v0}"}},
                 {MessageKey{u8"gallery.event.063", String{u8"loaded={v0}"}, {{u8"v0", MessageArgType::UInt}}},
                  String{u8"loaded={v0}"}},
                 {MessageKey{u8"gallery.event.064.yes",
                             String{u8"Selection accepted: {v1}"},
                             {{u8"v1", MessageArgType::Text}}},
                  String{u8"接受选择={v1}"}},
                 {MessageKey{
                      u8"gallery.event.064.no", String{u8"Selection rejected: {v1}"}, {{u8"v1", MessageArgType::Text}}},
                  String{u8"拒绝选择={v1}"}},
                 {MessageKey{
                      u8"gallery.event.065", String{u8"Target submitted: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"已提交目标={v0}"}},
                 {MessageKey{u8"gallery.state.disabled", String{u8" · Disabled"}}, String{u8" · 已禁用"}},
                 {MessageKey{u8"gallery.state.enabled", String{u8" · Enabled"}}, String{u8" · 已启用"}},
                 {MessageKey{u8"gallery.state.readonly", String{u8" · Read-only"}}, String{u8" · 只读"}},
                 {MessageKey{u8"gallery.state.editable", String{u8" · Editable"}}, String{u8" · 可编辑"}},
                 {MessageKey{u8"gallery.state.no-time", String{u8"No time selected"}}, String{u8"未选择时间"}},
                 {MessageKey{u8"gallery.state.confirm", String{u8" · Confirm mode"}}, String{u8" · 确认模式"}},
                 {MessageKey{u8"gallery.state.immediate", String{u8" · Immediate mode"}}, String{u8" · 即时模式"}},
                 {MessageKey{u8"gallery.state.accept-selection", String{u8" · Accept selection"}},
                  String{u8" · 接受选择"}},
                 {MessageKey{u8"gallery.state.locked", String{u8" · Saved value locked"}}, String{u8" · 锁定已保存值"}},
                 {MessageKey{u8"gallery.state.input-readonly", String{u8" · Input read-only"}},
                  String{u8" · 文本只读"}},
                 {MessageKey{u8"gallery.state.input-editable", String{u8" · Input editable"}}, String{u8" · 可输入"}},
                 {MessageKey{u8"gallery.state.accept-change", String{u8" · Accept changes"}}, String{u8" · 接受修改"}},
                 {MessageKey{u8"gallery.state.reject-change", String{u8" · Reject changes"}}, String{u8" · 拒绝修改"}},
                 {MessageKey{u8"gallery.state.disabled-short", String{u8" · Disabled"}}, String{u8" · 禁用"}},
                 {MessageKey{u8"gallery.snapshot.000",
                             String{u8"Current page {v0}; {v1} items selected"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::UInt}}},
                  String{u8"当前第 {v0} 页，已选 {v1} 项"}},
                 {MessageKey{
                      u8"gallery.snapshot.001", String{u8"Current view: {v0}"}, {{u8"v0", MessageArgType::Text}}},
                  String{u8"当前视图：{v0}"}},
                 {MessageKey{
                      u8"gallery.snapshot.002",
                      String{u8"{v0}{v1}{v2}"},
                      {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}, {u8"v2", MessageArgType::Text}}},
                  String{u8"{v0}{v1}{v2}"}},
                 {MessageKey{
                      u8"gallery.snapshot.003",
                      String{u8"{v0}{v1}{v2}"},
                      {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}, {u8"v2", MessageArgType::Text}}},
                  String{u8"{v0}{v1}{v2}"}},
                 {MessageKey{u8"gallery.snapshot.004",
                             String{u8"current={v0} · {v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"current={v0} · {v1}"}},
                 {MessageKey{u8"gallery.snapshot.005",
                             String{u8"{v0}{v1}{v2}{v3} · {v4}"},
                             {{u8"v0", MessageArgType::Text},
                              {u8"v1", MessageArgType::Text},
                              {u8"v2", MessageArgType::Text},
                              {u8"v3", MessageArgType::Text},
                              {u8"v4", MessageArgType::Text}}},
                  String{u8"{v0}{v1}{v2}{v3} · {v4}"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{
                      u8"gallery.snapshot.006",
                      String{u8"{v0}{v1}{v2}"},
                      {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}, {u8"v2", MessageArgType::Text}}},
                  String{u8"{v0}{v1}{v2}"}},
                 {MessageKey{u8"gallery.snapshot.007",
                             String{u8"Source value={v0} · {v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"原值={v0} · {v1}"}},
                 {MessageKey{u8"gallery.snapshot.008",
                             String{u8"Accepted={v0}{v1}{v2}{v3} · {v4} · {v5}"},
                             {{u8"v0", MessageArgType::Text},
                              {u8"v1", MessageArgType::Text},
                              {u8"v2", MessageArgType::Text},
                              {u8"v3", MessageArgType::Text},
                              {u8"v4", MessageArgType::Text},
                              {u8"v5", MessageArgType::Text}}},
                  String{u8"已接受={v0}{v1}{v2}{v3} · {v4} · {v5}"}},
                 {MessageKey{u8"gallery.snapshot.009",
                             String{u8"Accepted value: {v0}\n{v1}"},
                             {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}}},
                  String{u8"接受值：{v0}\n{v1}"}},
                 {MessageKey{
                      u8"gallery.snapshot.010",
                      String{u8"Accepted value={v0} · {v1} · {v2}"},
                      {{u8"v0", MessageArgType::Text}, {u8"v1", MessageArgType::Text}, {u8"v2", MessageArgType::Text}}},
                  String{u8"接受值={v0} · {v1} · {v2}"}},
                 {MessageKey{u8"gallery.snapshot.011",
                             String{u8"Selected={v0} · checked={v1} · half={v2} · {v3}"},
                             {{u8"v0", MessageArgType::Text},
                              {u8"v1", MessageArgType::Text},
                              {u8"v2", MessageArgType::Text},
                              {u8"v3", MessageArgType::Text}}},
                  String{u8"选择={v0} · 勾选={v1} · 半选={v2} · {v3}"}},
                 {MessageKey{u8"gallery.snapshot.012",
                             String{u8"Accepted value={v0} · close requests={v1} · {v2} · {v3}"},
                             {{u8"v0", MessageArgType::Text},
                              {u8"v1", MessageArgType::Text},
                              {u8"v2", MessageArgType::Text},
                              {u8"v3", MessageArgType::Text}}},
                  String{u8"接受值={v0} · 关闭请求={v1} · {v2} · {v3}"}},
                 {MessageKey{u8"gallery.empty.native", String{u8"No example is registered on this page yet. Follow "
                                                              u8"related links for existing native workflows."}},
                  String{u8"本页尚无已登记样例，可通过关联入口查看已有原生流程。"}},
                 {MessageKey{u8"gallery.empty.planned",
                             String{u8"No stable native component is available yet. This page "
                                    u8"retains reference information and missing scope."}},
                  String{u8"尚未提供稳定原生组件。此页保留参考信息与缺失范围。"}},
                 {MessageKey{u8"gallery.empty.web",
                             String{u8"This capability belongs to the Web scope; no native demo is provided."}},
                  String{u8"此能力属于 Web 范围，未提供原生演示。"}},
                 {MessageKey{u8"gallery.empty.deprecated",
                             String{u8"This entry is deprecated; only reference notes remain."}},
                  String{u8"此条目已弃用，仅保留参考说明。"}},
                 {MessageKey{u8"gallery.empty.outside",
                             String{u8"This capability is outside the current native implementation scope."}},
                  String{u8"此能力不在当前原生实现范围。"}},
                 {MessageKey{u8"gallery.drawer.right", String{u8"Right"}}, String{u8"右侧"}},
                 {MessageKey{u8"gallery.drawer.left", String{u8"Left"}}, String{u8"左侧"}},
                 {MessageKey{u8"gallery.drawer.top", String{u8"Top"}}, String{u8"顶部"}},
                 {MessageKey{u8"gallery.drawer.bottom", String{u8"Bottom"}}, String{u8"底部"}},
                 {MessageKey{u8"gallery.config-provider.size", String{u8"Change default size"}},
                  String{u8"切换默认尺寸"}},
                 {MessageKey{u8"gallery.config-provider.disabled", String{u8"Toggle default disabled"}},
                  String{u8"切换默认禁用"}},
                 {MessageKey{u8"gallery.config-provider.direction", String{u8"Switch layout direction"}},
                  String{u8"切换布局方向"}},
                 {MessageKey{u8"gallery.config-provider.inherited", String{u8"Inherited defaults"}},
                  String{u8"继承默认配置"}},
                 {MessageKey{u8"gallery.config-provider.explicit", String{u8"Explicit middle size and enabled"}},
                  String{u8"显式中等尺寸并启用"}},
                 {MessageKey{u8"gallery.config-provider.nested", String{u8"Nested enabled scope"}},
                  String{u8"嵌套启用作用域"}},
                 {MessageKey{u8"gallery.config-provider.placeholder", String{u8"Edit this retained draft"}},
                  String{u8"编辑这份保留草稿"}},
                 {MessageKey{u8"gallery.config-provider.activations", String{u8"Activations"}}, String{u8"激活次数"}},
                 {MessageKey{u8"gallery.sample.config-provider.context.title",
                             String{u8"Scoped defaults and retained editing"}},
                  String{u8"作用域默认值与保留编辑"}}});
        },
        [](std::vector<GalleryMessageDefinition>& target) {
            target.insert(
                target.end(),
                {{MessageKey{u8"gallery.sample.card.rounded-mask.title",
                             String{u8"Rounded content and retained editing"}},
                  String{u8"圆角内容与保留编辑"}},
                 {MessageKey{u8"gallery.sample.modal.backdrop.title", String{u8"Background blur and mask options"}},
                  String{u8"背景模糊与遮罩选择"}},
                 {MessageKey{u8"gallery.composition.card.toggle", String{u8"Toggle rounded content clipping"}},
                  String{u8"切换圆角内容裁剪"}},
                 {MessageKey{u8"gallery.composition.card.enabled", String{u8"Rounded clipping enabled"}},
                  String{u8"圆角裁剪已启用"}},
                 {MessageKey{u8"gallery.composition.card.overflow", String{u8"Allow content overflow"}},
                  String{u8"允许内容溢出"}},
                 {MessageKey{u8"gallery.composition.card.draft", String{u8"Keep this note while toggling clipping"}},
                  String{u8"保留此备注后切换裁剪"}},
                 {MessageKey{u8"gallery.composition.card.action", String{u8"Action inside the card"}},
                  String{u8"卡片内的操作"}},
                 {MessageKey{u8"gallery.composition.card.description",
                             String{u8"Cover, content and actions share the card corners"}},
                  String{u8"封面、正文与操作共享卡片圆角"}},
                 {MessageKey{u8"gallery.composition.modal.description",
                             String{u8"Edit the background note, then open a dialog to inspect it"}},
                  String{u8"更改背景备注，再打开对话框查看当前背景"}},
                 {MessageKey{u8"gallery.composition.modal.background", String{u8"Customer details and notes"}},
                  String{u8"客户资料与备注"}},
                 {MessageKey{u8"gallery.composition.modal.blur", String{u8"Open a dialog with background blur"}},
                  String{u8"打开背景模糊对话框"}},
                 {MessageKey{u8"gallery.composition.modal.tint", String{u8"Use a solid mask"}},
                  String{u8"使用纯色遮罩"}},
                 {MessageKey{u8"gallery.composition.modal.none", String{u8"No mask"}}, String{u8"无遮罩"}},
                 {MessageKey{u8"gallery.composition.modal.service", String{u8"Open a service dialog"}},
                  String{u8"打开服务对话框"}},
                 {MessageKey{u8"gallery.composition.modal.confirm", String{u8"Confirm action"}}, String{u8"操作确认"}},
                 {MessageKey{u8"gallery.composition.modal.update", String{u8"Confirm this customer details update"}},
                  String{u8"请确认本次客户资料更新"}},
                 {MessageKey{u8"gallery.composition.modal.title", String{u8"Customer details"}}, String{u8"客户资料"}},
                 {MessageKey{u8"gallery.composition.modal.note", String{u8"Notes inside the dialog"}},
                  String{u8"对话框中的备注"}}});
        }};
    static const auto declarations = [] {
        std::vector<GalleryMessageDefinition> result;
        result.reserve(2187);
        for (const auto append : append_batches) {
            append(result);
        }
        return result;
    }();
    return declarations;
}

const ryn::MessageKey& gallery_message(std::string_view identity) {
    static const auto by_identity = [] {
        std::unordered_map<std::string_view, const ryn::MessageKey*> result;
        result.reserve(gallery_messages().size());
        for (const auto& declaration : gallery_messages()) {
            result.emplace(declaration.key.id().bytes(), &declaration.key);
        }
        return result;
    }();
    const auto found = by_identity.find(identity);
    if (found == by_identity.end()) {
        throw std::invalid_argument("Gallery message is not declared: " + std::string{identity});
    }
    return *found->second;
}

const ryn::MessageCatalog& gallery_message_catalog() {
    static const ryn::MessageCatalog catalog([] {
        std::vector<ryn::MessageTranslation> translations;
        translations.reserve(gallery_messages().size() * 2);
        for (const auto& declaration : gallery_messages()) {
            translations.push_back({ryn::locales::en_US, declaration.key, declaration.key.fallback()});
            translations.push_back({ryn::locales::zh_CN, declaration.key, declaration.chinese});
        }
        return translations;
    }());
    return catalog;
}

ryn::Binding<ryn::String> gallery_text(std::string_view identity, ryn::Prop<ryn::MessageArgs> args) {
    return ryn::useI18n().text(gallery_message(identity), std::move(args));
}

void gallery_label(std::string_view identity) {
    ryn::Text(ryn::TextProps{}.content(gallery_text(identity)));
}

ryn::Binding<ryn::String> gallery_field(std::string_view identity, std::string_view field) {
    const auto prefix = identity.starts_with("gallery.") ? std::string{} : std::string{"gallery."};
    return gallery_text(prefix + std::string{identity} + "." + std::string{field});
}

void GalleryFeedback::context(ryn::I18n context) {
    context_ = std::move(context);
}

GalleryMessage::GalleryMessage(std::string_view identity, ryn::MessageArgs arguments)
    : key(gallery_message(identity)), args(std::move(arguments)) {}

bool GalleryFeedback::is(std::string_view identity) const {
    const auto value = value_.get();
    const auto* message = std::get_if<GalleryMessage>(&value);
    return message && message->key.id().bytes() == identity;
}

std::variant<ryn::String, GalleryMessage> GalleryFeedback::source() const {
    return value_.get();
}

bool GalleryFeedback::restore(std::variant<ryn::String, GalleryMessage> source) const {
    return value_.set(std::move(source));
}

bool GalleryFeedback::set(GalleryMessage message) const {
    return value_.set(std::move(message));
}

bool GalleryFeedback::set(ryn::String literal) const {
    return value_.set(std::move(literal));
}

ryn::String GalleryFeedback::get() const {
    const auto value = value_.get();
    if (const auto* literal = std::get_if<ryn::String>(&value)) {
        return *literal;
    }
    const auto& message = std::get<GalleryMessage>(value);
    return context_.translate(message.key, message.args);
}

GalleryFeedback::operator ryn::Prop<ryn::String>() const {
    return ryn::bind([value = *this] { return value.get(); });
}

} // namespace rynui::example
