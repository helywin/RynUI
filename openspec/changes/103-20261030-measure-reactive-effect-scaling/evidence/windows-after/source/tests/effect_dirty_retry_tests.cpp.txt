#include "graphics/rounded_effect.hpp"
#include "support/allocation_probe.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void retry_dirty_growth(bool geometry) {
    using namespace ryn::graphics;
    std::size_t failures{};
    for (std::size_t warm = 1; warm <= 16; ++warm) {
        for (std::size_t fail = 0; fail < 3; ++fail) {
            RoundedEffectStore store;
            std::vector<RoundedEffectInstance> values;
            for (std::size_t index = 0; index < 64; ++index) {
                values.push_back(make_shadow_effect({{16.0F + static_cast<float>(index) * 16.0F, 16, 8, 8}, 2},
                                                    {ryn::ShadowKind::outer, {}, 0, 0, ryn::Color::rgba8(10, 20, 30)}));
            }
            const auto ids = store.add_batch(values);
            static_cast<void>(store.compact({0, 0, 2048, 2048}));
            store.clear_dirty_ranges();
            const auto update = [&](std::size_t index) {
                if (geometry) {
                    auto next = store.at(ids[index]).geometry;
                    next.translation.x = 0.25F;
                    return store.update_geometry(ids[index], next);
                }
                auto next = store.at(ids[index]).material;
                next.opacity = 0.5F;
                return store.update_material(ids[index], next);
            };
            for (std::size_t index = 0; index < warm; ++index) {
                require(update(index * 2), "warm update was not applied");
            }
            const auto target = warm * 2;
            const auto before = store.at(ids[target]);
            const std::vector<RoundedEffectInstance> packed_before(store.packed_instances().begin(),
                                                                   store.packed_instances().end());
            const auto dirty = geometry ? store.geometry_dirty_ranges() : store.material_dirty_ranges();
            const std::vector<RoundedEffectInstanceRange> dirty_before(dirty.begin(), dirty.end());
            const auto diagnostics = store.diagnostics();
            bool failed{};
            ryn_test::allocation::begin(fail, 64);
            try {
                static_cast<void>(update(target));
            } catch (const std::bad_alloc&) {
                failed = true;
            } catch (...) {
                ryn_test::allocation::end();
                throw;
            }
            ryn_test::allocation::end();
            if (!failed) {
                continue;
            }
            ++failures;
            require(store.at(ids[target]) == before, "dirty allocation failure published the logical update");
            require(std::ranges::equal(store.packed_instances(), packed_before),
                    "dirty allocation failure published packed data");
            require(std::ranges::equal(geometry ? store.geometry_dirty_ranges() : store.material_dirty_ranges(),
                                       dirty_before),
                    "dirty allocation failure lost an earlier range");
            require(store.diagnostics().material_updates == diagnostics.material_updates &&
                        store.diagnostics().geometry_updates == diagnostics.geometry_updates,
                    "dirty allocation failure published diagnostics");
            require(update(target), "retry treated the failed update as already accepted");
            require(!store.compact({0, 0, 2048, 2048}), "retry unexpectedly rebuilt packed topology");
            const auto retried = geometry ? store.geometry_dirty_ranges() : store.material_dirty_ranges();
            require(retried.size() == warm + 1, "retry did not retain every sparse dirty range");
            for (std::size_t index = 0; index < ids.size(); ++index) {
                require(store.contains(ids[index]) && store.packed_index(ids[index]) == index,
                        "retry replaced an instance identity or packed index");
                if (index != target) {
                    require(store.at(ids[index]) == packed_before[index], "retry changed a neighboring instance");
                }
            }
        }
    }
    require(failures > 0, "dirty probe did not inject a storage growth failure");
    std::cout << "dirty_growth_bad_alloc=" << failures << '\n';
}

} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER)
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
#endif
    try {
        require(argc == 2, "expected material or geometry");
        const std::string_view selected{argv[1]};
        require(selected == "material" || selected == "geometry", "unknown case");
        retry_dirty_growth(selected == "geometry");
    } catch (const std::exception& error) {
        ryn_test::allocation::end();
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
