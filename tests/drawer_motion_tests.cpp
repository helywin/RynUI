#include "component/modal_component.hpp"
#include "component/drawer_model.hpp"
#include "component/app_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>
#include <chrono>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;
constexpr std::int64_t T = 300000;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < .02F;
}

void sync(Fixture& f, float width = 720, float height = 600) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Drawer motion sync failed");
}

void at(Fixture& f, std::int64_t time, float width = 720, float height = 600) {
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(time)));
    sync(f, width, height);
}

float eased(float t) {
    return animation::ant_easing(animation::AntEasingPreset::ease_in_out).sample(t);
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& node = f.nodes.require(f.services.components().root(id));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

void pointer(Fixture& f, PointerAction action, float x, float y) {
    f.services.pointer().dispatch({PointerIdentity::mouse(), action,
                                   action == PointerAction::move ? PointerButton::none : PointerButton::primary, x, y});
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    pointer(f, PointerAction::down, r.x + r.width / 2, r.y + r.height / 2);
    pointer(f, PointerAction::up, r.x + r.width / 2, r.y + r.height / 2);
}

void position(const detail::ModalSnapshot& s, DrawerPlacement edge, float p, float width = 320, float height = 260,
              float viewport_width = 720, float viewport_height = 600) {
    auto expected = detail::drawer_bounds({edge, width, height}, {0, 0, viewport_width, viewport_height});
    switch (edge) {
    case DrawerPlacement::Left:
        expected.x -= (1 - p) * expected.width;
        break;
    case DrawerPlacement::Right:
        expected.x += (1 - p) * expected.width;
        break;
    case DrawerPlacement::Top:
        expected.y -= (1 - p) * expected.height;
        break;
    case DrawerPlacement::Bottom:
        expected.y += (1 - p) * expected.height;
        break;
    }
    check(near(s.bounds.x, expected.x) && near(s.bounds.y, expected.y) && near(s.bounds.width, expected.width) &&
              near(s.bounds.height, expected.height) && near(s.progress, p),
          "Drawer edge projection disagrees with sampled progress");
}

void mask(Fixture& f, const detail::ModalSnapshot& s, float p) {
    const auto color = f.services.components().theme_scope(s.overlay)->snapshot().drawer().colors.mask;
    bool found{};
    for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
        if (command.kind != graphics::SceneDrawKind::quad) {
            continue;
        }
        for (auto i = command.first_instance; i < command.first_instance + command.instance_count; ++i) {
            const auto& q = f.services.surfaces().instances().at(i);
            if (q.bounds == std::array{0.0F, 0.0F, 720.0F, 600.0F}) {
                check(near(q.color[3], color.alpha() * p) && near(q.opacity, 1),
                      "Drawer mask multiplied progress more than once");
                found = true;
            }
            if (q.bounds == std::array{s.bounds.x, s.bounds.y, s.bounds.width, s.bounds.height}) {
                check(near(q.opacity, 1), "Drawer faded its whole panel subtree");
            }
        }
    }
    check(found == s.paint_visible, "Drawer shield paint lifetime disagrees with presentation");
}

void idle(Fixture& f, std::int64_t time, int& callbacks, int expected) {
    at(f, time);
    sync(f);
    at(f, time + 10 * T);
    sync(f);
    check(callbacks == expected && !f.services.modal().next_auxiliary_deadline(), "Drawer completion was not finite");
}

void edges() {
    for (auto edge : {DrawerPlacement::Left, DrawerPlacement::Right, DrawerPlacement::Top, DrawerPlacement::Bottom}) {
        Fixture f;
        f.services.set_motion_preference(animation::MotionPreference::normal);
        Signal<bool> open{false};
        InputRef editor;
        int mounts{};
        int title_mounts{};
        int footer_mounts{};
        int after{};
        f.services.mount(Content{[&] {
            Drawer(DrawerProps{}
                       .open(open)
                       .placement(edge)
                       .width(320)
                       .height(260)
                       .resizable(DrawerResize{true})
                       .afterOpenChange([&](bool) { ++after; }),
                   DrawerContent{[&] {
                       ++mounts;
                       Input(InputProps{}.ref(editor).defaultValue(u8"retained draft"));
                       for (int i = 0; i < 30; ++i) {
                           Text(u8"Body clipping and scrolling");
                       }
                   }},
                   {.title = DrawerTitle{[&] {
                        ++title_mounts;
                        Text(u8"Motion title");
                    }},
                    .footer = DrawerFooter{[&] {
                        ++footer_mounts;
                        Text(u8"Motion footer");
                    }}});
        }});
        sync(f);
        const auto id = f.services.modal().mounted().front();
        const auto input = f.inputs.mounted_inputs().front();
        const auto component_count = f.services.components().component_count();
        check(after == 0, "initial closed Drawer sent after(false)");
        open.set(true);
        sync(f);
        auto s = f.services.modal().snapshot(id);
        check(s.visible && s.paint_visible && s.transitioning && after == 0, "normal enter was immediate");
        position(s, edge, 0);
        mask(f, s, 0);
        const auto title = bounds(f, s.title);
        const auto footer = bounds(f, s.footer);
        const auto handle = bounds(f, s.resize_handle);
        const auto body = s.body_bounds;
        const auto input_bounds = bounds(f, input.component);
        at(f, T / 2);
        s = f.services.modal().snapshot(id);
        position(s, edge, eased(.5F));
        mask(f, s, s.progress);
        const auto dx = s.body_bounds.x - body.x;
        const auto dy = s.body_bounds.y - body.y;
        const auto moved_input = bounds(f, input.component);
        const auto clip = f.inputs.layout_snapshot(input.component).clip;
        check(near(moved_input.x - input_bounds.x, dx) && near(moved_input.y - input_bounds.y, dy),
              "Drawer editor did not move with retained body geometry");
        if (clip.width > 0 && clip.height > 0) {
            check(clip.x >= std::max(0.0F, s.body_bounds.x) - .02F &&
                      clip.y >= std::max(0.0F, s.body_bounds.y) - .02F &&
                      clip.x + clip.width <= std::min(720.0F, s.body_bounds.x + s.body_bounds.width) + .02F &&
                      clip.y + clip.height <= std::min(600.0F, s.body_bounds.y + s.body_bounds.height) + .02F,
                  "Drawer editor clip did not follow translated viewport");
        }
        for (const auto& pair :
             {std::pair{s.title, title}, std::pair{s.footer, footer}, std::pair{s.resize_handle, handle}}) {
            const auto moved = bounds(f, pair.first);
            check(near(moved.x - pair.second.x, dx) && near(moved.y - pair.second.y, dy),
                  "Drawer rich region did not move with its body");
        }
        at(f, T);
        check(after == 0, "Drawer notified inside terminal geometry sync");
        sync(f);
        check(after == 1 && editor.focus({InputFocusCursor::All}), "Drawer enter completion/focus missing");
        const auto selection = f.inputs.editors().require(input.editor).selection();
        const auto draft = f.inputs.editors().require(input.editor).value();
        const auto session = f.inputs.sessions().active();
        open.set(false);
        check(!editor.focus() && !bool(f.inputs.dispatch(TextCommitted{String{u8"stale"}, session})),
              "close did not synchronously revoke the old text session");
        sync(f);
        s = f.services.modal().snapshot(id);
        check(!s.visible && s.paint_visible && editor.bound(), "exit removed its retained body too early");
        at(f, T + T / 2);
        s = f.services.modal().snapshot(id);
        position(s, edge, 1 - eased(.5F));
        mask(f, s, s.progress);
        at(f, 2 * T);
        check(!f.services.modal().snapshot(id).paint_visible && after == 1, "exit terminal paint/timing wrong");
        sync(f);
        check(after == 2 && mounts == 1 && title_mounts == 1 && footer_mounts == 1 &&
                  f.services.components().component_count() == component_count &&
                  f.inputs.editors().require(input.editor).selection() == selection &&
                  f.inputs.editors().require(input.editor).value() == draft,
              "motion rebuilt slots or changed the retained editor");
        idle(f, 3 * T, after, 2);
        f.services.dispose();
        check(f.services.surfaces().size() == 0 && f.services.interactions().size() == 0,
              "Drawer motion disposal leaked retained resources");
    }
}

void reversal() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> open{true};
    int after{};
    int closes{};
    int mounts{};
    f.services.mount(Content{[&] {
        Drawer(DrawerProps{}.open(open).afterOpenChange([&](bool) { ++after; }).afterClose([&] { ++closes; }),
               DrawerContent{[&] {
                   ++mounts;
                   Input(InputProps{}.defaultValue(u8"keep"));
               }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted().front();
    at(f, 90000);
    const auto first = f.services.modal().snapshot(id);
    open.set(false);
    sync(f);
    check(near(f.services.modal().snapshot(id).progress, first.progress), "close reversal jumped");
    at(f, 120000);
    const auto second = f.services.modal().snapshot(id);
    open.set(true);
    sync(f);
    check(near(f.services.modal().snapshot(id).progress, second.progress) &&
              f.services.modal().snapshot(id).motion_revision > first.motion_revision,
          "reopen did not sample/invalidate the old interval");
    at(f, 120000);
    at(f, 100000);
    check(near(f.services.modal().snapshot(id).progress, second.progress), "repeated/backwards clock moved Drawer");
    at(f, 10 * T);
    sync(f);
    check(after == 1 && closes == 0 && mounts == 1 && f.services.modal().snapshot(id).progress == 1,
          "cancelled revision delivered close or remounted content");
    idle(f, 11 * T, after, 1);
}

struct LateFailure final : detail::WindowComponentParticipant {
    bool fail{};

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

    bool synchronize_auxiliary_fragments() override {
        if (fail) {
            throw std::runtime_error("failure after Drawer fragment candidate");
        }
        return false;
    }
};

void completion() {
    for (int mode = 0; mode < 5; ++mode) {
        Fixture f;
        f.services.set_motion_preference(animation::MotionPreference::normal);
        Signal<bool> open{true};
        runtime::ComponentId id;
        int opens{};
        int closes{};
        int changed{};
        f.services.mount(Content{[&] {
            Drawer(DrawerProps{}
                       .open(open)
                       .afterOpenChange([&](bool value) {
                           if (value) {
                               ++opens;
                               if (mode == 0 && opens == 1) {
                                   open.set(false);
                               }
                           } else {
                               ++changed;
                               if (mode == 1) {
                                   open.set(true);
                               } else if (mode == 2) {
                                   f.services.destroy(id);
                               } else if (mode == 3) {
                                   throw std::runtime_error("after(false) throws once");
                               }
                           }
                       })
                       .afterClose([&] {
                           ++closes;
                           if (mode == 4) {
                               open.set(true);
                           }
                       }),
                   DrawerContent{[] { Input(InputProps{}); }});
        }});
        id = f.services.modal().mounted().front();
        sync(f);
        at(f, T);
        sync(f);
        if (mode != 0) {
            open.set(false);
        }
        at(f, 2 * T);
        bool threw{};
        try {
            sync(f);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw == (mode == 3) && changed == 1 && closes == ((mode == 1 || mode == 2) ? 0 : 1),
              "Drawer completion reentry/exception revision guard failed");
        at(f, 3 * T);
        sync(f);
        at(f, 4 * T);
        sync(f);
        check(changed == 1 && (mode == 2 || !f.services.modal().next_auxiliary_deadline()),
              "Drawer replayed consumed completion");
    }
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    int after{};
    f.services.mount(
        Content{[&] { Drawer(DrawerProps{}.open().afterOpenChange([&](bool) { ++after; }), DrawerContent{[] {}}); }});
    sync(f);
    LateFailure failure;
    f.services.attach(failure);
    failure.fail = true;
    const auto successful = f.services.successful_sync_revision();
    bool threw{};
    try {
        at(f, T);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && successful == f.services.successful_sync_revision() && after == 0,
          "failed scene sync advanced the completion authority");
    failure.fail = false;
    sync(f);
    check(after == 0, "failed terminal sync was consumed before a successful retry");
    sync(f);
    check(after == 1, "successful terminal retry did not release completion");
    f.services.detach(failure);
}

void input_motion() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> open{true};
    InputRef body;
    InputRef extra;
    InputRef footer;
    int requests{};
    f.services.mount(Content{[&] {
        Drawer(DrawerProps{}.open(open).title(u8"Input boundary").resizable(DrawerResize{true}).onCancel([&](auto) {
            ++requests;
        }),
               DrawerContent{[&] { Input(InputProps{}.ref(body)); }},
               {.footer = DrawerFooter{[&] { Input(InputProps{}.ref(footer)); }},
                .extra = DrawerExtra{[&] { Input(InputProps{}.ref(extra)); }}});
    }});
    sync(f);
    const auto id = f.services.modal().mounted().front();
    at(f, T);
    sync(f);
    check(body.focus(), "body focus failed");
    click(f, f.services.modal().snapshot(id).close);
    check(requests == 1 && f.services.modal().snapshot(id).progress == 1 &&
              !f.services.modal().snapshot(id).transitioning,
          "controlled refusal started an exit");
    const auto session = f.inputs.sessions().active();
    const auto s = f.services.modal().snapshot(id);
    open.set(false);
    check(!body.focus() && !extra.focus() && !footer.focus() &&
              !f.services.interactions().component_active(s.overlay) &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"stale"}, session})),
          "last paint-only overlay retained an input route");
    for (const auto region : {s.body, s.extra, s.footer, s.close, s.resize_handle}) {
        for (auto hit : f.services.interactions().declaration_order()) {
            const auto* record = f.services.interactions().find(hit);
            if (record && record->component == region) {
                check(!f.services.interactions().branch_active(hit), "exit retained a direct control hit");
            }
        }
    }
    click(f, s.close);
    f.services.focus().dispatch({Key::escape, KeyAction::down});
    check(requests == 1 && f.services.modal().snapshot(id).paint_visible, "exit consumed pointer/keyboard input");
    at(f, 2 * T);
    sync(f);
}

void geometry() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> open{true};
    Signal<float> width{320};
    Signal<float> height{260};
    Signal<bool> rtl{false};
    Signal<DrawerPlacement> edge{DrawerPlacement::Right};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    InputRef editor;
    int mounts{};
    std::vector<float> resize;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Drawer(DrawerProps{}
                             .open(open)
                             .width(width)
                             .height(height)
                             .placement(edge)
                             .rtl(rtl)
                             .title(u8"Geometry")
                             .resizable(DrawerResize{true, 180, 500})
                             .onResize([&](float v) { resize.push_back(v); }),
                         DrawerContent{[&] {
                             ++mounts;
                             Input(InputProps{}.ref(editor));
                         }});
              }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted().front();
    const auto input_id = f.inputs.mounted_inputs().front().editor;
    at(f, T / 2);
    const auto p = f.services.modal().snapshot(id).progress;
    width.set(400);
    height.set(280);
    rtl.set(true);
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
    dark.seed.motion_unit = Duration::milliseconds(200);
    theme.set(dark);
    for (const auto placement :
         {DrawerPlacement::Left, DrawerPlacement::Top, DrawerPlacement::Bottom, DrawerPlacement::Right}) {
        edge.set(placement);
        sync(f);
        position(f.services.modal().snapshot(id), placement, p, 400, 280);
    }
    f.services.set_display_scale(1.5F);
    sync(f, 190, 170);
    position(f.services.modal().snapshot(id), DrawerPlacement::Right, p, 400, 280, 190, 170);
    f.services.set_display_scale(1.25F);
    at(f, T);
    sync(f);
    auto handle = bounds(f, f.services.modal().snapshot(id).resize_handle);
    pointer(f, PointerAction::down, handle.x + 2, handle.y + 20);
    pointer(f, PointerAction::move, handle.x - 30, handle.y + 20);
    check(!resize.empty() && f.services.modal().snapshot(id).bounds.width == 400, "controlled resize was committed");
    width.set(resize.back());
    sync(f);
    check(f.services.modal().snapshot(id).bounds.width == resize.back(), "accepted resize did not reproject");
    edge.set(DrawerPlacement::Top);
    pointer(f, PointerAction::up, handle.x - 30, handle.y + 20);
    check(!f.services.modal().snapshot(id).resizing, "placement change retained old-axis capture");
    check(mounts == 1 && f.inputs.mounted_inputs().front().editor == input_id, "dynamic geometry remounted editor");
}

void lower_sibling_reopen_preserves_push() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> first{false};
    Signal<bool> second{false};
    InputRef first_input;
    InputRef second_input;
    f.services.mount(Content{[&] {
        Drawer(DrawerProps{}.open(true).width(400), DrawerContent{[&] {
                   Drawer(DrawerProps{}.open(first).width(260).push(DrawerPush{true, 80}),
                          DrawerContent{[&] { Input(InputProps{}.ref(first_input)); }});
                   Drawer(DrawerProps{}.open(second).width(220).push(DrawerPush{true, 140}),
                          DrawerContent{[&] { Input(InputProps{}.ref(second_input)); }});
               }});
    }});
    sync(f);
    const auto ids = std::vector(f.services.modal().mounted().begin(), f.services.modal().mounted().end());
    at(f, T);
    sync(f);
    first.set(true);
    at(f, 2 * T);
    sync(f);
    second.set(true);
    at(f, 3 * T);
    sync(f);
    const auto paint_order = [&] {
        std::vector<runtime::ComponentId> result;
        for (const auto& entry : f.services.components().paint_traversal()) {
            result.push_back(entry.component);
        }
        return result;
    };
    const auto order = paint_order();
    first.set(false);
    at(f, 3 * T + T / 2);
    const auto before = f.services.modal().snapshot(ids[0]).bounds;
    check(near(before.x, 180), "higher sibling did not retain push ownership");
    first.set(true);
    sync(f);
    check(near(f.services.modal().snapshot(ids[0]).bounds.x, before.x),
          "lower sibling reopening reordered the parent push without advancing time");
    check(order == paint_order(), "painted Drawer reversal changed its opening order");
    at(f, 5 * T);
    sync(f);
    check(near(f.services.modal().snapshot(ids[0]).bounds.x, 180) && second_input.focus() && !first_input.focus(),
          "lower sibling reversal changed the final push or input winner");
}

void nested() {
    lower_sibling_reopen_preserves_push();
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> parent{true};
    Signal<bool> first{false};
    Signal<bool> second{false};
    Signal<bool> grandchild{false};
    InputRef child_input;
    f.services.mount(Content{[&] {
        Drawer(DrawerProps{}.open(parent).width(400), DrawerContent{[&] {
                   Drawer(DrawerProps{}.open(first).width(260).push(DrawerPush{true, 80}), DrawerContent{[&] {
                              Input(InputProps{}.ref(child_input));
                              Drawer(DrawerProps{}.open(grandchild).width(180).push(DrawerPush{true, 30}),
                                     DrawerContent{[] {}});
                          }});
                   Drawer(DrawerProps{}.open(second).width(220).push(DrawerPush{true, 140}), DrawerContent{[] {}});
               }});
    }});
    sync(f);
    const auto ids = std::vector(f.services.modal().mounted().begin(), f.services.modal().mounted().end());
    at(f, T);
    sync(f);
    first.set(true);
    at(f, T + T / 2);
    const auto p = f.services.modal().snapshot(ids[1]).progress;
    check(near(f.services.modal().snapshot(ids[0]).bounds.x, 320 - 80 * p), "child midpoint push is not continuous");
    at(f, 2 * T);
    sync(f);
    second.set(true);
    grandchild.set(true);
    at(f, 2 * T + T / 2);
    const auto q = f.services.modal().snapshot(ids[3]).progress;
    check(near(f.services.modal().snapshot(ids[0]).bounds.x, 320 - std::lerp(80.0F, 140.0F, q)),
          "sibling/ grandchild push used the wrong owner or added distances");
    at(f, 3 * T);
    sync(f);
    second.set(false);
    at(f, 3 * T + T / 2);
    const auto exit = f.services.modal().snapshot(ids[3]).progress;
    check(near(f.services.modal().snapshot(ids[0]).bounds.x, 320 - std::lerp(80.0F, 140.0F, exit)),
          "exiting sibling stopped contributing push");
    const auto before = f.services.modal().snapshot(ids[0]).bounds;
    second.set(true);
    sync(f);
    check(near(f.services.modal().snapshot(ids[0]).bounds.x, before.x), "sibling reversal jumped push");
    parent.set(false);
    check(!child_input.focus(), "parent close left child family input active");
    at(f, 3 * T + T * 3 / 4);
    const auto family_mid = f.services.modal().snapshot(ids[1]).bounds;
    parent.set(true);
    sync(f);
    check(near(f.services.modal().snapshot(ids[1]).bounds.x, family_mid.x),
          "parent reversal dropped its escaped child family translation");
    parent.set(false);
    at(f, 5 * T);
    sync(f);
    check(first.get() && grandchild.get() && second.get() && !f.services.modal().next_auxiliary_deadline(),
          "parent terminal mutated child business open or retained a deadline");
    for (auto id : ids) {
        check(!f.services.modal().snapshot(id).paint_visible, "parent terminal retained an escaped child layer");
    }
    parent.set(true);
    at(f, 6 * T);
    sync(f);
    check(f.services.modal().snapshot(ids[1]).paint_visible, "parent reopen did not restore child owner visibility");
}

void gates() {
    for (int mode = 0; mode < 5; ++mode) {
        Fixture f;
        f.services.set_motion_preference(animation::MotionPreference::normal);
        Signal<bool> open{true};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        runtime::ComponentId branch;
        int after{};
        f.services.mount(Content{[&] {
            auto& build = runtime::require_component_build_context();
            branch = build.mount_component<int>(0);
            const auto node = build.root(branch);
            f.layout.set_layout(node, layout::BoxLayout{});
            build.on_resource_cleanup(branch, [&, node] { f.layout.remove_layout(node); });
            build.mount_slot(branch, Content{[&] {
                                 Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                                           Drawer(DrawerProps{}.open(open).afterOpenChange([&](bool) { ++after; }),
                                                  DrawerContent{[] {}});
                                       }});
                             }});
        }});
        sync(f);
        at(f, T / 2);
        open.set(false);
        if (mode == 0) {
            f.services.set_motion_preference(animation::MotionPreference::reduced);
        } else if (mode == 1) {
            ThemeConfig off;
            off.seed.motion = false;
            theme.set(off);
        } else if (mode == 2) {
            f.services.focus().set_window_active(false);
        } else if (mode == 3) {
            f.services.components().set_branch_active(branch, false);
        } else {
            f.services.dispose();
            check(!f.services.modal().next_auxiliary_deadline() && after == 0, "dispose delivered stale completion");
            continue;
        }
        sync(f);
        check(!f.services.modal().next_auxiliary_deadline() && after == 0, "static gate left periodic exit work");
        sync(f);
        idle(f, 2 * T, after, 1);
        if (mode == 2 || mode == 3) {
            if (mode == 2) {
                f.services.set_window_active(true);
            } else {
                f.services.components().set_branch_active(branch, true);
            }
            sync(f);
            check(after == 1, "owner reactivation replayed a completed exit");
            open.set(true);
            at(f, 14 * T);
            sync(f);
            check(after == 2 && !f.services.modal().next_auxiliary_deadline(),
                  "owner reveal did not start a new enter");
        }
    }
    // Lazy factory failure/reentry and clear cleanup reentry use real host paths.
    for (bool throws : {false, true}) {
        Fixture f;
        f.services.set_motion_preference(animation::MotionPreference::normal);
        Signal<bool> open{false};
        f.services.mount(Content{[&] {
            Drawer(DrawerProps{}.open(open).lazy(), DrawerContent{[&] {
                       Input(InputProps{});
                       if (throws) {
                           throw std::runtime_error("partial lazy body");
                       }
                       open.set(false);
                   }});
        }});
        sync(f);
        const auto id = f.services.modal().mounted().front();
        bool caught{};
        try {
            open.set(true);
        } catch (const std::runtime_error&) {
            caught = true;
        }
        check(caught == throws, "lazy factory exception boundary changed");
        if (throws) {
            check(!f.services.components().contains(id) && f.inputs.mounted_inputs().empty(),
                  "partial lazy factory left interactive resources");
        } else {
            sync(f);
            check(!f.services.modal().snapshot(id).visible && !f.services.modal().next_auxiliary_deadline(),
                  "lazy factory open reentry published a half-entered Drawer");
        }
    }
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> open{true};
    InputRef editor;
    int mounts{};
    int closes{};
    f.services.mount(Content{[&] {
        Drawer(DrawerProps{}.open(open).destroyOnHidden().afterClose([&] { ++closes; }), DrawerContent{[&] {
                   ++mounts;
                   Input(InputProps{}.ref(editor));
                   runtime::require_component_build_context()
                       .scope(f.inputs.mounted_inputs().back().component)
                       .on_cleanup([&] {
                           if (mounts == 1) {
                               open.set(true);
                           }
                       });
               }});
    }});
    sync(f);
    at(f, T);
    sync(f);
    open.set(false);
    at(f, T + T / 2);
    check(editor.bound() && !editor.focus(), "destroyOnHidden destroyed midpoint editor");
    at(f, 2 * T);
    sync(f);
    at(f, 3 * T);
    sync(f);
    if (!(open.get() && mounts == 2 && editor.bound() && closes == 0)) {
        std::cerr << "reentry: open=" << open.get() << " mounts=" << mounts << " bound=" << editor.bound()
                  << " closes=" << closes << '\n';
    }
    check(open.get() && mounts == 2 && editor.bound() && closes == 0, "old clear/close harmed reentrant new body");

    Fixture action_host;
    action_host.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> drawer_open{false};
    int action_cancels{};
    const ConfirmAction action{[&](ConfirmActionRequest, ConfirmAction::Complete) {
                                   return [&] {
                                       ++action_cancels;
                                   };
                               },
                               [](std::function<void()> fn) { fn(); }, std::chrono::milliseconds{1000}};
    action_host.services.mount(Content{[&] {
        Modal(ModalProps{}.open().onOkAsync(action), ModalContent{[] {}});
        Drawer(DrawerProps{}.open(drawer_open), DrawerContent{[] {}});
    }});
    sync(action_host);
    click(action_host, action_host.services.modal().snapshot(action_host.services.modal().mounted()[0]).ok);
    drawer_open.set(true);
    check(action_host.services.modal().next_auxiliary_deadline() == animation::AnimationTime::microseconds(16000),
          "Drawer/action deadlines did not choose the earliest work");
    action_host.services.set_motion_preference(animation::MotionPreference::reduced);
    sync(action_host);
    sync(action_host);
    check(action_host.services.modal().next_auxiliary_deadline() == animation::AnimationTime::microseconds(1000000),
          "static Drawer completion erased a live ConfirmAction deadline");
    action_host.services.dispose();
    check(action_cancels == 1, "combined motion/action disposal repeated cancellation");
}

bool ready(const DrawerHandle& handle) {
    return handle.closed().wait_for(std::chrono::seconds{0}) == std::future_status::ready;
}

void service() {
    for (int mode = 0; mode < 9; ++mode) {
        Fixture f;
        f.services.set_motion_preference(animation::MotionPreference::normal);
        AppRef app;
        AppRef captured;
        runtime::ComponentId app_root;
        runtime::ComponentId source;
        InputRef editor;
        DrawerHandle handle;
        DrawerHandle reopened;
        Signal<float> width{320};
        std::vector<int> order;
        f.services.mount(Content{[&] {
            App(AppProps{}.ref(app), AppContent{[&] {
                    auto& build = runtime::require_component_build_context();
                    app_root = build.nearest_state<detail::AppState>()->binding->owner;
                    source = build.mount_component<int>(0);
                    const auto node = build.root(source);
                    f.layout.set_layout(node, layout::BoxLayout{});
                    build.on_resource_cleanup(source, [&, node] { f.layout.remove_layout(node); });
                    build.mount_slot(source, Content{[&] { captured = useApp(); }});
                }});
        }});
        handle = captured.drawer().open(DrawerProps{}
                                            .width(width)
                                            .onCancel([&](DrawerCancelReason) { order.push_back(1); })
                                            .afterOpenChange([&](bool open) {
                                                if (!open) {
                                                    check(ready(handle) && !editor.bound(),
                                                          "service callback preceded resources/future retirement");
                                                    order.push_back(2);
                                                    if (mode == 6) {
                                                        reopened = app.drawer().open(
                                                            DrawerProps{}, DrawerContent{[] { Text(u8"New entry"); }});
                                                        throw std::runtime_error("service after(false) throws");
                                                    }
                                                }
                                            })
                                            .afterClose([&] { order.push_back(3); }),
                                        DrawerContent{[&] {
                                            Input(InputProps{}.ref(editor).onBlur([&] {
                                                if (mode == 8) {
                                                    throw std::runtime_error("close input callback failed");
                                                }
                                            }));
                                        }});
        const auto id = f.services.modal().mounted().back();
        sync(f);
        at(f, T);
        sync(f);
        check(editor.focus(), "service editor focus failed");
        if (mode == 7) {
            f.services.components().set_branch_active(source, false);
            sync(f);
            sync(f);
            order.clear();
        }
        bool threw{};
        try {
            if (mode == 1) {
                click(f, f.services.modal().snapshot(id).close);
            } else {
                check(handle.close(DrawerCancelReason::Escape), "service close was refused");
            }
        } catch (const std::runtime_error&) {
            threw = true;
        }
        if (mode == 8) {
            check(threw && ready(handle) && handle.closed().get().kind == PanelCloseKind::Failed &&
                      !f.services.modal().next_auxiliary_deadline(),
                  "close failure did not immediately retire once");
            continue;
        }
        check(!threw && !ready(handle) && !handle.bound() && !editor.focus() && !handle.update({}) && !handle.close() &&
                  f.services.components().state<detail::AppState>(app_root)->binding->panels.size() == 1,
              "closing entry lost owner membership or retained business permissions");
        if (mode >= 2 && mode <= 5) {
            if (mode == 2) {
                check(handle.destroy(), "hard destroy refused a closing entry");
            } else if (mode == 3) {
                f.services.destroy(source);
            } else if (mode == 4) {
                f.services.destroy(app_root);
            } else {
                f.services.dispose();
            }
            check(ready(handle) &&
                      handle.closed().get().kind ==
                          (mode == 2 ? PanelCloseKind::Destroyed : PanelCloseKind::OwnerDisposed) &&
                      !f.services.modal().next_auxiliary_deadline(),
                  "hard retirement waited for motion or lost cause");
            const auto kind = handle.closed().get().kind;
            check(!handle.destroy() &&
                      f.services.modal().tick_auxiliary(animation::AnimationTime::microseconds(10 * T)) == 0 &&
                      handle.closed().get().kind == kind,
                  "old completion bridge overwrote hard retirement");
            continue;
        }
        if (mode != 7) {
            width.set(400);
            at(f, T + T / 2);
            check(!ready(handle) && f.services.modal().snapshot(id).paint_visible &&
                      near(f.services.modal().snapshot(id).bounds.width, 400),
                  "service midpoint settled early or disconnected live size");
            at(f, 2 * T);
        } else {
            sync(f);
        }
        check(!ready(handle), "service settled inside terminal scene traversal");
        threw = false;
        try {
            sync(f);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(ready(handle), "service future remained pending after its valid completion");
        const auto result = handle.closed().get();
        check(threw == (mode == 6) && result.kind == (mode == 1 ? PanelCloseKind::Cancelled : PanelCloseKind::Closed) &&
                  result.drawer_cancel == (mode == 1 ? DrawerCancelReason::CloseButton : DrawerCancelReason::Escape),
              "service completion lost fixed cause or callback exception");
        check(order == (mode == 1 ? std::vector{1, 2, 3} : std::vector{2, 3}),
              "service cancellation/after order changed");
        if (mode == 6) {
            check(reopened.bound() && !ready(reopened), "old completion destroyed the callback's new entry");
            check(reopened.destroy() && ready(reopened), "new entry hard cleanup failed");
        }
        const auto saved = order;
        at(f, 4 * T);
        sync(f);
        at(f, 5 * T);
        sync(f);
        check(order == saved && !f.services.modal().next_auxiliary_deadline(), "old service bridge replayed");
    }
    Fixture first;
    Fixture second;
    first.services.set_motion_preference(animation::MotionPreference::normal);
    second.services.set_motion_preference(animation::MotionPreference::normal);
    AppRef a;
    AppRef b;
    first.services.mount(Content{[&] { App(AppProps{}.ref(a), AppContent{[] {}}); }});
    second.services.mount(Content{[&] { App(AppProps{}.ref(b), AppContent{[] {}}); }});
    const auto one = a.drawer().open(DrawerProps{}, DrawerContent{[] {}});
    const auto two = b.drawer().open(DrawerProps{}, DrawerContent{[] {}});
    check(one.close() && !ready(one), "first-window close did not pend");
    first.services.dispose();
    check(ready(one) && one.closed().get().kind == PanelCloseKind::OwnerDisposed && two.bound() && !ready(two),
          "closing owner cleanup crossed windows");
    check(two.destroy() && ready(two), "second-window hard cleanup failed");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "edges";
        if (name == "edges") {
            edges();
        } else if (name == "reversal") {
            reversal();
        } else if (name == "completion") {
            completion();
        } else if (name == "input") {
            input_motion();
        } else if (name == "geometry") {
            geometry();
        } else if (name == "nested") {
            nested();
        } else if (name == "gates") {
            gates();
        } else if (name == "service") {
            service();
        } else {
            throw std::runtime_error("unknown Drawer motion case");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
