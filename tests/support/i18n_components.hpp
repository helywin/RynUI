#pragma once

#include "component/empty_result_component.hpp"
#include "component/modal_component.hpp"
#include "component/popconfirm_component.hpp"
#include "i18n_test.hpp"
#include "input_fixture.hpp"

#include <ryn/config_provider.hpp>

#include <algorithm>

namespace ryn_test::i18n_components {
using namespace ryn;
using namespace ryn::runtime;
using namespace ryn_test::i18n;
using Fixture = ryn_test::input_component::Fixture;

inline void synchronize(Fixture& fixture) {
    for (int frame = 0; frame < 3; ++frame) {
        require(fixture.services.layout_and_synchronize({720, 600}, {0, 0, 720, 600}), "i18n component layout failed");
    }
}

inline bool descendant(Fixture& fixture, ComponentId child, ComponentId ancestor) {
    for (auto current = std::optional{child}; current && fixture.services.components().contains(*current);
         current = fixture.services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}

inline detail::MountedTextComponent text_component(Fixture& fixture, ComponentId ancestor) {
    const auto texts = fixture.services.text().mounted_texts();
    const auto found =
        std::ranges::find_if(texts, [&](const auto& text) { return descendant(fixture, text.component, ancestor); });
    require(found != texts.end(), "real component label Text was not mounted");
    return *found;
}

inline String label_text(Fixture& fixture, ComponentId ancestor) {
    const auto mounted = text_component(fixture, ancestor);
    return String::from_utf8(fixture.scene.text_state(mounted.scene).content().bytes()).value();
}

inline ComponentId mount_owned(Fixture& fixture, Content content) {
    ComponentId owner;
    fixture.services.mount(Content{[&] {
        auto& build = require_component_build_context();
        owner = build.mount_component<int>(0);
        const auto node = build.root(owner);
        fixture.layout.set_layout(node, layout::BoxLayout{});
        build.on_resource_cleanup(owner, [&fixture, node] { fixture.layout.remove_layout(node); });
        build.mount_slot(owner, content);
    }});
    return owner;
}

inline void labels(Fixture& fixture, std::size_t index, const String& empty, const String& ok, const String& cancel) {
    const auto presentation =
        fixture.services.empty_result().snapshot(fixture.services.empty_result().empties()[index]);
    const auto modal = fixture.services.modal().snapshot(fixture.services.modal().mounted()[index]);
    const auto popup = fixture.services.popconfirm().snapshot(fixture.services.popconfirm().mounted()[index]);
    require(label_text(fixture, presentation.label) == empty && label_text(fixture, modal.ok) == ok &&
                label_text(fixture, modal.cancel) == cancel && label_text(fixture, popup.confirm) == ok &&
                label_text(fixture, popup.cancel) == cancel,
            "real Empty/Modal/Popconfirm labels did not resolve the expected source");
}

inline void defaults() {
    Empty();
    Modal(ModalProps{}, ModalContent{[] { Text(u8"Business body"); }});
    Popconfirm(PopconfirmProps{}.title(u8"Business question"),
               PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Trigger"); }); }});
}
} // namespace ryn_test::i18n_components
