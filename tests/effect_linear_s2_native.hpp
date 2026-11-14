// Included inside the existing native fixture namespace, only by the S2 target.
struct LinearS2Case {
    std::string name;
    RoundedEffectInstance effect;
    float scale{1};
};

std::vector<LinearS2Case> linear_s2_cases() {
    std::vector<LinearS2Case> result;
    const auto ramp = [](std::vector<float> positions, float alpha = .5F) {
        std::vector<EffectGradientStop> stops;
        for (std::size_t i = 0; i < positions.size(); ++i) {
            stops.push_back({positions[i], i % 2 ? Color{0, 0, 1, alpha} : Color{1, 0, 0, alpha}});
        }
        return EffectGradientRamp{stops};
    };
    const auto add = [&](std::string name, EffectLinearDomain domain, std::vector<float> stops, float scale = 1) {
        auto effect = fill({0, 0, 32 / scale, 24 / scale});
        effect.material.paint = EffectPaint{{ramp(stops), domain}, std::nullopt};
        result.push_back({std::move(name), effect, scale});
    };
    for (int k = 90; k <= 93; ++k) {
        const auto y = std::ldexp(1.0F, -k);
        add("k" + std::to_string(k), {{0, y}, {1, y}}, {0, 1});
    }
    add("scale2-bound", {{0, std::ldexp(1.0F, -90)}, {1, std::ldexp(1.0F, -90)}}, {0, 1}, 2);
    add("joint-pair", {{0, std::ldexp(1.0F, -30)}, {1, std::ldexp(1.0F, -30)}},
        {std::ldexp(1.0F, -64), std::ldexp(1.0F, 60)});
    const auto tiny = std::numeric_limits<float>::denorm_min();
    const auto maximum = std::numeric_limits<float>::max();
    add("mixed-277", {{.5F, 0}, {1.5F, 0}}, {-tiny, maximum});
    add("tiny", {{.5F, 0}, {1.5F, 0}}, {-tiny, tiny});
    add("maximum", {{.5F, 0}, {1.5F, 0}}, {-maximum, maximum});
    add("l1-a", {{-16777211, -16777210}, {-16777203, -16777218}}, {0, 1.0F / 32, 1.0F / 32, 1}, 1.5F);
    add("l1-b", {{-16777210, -16777210}, {-16777202, -16777218}}, {0, 1.0F / 32, 1.0F / 32, 1}, 1.5F);
    add("l2", {{33554432, 33554432}, {33554440, 33554424}}, {-.125F, 0});
    result.back().effect.geometry.shape.rect = {33554432, 33554432, 32, 24};
    result.back().effect.geometry.translation = {-33554432, -33554432};
    add("adjacent", {{-100000000, -100000000}, {-99999992, -100000008}},
        {std::nextafter(-.0625F, -1.0F), std::nextafter(-.0625F, 0.0F)});
    add("duplicates", {{0, 0}, {1, 0}}, {-1, -0.0F, 0, .5F, .5F, 1});
    add("same", {{0, 0}, {1, 0}}, {.5F, .5F, .5F});
    // Actual foreground/underlay routes ff/fs/sf/ss, distinct PM colors.
    const auto fast = result[0].effect.material.paint->foreground;
    const auto slow = result[3].effect.material.paint->foreground;
    for (int mask = 0; mask < 4; ++mask) {
        auto effect = fill({0, 0, 32, 24});
        const auto under_domain = std::get<EffectLinearDomain>((mask & 2 ? slow : fast).domain());
        effect.material.paint = EffectPaint{
            mask & 1 ? slow : fast,
            EffectPaintField{EffectGradientRamp{{0, Color{0, 1, 0, .5F}}, {1, Color{1, 1, 0, .5F}}}, under_domain}};
        result.push_back({"dual-" + std::to_string(mask), effect, 1});
    }
    auto alpha = result[0].effect;
    alpha.material.paint->foreground = {ramp({0, 1}, std::nextafter(1.0F, 0.0F)), EffectLinearDomain{{0, 0}, {1, 0}}};
    alpha.material.paint->underlay = constant(Color{0, 1, 0, .5F});
    result.push_back({"nextdown-alpha", alpha, 1});
    for (const auto coverage_left : {0.0F, .5F, 1.0F}) {
        for (const auto opacity : {0.0F, .5F, 1.0F}) {
            auto effect = result[result.size() - 1].effect;
            effect.geometry.shape.rect = {coverage_left, 0, 32, 24};
            effect.material.opacity = opacity;
            result.push_back(
                {"coverage-" + std::to_string(coverage_left) + "-opacity-" + std::to_string(opacity), effect, 1});
        }
    }
    auto clipped = alpha;
    clipped.geometry.ancestor_clip = EffectClip{1, {0, 0, 20, 20}};
    result.push_back({"hard-clip", clipped, 1});
    return result;
}

std::vector<std::byte> linear_s2_code(const std::string& variant, const SdlSceneRenderer& renderer) {
    const auto suffix = std::string(renderer.shader_format()) == "DXIL" ? ".dxil" : ".spv";
    const auto path = std::filesystem::path(RYNUI_LINEAR_S2_VARIANT_DIRECTORY) / (variant + suffix);
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Missing fixed S2 test fragment: " + path.string());
    const std::vector<char> data{std::istreambuf_iterator<char>{input}, {}};
    std::vector<std::byte> result(data.size());
    std::memcpy(result.data(), data.data(), data.size());
    return result;
}

void linear_s2_native(PlatformState& platform, const std::string& driver, CostClock::time_point started) {
    const auto cases_to_run = linear_s2_cases();
    std::vector<SceneRgbaReadback> auto_frames;
    const std::array variants{"auto",   "wide",   "probe6",      "probe7",      "probe1",      "probe2",     "probe3",
                              "probe4", "probe5", "wide-probe1", "wide-probe2", "wide-probe3", "wide-probe4"};
    require(platform.window_metrics().pixel_width == 32 && platform.window_metrics().pixel_height == 24,
            "S2 requires an actual 32x24 hidden viewport");
    // Fixed fragments below affect only the root pass. The separate composition
    // helper uses the normal composition shader, without a forced-wide seam.
    for (const std::string variant : variants) {
        require(CostClock::now() - started < std::chrono::seconds(55), "S2 native reached unchanged 55-second budget");
        SdlSceneRenderer renderer(platform);
        require(renderer.gpu_driver() == driver, "S2 variant backend changed");
        require(std::string(renderer.shader_format()) == (driver == "direct3d12" ? "DXIL" : "SPIR-V"),
                "S2 actual variant shader format changed");
        require(!renderer.capabilities().effect_paints && !renderer.capabilities().effect_linear_fields,
                "S2 test enabled production paint flags");
        require(renderer.begin_pending_paint_validation_for_test(), "S2 pending test scope");
        const auto code = linear_s2_code(variant, renderer);
        require(renderer.use_linear_test_fragment_for_test(code), "S2 fresh-renderer fixed fragment seam");
        renderer.set_clear_color(Color{0, 0, 0, 0});
        for (std::size_t i = 0; i < cases_to_run.size(); ++i) {
            const auto& test = cases_to_run[i];
            Fixture f(renderer, platform, test.scale);
            static_cast<void>(f.effects.add(test.effect));
            const auto frame = f.read("s2-" + variant + "-" + test.name);
            require(f.resources.effects().instance_count() == 1, "S2 fixture did not keep one real draw");
            const bool normal = variant == "auto" || variant == "wide";
            if (variant == "auto") {
                auto_frames.push_back(frame);
            } else if (variant == "wide") {
                require(frame.width == auto_frames[i].width && frame.height == auto_frames[i].height &&
                            frame.row_pitch == auto_frames[i].row_pitch && frame.pixels == auto_frames[i].pixels,
                        test.name + ": auto/force-wide normal RGBA bytes differ");
            }
            for (const auto xy : {std::array{0, 0}, std::array{7, 5}, std::array{16, 16}, std::array{24, 20}}) {
                const runtime::Point point{float(xy[0]) + .5F, float(xy[1]) + .5F};
                const auto offset = std::size_t(xy[1]) * frame.row_pitch + std::size_t(xy[0]) * 4;
                const auto& base = f.resources.effects().instances()[0];
                const auto& bounds = base.clip_bounds;
                if (point.x < bounds[0] || point.y < bounds[1] || point.x >= bounds[2] || point.y >= bounds[3]) {
                    pixel(frame, xy[0], xy[1], {0, 0, 0, 0}, test.name + ": probe preserves hard clip", 0);
                    continue; // Original hard clip remains before the byte probe.
                }
                if (normal) {
                    const auto coverage = rounded_effect_gpu_coverage_reference(point, base);
                    pixel(frame, xy[0], xy[1], source_linear_pixel(test.effect, test.scale, point, coverage),
                          test.name + ": independent source paint", 2);
                    continue;
                }
                std::uint32_t expected{};
                const bool forced = variant.starts_with("wide-");
                const int mode = variant.back() - '0';
                const auto& paint = *test.effect.material.paint;
                const auto source =
                    effect_source_oracle::coordinate(std::get<EffectLinearDomain>(paint.foreground.domain()),
                                                     test.effect.geometry.translation, test.scale, point);
                std::vector<float> positions;
                for (const auto& stop : paint.foreground.ramp().stops()) {
                    positions.push_back(stop.position);
                }
                const auto selection = effect_linear_s2_tests::source_selection(source, positions);
                if (mode == 6) {
                    expected = 0x7e39ad;
                } else if (mode == 7) {
                    expected = 0xc1;
                } else if (mode == 5) {
                    const auto route = [&](const EffectPaintField& field) {
                        const auto raw =
                            effect_linear_s2_tests::inputs(std::get<EffectLinearDomain>(field.domain()),
                                                           test.effect.geometry.translation, test.scale, point);
                        ryn::detail::effect_linear_exact::Selection unused;
                        ryn::detail::effect_linear_exact::Failure reason;
                        const auto stops = field.ramp().stops();
                        static_cast<void>(ryn::detail::effect_linear_exact::try_selection192(
                            raw, static_cast<std::uint32_t>(stops.size()), [&](auto j) { return stops[j].position; },
                            unused, reason));
                        return static_cast<std::uint32_t>(reason);
                    };
                    expected = route(paint.foreground) | ((paint.underlay ? route(*paint.underlay) : 255u) << 8);
                    if (xy == std::array{0, 0} && i < 4) {
                        const std::array expected_route{0u, 5u, 4u, 2u};
                        require((expected & 255u) == expected_route[i], "S2 CPU boundary route changed");
                    }
                } else {
                    expected = mode <= 2 ? selection.next : std::bit_cast<std::uint32_t>(selection.fraction);
                    expected = mode == 2 || mode == 4 ? expected >> 24 : expected & 0xffffff;
                }
                for (unsigned byte = 0; byte < 4; ++byte) {
                    const auto target = byte == 3 ? 255u : ((expected >> (8 * byte)) & 255u);
                    require(std::to_integer<unsigned>(frame.pixels[offset + byte]) == target,
                            test.name + ": " + variant + (forced ? " forced" : " auto") + " exact byte probe differs");
                }
            }
        }
        renderer.end_pending_paint_validation_for_test();
        require(!renderer.capabilities().effect_paints, "S2 scope exit enabled product flags");
    }
}

void linear_s2_composition_crop(PlatformState& platform, const std::string& driver, CostClock::time_point started) {
    SdlSceneRenderer renderer(platform);
    require(renderer.gpu_driver() == driver, "S2 composition backend changed");
    require(std::string(renderer.shader_format()) == (driver == "direct3d12" ? "DXIL" : "SPIR-V"),
            "S2 composition shader format changed");
    const auto product_flags_off = [&] {
        const auto caps = renderer.capabilities();
        return !caps.effect_paints && !caps.effect_linear_fields && !caps.effect_conic_fields &&
               !caps.effect_underlays && !caps.effect_arc_caps;
    };
    require(product_flags_off(), "S2 composition enabled production paint");
    require(renderer.begin_pending_paint_validation_for_test(), "S2 composition pending scope");
    renderer.set_clear_color(Color{0, 0, 0, 0});
    std::uint64_t previous_readback{};
    // These are simulated scene metrics over the actual fixed hidden window,
    // not evidence of OS/monitor DPI changes. No root test fragment is installed.
    for (const float scale : {1.0F, 1.5F, 2.0F}) {
        for (const bool underlay : {false, true}) {
            require(CostClock::now() - started < std::chrono::seconds(55), "S2 composition exhausted 55-second budget");
            Fixture f(renderer, platform, scale);
            auto sentinel = fill({0, 0, 32 / scale, 24 / scale});
            sentinel.material.paint = EffectPaint{constant(Color{1, 0, 1}), std::nullopt};
            static_cast<void>(f.effects.add(sentinel));
            auto effect = fill({0, 0, 32 / scale, 24 / scale});
            effect.geometry.translation = {.5F, -.25F};
            effect.material.opacity = .75F;
            // Original raw logical domain stays fixed as scale changes. At
            // scale 1, GLOBAL x=12.5 is exactly the right-continuous duplicate.
            effect.material.paint = EffectPaint{{EffectGradientRamp{{0, Color{1, 0, 0, .5F}},
                                                                    {.5F, Color{1, 0, 0, .5F}},
                                                                    {.5F, Color{0, 0, 1, .5F}},
                                                                    {1, Color{0, 0, 1, .5F}}},
                                                 EffectLinearDomain{{6, 3}, {18, 3}}},
                                                std::nullopt};
            if (underlay) {
                const auto y = std::ldexp(1.0F, -93);
                effect.material.paint->underlay = {
                    EffectGradientRamp{{0, Color{0, 1, 0, .5F}}, {1, Color{1, 1, 0, .5F}}},
                    EffectLinearDomain{{0, y}, {32, y}}};
            }
            static_cast<void>(f.effects.add(effect));
            f.order.append_command({SceneDrawKind::rounded_effect, 1, 1, invalid_glyph_atlas_page});
            const runtime::RoundedContentClip shape{{9.25F / scale, 6.5F / scale, 18.5F / scale, 14 / scale}, 0};
            auto program = std::make_shared<SceneCompositionProgram>();
            program->scene_revision = f.order.revision();
            // Hard clip cuts the left AA fringe; the other three crop edges
            // retain the real one-pixel draw guard outside the mask.
            program->requests.push_back({{1, 1}, shape, {10 / scale, 0, 22 / scale, 24 / scale}});
            program->operations = {{SceneCompositionOpKind::BeginMaskedGroup, 0, 0, 0},
                                   {SceneCompositionOpKind::DrawRange, 0, 1, 0},
                                   {SceneCompositionOpKind::EndMaskedGroup, 0, 0, 0}};
            f.order.set_composition(program);
            const auto plan = pack_composition(f.order, f.metrics, renderer.capabilities());
            const CompositionCrop expected_crop{10, 5, 19, 17};
            require(plan.requests.size() == 1 && plan.requests[0].crop == expected_crop && plan.targets.size() == 2 &&
                        plan.requests[0].target == 1 && plan.targets[1] == CompositionTargetSpec{19, 17} &&
                        expected_crop.width < f.metrics.pixel_width && expected_crop.height < f.metrics.pixel_height,
                    "S2 composition did not request the exact nonzero smaller target");
            require(f.resources.synchronize({nullptr, f.atlas, f.glyphs, &f.effects, f.metrics, nullptr, &f.order}),
                    std::string{"S2 composition common upload: "} + renderer.last_error());
            const auto attachment = f.resources.attach(f.order);
            const auto& composition = attachment.composition();
            require(composition && composition->uploaded && composition->targets.size() == 2 &&
                        composition->targets[1] && composition->plan.requests[0].crop == expected_crop &&
                        composition->plan.targets[1] == CompositionTargetSpec{19, 17} &&
                        composition->owner == renderer.owner_id() && composition->epoch == renderer.device_epoch(),
                    "S2 composition upload has no actual owned cropped target");
            const auto& tables = f.resources.effects().paint_tables();
            const auto binding = f.resources.effects().draw_binding(1, 1);
            require(f.resources.effects().instance_count() == 2 && binding.base_byte_offset == 112 &&
                        binding.link_byte_offset == 16 && tables.links.size() == 2 &&
                        tables.links[1].foreground_field == 1 && tables.fields[1].first_stop == 2 &&
                        tables.fields[1].raw_domain_bits ==
                            std::bit_cast<std::array<std::uint32_t, 4>>(std::array{6.0F, 3.0F, 18.0F, 3.0F}) &&
                        tables.fields[1].affine_bits ==
                            std::bit_cast<std::array<std::uint32_t, 4>>(std::array{.5F, -.25F, scale, 0.0F}),
                    "S2 composition lost nonzero first/stop or original raw affine inputs");
            require(renderer.attach_scene(attachment), "S2 composition attach");
            const auto before = renderer.counters();
            SceneRgbaReadback frame;
            require(renderer.read_frame_rgba(frame),
                    std::string{"S2 composition actual RGBA readback: "} + renderer.last_error());
            require(frame.width == 32 && frame.height == 24 && frame.row_pitch == frame.width * 4 &&
                        frame.pixels.size() == std::size_t(frame.row_pitch) * frame.height &&
                        frame.evidence.fence_completed && frame.evidence.readback_id > previous_readback &&
                        frame.evidence.fence_id != 0 && frame.evidence.scene_revision == f.order.revision() &&
                        frame.evidence.scene_metrics == f.metrics &&
                        frame.evidence.renderer_owner == renderer.owner_id() &&
                        frame.evidence.device_epoch == renderer.device_epoch(),
                    "S2 composition readback has no owned current scene/fence/metrics receipt");
            previous_readback = frame.evidence.readback_id;
            const auto after = renderer.counters();
            require(after.render_passes == before.render_passes + 3 && after.effect_draws == before.effect_draws + 1 &&
                        after.effect_instances == before.effect_instances + 1,
                    "S2 composition did not execute Begin/DrawRange(first=1)/End passes");
            for (const auto xy : {std::array{11, 8}, std::array{12, 12}, std::array{13, 12}, std::array{18, 12},
                                  std::array{19, 12}, std::array{24, 18}, std::array{25, 12}, std::array{26, 12}}) {
                const runtime::Point point{float(xy[0]) + .5F, float(xy[1]) + .5F};
                // Classify fixture inputs only; expected RGBA below is obtained
                // independently from the source oracle, never packed reference.
                const auto try_field = [&](const EffectPaintField& field) {
                    const auto raw = effect_linear_s2_tests::inputs(std::get<EffectLinearDomain>(field.domain()),
                                                                    effect.geometry.translation, scale, point);
                    const auto stops = field.ramp().stops();
                    effect_linear_exact::Selection unused;
                    effect_linear_exact::Failure reason;
                    return effect_linear_exact::try_selection192(
                        raw, static_cast<std::uint32_t>(stops.size()), [&](auto j) { return stops[j].position; },
                        unused, reason);
                };
                require(try_field(effect.material.paint->foreground), "S2 composition foreground fixture is not fast");
                require(!underlay || !try_field(*effect.material.paint->underlay),
                        "S2 composition underlay fixture did not require wide fallback");
                const auto expected = source_linear_pixel(effect, scale, point);
                if (scale == 1 && !underlay && xy == std::array{12, 12}) {
                    require(expected == std::array<float, 4>{0, 0, .375F, .375F},
                            "S2 composition source duplicate is not right-continuous blue");
                }
                pixel(frame, xy[0], xy[1], expected, "S2 actual composition: independent GLOBAL source pixel center",
                      2);
            }
            // Check every exterior pixel plus the crop's three transparent guard
            // edges and the hard-clipped left fringe. No packed/CPU renderer.
            for (int y = 0; y < 24; ++y) {
                for (int x = 0; x < 32; ++x) {
                    if (x < 10 || x >= 28 || y < 6 || y >= 21) {
                        pixel(frame, x, y, {0, 0, 0, 0}, "S2 composition crop guard/hard clip", 0);
                    }
                }
            }
            std::cout << "linear_s2_composition scene_scale=" << scale
                      << " metrics=simulated platform_scale=" << frame.evidence.metrics.display_scale
                      << " underlay=" << underlay
                      << " crop=10,5,19,17 first=1 first_stop=2 shader=normal-composition forced_wide=unavailable"
                      << " readback=" << frame.evidence.readback_id << " fence=" << frame.evidence.fence_id << '\n';
            ++cases;
        }
    }
    renderer.end_pending_paint_validation_for_test();
    require(product_flags_off(), "S2 composition left production paint enabled");
}
