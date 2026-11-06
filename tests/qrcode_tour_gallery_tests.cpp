#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "reference_surface.hpp"
#include "component/qrcode_component.hpp"
#include "support/navigation_fixture.hpp"
#include "support/timeline_fixture.hpp"

namespace {
using namespace ryn_test::navigation;
using namespace rynui::example;
int checks{};

void expect(bool v, const char* text) {
    ++checks;
    check(v, text);
}

struct GalleryFixture final : Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    GallerySamplePage page;

    void update() {
        expect(buttons.layout_and_synchronize({900, 1800}, {0, 0, 900, 1800}, {}, 0, true),
               "QRCode/Tour Gallery layout failed");
    }

    void mount(std::string_view identity) {
        GalleryPageModel model;
        page = make_gallery_sample_page(model.find(identity)->route(), {});
        page.set_width(760);
        surfaces.mount(page.content, &inputs);
        update();
    }

    void action(std::string_view name) {
        const auto owner = page.target(name);
        expect(owner.valid(), "QRCode Gallery target absent");
        for (const auto& b : buttons.mounted_buttons()) {
            if (descendant(*this, b.component, owner)) {
                click(*this, b.component);
                update();
                return;
            }
        }
        throw std::runtime_error("QRCode Gallery target has no native Button");
    }

    void cleanup() {
        services.dispose();
        expect(services.components().component_count() == 0 && services.interactions().size() == 0 &&
                   services.images().live_count() == 0 && services.rounded_effects().live_count() == 0 &&
                   inputs.mounted_inputs().empty() && timeline_test::blocked_roots(services.interactions()).empty(),
               "QRCode Gallery resources leaked");
    }
};

void qrcode() {
    GalleryFixture f;
    f.mount("ant.component.qr-code");
    const auto id = f.page.target("gallery.qrcode.main");
    expect(f.services.qrcode().snapshot(id).data.has_value(), "Gallery QR model absent");
    f.action("gallery.qrcode.expire");
    expect(f.services.qrcode().snapshot(id).status == QRCodeStatus::Expired, "Expire control is fake");
    const auto refresh = f.services.qrcode().snapshot(id).refresh;
    for (const auto& b : f.buttons.mounted_buttons()) {
        if (descendant(f, b.component, refresh)) {
            click(f, b.component);
            break;
        }
    }
    f.update();
    expect(f.services.qrcode().snapshot(id).status == QRCodeStatus::Active, "Gallery refresh callback failed");
    f.action("gallery.qrcode.status");
    f.action("gallery.qrcode.status");
    expect(f.services.qrcode().snapshot(id).status == QRCodeStatus::Loading, "Loading control is fake");
    f.action("gallery.qrcode.status");
    expect(f.services.qrcode().snapshot(id).status == QRCodeStatus::Scanned, "Scanned control is fake");
    f.action("gallery.qrcode.status");
    f.action("gallery.qrcode.invalid");
    expect(!f.services.qrcode().snapshot(id).data, "Invalid content generated fake code");
    f.action("gallery.qrcode.invalid");
    f.action("gallery.qrcode.logo");
    expect(f.services.qrcode().snapshot(id).logo_status == ImageStatus::Ready, "Gallery shared logo not decoded");
    f.action("gallery.qrcode.export");
    expect(f.page.sample_runs("gallery.sample.qr-code.native") == 1 &&
               gallery_sample_source("gallery.sample.qr-code.native"),
           "QRCode factory reran or source absent");
    f.cleanup();
}
} // namespace

int main(int argc, char** argv) {
    try {
        (void)argc;
        (void)argv;
        qrcode();
        std::cout << checks << " Gallery checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
