#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "reference_surface.hpp"
#include "component/image_component.hpp"
#include "component/upload_component.hpp"
#include "support/navigation_fixture.hpp"

namespace {
using namespace ryn_test::navigation;
using namespace rynui::example;
int checks{};

void expect(bool value, const char* message) {
    ++checks;
    check(value, message);
}

struct GalleryFixture final : Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    GallerySamplePage page;

    void update() {
        expect(buttons.layout_and_synchronize({900, 1800}, {0, 0, 900, 1800}, {}, 0, true),
               "Media Gallery layout failed");
    }

    void mount(std::string_view identity) {
        GalleryPageModel model;
        page = make_gallery_sample_page(model.find(identity)->route(), {});
        page.set_width(760);
        surfaces.mount(page.content, &inputs);
        update();
        for (const auto& sample : gallery_samples()) {
            if (sample.component_identity == identity) {
                expect(page.sample_runs(sample.identity) == 1 && gallery_sample_source(sample.identity),
                       "Media factory/source absent");
            }
        }
    }

    void action(std::string_view name) {
        const auto target = page.target(name);
        expect(target.valid(), "Media control missing");
        for (const auto& button : buttons.mounted_buttons()) {
            if (descendant(*this, button.component, target)) {
                click(*this, button.component);
                update();
                return;
            }
        }
        throw std::runtime_error("Media control is not a native button");
    }

    void cleanup() {
        services.dispose();
        expect(services.components().component_count() == 0 && services.interactions().size() == 0 &&
                   services.images().live_count() == 0 && services.rounded_effects().live_count() == 0 &&
                   inputs.mounted_inputs().empty(),
               "Media page leaked");
    }
};

void image() {
    GalleryFixture f;
    f.mount("ant.component.image");
    const auto id = f.page.target("gallery.image.primary");
    expect(f.services.image().snapshot(id).status == ImageStatus::Ready &&
               f.services.image().snapshot(id).image.has_value(),
           "Image page has no real pixels");
    f.action("gallery.image.load");
    expect(f.services.image().snapshot(id).status == ImageStatus::Loading, "Image loading control is fake");
    f.action("gallery.image.deliver");
    expect(f.services.image().snapshot(id).status == ImageStatus::Ready, "Image delivery did not load");
    f.action("gallery.image.load");
    f.action("gallery.image.failure");
    expect(f.services.image().snapshot(id).status == ImageStatus::Error, "Image error control is fake");
    f.action("gallery.image.load");
    f.action("gallery.image.deliver");
    f.action("gallery.image.preview");
    expect(f.services.image().snapshot(id).open, "Image preview control failed");
    f.services.image().action(id, ImageAction::Close);
    f.update();
    f.action("gallery.image.reorder");
    f.cleanup();
}

void upload() {
    GalleryFixture f;
    f.mount("ant.component.upload");
    const auto id = f.page.target("gallery.upload.primary");
    f.action("gallery.upload.fixture");
    auto v = f.services.upload().snapshot(id);
    expect(v.files.size() == 1 && v.files[0].status == UploadFileStatus::Uploading && v.rows[0].image.valid(),
           "Upload fixture/transport did not start");
    const auto row = v.rows[0].component;
    f.action("gallery.upload.ignore");
    expect(f.services.upload().snapshot(id).files.size() == 1, "Actual beforeUpload Ignore not honored");
    f.action("gallery.upload.progress");
    v = f.services.upload().snapshot(id);
    expect(v.files[0].percent == 20 && v.rows[0].component == row, "Manual progress lost stable row");
    f.action("gallery.upload.failure");
    expect(f.services.upload().snapshot(id).files[0].status == UploadFileStatus::Error,
           "Synthetic failure control did not fail");
    expect(f.services.upload().retry(id, v.files[0].uid), "Gallery retry failed");
    f.update();
    f.action("gallery.upload.success");
    expect(f.services.upload().snapshot(id).files[0].status == UploadFileStatus::Done,
           "Synthetic success control did not finish");
    f.action("gallery.upload.custom");
    const auto content = f.services.upload().snapshot(id).rows[0].content;
    f.action("gallery.upload.list");
    expect(f.services.upload().snapshot(id).rows[0].content == content, "List mode remounted rich file draft");
    expect(f.services.upload().remove(id, v.files[0].uid), "Gallery remove request failed");
    f.action("gallery.upload.remove");
    expect(f.services.upload().snapshot(id).files.empty(), "Manual remove approval failed");
    f.cleanup();
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view(argv[1]) == "upload") {
            upload();
        } else {
            image();
        }
        std::cout << checks << " actual media Gallery checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
