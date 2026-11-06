#include "input/input_constraints.hpp"
#include "component/modal_component.hpp"
#include <ryn/text_area.hpp>
#include "support/allocation_probe.hpp"
#include "support/timeline_fixture.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
using namespace timeline_test;

namespace {
int checks{};

void suppress_dialogs() {
#if defined(_MSC_VER)
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _CALL_REPORTFAULT);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
#endif
}

void expect(bool v, const char* message) {
    ++checks;
    check(v, message);
}

template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception&) {
        rejected = true;
    }
    expect(rejected, "Invalid input capability was accepted");
}

struct Inputs final : Fixture {
    std::array<InputRef, 4> refs;
    std::array<runtime::ComponentId, 4> ids;
    runtime::ComponentId owner;

    void mount() {
        services.mount(Content{[&] {
            Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                     for (auto& ref : refs) {
                         Input(InputProps{}.ref(ref));
                     }
                 }});
        }});
        sync(*this);
        owner = services.components().root_components().front();
        for (std::size_t i = 0; i < 4; ++i) {
            ids[i] = inputs.mounted_inputs()[i].component;
        }
    }

    input::InputRootRef root(std::size_t i) {
        return services.interactions().input_root(ids[i]);
    }

    input::InputConstraintLease lease(input::InputConstraintKind kind) {
        return services.create_input_constraint(services.interactions().input_root(owner), kind);
    }

    void clean() {
        services.dispose();
        const auto d = services.interactions().input_constraint_diagnostics();
        expect(d.leases == 0 && d.companions == 0 && services.interactions().size() == 0 &&
                   blocked_roots(services.interactions()).empty(),
               "Owned/legacy input constraints leaked on retirement");
    }
};

void ownership() {
    Inputs f;
    f.mount();
    auto a = f.lease(InputConstraintKind::BlockSubtrees);
    auto b = f.lease(InputConstraintKind::BlockSubtrees);
    auto batch = f.services.interactions().begin_input_constraint_update();
    batch.replace(a, {{f.root(0)}});
    batch.replace(b, {{f.root(0)}});
    expect(f.services.publish_input_constraints(batch), "Owned blocks did not publish");
    expect(!f.refs[0].focus() && f.refs[1].focus(), "Owned block did not gate exact subtree");
    f.services.release_input_constraint(a);
    expect(!f.refs[0].focus(), "First owner release erased the independent owner");
    f.services.set_input_blocked(f.ids[0], true);
    const auto revision = f.services.interactions().input_boundary_revision();
    const auto frames = f.services.frame_request_revision();
    for (int i = 0; i < 100; ++i) {
        expect(!f.services.set_input_blocked(f.ids[0], true), "Legacy repeated true changed ownership");
        auto stable = f.services.interactions().begin_input_constraint_update();
        stable.replace(b, {{f.root(0)}});
        expect(!f.services.publish_input_constraints(stable), "Identical owned replace published");
    }
    expect(f.services.interactions().input_boundary_revision() == revision &&
               f.services.frame_request_revision() == frames,
           "Stable setters requested frames/revisions");
    f.services.release_input_constraint(b);
    expect(!f.refs[0].focus(), "Owned release erased the independent legacy lane");
    f.services.set_input_blocked(f.ids[0], false);
    expect(f.refs[0].focus(), "Legacy lane did not release exactly once");
    f.clean();
}

void algebra() {
    Inputs f;
    f.mount();
    auto a = f.lease(InputConstraintKind::RestrictToSubtrees);
    auto b = f.lease(InputConstraintKind::RestrictToSubtrees);
    auto block = f.lease(InputConstraintKind::BlockSubtrees);
    const auto roots = [&](unsigned bits) {
        InputConstraintSpec spec;
        for (unsigned i = 0; i < 4; ++i) {
            if (bits & (1U << i)) {
                spec.roots.push_back(f.root(i));
            }
        }
        return spec;
    };
    for (unsigned x = 0; x < 16; ++x) {
        for (unsigned y = 0; y < 16; ++y) {
            for (unsigned z = 0; z < 16; ++z) {
                auto batch = f.services.interactions().begin_input_constraint_update();
                batch.replace(a, roots(x));
                batch.replace(b, roots(y));
                batch.replace(block, roots(z));
                f.services.interactions().publish_input_constraints(batch);
                for (unsigned i = 0; i < 4; ++i) {
                    const bool permitted = (x & (1U << i)) && (y & (1U << i)) && !(z & (1U << i));
                    expect(f.services.interactions().component_active(f.ids[i]) == permitted,
                           "Actual C++ restriction/block algebra differs from independent bit sets");
                }
                expect(f.services.interactions().subtree_intersects_input_domain(f.owner) == bool(x & y & ~z & 15U),
                       "Maintenance domain confused ancestors with allowed recipients");
            }
        }
    }
    f.clean();
}

void lifetime() {
    Inputs a;
    Inputs b;
    a.mount();
    b.mount();
    expect(a.ids[0] == b.ids[0], "Window alias fixture did not share local IDs");
    auto lease = a.lease(InputConstraintKind::RestrictToSubtrees);
    const auto old = lease;
    const auto foreign = b.root(0);
    rejects(
        [&] { static_cast<void>(a.services.create_input_constraint(foreign, InputConstraintKind::BlockSubtrees)); });
    rejects([&] {
        auto batch = a.services.interactions().begin_input_constraint_update();
        batch.replace(lease, {{foreign}});
    });
    auto batch = a.services.interactions().begin_input_constraint_update();
    batch.replace(lease, {{a.root(1)}, true});
    a.services.publish_input_constraints(batch);
    expect(a.refs[1].focus() && !a.refs[0].focus() && b.refs[0].focus(), "Foreign window or empty allow policy leaked");
    bool thread_rejected = false;
    bool released = true;
    std::thread thread([&] {
        try {
            static_cast<void>(lease.valid());
        } catch (const std::logic_error&) {
            thread_rejected = true;
        }
        released = lease.release();
    });
    thread.join();
    expect(thread_rejected && !released && lease.valid(), "Foreign thread changed an input capability");
    const auto retired = a.root(1);
    a.services.destroy(a.ids[1]);
    expect(!retired.valid() && !a.refs[0].focus(), "Target retirement removed the background restriction");
    auto empty = a.services.interactions().begin_input_constraint_update();
    empty.replace(lease, {{}, true});
    a.services.publish_input_constraints(empty);
    expect(a.services.interactions().has_input_confinement() && a.services.interactions().consumes_unhandled_scroll() &&
               !a.refs[2].focus(),
           "Empty allow list became unrestricted");
    a.services.release_input_constraint(lease);
    auto next = a.lease(InputConstraintKind::BlockSubtrees);
    auto stale = old;
    expect(!stale.release() && next.valid(), "Stale release affected a reused lease generation");
    a.clean();
    b.clean();
}

void text_admission() {
    Inputs f;
    f.mount();
    expect(f.refs[0].focus(), "Text admission initial focus failed");
    const auto stamp = f.inputs.sessions().active();
    f.services.interactions().set_input_blocked(f.ids[0], true);
    expect(!f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 0}, stamp}),
           "Raw registry gate leaked preedit");
    expect(!f.inputs.dispatch(TextCommitted{String{u8"forbidden"}, stamp}), "Raw registry gate leaked text");
    expect(!f.inputs.dispatch(CandidatesChanged{std::vector<String>{String{u8"forbidden"}}, std::size_t{0},
                                                CandidateOrientation::vertical, stamp}),
           "Raw registry gate leaked candidate state");
    expect(!f.inputs.sessions().active().valid(), "Denied recipient kept a valid active session");
    f.services.set_input_blocked(f.ids[0], false);
    expect(f.refs[0].focus(), "Text admission did not recover");
    expect(!f.inputs.dispatch(TextCommitted{String{u8"old epoch"}, stamp}),
           "Old session stamp survived a gate transition");
    f.clean();
}

void callback_admission() {
    Fixture f;
    InputRef ref;
    runtime::ComponentId id;
    bool entered = false;
    bool composition = true;
    bool text = true;
    TextInputSessionStamp stamp;
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(ref).onSubmit([&](String) {
            entered = true;
            f.services.set_input_blocked(id, true);
            composition = bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 0}, stamp}));
            text = bool(f.inputs.dispatch(TextCommitted{String{u8"forbidden"}, stamp}));
        }));
    }});
    sync(f);
    id = f.inputs.mounted_inputs()[0].component;
    expect(ref.focus(), "Callback admission initial focus failed");
    stamp = f.inputs.sessions().active();
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    expect(entered && !composition && !text && !f.inputs.sessions().active().valid(),
           "Synchronous keyboard callback bypassed text/IME admission");
    f.services.set_input_blocked(id, false);
    int activations = 0;
    FocusHandlers handlers;
    handlers.activation_allowed = [&] {
        f.services.set_input_blocked(id, true);
        return true;
    };
    handlers.activate = [&] {
        ++activations;
    };
    f.services.interactions().set_focus_handlers(f.inputs.mounted_inputs()[0].interaction, std::move(handlers));
    expect(ref.focus(), "Activation admission initial focus failed");
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    expect(activations == 0 && !f.services.interactions().component_active(id),
           "Activation gate callback bypassed fresh branch eligibility");
    f.services.set_input_blocked(id, false);
    f.services.dispose();
    expect(blocked_roots(f.services.interactions()).empty(), "Callback cleanup left a legacy block");
}

void modal_companions() {
    Fixture f;
    Signal<bool> open{true};
    InputRef background;
    InputRef target;
    InputRef sibling;
    InputRef controls;
    bool watch = false;
    bool observed = false;
    runtime::ComponentId bg;
    runtime::ComponentId t;
    runtime::ComponentId other;
    runtime::ComponentId c;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Input(InputProps{}.ref(background));
                 Modal(ModalProps{}.open(open).title(u8"real Modal"), ModalContent{[&] {
                           Input(InputProps{}.ref(target));
                           Input(InputProps{}.ref(sibling).onBlur([&] {
                               if (watch) {
                                   observed = true;
                                   expect(f.services.interactions().component_active(t) &&
                                              f.services.interactions().component_active(c) &&
                                              !f.services.interactions().component_active(bg) &&
                                              !f.services.interactions().component_active(other),
                                          "Callback observed a partially published Tour/Modal domain");
                               }
                           }));
                       }});
                 Input(InputProps{}.ref(controls));
             }});
    }});
    sync(f);
    sync(f);
    const auto inputs = f.inputs.mounted_inputs();
    expect(inputs.size() == 4, "Actual Modal companion fixture failed");
    bg = inputs[0].component;
    t = inputs[1].component;
    other = inputs[2].component;
    c = inputs[3].component;
    const auto root = [&](runtime::ComponentId id) {
        return f.services.interactions().input_root(id);
    };
    const auto modal = f.services.modal().mounted().front();
    const auto domain = f.services.modal().snapshot(modal).overlay;
    expect(f.services.interactions().input_boundary() == domain &&
               f.services.modal().effective_input_domain() == domain,
           "Modal compatibility/effective domain changed");
    expect(target.focus() && !controls.focus() && !background.focus() && sibling.focus(),
           "Original Modal confinement changed");
    auto allow = f.services.create_input_constraint(root(c), InputConstraintKind::RestrictToSubtrees);
    auto hidden = f.services.create_input_constraint(root(c), InputConstraintKind::BlockSubtrees);
    auto companion = f.services.modal().register_tour_controls(root(c), {root(c)});
    rejects([&] { static_cast<void>(f.services.modal().register_tour_controls(root(c), {root(bg)})); });
    const std::array changes{InputConstraintUpdate{allow, InputConstraintSpec{{root(t), root(c)}, true}},
                             InputConstraintUpdate{hidden, std::nullopt}};
    watch = true;
    const auto before = f.services.interactions().input_boundary_revision();
    companion.update(true, changes);
    watch = false;
    expect(observed && f.services.interactions().input_boundary_revision() == before + 1,
           "Tour restriction and Modal grant did not publish once");
    expect(target.focus() && controls.focus() && !sibling.focus() && !background.focus(),
           "Modal target/external controls did not share the exact allowed domain");
    expect(f.services.interactions().subtree_intersects_input_domain(domain) &&
               !f.services.interactions().component_active(domain),
           "Modal maintenance and input receiving eligibility were conflated");
    f.services.set_input_blocked(t, true);
    expect(!target.focus() && controls.focus(), "Companion bypassed an independent legacy block");
    f.services.set_input_blocked(t, false);
    f.services.components().set_branch_active(t, false);
    expect(!target.focus() && controls.focus(), "Companion bypassed hidden target branch");
    f.services.components().set_branch_active(t, true);
    const std::array outside{InputConstraintUpdate{allow, InputConstraintSpec{{root(bg), root(c)}, true}}};
    companion.update(true, outside);
    expect(!background.focus() && !target.focus() && controls.focus(),
           "Tour target outside Modal bypassed the explicit product boundary");
    expect(!f.services.interactions().subtree_intersects_input_domain(bg, &allow),
           "Outside Modal target was incorrectly considered available geometry");
    // Modal first: its authority retires, while the independent Tour restriction remains.
    open.set(false);
    sync(f);
    expect(!f.services.interactions().input_boundary() && background.focus() && controls.focus(),
           "Closing Modal did not preserve the independent Tour domain");
    const std::array close{InputConstraintUpdate{allow, std::nullopt},
                           InputConstraintUpdate{hidden, InputConstraintSpec{{root(c)}}}};
    companion.update(false, close);
    expect(background.focus() && !controls.focus(), "Closing/fading Tour still accepted private control input");
    open.set(true);
    sync(f);
    expect(target.focus() && !background.focus() && !controls.focus(),
           "Reopened Modal inherited an old companion grant");
    const std::array reopen{InputConstraintUpdate{allow, InputConstraintSpec{{root(t), root(c)}, true}},
                            InputConstraintUpdate{hidden, std::nullopt}};
    companion.update(true, reopen);
    expect(target.focus() && controls.focus() && !sibling.focus(),
           "New Modal authority did not independently authorize controls");
    companion.update(false, close);
    expect(target.focus() && sibling.focus() && !controls.focus() && !background.focus(),
           "Tour first retirement released the enclosing Modal");
    f.services.dispose();
    const auto d = f.services.interactions().input_constraint_diagnostics();
    expect(d.leases == 0 && d.companions == 0 && !companion.bound() && !allow.valid() && !hidden.valid() &&
               blocked_roots(f.services.interactions()).empty(),
           "Actual Modal/Tour owner cleanup leaked leases/grants");
}

void hot_path() {
    Inputs f;
    f.mount();
    const auto point = bounds(f, f.ids[1]);
    const runtime::Point hit{point.x + point.width / 2, point.y + point.height / 2};
    const auto run = [&](const char* mode) {
        const auto before = f.services.hit_test().diagnostics();
        const auto frames = f.services.frame_request_revision();
        const auto start = std::chrono::steady_clock::now();
        ryn_test::allocation::begin();
        for (int i = 0; i < 20000; ++i) {
            expect(f.services.hit_test().hit_test(hit).has_value(), "Allowed actual hit was lost");
            expect(!f.services.hit_test().hit_test({590, 790}), "Geometric miss became an input target");
        }
        const auto allocations = ryn_test::allocation::end();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        const auto after = f.services.hit_test().diagnostics();
        expect(allocations == 0 && after.branch_checks - before.branch_checks == 20000 &&
                   after.snapshot_rebuilds == before.snapshot_rebuilds && f.services.frame_request_revision() == frames,
               "Hot constraints allocated/rebuilt/requested frames or checked branches before geometry");
        std::cout << mode << " queries=40000 allocations=" << allocations
                  << " branch_checks=" << after.branch_checks - before.branch_checks << " elapsed_us=" << elapsed
                  << '\n';
    };
    run("no_constraints");
    auto lease = f.lease(InputConstraintKind::RestrictToSubtrees);
    auto batch = f.services.interactions().begin_input_constraint_update();
    batch.replace(lease, {{f.root(1), f.root(2)}, true});
    f.services.publish_input_constraints(batch);
    sync(f);
    run("stable_tour_domain");
    const auto step_start = std::chrono::steady_clock::now();
    const auto step_frames = f.services.frame_request_revision();
    ryn_test::allocation::begin();
    auto step = f.services.interactions().begin_input_constraint_update();
    step.replace(lease, {{f.root(1), f.root(3)}, true});
    f.services.publish_input_constraints(step);
    const auto step_allocations = ryn_test::allocation::end();
    const auto step_elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - step_start).count();
    std::cout << "one_step allocations=" << step_allocations
              << " frame_requests=" << f.services.frame_request_revision() - step_frames
              << " elapsed_us=" << step_elapsed << '\n';
    sync(f);
    run("after_step");
    const auto frames = f.services.frame_request_revision();
    for (int i = 0; i < 50; ++i) {
        sync(f);
    }
    expect(f.services.frame_request_revision() == frames && !f.services.next_frame_deadline(),
           "Stable owned restriction created idle frames/deadlines");
    f.clean();
}

struct CreationResult final {
    std::size_t allocations{};
    bool failed{};
};

CreationResult creation_attempt(bool companions, std::size_t failure) {
    Inputs f;
    f.mount();
    auto& registry = f.services.interactions();
    auto lease = f.lease(InputConstraintKind::RestrictToSubtrees);
    const auto authority = registry.input_authority(lease);
    const auto owner = f.root(2);
    const std::array controls{owner};
    auto existing = f.services.create_input_companion(authority, owner, controls);
    auto setup = registry.begin_input_constraint_update();
    setup.replace(lease, {{f.root(1)}, true});
    setup.enable(existing);
    setup.boundary_view(authority);
    static_cast<void>(f.services.publish_input_constraints(setup));
    expect(f.refs[1].focus() && f.refs[2].focus() && !f.refs[0].focus(), "Creation sweep authority setup failed");
    const auto revision = registry.input_boundary_revision();
    const auto diagnostics = registry.input_constraint_diagnostics();
    const auto focus = f.services.focus().state().focused;
    const auto ticket = f.services.focus().focus_request_revision();
    std::array<InputConstraintLease, 16> leases;
    std::array<InputCompanionLease, 16> grants;
    std::size_t created{};
    CreationResult result;
    std::fprintf(stderr, "input creation companions=%d failure=%zu\n", companions, failure);
    // Fixed handle arrays are prepared above. Only production creation runs in the fault scope;
    // no test-side vector default/move construction, focus request or notification is included.
    ryn_test::allocation::begin(failure);
    try {
        for (; created < leases.size(); ++created) {
            if (companions) {
                grants[created] = registry.create_input_companion(authority, owner, controls);
            } else {
                leases[created] = registry.create_input_constraint(owner, InputConstraintKind::BlockSubtrees);
            }
        }
    } catch (const std::bad_alloc&) {
        result.failed = true;
    } catch (...) {
        static_cast<void>(ryn_test::allocation::end());
        throw;
    }
    result.allocations = ryn_test::allocation::end();
    for (std::size_t i = 0; i < created; ++i) {
        expect(companions ? grants[i].release() : leases[i].release(),
               "Creation sweep lost a returned inactive handle");
    }
    const auto after = registry.input_constraint_diagnostics();
    expect(after.leases == diagnostics.leases && after.active_leases == diagnostics.active_leases &&
               after.companions == diagnostics.companions && after.active_companions == diagnostics.active_companions &&
               after.publications == diagnostics.publications && registry.input_boundary_revision() == revision &&
               registry.is_boundary_authority(authority) && registry.input_boundary() == f.ids[1] && existing.valid() &&
               f.services.focus().state().focused == focus && f.services.focus().focus_request_revision() == ticket,
           "New slot/growth failure leaked a live slot or changed the published authority/focus");
    expect(f.refs[1].focus() && f.refs[2].focus() && !f.refs[0].focus() && !f.refs[3].focus(),
           "New slot/growth failure changed actual restriction/companion routing");
    f.clean();
    return result;
}

void creation_rollback() {
    constexpr std::size_t limit = 512;
    for (const bool companions : {false, true}) {
        const auto measured = creation_attempt(companions, std::numeric_limits<std::size_t>::max());
        expect(!measured.failed && measured.allocations > 0 && measured.allocations < limit,
               "Creation allocation count is empty or exceeds the bounded sweep");
        std::size_t failures{};
        for (std::size_t failure = 0; failure <= measured.allocations; ++failure) {
            const auto result = creation_attempt(companions, failure);
            expect(result.failed == (failure < measured.allocations), "Creation sweep skipped a fault index");
            if (result.failed) {
                ++failures;
            } else {
                expect(result.allocations == measured.allocations,
                       "Creation success sentinel allocation count drifted");
            }
        }
        expect(failures == measured.allocations, "Creation allocation sweep was incomplete");
        std::cout << "creation companions=" << companions << " allocations=" << measured.allocations
                  << " caught=" << failures << " success_sentinel=1\n";
    }
}

void rollback_and_callbacks() {
    creation_rollback();
    Inputs f;
    f.mount();
    auto lease = f.lease(InputConstraintKind::RestrictToSubtrees);
    const auto select = [&](unsigned i) {
        auto batch = f.services.interactions().begin_input_constraint_update();
        batch.replace(lease, {{f.root(i)}, true});
        f.services.publish_input_constraints(batch);
    };
    select(1);
    const auto update = [&] {
        auto batch = f.services.interactions().begin_input_constraint_update();
        batch.replace(lease, {{f.root(2)}, true});
        static_cast<void>(f.services.interactions().publish_input_constraints(batch));
    };
    ryn_test::allocation::begin();
    try {
        update();
    } catch (...) {
        static_cast<void>(ryn_test::allocation::end());
        throw;
    }
    const auto allocations = ryn_test::allocation::end();
    expect(allocations > 0 && allocations < 512, "Batch allocation count is empty or exceeds the bounded sweep");
    select(1);
    std::size_t failures{};
    for (std::size_t failure = 0; failure <= allocations; ++failure) {
        const auto before = f.services.interactions().input_boundary_revision();
        bool rejected = false;
        std::fprintf(stderr, "input batch failure=%zu\n", failure);
        ryn_test::allocation::begin(failure);
        try {
            update();
        } catch (const std::bad_alloc&) {
            rejected = true;
        } catch (...) {
            static_cast<void>(ryn_test::allocation::end());
            throw;
        }
        const auto attempted = ryn_test::allocation::end();
        expect(rejected == (failure < allocations), "Batch sweep skipped a fault index");
        if (rejected) {
            ++failures;
            expect(f.services.interactions().input_boundary_revision() == before &&
                       f.services.interactions().component_active(f.ids[1]) &&
                       !f.services.interactions().component_active(f.ids[2]),
                   "Allocation failure partially published an input policy");
        } else {
            expect(attempted == allocations, "Batch success sentinel allocation count drifted");
            expect(f.services.interactions().component_active(f.ids[2]), "Successful transaction did not publish");
            select(1);
        }
    }
    expect(failures == allocations, "Policy allocation rollback sweep was incomplete");
    std::cout << "batch allocations=" << allocations << " caught=" << failures << " success_sentinel=1\n";
    f.services.release_input_constraint(lease);

    struct Participant final : detail::WindowComponentParticipant {
        std::function<void()> callback;

        void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

        void on_input_boundary_changed() override {
            if (callback) {
                callback();
            }
        }
    } participant;

    unsigned calls{};
    participant.callback = [&] {
        ++calls;
        if (calls == 1) {
            throw std::runtime_error("boundary callback");
        }
    };
    f.services.attach(participant);
    rejects([&] { f.services.set_input_blocked(f.ids[0], true); });
    expect(calls == 1 && !f.refs[0].focus(), "Throwing callback changed the already accepted policy");
    f.services.synchronize_input_constraints();
    expect(calls == 2, "A throwing callback permanently acknowledged an unfinished notification revision");
    participant.callback = [&] {
        ++calls;
        if (calls == 3) {
            f.services.set_input_blocked(f.ids[1], true);
        }
    };
    f.services.set_input_blocked(f.ids[2], true);
    expect(calls == 4 && !f.refs[0].focus() && !f.refs[1].focus() && !f.refs[2].focus(),
           "Reentrant publication did not retire/restart the old notification pass");
    participant.callback = [&] {
        ++calls;
        const bool current = f.services.interactions().component_active(f.ids[3]);
        f.services.set_input_blocked(f.ids[3], current);
    };
    const auto start = calls;
    rejects([&] { f.services.set_input_blocked(f.ids[2], false); });
    expect(calls - start == 32, "Nonconverging callbacks were not explicitly bounded");
    f.services.detach(participant);
    for (auto id : f.ids) {
        f.services.set_input_blocked(id, false);
    }
    f.clean();
}

void scope_retirement() {
    Fixture f;
    InputRef ref;
    unsigned blurred{};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] { Input(InputProps{}.ref(ref).onBlur([&] { ++blurred; })); }});
    }});
    sync(f);
    const auto owner = f.services.components().root_components().front();
    const auto root = f.services.interactions().input_root(owner);
    auto lease = f.services.create_input_constraint(root, InputConstraintKind::RestrictToSubtrees);
    auto batch = f.services.interactions().begin_input_constraint_update();
    batch.replace(lease, {{root}, true});
    f.services.publish_input_constraints(batch);
    expect(ref.focus(), "Scope retirement initial focus failed");
    const auto stamp = f.inputs.sessions().active();
    expect(bool(f.inputs.dispatch(CompositionChanged{String{u8"draft"}, {0, 0}, stamp})),
           "Scope retirement preedit setup failed");
    f.services.components().scope(owner).dispose();
    expect(f.services.components().contains(owner) && !lease.valid() &&
               f.services.interactions().input_constraint_diagnostics().leases == 0,
           "Retained owner storage extended its input lease beyond scope retirement");
    expect(!ref.focus() && !f.inputs.dispatch(TextCommitted{String{u8"forbidden"}, stamp}),
           "Retired ancestor scope still admitted descendant input");
    f.services.synchronize_input_constraints();
    expect(blurred == 0 && !f.inputs.sessions().active().valid() && !f.inputs.next_caret_deadline(),
           "Quiet scope retirement delivered user callbacks or kept IME active");
    f.services.dispose();
    expect(blurred == 0 && f.services.interactions().size() == 0 && blocked_roots(f.services.interactions()).empty(),
           "Quiet owner retirement leaked resources or callbacks");
}

void pointer_focus_routes() {
    Fixture f;
    unsigned a{};
    unsigned b{};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Button(ButtonProps{}.onClick([&] { ++a; }), ButtonContent{[] { Text(u8"A"); }});
                 Button(ButtonProps{}.onClick([&] { ++b; }), ButtonContent{[] { Text(u8"B"); }});
             }});
    }});
    sync(f);
    const auto owner = f.services.components().root_components().front();
    const auto first = f.buttons.mounted_buttons()[0].component;
    const auto second = f.buttons.mounted_buttons()[1].component;
    const auto p = bounds(f, first);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, p.x + p.width / 2, p.y + p.height / 2});
    auto lease = f.services.create_input_constraint(f.services.interactions().input_root(owner),
                                                    InputConstraintKind::RestrictToSubtrees);
    auto batch = f.services.interactions().begin_input_constraint_update();
    batch.replace(lease, {{f.services.interactions().input_root(second)}, true});
    f.services.publish_input_constraints(batch);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, p.x + p.width / 2, p.y + p.height / 2});
    expect(a == 0, "Gate change left an old pointer press activatable");
    click(f, second);
    expect(b == 1, "Allowed target Button did not receive actual pointer input");
    f.services.dispose();
    expect(f.services.interactions().input_constraint_diagnostics().leases == 0, "Pointer route leaked its lease");
    Inputs inputs;
    inputs.mount();
    auto domain = inputs.lease(InputConstraintKind::RestrictToSubtrees);
    auto edit = inputs.services.interactions().begin_input_constraint_update();
    edit.replace(domain, {{inputs.root(1), inputs.root(2)}, true});
    inputs.services.publish_input_constraints(edit);
    expect(inputs.refs[1].focus() && !inputs.refs[0].focus(), "Ref focus ignored owned restriction");
    inputs.services.focus().dispatch({Key::tab, KeyAction::down});
    expect(inputs.services.focus().state().focused == inputs.inputs.mounted_inputs()[2].interaction,
           "Tab escaped the allowed domain");
    inputs.services.focus().dispatch({Key::tab, KeyAction::down, KeyModifier::shift});
    expect(inputs.services.focus().state().focused == inputs.inputs.mounted_inputs()[1].interaction,
           "Shift Tab escaped the allowed domain");
    inputs.services.focus().defer_focus(inputs.inputs.mounted_inputs()[0].interaction, FocusModality::keyboard);
    expect(inputs.services.focus().state().focused != inputs.inputs.mounted_inputs()[0].interaction,
           "Deferred focus bypassed owned constraints");
    inputs.clean();
}

void child_modal_context() {
    Fixture f;
    Signal<bool> outer_open{true};
    Signal<bool> inner_open{false};
    InputRef background;
    InputRef target;
    InputRef controls;
    InputRef child;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Input(InputProps{}.ref(background));
                 Modal(ModalProps{}.open(outer_open), ModalContent{[&] { Input(InputProps{}.ref(target)); }});
                 Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                          Input(InputProps{}.ref(controls));
                          Modal(ModalProps{}.open(inner_open), ModalContent{[&] { Input(InputProps{}.ref(child)); }});
                      }});
             }});
    }});
    sync(f);
    sync(f);
    const auto owner = f.services.components().children(f.services.components().root_components().front()).back();
    const auto refs = f.inputs.mounted_inputs();
    const auto root = [&](runtime::ComponentId id) {
        return f.services.interactions().input_root(id);
    };
    auto restriction = f.services.create_input_constraint(root(owner), InputConstraintKind::RestrictToSubtrees);
    auto companion = f.services.modal().register_tour_controls(root(owner), {root(owner)});
    const std::array update{
        InputConstraintUpdate{restriction, InputConstraintSpec{{root(refs[1].component), root(owner)}, true}}};
    companion.update(true, update);
    expect(target.focus() && controls.focus() && !background.focus(), "Outer Modal companion setup failed");
    inner_open.set(true);
    sync(f);
    expect(child.focus() && !target.focus() && !controls.focus() && !background.focus(),
           "Child Modal granted its ancestor Tour controls or leaked background");
    expect(companion.stack_order().has_value(), "Tour did not reuse the actual Modal paint-stack context");
    auto controls_block = f.services.create_input_constraint(root(owner), InputConstraintKind::BlockSubtrees);
    f.services.components().set_branch_active(owner, false);
    const std::array suspended{InputConstraintUpdate{restriction, std::nullopt},
                               InputConstraintUpdate{controls_block, InputConstraintSpec{{root(owner)}, false}}};
    companion.update(false, suspended);
    expect(target.focus() && !controls.focus() && !child.focus() && !background.focus(),
           "Inactive retained controls left their child Modal authoritative during policy publication");
    f.services.components().set_branch_active(owner, true);
    const std::array resumed{
        InputConstraintUpdate{restriction, InputConstraintSpec{{root(refs[1].component), root(owner)}, true}},
        InputConstraintUpdate{controls_block, std::nullopt}};
    companion.update(true, resumed);
    expect(child.focus() && !target.focus() && !controls.focus() && !background.focus(),
           "Resumed controlled child Modal did not atomically regain its authority");
    inner_open.set(false);
    sync(f);
    expect(target.focus() && controls.focus() && !child.focus() && !background.focus(),
           "Closing child Modal did not restore the parent authority and exact controls");
    f.services.destroy(owner);
    expect(!companion.bound() && target.focus() && !background.focus(),
           "Retiring controls released the enclosing Modal");
    outer_open.set(false);
    sync(f);
    expect(background.focus(), "Last Modal did not retire its owned domain");
    f.services.dispose();
    const auto d = f.services.interactions().input_constraint_diagnostics();
    expect(d.leases == 0 && d.companions == 0, "Nested Modal authority/grant generation leaked");
}

void wheel_routes() {
    Fixture f;
    const String content{u8"line0\nline1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9"};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 TextArea(TextAreaProps{}.defaultValue(content).rows(2));
                 TextArea(TextAreaProps{}.defaultValue(content).rows(2));
             }});
    }});
    sync(f);
    const auto owner = f.services.components().root_components().front();
    const auto rows = f.inputs.mounted_inputs();
    auto lease = f.services.create_input_constraint(f.services.interactions().input_root(owner),
                                                    InputConstraintKind::RestrictToSubtrees);
    auto batch = f.services.interactions().begin_input_constraint_update();
    batch.replace(lease, {{f.services.interactions().input_root(rows[0].component)}, true});
    f.services.publish_input_constraints(batch);
    const auto target = f.inputs.layout_snapshot(rows[0].component).viewport;
    const auto background = f.inputs.layout_snapshot(rows[1].component).viewport;
    expect(f.inputs.dispatch(ScrollInputEvent{0, -1, target.x + 4, target.y + 4}),
           "Allowed TextArea did not consume native wheel routing");
    sync(f);
    expect(f.inputs.layout_snapshot(rows[0].component).vertical_scroll > 0,
           "Allowed TextArea wheel did not move actual retained text");
    const auto before = f.inputs.layout_snapshot(rows[1].component).vertical_scroll;
    expect(f.inputs.dispatch(ScrollInputEvent{0, -1, background.x + 4, background.y + 4}),
           "Owned confinement allowed external scroll fallback");
    sync(f);
    expect(f.inputs.layout_snapshot(rows[1].component).vertical_scroll == before,
           "Disallowed TextArea scrolled behind the Tour mask");
    f.services.release_input_constraint(lease);
    expect(!f.services.dispatch_scroll(ScrollInputEvent{0, -1, 590, 790}),
           "No-constraint external wheel fallback was lost");
    f.services.dispose();
    expect(f.services.interactions().input_constraint_diagnostics().leases == 0, "Wheel routing leaked a constraint");
}

void participant_generations() {
    Inputs f;
    f.mount();

    struct Participant final : detail::WindowComponentParticipant {
        std::function<void()> callback;

        void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

        void on_input_boundary_changed() override {
            if (callback) {
                callback();
            }
        }
    } first;

    alignas(Participant) std::byte storage[sizeof(Participant)];
    auto* second = std::construct_at(reinterpret_cast<Participant*>(storage));
    unsigned first_calls{};
    unsigned new_calls{};
    bool replaced = false;
    first.callback = [&] {
        ++first_calls;
        if (!replaced) {
            replaced = true;
            f.services.detach(*second);
            std::destroy_at(second);
            second = std::construct_at(reinterpret_cast<Participant*>(storage));
            second->callback = [&] {
                ++new_calls;
                expect(first_calls >= 2, "A reused participant address received the retired generation's notification");
            };
            f.services.attach(*second);
        }
    };
    f.services.attach(first);
    f.services.attach(*second);
    f.services.set_input_blocked(f.ids[0], true);
    expect(first_calls == 2 && new_calls == 1,
           "Participant registration epoch did not retire/restart exactly one pass");
    f.services.detach(first);
    f.services.detach(*second);
    std::destroy_at(second);
    f.services.set_input_blocked(f.ids[0], false);
    f.clean();
}

void focus_restore_tickets() {
    Fixture f;
    std::array<InputRef, 3> refs;
    int mode{};
    InteractionId second;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Input(InputProps{}.ref(refs[0]).onBlur([&] {
                     if (mode == 1) {
                         f.services.focus().defer_focus(second, FocusModality::keyboard);
                     } else if (mode == 2) {
                         f.services.focus().defer_focus({}, FocusModality::keyboard);
                     }
                 }));
                 Input(InputProps{}.ref(refs[1]));
                 Input(InputProps{}.ref(refs[2]));
             }});
    }});
    sync(f);
    const auto rows = f.inputs.mounted_inputs();
    second = rows[1].interaction;
    expect(refs[0].focus(), "Focus restore ticket setup failed");
    mode = 1;
    auto ticket = f.services.focus().focus_request_revision();
    f.services.set_input_blocked(rows[0].component, true);
    expect(f.services.focus().state().focused == second &&
               !f.services.focus().defer_focus_if_current(ticket, rows[2].interaction, FocusModality::keyboard),
           "Restore replaced a newer blur-callback focus request");
    f.services.set_input_blocked(rows[0].component, false);
    expect(refs[0].focus(), "Clear-focus ticket setup failed");
    mode = 2;
    ticket = f.services.focus().focus_request_revision();
    f.services.set_input_blocked(rows[0].component, true);
    expect(!f.services.focus().state().focused &&
               !f.services.focus().defer_focus_if_current(ticket, rows[2].interaction, FocusModality::keyboard),
           "Restore replaced a newer deferred clear request");
    f.services.set_input_blocked(rows[0].component, false);
    mode = 0;
    expect(refs[0].focus(), "Natural restore ticket setup failed");
    ticket = f.services.focus().focus_request_revision();
    f.services.set_input_blocked(rows[0].component, true);
    expect(f.services.focus().defer_focus_if_current(ticket, second, FocusModality::keyboard) &&
               f.services.focus().state().focused == second,
           "Natural gate blur did not allow a current restore ticket");
    ticket = f.services.focus().focus_request_revision();
    static_cast<void>(refs[1].focus());
    expect(!f.services.focus().defer_focus_if_current(ticket, rows[2].interaction, FocusModality::keyboard),
           "Idempotent newer focus intent did not retire the old restore ticket");
    f.services.destroy(rows[1].component);
    ticket = f.services.focus().focus_request_revision();
    expect(!f.services.focus().defer_focus_if_current(ticket, second, FocusModality::keyboard),
           "Restore ticket accepted a retired interaction generation");
    ticket = f.services.focus().focus_request_revision();
    f.services.set_window_active(false);
    expect(!f.services.focus().defer_focus_if_current(ticket, rows[2].interaction, FocusModality::keyboard),
           "Inactive window accepted an old focus restore ticket");
    f.services.set_window_active(true);
    f.services.set_input_blocked(rows[0].component, false);
    f.services.dispose();
    expect(f.services.interactions().size() == 0 && blocked_roots(f.services.interactions()).empty(),
           "Focus ticket fixture leaked input state");
}

} // namespace

int main(int argc, char** argv) {
    suppress_dialogs();
    try {
        const std::string name = argc > 1 ? argv[1] : "all";
        if (name == "ownership" || name == "all") {
            ownership();
        }
        if (name == "algebra" || name == "all") {
            algebra();
        }
        if (name == "lifetime" || name == "all") {
            lifetime();
        }
        if (name == "text" || name == "all") {
            text_admission();
        }
        if (name == "callback" || name == "all") {
            callback_admission();
        }
        if (name == "modal" || name == "all") {
            modal_companions();
        }
        if (name == "hot" || name == "all") {
            hot_path();
        }
        if (name == "rollback" || name == "all") {
            rollback_and_callbacks();
        }
        if (name == "scope" || name == "all") {
            scope_retirement();
        }
        if (name == "routes" || name == "all") {
            pointer_focus_routes();
        }
        if (name == "child_modal" || name == "all") {
            child_modal_context();
        }
        if (name == "wheel" || name == "all") {
            wheel_routes();
        }
        if (name == "participant" || name == "all") {
            participant_generations();
        }
        if (name == "focus_ticket" || name == "all") {
            focus_restore_tickets();
        }
        std::cout << "Input constraint checks=" << checks << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
