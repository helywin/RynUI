#include "support/rate_fixture.hpp"
#include <limits>
#include <string_view>

namespace {
using namespace rate_test;

void real_text(bool reproduce_before) {
    Fixture f;
    f.services.mount(Content{[] { Rate(RateProps{}.count(1).character(RateCharacter{u8"a"})); }});
    sync(f);
    const auto id = f.services.rate().mounted()[0];
    const auto root = f.services.components().root(id);
    const auto scope = f.services.components().theme_scope(id);
    const auto* before = &scope->snapshot();
    const auto generation = scope->generation();
    const auto pages = f.scene.atlas().page_count();
    int notifications = 0;
    auto subscription = scope->capture([&](theme_runtime::DirtyPhase) { ++notifications; },
                                       [&] { static_cast<void>(scope->rate_metrics()); });
    auto config = std::make_unique<ThemeConfig>();
    config->rate.tokens.star_size = dp(100000000);
    bool preflight_rejected = false;
    bool font_failed = false;
    try {
        scope->update(*config);
        sync(f);
    } catch (const std::invalid_argument&) {
        preflight_rejected = true;
    } catch (const std::runtime_error& e) {
        font_failed = std::string_view{e.what()} == "Input validation fonts failed to load";
        if (!font_failed) {
            throw;
        }
    }
    if (reproduce_before) {
        const auto rejected =
            f.fonts->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, font::FontRasterConfig{100000000, 1});
        check(!preflight_rejected && font_failed && scope->snapshot().rate().metrics.star_size == 100000000 &&
                  scope->generation() > generation && &scope->snapshot() != before &&
                  f.scene.atlas().page_count() == pages && !rejected &&
                  rejected.error.stage == font::FontErrorStage::pixel_size_configuration &&
                  rejected.error.kind == font::FontErrorKind::invalid_pixel_size,
              "baseline did not reproduce published Rate theme followed by FontRuntime rejection");
        std::cout << "BEFORE: published star_size=100000000; FontRuntime invalid_pixel_size; notifications="
                  << notifications << "; atlas pages unchanged=" << pages << '\n';
        return;
    }
    check(preflight_rejected && !font_failed && &scope->snapshot() == before && scope->generation() == generation &&
              notifications == 0 && f.scene.atlas().page_count() == pages,
          "oversized Rate font was published or reached native font loading");
    config->rate.tokens.star_size = dp(22);
    scope->update(*config);
    sync(f);
    check(notifications == 1 && scope->snapshot().rate().metrics.star_size == 22 &&
              glyphs(f, root, bounds(f, root)) > 0,
          "valid font recovery did not render the retained native character");
    pointer(f, f.services.rate().snapshot(id).items[0], PointerAction::move);
    sync(f);
    check(scope->snapshot().rate().metrics.star_size == 22 && glyphs(f, root, bounds(f, root)) > 0,
          "valid hover font recovery did not render");
}

void sizes_and_quantization() {
    constexpr auto maximum = std::numeric_limits<int>::max() / 64;
    const float adjacent = std::nextafter(static_cast<float>(maximum), 0.0F);
    const float product_size = 1082401;
    const float product_scale = 31;
    check(std::lround(adjacent) <= maximum && static_cast<double>(product_size) * product_scale == maximum &&
              std::round(static_cast<double>(product_size * product_scale)) > maximum,
          "test no longer distinguishes the production float product from double multiplication");
    auto scope = theme_runtime::ThemeScope::create_default();
    const auto* before = &scope->snapshot();
    const auto generation = scope->generation();
    int notifications = 0;
    auto subscription = scope->capture([&](theme_runtime::DirtyPhase) { ++notifications; },
                                       [&] {
                                           static_cast<void>(scope->rate_colors());
                                           static_cast<void>(scope->rate_metrics());
                                       });
    const auto reject = [&](const ThemeConfig& config) {
        bool rejected = false;
        try {
            scope->update(config);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && &scope->snapshot() == before && scope->generation() == generation && notifications == 0,
              "invalid normal/hover font partially published a snapshot or notification");
    };
    for (auto member :
         {&RateTokenOverride::star_size, &RateTokenOverride::star_size_small, &RateTokenOverride::star_size_large}) {
        for (bool hover : {false, true}) {
            auto config = std::make_unique<ThemeConfig>();
            config->rate.tokens.star = Color::rgba8(3, 5, 7);
            config->rate.tokens.*member = dp(hover ? 20000000.0F : 100000000.0F);
            config->rate.tokens.hover_scale = hover ? 2.0F : 1.0F;
            reject(*config);
        }
        auto boundary = std::make_unique<ThemeConfig>();
        boundary->rate.tokens.*member = dp(product_size);
        boundary->rate.tokens.hover_scale = product_scale;
        reject(*boundary);
        boundary->rate.tokens.*member = dp(adjacent);
        boundary->rate.tokens.hover_scale = 1.0F;
        const auto accepted = std::make_unique<ThemeSnapshot>(resolve_theme(*boundary));
        check(accepted->rate().metrics.hover_scale == 1, "valid adjacent native font boundary rejected");
    }
    auto config = std::make_unique<ThemeConfig>();
    config->rate.algorithm = true;
    config->rate.seed.control_height = dp(160000000);
    reject(*config);
    config = std::make_unique<ThemeConfig>();
    config->rate.tokens.star_size = dp(.5F);
    config->rate.tokens.star_size_small = dp(.5F);
    config->rate.tokens.star_size_large = dp(.5F);
    const auto minimum = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
    check(std::lround(minimum->rate().metrics.star_size) == 1, "valid .5 logical font quantization rejected");
    config->rate.tokens.star_size = dp(22);
    config->rate.tokens.star_size_small = dp(17);
    config->rate.tokens.star_size_large = dp(27);
    scope->update(*config);
    check(notifications == 1 && scope->snapshot().rate().metrics.star_size_large == 27,
          "valid direct update failed after rejected fonts");
}

void inherited_preflight() {
    auto root = theme_runtime::ThemeScope::create_default();
    auto config = std::make_unique<ThemeConfig>();
    config->rate.tokens.hover_scale = 31.0F;
    auto child = theme_runtime::ThemeScope::create(root, *config);
    auto grandchild = theme_runtime::ThemeScope::create(child, *std::make_unique<ThemeConfig>());
    const std::array scopes{root, child, grandchild};
    std::array<const ThemeSnapshot*, 3> before{};
    std::array<std::uint64_t, 3> generations{};
    std::array<int, 3> notifications{};
    std::array<theme_runtime::Subscription, 3> subscriptions;
    for (std::size_t i = 0; i < scopes.size(); ++i) {
        before[i] = &scopes[i]->snapshot();
        generations[i] = scopes[i]->generation();
        subscriptions[i] = scopes[i]->capture([&, i](theme_runtime::DirtyPhase) { ++notifications[i]; },
                                              [&, i] {
                                                  static_cast<void>(scopes[i]->rate_colors());
                                                  static_cast<void>(scopes[i]->rate_metrics());
                                              });
    }
    for (auto member :
         {&RateTokenOverride::star_size, &RateTokenOverride::star_size_small, &RateTokenOverride::star_size_large}) {
        config = std::make_unique<ThemeConfig>();
        config->rate.tokens.*member = dp(1082401);
        config->rate.tokens.star = Color::rgba8(11, 13, 17);
        // This parent is valid alone; only the child's inherited size + local hover scale is invalid.
        static_cast<void>(resolve_theme(*config));
        bool rejected = false;
        try {
            root->update(*config);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "invalid inherited hover product accepted");
        for (std::size_t i = 0; i < scopes.size(); ++i) {
            check(before[i] == &scopes[i]->snapshot() && generations[i] == scopes[i]->generation() &&
                      notifications[i] == 0,
                  "ancestor preflight partially published snapshots or notifications");
        }
    }
    config = std::make_unique<ThemeConfig>();
    config->rate.tokens.star_size = dp(1082400);
    root->update(*config);
    for (std::size_t i = 0; i < scopes.size(); ++i) {
        check(notifications[i] == 1 && scopes[i]->generation() > generations[i], "inherited font recovery failed");
    }
    check(child->snapshot().rate().metrics.star_size * child->snapshot().rate().metrics.hover_scale == 33554400 &&
              grandchild->snapshot().rate() == child->snapshot().rate(),
          "inherited float product recovery drifted");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const bool before = argc > 1 && std::string_view{argv[1]} == "--reproduce-before";
        real_text(before);
        if (!before) {
            sizes_and_quantization();
            inherited_preflight();
        }
        std::cout << "Rate font limit passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
