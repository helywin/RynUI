#pragma once
#include "gallery_existing_data_entry_acceptance.hpp"

namespace rynui::example {
void accept_status_data_display(const GalleryPageRoute&, ExistingDataEntryActions,
                                const std::function<void(int)>& resize);
} // namespace rynui::example
