#include "support/i18n_components.hpp"

namespace {
using namespace ryn_test::i18n_components;

void inherited_nested_and_independent_windows() {
    Fixture fixture;
    Signal<LocaleId> language{locales::zh_CN};
    int mounts = 0;
    fixture.services.mount(Content{[&] {
        defaults();
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           ++mounts;
                           defaults();
                           ConfigProvider(ConfigProviderProps{}.locale(locales::en_US),
                                          ConfigProviderContent{[] { defaults(); }});
                       }});
    }});
    synchronize(fixture);
    labels(fixture, 0, u8"No data", u8"OK", u8"Cancel");
    labels(fixture, 1, u8"暂无数据", u8"确定", u8"取消");
    labels(fixture, 2, u8"No data", u8"OK", u8"Cancel");
    const auto empty = fixture.services.empty_result().empties()[1];
    const auto label = text_component(fixture, fixture.services.empty_result().snapshot(empty).label);
    const auto root = fixture.services.components().root(label.component);
    const auto count = fixture.services.components().component_count();

    Fixture other;
    other.services.mount(Content{[] { defaults(); }});
    synchronize(other);
    language.set(locales::en_US);
    synchronize(fixture);
    labels(fixture, 1, u8"No data", u8"OK", u8"Cancel");
    language.set(locales::zh_CN);
    synchronize(fixture);
    labels(fixture, 1, u8"暂无数据", u8"确定", u8"取消");
    labels(fixture, 2, u8"No data", u8"OK", u8"Cancel");
    labels(other, 0, u8"No data", u8"OK", u8"Cancel");
    require(mounts == 1 && fixture.services.components().component_count() == count &&
                fixture.services.components().root(label.component) == root &&
                text_component(fixture, fixture.services.empty_result().snapshot(empty).label).scene == label.scene,
            "language switching remounted real default labels or a Provider subtree");
}

void equal_catalog_and_unrelated_fields_stay_idle() {
    Fixture fixture;
    Signal<int> revision{0};
    Signal<LocaleId> language{locales::zh_CN};
    Signal<MessageCatalog> messages{MessageCatalog{}};
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).messages(messages).catalog(bind([&] {
            static_cast<void>(revision.get());
            return LocaleCatalog(std::vector<LocaleDefinition>{});
        })),
                       ConfigProviderContent{[] { defaults(); }});
    }});
    synchronize(fixture);
    static_cast<void>(fixture.frames.consume_request());
    const auto requests = fixture.frames.counters().requests;
    const auto empty = fixture.services.empty_result().empties().front();
    const auto label = text_component(fixture, fixture.services.empty_result().snapshot(empty).label);
    const auto text_revision = fixture.scene.revisions(label.scene).content;
    const auto measures = fixture.scene.record_counters(label.scene).measurement_synchronizations;
    for (int i = 1; i <= 1000; ++i) {
        revision.set(i);
    }
    messages.set(MessageCatalog({{locales::zh_CN, MessageKey{u8"unused", String{u8"Unused"}}, String{u8"未使用"}}}));
    language.set(locales::zh_CN);
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests &&
                fixture.scene.revisions(label.scene).content == text_revision &&
                fixture.scene.record_counters(label.scene).measurement_synchronizations == measures &&
                !fixture.services.next_frame_deadline(),
            "equal configuration or an unrelated field dirtied default labels or scheduled a frame");
    labels(fixture, 0, u8"暂无数据", u8"确定", u8"取消");
}

void retired_owner_releases_locale_observers() {
    Fixture fixture;
    Signal<LocaleId> language{locales::zh_CN};
    int reads = 0;
    const auto owner = mount_owned(fixture, Content{[&] {
                                       ConfigProvider(ConfigProviderProps{}.locale(bind([&] {
                                           ++reads;
                                           return language.get();
                                       })),
                                                      ConfigProviderContent{[] { defaults(); }});
                                   }});
    synchronize(fixture);
    std::weak_ptr<ConfigScope> config =
        fixture.services.components().config_scope(fixture.services.empty_result().empties().front());
    require(fixture.services.destroy(owner), "localized component owner did not retire");
    synchronize(fixture);
    const auto before = reads;
    static_cast<void>(fixture.frames.consume_request());
    const auto requests = fixture.frames.counters().requests;
    language.set(locales::en_US);
    require(reads == before && config.expired() && fixture.services.empty_result().empties().empty() &&
                fixture.services.modal().mounted().empty() && fixture.services.popconfirm().mounted().empty() &&
                fixture.services.components().component_count() == 0 && fixture.scene.size() == 0 &&
                !fixture.frames.pending() && fixture.frames.counters().requests == requests &&
                !fixture.services.next_frame_deadline(),
            "retired components retained locale subscriptions, scene state or idle work");
}

void reentrant_locale_keeps_the_latest_labels() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    fixture.services.mount(Content{
        [&] { ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[] { defaults(); }}); }});
    synchronize(fixture);
    const auto config = fixture.services.components().config_scope(fixture.services.empty_result().empties()[0]);
    bool reentered = false;
    Scope observer;
    static_cast<void>(connect_binding(observer, bind([config] { return config->snapshot().locale.effective; }),
                                      [&](const LocaleId& locale) {
                                          if (locale == locales::zh_CN && !reentered) {
                                              reentered = true;
                                              language.set(locales::en_US);
                                          }
                                      }));
    language.set(locales::zh_CN);
    synchronize(fixture);
    require(reentered && config->snapshot().locale.effective == locales::en_US,
            "reentrant language update did not keep the latest effective snapshot");
    labels(fixture, 0, u8"No data", u8"OK", u8"Cancel");
    static_cast<void>(fixture.frames.consume_request());
    const auto requests = fixture.frames.counters().requests;
    language.set(locales::en_US);
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests &&
                !fixture.services.next_frame_deadline(),
            "settled reentrant default labels retained idle work");
}
} // namespace

int main() {
    ryn_test::i18n::suppress_dialogs();
    try {
        inherited_nested_and_independent_windows();
        equal_catalog_and_unrelated_fields_stay_idle();
        retired_owner_releases_locale_observers();
        reentrant_locale_keeps_the_latest_labels();
        std::cout << "i18n component defaults passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
