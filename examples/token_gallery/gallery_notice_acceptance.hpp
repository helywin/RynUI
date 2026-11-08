#pragma once
#include "gallery_existing_data_entry_acceptance.hpp"
#include <string_view>

namespace rynui::example {
void accept_notice_loading(const GalleryPageRoute&, ExistingDataEntryActions,
                           const std::function<void(ryn::animation::AnimationDuration)>& advance,
                           const std::function<void(std::string_view)>& capture);
} // namespace rynui::example
