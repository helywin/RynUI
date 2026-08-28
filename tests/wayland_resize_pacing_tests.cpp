#include <cassert>
#include <cstdint>
#include <vector>

namespace {

struct Configuration {
    std::uint32_t serial{};
    std::uint32_t state{};
    std::int32_t width{};
    std::int32_t height{};
    std::uint32_t references{1};
    bool freed{};

    Configuration* ref() {
        assert(!freed);
        assert(references > 0);
        ++references;
        return this;
    }

    void unref() {
        assert(!freed);
        assert(references > 0);
        --references;
        if (references == 0) {
            freed = true;
        }
    }
};

struct Ack {
    std::uint32_t serial{};
    std::uint32_t state{};
    std::int32_t width{};
    std::int32_t height{};

    bool operator==(const Ack&) const = default;
};

struct ResizePacer {
    Configuration* pending{};
    std::vector<Ack> acked;
    std::uint32_t exposures{};
    std::uint32_t replacements{};
    std::uint32_t outstanding_refs{};
    std::uint32_t max_outstanding_refs{};

    void configure(Configuration& configuration, bool resizing) {
        if (!resizing) {
            release_pending();
            ack(configuration);
            return;
        }

        const bool request_exposure = pending == nullptr;
        Configuration* retained = configuration.ref();
        if (pending) {
            ++replacements;
            pending->unref();
        } else {
            ++outstanding_refs;
            if (outstanding_refs > max_outstanding_refs) {
                max_outstanding_refs = outstanding_refs;
            }
        }
        pending = retained;
        if (request_exposure) {
            ++exposures;
        }
    }

    void frame_callback() {
        if (!pending) {
            return;
        }
        Configuration* configuration = pending;
        pending = nullptr;
        --outstanding_refs;
        ack(*configuration);
        configuration->unref();
        ++exposures;
    }

    void focus_changed(bool) {
    }

    void destroy() {
        release_pending();
    }

private:
    void ack(const Configuration& configuration) {
        acked.push_back({configuration.serial, configuration.state,
                         configuration.width, configuration.height});
    }

    void release_pending() {
        if (pending) {
            Configuration* configuration = pending;
            pending = nullptr;
            --outstanding_refs;
            configuration->unref();
        }
    }
};

void deliver_configure(ResizePacer& pacer, Configuration& configuration,
                       bool resizing) {
    pacer.configure(configuration, resizing);
    configuration.unref(); // libdecor releases its callback-owned reference.
}

void test_callback_return_keeps_payload_until_latest_ack() {
    ResizePacer pacer;
    Configuration first{10, 0x100, 800, 600};
    Configuration second{11, 0x100, 810, 610};
    Configuration latest{12, 0x100, 820, 620};

    deliver_configure(pacer, first, true);
    assert(!first.freed);
    deliver_configure(pacer, second, true);
    assert(first.freed);
    deliver_configure(pacer, latest, true);
    assert(second.freed);
    pacer.frame_callback();

    assert((pacer.acked == std::vector<Ack>{{12, 0x100, 820, 620}}));
    assert(latest.freed);
    assert(pacer.replacements == 2);
    assert(pacer.max_outstanding_refs == 1);
    assert(pacer.outstanding_refs == 0);
}

void test_same_serial_burst_is_acked_once() {
    ResizePacer pacer;
    Configuration first{20, 0x100, 900, 700};
    Configuration latest{20, 0x100, 910, 710};

    deliver_configure(pacer, first, true);
    deliver_configure(pacer, latest, true);
    pacer.frame_callback();

    assert((pacer.acked == std::vector<Ack>{{20, 0x100, 910, 710}}));
    assert(first.freed);
    assert(latest.freed);
}

void test_serials_advance_on_frames_without_focus_events() {
    ResizePacer pacer;
    Configuration first{30, 0x100, 920, 720};
    Configuration second{31, 0x100, 930, 730};

    deliver_configure(pacer, first, true);
    pacer.frame_callback();
    deliver_configure(pacer, second, true);
    pacer.frame_callback();

    assert((pacer.acked == std::vector<Ack>{
        {30, 0x100, 920, 720}, {31, 0x100, 930, 730}}));
}

void test_resize_end_releases_pending_and_commits_final_state() {
    ResizePacer pacer;
    Configuration pending{40, 0x100, 940, 740};
    Configuration final{41, 0, 950, 750};

    deliver_configure(pacer, pending, true);
    pacer.focus_changed(false);
    pacer.focus_changed(true);
    assert(pacer.acked.empty());

    deliver_configure(pacer, final, false);
    assert((pacer.acked == std::vector<Ack>{{41, 0, 950, 750}}));
    assert(pending.freed);
    assert(final.freed);
    assert(pacer.outstanding_refs == 0);
}

void test_destroy_releases_pending_reference() {
    ResizePacer pacer;
    Configuration pending{50, 0x100, 960, 760};

    deliver_configure(pacer, pending, true);
    pacer.destroy();

    assert(pending.freed);
    assert(pacer.outstanding_refs == 0);
    assert(pacer.pending == nullptr);
}

} // namespace

int main() {
    test_callback_return_keeps_payload_until_latest_ack();
    test_same_serial_burst_is_acked_once();
    test_serials_advance_on_frames_without_focus_events();
    test_resize_end_releases_pending_and_commits_final_state();
    test_destroy_releases_pending_reference();
}
