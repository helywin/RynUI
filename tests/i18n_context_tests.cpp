#include "runtime/component_host.hpp"
#include "support/i18n_test.hpp"

#include <ryn/config_provider.hpp>

#include <array>
#include <thread>

namespace {
using namespace ryn;
using namespace ryn::runtime;
using namespace ryn_test::i18n;

struct State final {};

struct ChildSlot final {};

using Child = SlotContent<ChildSlot>;

ComponentId dummy() {
    return require_component_build_context().mount_component<State>();
}

const MessageKey label{u8"app.label", String{u8"English"}};

MessageCatalog messages() {
    return MessageCatalog({{locales::zh_CN, label, String{u8"中文"}}});
}

void inheritance_and_slot_routes() {
    NodeStore nodes;
    ComponentHost host(nodes);
    Signal<LocaleId> language{locales::zh_CN};
    Signal<bool> disabled{true};
    std::array<ComponentId, 7> ids;
    I18n inherited;
    I18n nested;
    CapturedComponentContext captured;
    const auto theme = std::make_unique<ThemeConfig>();
    theme->seed.color_primary = ryn::Color::rgba8(200, 30, 40);
    int before_calls = 0;
    host.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}
                .locale(language)
                .messages(messages())
                .disabled(disabled)
                .size(ControlSize::Large)
                .direction(TextDirection::RightToLeft)
                .theme(*theme),
            ConfigProviderContent{[&] {
                inherited = useI18n();
                captured = require_component_build_context().capture_context();
                ids[0] = dummy();
                auto& build = require_component_build_context();
                build.mount_slot(ids[0], Child{[&] { ids[1] = dummy(); }});
                build.mount_slot_with_before_child(
                    ids[0], Child{[&] { Theme(ThemeProps{}, ThemeContent{[&] { ids[2] = dummy(); }}); }},
                    [&] { ++before_calls; });
                build.mount_slot_with_semantic_foreground(ids[0], Child{[&] { ids[3] = dummy(); }},
                                                          SemanticForeground{1, 0, 0, 1});
                build.mount_slot_with_semantic_text_style(ids[0], Child{[&] { ids[4] = dummy(); }},
                                                          SemanticForeground{0, 1, 0, 1}, SemanticTypography{});
                ConfigProvider(
                    ConfigProviderProps{}.locale(locales::en_US).disabled(false).direction(TextDirection::Auto),
                    ConfigProviderContent{[&] {
                        nested = useI18n();
                        ids[5] = dummy();
                    }});
            }});
        ids[6] = dummy();
    }});
    require(before_calls == 1 && host.component_count() == ids.size(),
            "provider introduced visual owners or lost before-child hook");
    for (std::size_t i = 0; i < 5; ++i) {
        const auto& config = host.config_scope(ids[i])->snapshot();
        require(config.locale.effective == locales::zh_CN && config.disabled && config.size == ControlSize::Large &&
                    config.direction == TextDirection::RightToLeft,
                "a slot route lost configuration fields");
        require(host.theme_scope(ids[i])->snapshot().seed().color_primary == *theme->seed.color_primary,
                "config/Theme wrapper lost the active Theme");
    }
    const auto& inner = host.config_scope(ids[5])->snapshot();
    require(inner.locale.effective == locales::en_US && !inner.disabled && inner.direction == TextDirection::Auto &&
                inner.size == ControlSize::Large,
            "explicit false/Auto was confused with unset");
    require(inherited.translate(label) == String{u8"中文"} && nested.translate(label) == String{u8"English"},
            "nested locale lookup mixed candidate languages");
    require(host.config_scope(ids[6])->snapshot().locale.effective == locales::en_US &&
                !host.config_scope(ids[6])->snapshot().disabled,
            "sibling inherited provider state");
    const auto theme_generation = host.theme_scope(ids[0])->generation();
    language.set(locales::en_US);
    require(host.theme_scope(ids[0])->generation() == theme_generation, "locale update resolved Theme");
    ComponentId appended;
    host.append_slot(ids[0], Content{[&] { appended = dummy(); }});
    require(host.config_scope(appended) == host.config_scope(ids[0]), "lazy append captured a stale/default config");
    ComponentId service_child;
    host.append_slot(ids[6], Content{[&] { service_child = dummy(); }}, captured);
    require(host.config_scope(service_child) == host.config_scope(ids[0]),
            "explicit captured append used holder config");
    NodeStore other_nodes;
    ComponentHost other(other_nodes);
    ComponentId other_id;
    I18n other_i18n;
    other.mount(Content{[&] {
        other_i18n = useI18n();
        other_id = dummy();
    }});
    rejects([&] { other.append_slot(other_id, Content{[] { static_cast<void>(dummy()); }}, captured); },
            I18nErrorCode::StaleContext);
    language.set(locales::zh_CN);
    require(other_i18n.locale().effective == locales::en_US && other_i18n.translate(label) == String{u8"English"},
            "separate window defaults shared mutable locale");
    host.dispose();
    require(!inherited.active() && !nested.active() && other_i18n.active(),
            "window disposal crossed an owner boundary");
    require(std::get<I18nError>(inherited.tryTranslate(label)).code == I18nErrorCode::StaleContext,
            "stale helper silently fell back to another window");
}

void formats_and_equal_projections() {
    NodeStore nodes;
    ComponentHost host(nodes);
    Signal<LocaleId> language{locales::zh_CN};
    Signal<int> revision{0};
    Signal<MessageCatalog> dictionary{messages()};
    ComponentId inherited;
    ComponentId pinned;
    I18n tr;
    int mounts = 0;
    host.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).messages(dictionary).catalog(bind([&] {
            static_cast<void>(revision.get());
            return LocaleCatalog(std::vector<LocaleDefinition>{});
        })),
                       ConfigProviderContent{[&] {
                           ++mounts;
                           tr = useI18n();
                           inherited = dummy();
                           ConfigProvider(ConfigProviderProps{}.formatLocale(locales::en_US),
                                          ConfigProviderContent{[&] { pinned = dummy(); }});
                       }});
    }});
    Scope observations;
    int callbacks = 0;
    const auto binding = tr.text(label);
    static_cast<void>(connect_binding(observations, binding, [&](const String&) { ++callbacks; }));
    const auto generation = host.config_scope(inherited)->generation();
    for (int i = 1; i <= 10000; ++i) {
        revision.set(i);
    }
    require(callbacks == 1 && generation == host.config_scope(inherited)->generation() && mounts == 1,
            "equal catalog contents emitted config or text notifications");
    const MessageKey unrelated{u8"app.unused", String{u8"Unused"}};
    dictionary.set(
        MessageCatalog({{locales::zh_CN, label, String{u8"中文"}}, {locales::zh_CN, unrelated, String{u8"不使用"}}}));
    require(callbacks == 1, "an unused key updated an unrelated text projection");
    language.set(locales::en_US);
    require(callbacks == 2 && binding.get() == String{u8"English"} && mounts == 1,
            "locale switch remounted content or missed dependent text");
    require(host.config_scope(inherited)->snapshot().format_locale.effective == locales::en_US &&
                host.config_scope(pinned)->snapshot().format_locale.effective == locales::en_US,
            "format locale default/pinning failed");
    language.set(locales::zh_CN);
    require(host.config_scope(inherited)->snapshot().format_locale.effective == locales::zh_CN &&
                host.config_scope(pinned)->snapshot().format_locale.effective == locales::en_US,
            "format locale did not track language independently");
    const auto deep_theme = std::make_unique<ThemeConfig>();
    deep_theme->alias.color_text = ryn::Color::rgba8(15, 25, 35);
    const auto props = ConfigProviderProps{}.locale(language).theme(*deep_theme);
    NodeStore deep_nodes;
    ComponentHost deep(deep_nodes);
    ComponentId leaf;
    std::function<void(unsigned)> nest;
    nest = [&](unsigned remaining) {
        ConfigProvider(props, ConfigProviderContent{[&, remaining] {
                           if (remaining) {
                               nest(remaining - 1);
                           } else {
                               leaf = dummy();
                           }
                       }});
    };
    deep.mount(Content{[&] { nest(24); }});
    require(deep.component_count() == 1 && deep.config_scope(leaf)->snapshot().locale.effective == locales::zh_CN,
            "24 nested Providers lost scope or required visual wrapper nodes");
    require(deep.theme_scope(leaf)->snapshot().alias().color_text == *deep_theme->alias.color_text,
            "24 nested Providers lost the shared Theme initialization path");
    bool wrong_thread = false;
    std::thread worker([&] {
        try {
            static_cast<void>(tr.locale());
        } catch (const std::logic_error&) {
            wrong_thread = true;
        }
    });
    worker.join();
    require(wrong_thread, "captured runtime context allowed cross-thread access");
}

void rejected_repeated_input_and_reentry() {
    NodeStore nodes;
    ComponentHost host(nodes);
    Signal<LocaleId> language{locales::en_US};
    Signal<int> revision{0};
    TextDirection direction = TextDirection::LeftToRight;
    ComponentId owner;
    ComponentId leaf;
    I18n tr;
    int reads = 0;
    host.mount(Content{[&] {
        owner = dummy();
        require_component_build_context().mount_slot(
            owner, Child{[&] {
                ConfigProvider(ConfigProviderProps{}.locale(language).messages(messages()).direction(bind([&] {
                    static_cast<void>(revision.get());
                    ++reads;
                    return direction;
                })),
                               ConfigProviderContent{[&] {
                                   tr = useI18n();
                                   leaf = dummy();
                               }});
            }});
    }});
    const auto scope = host.config_scope(leaf);
    const auto generation = scope->generation();
    const auto* snapshot = &scope->snapshot();
    direction = static_cast<TextDirection>(99);
    for (int i = 1; i <= 2; ++i) {
        const auto before_reads = reads;
        rejects([&] { revision.set(i); }, I18nErrorCode::InvalidSchema);
        require(reads == before_reads + 1 && generation == scope->generation() && snapshot == &scope->snapshot(),
                "invalid repeated binding was cached or published");
    }
    direction = TextDirection::Auto;
    revision.set(3);
    require(scope->snapshot().direction == TextDirection::Auto && scope->generation() == generation + 1,
            "valid config did not recover after rejection");
    Scope observations;
    bool reenter = true;
    static_cast<void>(connect_binding(observations, tr.text(label), [&](const String& value) {
        if (reenter && value == String{u8"中文"}) {
            reenter = false;
            language.set(locales::en_US);
        }
    }));
    language.set(locales::zh_CN);
    require(tr.locale().effective == locales::en_US && tr.translate(label) == String{u8"English"},
            "reentrant locale change was overwritten by an older publication");
    const auto text = tr.text(label);
    static_cast<void>(connect_binding(observations, text, [&](const String& value) {
        if (value == String{u8"中文"}) {
            require(host.destroy(owner), "callback owner retirement failed");
        }
    }));
    language.set(locales::zh_CN);
    require(!tr.active() && !scope->active() && !host.contains(leaf), "destroy callback retained an active scope");
    rejects([&] { static_cast<void>(text.get()); }, I18nErrorCode::StaleContext);
    const auto retired_reads = reads;
    revision.set(4);
    language.set(locales::en_US);
    require(reads == retired_reads && nodes.size() == 0, "retired provider still observed config changes");
}

void descendant_validation_preserves_published_configuration() {
    const LocaleId custom("fr-FR");
    const LocaleCatalog extended({{custom, LocaleCatalog::builtin().pack(locales::en_US)}});
    auto parent = ConfigScope::create_default();
    ConfigOverrides parent_values;
    parent_values.catalog = extended;
    parent->update(parent_values);
    ConfigOverrides child_values;
    child_values.messages = MessageCatalog({{custom, label, String{u8"French custom"}}});
    auto child = ConfigScope::create(parent, child_values);
    const auto* parent_snapshot = &parent->snapshot();
    const auto* child_snapshot = &child->snapshot();
    const auto parent_generation = parent->generation();
    const auto child_generation = child->generation();
    parent_values.catalog = LocaleCatalog{};
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        rejects([&] { parent->update(parent_values); }, I18nErrorCode::UnknownLocale);
        require(&parent->snapshot() == parent_snapshot && &child->snapshot() == child_snapshot &&
                    parent->generation() == parent_generation && child->generation() == child_generation,
                "a descendant's unknown reference partially published an ancestor config");
    }
    child_values.messages = MessageCatalog{};
    child->update(child_values);
    require(parent->update(parent_values), "preflight did not recover after fixing the descendant reference");
    parent->dispose();
    require(!child->active(), "parent scope disposal left descendant context usable");
}

} // namespace

int main() {
    suppress_dialogs();
    try {
        inheritance_and_slot_routes();
        formats_and_equal_projections();
        rejected_repeated_input_and_reentry();
        descendant_validation_preserves_published_configuration();
        std::cout << "i18n context: 4 contract groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
