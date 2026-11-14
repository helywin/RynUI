#include "gallery_demo_hash.hpp"
#include "gallery_demo_json.hpp"
#include "gallery_demo_protocol.hpp"
#include "demo_differential_control_geometry.hpp"
#include "icons/bundled_icon_catalog.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "text/text_scene_service.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <cstdlib>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
#include <stdexcept>
#include <sstream>

namespace {
using namespace ryn;
using namespace ryn::detail;
using namespace ryn::graphics;
using namespace rynui::example;
using Json = GalleryDemoJson;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

Json integer(std::uint64_t value) {
    require(value <= static_cast<std::uint64_t>(INT64_MAX), "JSON integer overflow");
    return static_cast<std::int64_t>(value);
}

Json process_ancestry(std::int64_t pid) {
    Json::Array result;
    for (int depth = 0; depth < 32 && pid > 0; ++depth) {
        result.emplace_back(pid);
#if defined(_WIN32)
        break;
#else
        std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
        std::string line;
        require(bool(std::getline(stat, line)), "actual process ancestry unavailable");
        const auto end = line.rfind(')');
        require(end != std::string::npos, "actual process ancestry malformed");
        std::istringstream fields(line.substr(end + 1));
        char state{};
        std::int64_t parent{};
        require(bool(fields >> state >> parent), "actual process parent unavailable");
        if (parent == pid) {
            break;
        }
        pid = parent;
#endif
    }
    return result;
}

std::filesystem::path owned_path(const std::filesystem::path& value) {
    const auto root = std::filesystem::weakly_canonical(std::filesystem::path{RYNUI_CONTROL_SOURCE_ROOT} / "out");
    const auto path = std::filesystem::weakly_canonical(value);
    const auto relative = path.lexically_relative(root);
    require(!relative.empty() && relative != "." && *relative.begin() != ".." && !relative.is_absolute(),
            "control file must stay strictly under this checkout out");
    return path;
}

Json read_json(const std::filesystem::path& path) {
    require(std::filesystem::file_size(path) <= 16 * 1024 * 1024, "control JSON exceeds 16 MiB");
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::badbit | std::ios::failbit);
    std::string text(static_cast<std::size_t>(std::filesystem::file_size(path)), '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    return Json::parse(text);
}

void write(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    require(!std::filesystem::exists(path), "control output already exists");
    std::ofstream output(path, std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void write_json(const std::filesystem::path& path, const Json& value) {
    const auto bytes = value.dump() + '\n';
    write(path, std::as_bytes(std::span{bytes.data(), bytes.size()}));
}

runtime::Rect rect(const Json& value) {
    return {static_cast<float>(value.at("x").number()), static_cast<float>(value.at("y").number()),
            static_cast<float>(value.at("width").number()), static_cast<float>(value.at("height").number())};
}

Json rectangle(runtime::Rect value) {
    return Json::Object{{"x", value.x}, {"y", value.y}, {"width", value.width}, {"height", value.height}};
}

std::array<float, 4> rgba(const Json& bytes, float alpha = 1) {
    require(bytes.array().size() == 3, "control RGB extent");
    std::array<float, 4> result{0, 0, 0, alpha};
    for (std::size_t index = 0; index < 3; ++index) {
        const auto value = bytes.array()[index].number();
        require(value >= 0 && value <= 255, "control RGB channel range");
        result[index] = static_cast<float>(value / 255);
    }
    return result;
}

std::string utf8(char32_t value) {
    std::string result;
    if (value <= 0x7F) {
        result.push_back(static_cast<char>(value));
    } else if (value <= 0x7FF) {
        result.push_back(static_cast<char>(0xC0 | (value >> 6)));
        result.push_back(static_cast<char>(0x80 | (value & 63)));
    } else if (value <= 0xFFFF) {
        result.push_back(static_cast<char>(0xE0 | (value >> 12)));
        result.push_back(static_cast<char>(0x80 | ((value >> 6) & 63)));
        result.push_back(static_cast<char>(0x80 | (value & 63)));
    } else {
        result.push_back(static_cast<char>(0xF0 | (value >> 18)));
        result.push_back(static_cast<char>(0x80 | ((value >> 12) & 63)));
        result.push_back(static_cast<char>(0x80 | ((value >> 6) & 63)));
        result.push_back(static_cast<char>(0x80 | (value & 63)));
    }
    return result;
}

IconName icon_name(std::string_view name) {
    for (std::size_t index = 0; index < bundled_icon_count; ++index) {
        const auto icon = static_cast<IconName>(index);
        if (bundled_icon_entry(icon).name == name) {
            return icon;
        }
    }
    throw std::invalid_argument("unknown formal control Icon");
}

struct Board final {
    SdlSceneRenderer& renderer;
    font::FontRuntime& fonts;
    runtime::FrameRequestState frames;
    TextSceneService text;
    runtime::NodeStore nodes;
    QuadInstanceStore quads;
    RoundedEffectStore effects;

    struct PendingDraw final {
        SceneDrawCommand command;
        RoundedEffectId effect;
    };

    std::vector<PendingDraw> pending;
    OrderedScene order;
    SceneResources resources;
    SceneDeviceMetrics metrics;
    font::FontRasterPolicy raster_policy;
    Json::Array observations;

    Board(SdlSceneRenderer& renderer, font::FontRuntime& fonts, text::TextEngine& engine, SceneDeviceMetrics metrics)
        : renderer(renderer), fonts(fonts), text(fonts, engine, frames), resources(renderer), metrics(metrics) {}

    std::uint32_t fill(runtime::Rect area, std::array<float, 4> color, float radius = 0, float opacity = 1) {
        const auto primitive =
            quads.add(nodes.create_root(), {{area.x, area.y, area.width, area.height}, color, opacity, radius});
        pending.push_back({{SceneDrawKind::quad, primitive.instance_index, 1}, {}});
        return primitive.instance_index;
    }

    void actual_color(Json::Object& source, std::array<float, 4> color, float opacity, bool second = false) {
        Json::Array channels;
        for (std::size_t index = 0; index < 3; ++index) {
            require(std::isfinite(color[index]) && color[index] >= 0 && color[index] <= 1,
                    "actual control material is outside encoded SDR");
            channels.emplace_back(static_cast<std::int64_t>(std::lround(color[index] * 255)));
        }
        source.insert_or_assign(second ? "second_foreground" : "foreground", std::move(channels));
        source.insert_or_assign(second ? "second_alpha" : "alpha", color[3] * opacity);
        source.insert_or_assign(second ? "second_encoded_rgba" : "encoded_rgba",
                                Json::Array{color[0], color[1], color[2], color[3]});
        source.insert_or_assign(second ? "second_opacity" : "opacity", opacity);
        source.insert_or_assign(second ? "second_source_alpha" : "source_alpha", color[3]);
        source.insert_or_assign(second ? "second_paint_opacity" : "paint_opacity", opacity);
    }

    runtime::Rect actual_quad(Json::Object& source, std::uint32_t index, bool second = false) {
        const auto& instance = quads.at(index);
        actual_color(source, instance.color, instance.opacity, second);
        source.insert_or_assign(second ? "second_primitive_index" : "primitive_index", integer(index));
        source.insert_or_assign(second ? "second_translation" : "translation",
                                Json::Array{instance.translation[0], instance.translation[1]});
        source.insert_or_assign("clip_kind", "viewport");
        if (!second) {
            source.insert_or_assign("radius", instance.corner_radius);
        }
        return {instance.bounds[0], instance.bounds[1], instance.bounds[2], instance.bounds[3]};
    }

    runtime::Rect actual_effect(Json::Object& source, RoundedEffectId id) {
        const auto& instance = effects.at(id);
        const auto& g = instance.geometry;
        const auto& m = instance.material;
        actual_color(source, {m.color.red(), m.color.green(), m.color.blue(), m.color.alpha()}, m.opacity);
        source.insert_or_assign("radius", g.shape.radius);
        source.insert_or_assign("primitive",
                                Json::Object{{"index", integer(id.index)}, {"generation", integer(id.generation)}});
        source.insert_or_assign("effect_kind", static_cast<std::int64_t>(g.kind));
        source.insert_or_assign("offset", Json::Array{g.offset.x, g.offset.y});
        source.insert_or_assign("blur", g.blur);
        source.insert_or_assign("spread", g.spread);
        source.insert_or_assign("outline_width", g.outline_width);
        source.insert_or_assign("outline_offset", g.outline_offset);
        source.insert_or_assign("translation", Json::Array{g.translation.x, g.translation.y});
        if (g.ancestor_clip) {
            source.insert_or_assign("clip_rect", rectangle(g.ancestor_clip->bounds));
        }
        return g.shape.rect;
    }

    std::vector<font::FontIdentity> chain(const Json& files, int size, int weight, bool replace) {
        std::vector<font::FontIdentity> result;
        for (const auto& file : files.array()) {
            if (file.at("weight").integer() != weight || file.at("identity").string() == "noto-emoji") {
                continue;
            }
            const auto path = owned_path(file.at("path").string());
            require(std::filesystem::file_size(path) <= 32 * 1024 * 1024, "control font exceeds 32 MiB");
            require(gallery_demo_file_sha256(path) == file.at("sha256").string(), "control font bytes differ");
            if (replace && file.at("identity").string() == "noto-sans-cjk-sc") {
                continue;
            }
            auto loaded = fonts.load_font_file(
                path, static_cast<long>(file.at("face_index").integer()),
                font::FontRasterConfig{static_cast<std::uint32_t>(size), metrics.display_scale, raster_policy});
            require(bool(loaded), "control pinned font load failed");
            result.push_back(loaded.font);
        }
        require(!result.empty(), "control font chain is empty");
        return result;
    }

    void glyph(const Json& cell, const Json& files, Json::Object& source) {
        const auto area = rect(cell.at("rect"));
        const auto size = static_cast<int>(cell.at("font_size").integer());
        const auto weight = static_cast<int>(cell.at("font_weight").integer());
        const auto is_icon = cell.at("kind").string() == "icon";
        auto selected = chain(files, size, weight, cell.contains("replace_font"));
        const auto name = cell.contains("render_icon") ? cell.at("render_icon").string() : cell.at("content").string();
        if (!is_icon) {
            const bool black_panel = cell.at("background").array()[0].integer() == 0;
            const std::array<float, 4> border_color =
                black_panel ? std::array{1.0F, 1.0F, 1.0F, 1.0F} : std::array{0.0F, 0.0F, 0.0F, 1.0F};
            fill({area.x - 4, area.y - 4, area.width + 8, 1}, border_color);
            fill({area.x - 4, area.y + area.height + 3, area.width + 8, 1}, border_color);
            fill({area.x - 4, area.y - 4, 1, area.height + 8}, border_color);
            fill({area.x + area.width + 3, area.y - 4, 1, area.height + 8}, border_color);
        }
        std::vector<std::pair<std::string, font::FontIdentity>> layers;
        if (is_icon) {
            const auto icon = icon_name(name);
            const auto actual = text.icon_font(IconSource{icon}, selected.front(), static_cast<std::uint32_t>(size));
            for (const auto layer : bundled_layers(icon)) {
                layers.emplace_back(utf8(layer.codepoint), actual);
            }
        } else {
            layers.emplace_back(name, font::FontIdentity{});
        }
        Json::Array boxes;
        Json::Array icon_layers;
        std::string actual_hash;
        double advance{};
        for (const auto& [content, icon_font] : layers) {
            const auto node = nodes.create_root();
            auto fallback = is_icon ? std::vector{icon_font} : selected;
            const auto id = text.create(node, String::from_utf8(content).value(), std::move(fallback), size,
                                        {is_icon ? static_cast<float>(size) : 32.0F, area.width});
            auto clip = area;
            const auto color = rgba(cell.at("foreground"));
            static_cast<void>(text.set_color(id, color));
            GlyphPlacement placement{{area.x, area.y}, {area.width, area.height}, clip, {}, color};
            require(text.synchronize(id, placement), "control formal Text/Icon synchronization failed");
            const auto& shaped = text.text_state(id).shaped();
            require(!shaped.glyphs.empty() && shaped.replacement_count == 0, "control font shaping is unavailable");
            const auto& measurement = text.text_state(id).measurement();
            advance = measurement.width;
            source.insert_or_assign("glyph_id", integer(shaped.glyphs.front().glyph_id));
            const auto actual_source = fonts.source(shaped.glyphs.front().font);
            require(actual_source.has_value(), "actual control font source unavailable");
            actual_hash = gallery_demo_sha256(
                {reinterpret_cast<const unsigned char*>(actual_source->bytes.data()), actual_source->bytes.size()});
            const auto font_metrics = fonts.metrics(shaped.glyphs.front().font);
            require(bool(font_metrics), "actual control font metrics unavailable");
            source.insert_or_assign("font_size", integer(font_metrics.metrics.logical_pixel_size));
            const auto declared = std::ranges::find_if(
                files.array(), [&](const auto& file) { return file.at("sha256").string() == actual_hash; });
            require(is_icon || declared != files.array().end(), "actual control font has no pinned identity");
            source.insert_or_assign("font_weight", is_icon ? Json{400} : declared->at("weight"));
            source.insert_or_assign("scene_identity",
                                    Json::Object{{"index", integer(id.index)}, {"generation", integer(id.generation)}});
            const auto& policy = actual_source->raster_policy;
            source.insert_or_assign("raster_policy",
                                    Json::Object{{"antialias", policy.antialias},
                                                 {"hinting", policy.hinting},
                                                 {"hint_style", static_cast<std::int64_t>(policy.hint_style)},
                                                 {"subpixel_order", static_cast<std::int64_t>(policy.subpixel_order)},
                                                 {"lcd_filter", static_cast<std::int64_t>(policy.lcd_filter)},
                                                 {"embedded_bitmap", policy.embedded_bitmap}});
            const auto& primitive = text.primitive(id);
            require(primitive.coverage.size() == primitive.instances.count, "control glyph coverage unavailable");
            for (std::uint32_t index = 0; index < primitive.instances.count; ++index) {
                const auto& instance = text.glyph_scene().instances().at(primitive.instances.first + index);
                actual_color(source, instance.color, instance.translation_opacity[2]);
                const auto padding = glyph_atlas_padding / metrics.display_scale;
                boxes.push_back(
                    rectangle({instance.position_size[0] + padding, instance.position_size[1] + padding,
                               instance.position_size[2] - 2 * padding, instance.position_size[3] - 2 * padding}));
            }
            if (cell.contains("clip_glyph_right_px") && !boxes.empty()) {
                double right{};
                for (const auto& box : boxes) {
                    right = std::max(right, box.at("x").number() + box.at("width").number());
                }
                clip.width = static_cast<float>(right - area.x - cell.at("clip_glyph_right_px").number());
                placement.clip_pixels = clip;
                require(text.synchronize(id, placement), "control clip input failed");
            }
            // Fault changes the actual draw stream before publication; retained
            // font/coverage metadata is deliberately still observable.
            if (!cell.contains("omit_draw")) {
                for (const auto range : primitive.draw_ranges) {
                    pending.push_back(
                        {{SceneDrawKind::glyph, range.instances.first, range.instances.count, range.atlas_page}, {}});
                }
            }
            if (is_icon) {
                require(shaped.scalars.size() == 1 && shaped.glyphs.size() == 1, "formal layer must shape one glyph");
                Json::Array instances;
                for (const auto range : primitive.draw_ranges) {
                    for (std::uint32_t i = 0; i < range.instances.count; ++i) {
                        const auto index = range.instances.first + i;
                        const auto& instance = text.glyph_scene().instances().at(index);
                        instances.emplace_back(Json::Object{
                            {"instance_index", integer(index)},
                            {"atlas_page", integer(range.atlas_page)},
                            {"atlas_uv", Json::Array{instance.uv_rect[0], instance.uv_rect[1], instance.uv_rect[2],
                                                     instance.uv_rect[3]}},
                            {"clip", Json::Array{instance.clip_bounds[0], instance.clip_bounds[1],
                                                 instance.clip_bounds[2], instance.clip_bounds[3]}},
                            {"bounds", Json::Array{instance.position_size[0], instance.position_size[1],
                                                   instance.position_size[2], instance.position_size[3]}},
                            {"translation",
                             Json::Array{instance.translation_opacity[0], instance.translation_opacity[1]}},
                            {"paint_opacity", instance.translation_opacity[2]},
                            {"color",
                             Json::Array{instance.color[0], instance.color[1], instance.color[2], instance.color[3]}}});
                    }
                }
                const auto declared = bundled_layers(icon_name(name))[icon_layers.size()];
                icon_layers.emplace_back(
                    Json::Object{{"layer_ordinal", integer(icon_layers.size())},
                                 {"codepoint", integer(shaped.scalars.front().value)},
                                 {"role", declared.secondary ? "secondary" : "primary"},
                                 {"layer_opacity", text.text_state(id).material().opacity},
                                 {"glyph_id", integer(shaped.glyphs.front().glyph_id)},
                                 {"font_sha256", actual_hash},
                                 {"font_source", actual_source->family == "RynUI Ant Icons" ? "bundled-icon" : "other"},
                                 {"face_index", static_cast<std::int64_t>(actual_source->face_index)},
                                 {"scene_identity",
                                  Json::Object{{"index", integer(id.index)}, {"generation", integer(id.generation)}}},
                                 {"ink_instances", std::move(instances)}});
            }
        }
        source.emplace("actual_font_sha256", actual_hash);
        source.emplace("content", name);
        source.emplace("advance", advance);
        source.emplace("ink_boxes", std::move(boxes));
        if (is_icon) {
            source.emplace("icon_layers", std::move(icon_layers));
        }
    }

    Json draw_commands() const {
        Json::Array commands;
        for (const auto& command : order.commands()) {
            commands.emplace_back(Json::Object{{"kind", command.kind == SceneDrawKind::glyph  ? "glyph"
                                                        : command.kind == SceneDrawKind::quad ? "quad"
                                                                                              : "rounded_effect"},
                                               {"first_instance", integer(command.first_instance)},
                                               {"instance_count", integer(command.instance_count)},
                                               {"atlas_page", integer(command.atlas_page)}});
        }
        return commands;
    }

    void bind_source(Json::Object& source) const {
        for (const auto& [key, kind] : {std::pair{"primitive_index", SceneDrawKind::quad},
                                        std::pair{"second_primitive_index", SceneDrawKind::quad},
                                        std::pair{"packed_index", SceneDrawKind::rounded_effect}}) {
            if (key == std::string_view{"packed_index"} && source.contains("primitive")) {
                const auto& value = source.at("primitive");
                const RoundedEffectId id{static_cast<std::uint32_t>(value.at("index").integer()),
                                         static_cast<std::uint32_t>(value.at("generation").integer())};
                const auto index = effects.packed_index(id);
                require(index.has_value(), "control effect source missing after compaction");
                source.insert_or_assign("packed_index", integer(*index));
            }
            if (!source.contains(key)) {
                continue;
            }
            const auto index = source.at(key).integer();
            Json::Array membership;
            for (std::size_t i = 0; i < order.commands().size(); ++i) {
                const auto& command = order.commands()[i];
                if (command.kind == kind && index >= command.first_instance &&
                    index < std::int64_t{command.first_instance} + command.instance_count) {
                    membership.emplace_back(integer(i));
                }
            }
            source.insert_or_assign(std::string{key} + "_commands", std::move(membership));
        }
        if (source.contains("quad_parts")) {
            for (auto& part : std::get<Json::Array>(source.at("quad_parts").value)) {
                bind_source(std::get<Json::Object>(part.value));
            }
        }
        if (source.contains("occluding_fill")) {
            bind_source(std::get<Json::Object>(source.at("occluding_fill").value));
        }
    }

    void build(const Json& request) {
        for (const auto& panel : request.at("panels").array()) {
            fill(rect(panel), rgba(panel.at("rgb")));
        }
        require(request.at("cells").array().size() <= 384, "control cell count exceeds 384");
        for (const auto& cell : request.at("cells").array()) {
            const auto area = rect(cell.at("rect"));
            auto observed_area = area;
            const auto kind = cell.at("kind").string();
            const auto& rgb = cell.contains("rgb") ? cell.at("rgb") : cell.at("foreground");
            const auto alpha =
                cell.contains("source_alpha") ? static_cast<float>(cell.at("source_alpha").number()) : 1.0F;
            const auto opacity =
                cell.contains("paint_opacity") ? static_cast<float>(cell.at("paint_opacity").number()) : 1.0F;
            Json::Object source{{"foreground", rgb}, {"alpha", alpha}};
            if (cell.contains("radius")) {
                source.emplace("radius", cell.at("radius"));
            }
            if (kind == "text" || kind == "icon") {
                glyph(cell, request.at("font_files"), source);
            } else if (kind == "shadow") {
                const auto& shadow = cell.at("shadow");
                source["foreground"] = shadow.at("rgb");
                source["alpha"] = shadow.at("alpha");
                const auto c = rgba(shadow.at("rgb"), static_cast<float>(shadow.at("alpha").number()));
                const auto effect =
                    effects.add(make_shadow_effect({area, static_cast<float>(cell.at("radius").number())},
                                                   {ShadowKind::outer,
                                                    {static_cast<float>(shadow.at("offset").array()[0].number()),
                                                     static_cast<float>(shadow.at("offset").array()[1].number())},
                                                    static_cast<float>(shadow.at("blur").number()),
                                                    static_cast<float>(shadow.at("spread").number()),
                                                    Color{c[0], c[1], c[2], c[3]}}));
                pending.push_back({{}, effect});
                observed_area = actual_effect(source, effect);
                const auto& occlusion = cell.at("occluding_fill");
                Json::Object fill_source;
                static_cast<void>(actual_quad(
                    fill_source,
                    fill(area, rgba(occlusion.at("rgb"), static_cast<float>(occlusion.at("alpha").number())),
                         static_cast<float>(cell.at("radius").number()))));
                source.emplace("occluding_fill", std::move(fill_source));
                if (cell.contains("reverse_shadow_order")) {
                    std::iter_swap(pending.end() - 1, pending.end() - 2);
                }
            } else if (kind == "border") {
                const auto color = rgba(rgb);
                const auto radius = static_cast<float>(cell.at("radius").number());
                if (radius == 0) {
                    Json::Array parts;
                    const auto edges = rynui::test::square_border(area);
                    for (std::size_t i = 0; i < edges.size(); ++i) {
                        if (i == 3 && cell.contains("omit_right_edge")) {
                            continue;
                        }
                        Json::Object part;
                        const auto actual = actual_quad(part, fill(edges[i], color));
                        part.emplace("rect", rectangle(actual));
                        part.emplace("edge", integer(i));
                        parts.emplace_back(std::move(part));
                    }
                    actual_color(source, color, 1);
                    source.emplace("border_width", 1);
                    source.emplace("quad_parts", std::move(parts));
                } else {
                    const auto effect = effects.add(
                        rynui::test::inside_border(area, radius, Color{color[0], color[1], color[2], color[3]}));
                    pending.push_back({{}, effect});
                    source.emplace("actual_shape", rectangle(actual_effect(source, effect)));
                    source.emplace("actual_shape_radius", effects.at(effect).geometry.shape.radius);
                    const auto canonical = rynui::test::border_box(effects.at(effect));
                    observed_area = canonical.rect;
                    source["radius"] = canonical.radius;
                }
            } else if (kind == "clip") {
                auto clip = area;
                clip.width = static_cast<float>(cell.at("clip_width").number());
                const auto color = rgba(rgb, alpha);
                const auto effect = effects.add(
                    make_shadow_effect({area, static_cast<float>(cell.at("radius").number())},
                                       {ShadowKind::outer, {}, 0, 0, Color{color[0], color[1], color[2], color[3]}}, {},
                                       EffectClip{1, clip}));
                pending.push_back({{}, effect});
                observed_area = actual_effect(source, effect);
            } else {
                require(kind == "fill" || kind == "alpha" || kind == "sourceover" || kind == "round" || kind == "clip",
                        "unknown control material");
                const auto radius = cell.contains("radius") ? static_cast<float>(cell.at("radius").number()) : 0.0F;
                if (kind == "alpha") {
                    // Actual single Effect material opacity; no group-opacity ABI exists.
                    auto material = make_shadow_effect(
                        {area, radius}, {ShadowKind::outer,
                                         {},
                                         0,
                                         0,
                                         Color{rgba(rgb, alpha)[0], rgba(rgb, alpha)[1], rgba(rgb, alpha)[2], alpha}});
                    material.material.opacity = opacity;
                    const auto effect = effects.add(material);
                    pending.push_back({{}, effect});
                    observed_area = actual_effect(source, effect);
                } else {
                    observed_area = actual_quad(source, fill(area, rgba(rgb, alpha), radius, opacity));
                }
                if (kind == "sourceover") {
                    static_cast<void>(actual_quad(
                        source,
                        fill(area, rgba(cell.at("second_rgb"), static_cast<float>(cell.at("second_alpha").number()))),
                        true));
                    if (cell.contains("reverse_layers")) {
                        std::iter_swap(pending.end() - 1, pending.end() - 2);
                    }
                }
            }
            observations.emplace_back(Json::Object{{"identity", cell.at("identity")},
                                                   {"kind", kind},
                                                   {"rect", rectangle(observed_area)},
                                                   {"source", std::move(source)}});
        }
        static_cast<void>(effects.compact(
            {0, 0, metrics.pixel_width / metrics.display_scale, metrics.pixel_height / metrics.display_scale}));
        for (const auto& draw : pending) {
            if (!draw.effect.valid()) {
                order.append_command(draw.command);
            } else if (const auto index = effects.packed_index(draw.effect)) {
                order.append_command({SceneDrawKind::rounded_effect, *index, 1});
            } else {
                throw std::runtime_error("declared control effect absent after compaction");
            }
        }
        for (auto& observation : observations) {
            auto& fields = std::get<Json::Object>(observation.value);
            bind_source(std::get<Json::Object>(fields.at("source").value));
        }
        require(resources.synchronize(
                    {&quads, text.atlas(), text.glyph_scene().instances(), &effects, metrics, nullptr, &order}),
                "control common resource upload failed");
        require(renderer.attach_scene(resources.attach(order)), "control common scene attach failed");
    }
};
} // namespace

int main(int argc, char** argv) {
    try {
        SDL_SetAssertionHandler(
            [](const SDL_AssertData* data, void*) -> SDL_AssertState {
                std::cerr << data->condition << '\n';
                return SDL_ASSERTION_ABORT;
            },
            nullptr);
        require(argc == 5 && std::string_view{argv[1]} == "--request" && std::string_view{argv[3]} == "--output",
                "expected --request <owned.json> --output <fresh-out>");
        const auto request = read_json(owned_path(argv[2]));
        const auto output = owned_path(argv[4]);
        require(!std::filesystem::exists(output), "control output must be fresh");
        std::filesystem::create_directories(output);
        require(request.at("calibration_kind").string() == "pilot-independent-v1", "unknown control kind");
        const auto repetitions = request.at("repetitions").integer();
        require(repetitions >= 1 && repetitions <= 16, "control repetitions exceed bound");
        PlatformConfig config;
        config.title = "RynUI independent pilot controls";
        config.width = 1280;
        config.height = 900;
        config.hidden = true;
        auto platform = PlatformState::create(config);
        require(bool(platform), "hidden control platform failed");
        const auto initial = platform.state->window_metrics();
        require(std::isfinite(initial.pixel_density) && initial.pixel_density > 0 &&
                    std::isfinite(initial.display_scale) && initial.display_scale > 0,
                "actual control display scale/density unavailable");
        const auto coordinate_scale = initial.display_scale / initial.pixel_density;
        auto* window_handle = static_cast<SDL_Window*>(platform.state->window());
        require(SDL_SetWindowSize(window_handle, static_cast<int>(std::lround(1280 * coordinate_scale)),
                                  static_cast<int>(std::lround(900 * coordinate_scale))) &&
                    SDL_SyncWindow(window_handle),
                "fixed logical control viewport could not be applied");
        const auto window = platform.state->window_metrics();
        require(std::abs(window.logical_width() - 1280) <= .01F && std::abs(window.logical_height() - 900) <= .01F &&
                    window.display_scale == request.at("actual_dpr").number(),
                "actual control viewport/DPR differs from the frozen input");
        SdlSceneRenderer renderer(*platform.state);
        const auto device = renderer.device_description();
        const std::string driver = renderer.gpu_driver();
#if defined(_WIN32)
        require(driver == "direct3d12", "control requires actual D3D12");
#else
        require(driver == "vulkan", "control requires actual Vulkan");
#endif
        const auto flags = SDL_GetWindowFlags(static_cast<SDL_Window*>(platform.state->window()));
        require((flags & SDL_WINDOW_HIDDEN) && !(flags & SDL_WINDOW_INPUT_FOCUS), "control window took desktop focus");
        const SceneDeviceMetrics metrics{static_cast<std::uint32_t>(window.pixel_width),
                                         static_cast<std::uint32_t>(window.pixel_height), window.display_scale};
        auto runtime = font::FontRuntime::create();
        require(bool(runtime.runtime), "control font runtime failed");
        text::TextEngine engine(*runtime.runtime);
        Board board(renderer, *runtime.runtime, engine, metrics);
        const auto& policy = request.at("raster_policy");
        gallery_demo_fields(policy,
                            {"antialias", "hinting", "hint_style", "subpixel_order", "lcd_filter", "embedded_bitmap"});
        require(policy.at("hint_style").integer() >= 0 && policy.at("hint_style").integer() <= 4 &&
                    policy.at("subpixel_order").integer() >= 0 && policy.at("subpixel_order").integer() <= 5 &&
                    policy.at("lcd_filter").integer() >= 0 && policy.at("lcd_filter").integer() <= 4,
                "unknown actual font raster policy");
        board.raster_policy = {policy.at("antialias").boolean(),
                               policy.at("hinting").boolean(),
                               static_cast<font::FontHintStyle>(policy.at("hint_style").integer()),
                               static_cast<font::FontSubpixelOrder>(policy.at("subpixel_order").integer()),
                               static_cast<font::FontLcdFilter>(policy.at("lcd_filter").integer()),
                               policy.at("embedded_bitmap").boolean()};
        const auto& clear = request.at("clear_rgba").array();
        require(clear.size() == 4 && clear[3].number() == 1, "control requires actual opaque native clear");
        renderer.set_clear_color(Color{static_cast<float>(clear[0].number()), static_cast<float>(clear[1].number()),
                                       static_cast<float>(clear[2].number()), static_cast<float>(clear[3].number())});
        board.build(request);
        const auto* launch_id = std::getenv("RYNUI_CONTROL_LAUNCH_ID");
        require(launch_id && request.at("process_launch_id").string() == launch_id, "actual process launch ID differs");
#if defined(_WIN32)
        const auto process_id = _getpid();
#else
        const auto process_id = getpid();
#endif
        const auto process_started = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto process_nonce = std::to_string(process_id) + ":" + std::to_string(process_started);
        const auto request_sha256 = gallery_demo_file_sha256(owned_path(argv[2]));
        const auto commands = board.draw_commands();
        const auto snapshot_sha256 = [&] {
            const auto encoded =
                Json{Json::Object{{"controls", board.observations}, {"commands", board.draw_commands()}}}.dump();
            return gallery_demo_sha256({reinterpret_cast<const unsigned char*>(encoded.data()), encoded.size()});
        };
        for (std::int64_t index = 0; index < repetitions; ++index) {
            const auto before = snapshot_sha256();
            SceneRgbaReadback raw;
            require(renderer.read_frame_rgba(raw), "control current raw readback failed");
            const auto raw_name = "capture-" + std::to_string(index) + ".rgba";
            write(output / raw_name, raw.pixels);
            const auto name = "capture-" + std::to_string(index) + ".bmp";
            // Production BMP encoding performs its own real readback. Preserve
            // both fences and prove stable owned bytes; never merge receipts.
            SceneReadbackEvidence codec;
            require(renderer.save_frame_bmp(output / name, &codec), "production control BMP codec failed");
            const auto& e = raw.evidence;
            require(e.scene_revision == board.order.revision(),
                    "readback does not belong to the observed control scene");
            require(snapshot_sha256() == before && board.draw_commands().dump() == commands.dump(),
                    "control source snapshot changed during capture");
            require(codec.fence_id > e.fence_id && codec.raw_rgba_fnv1a64 == e.raw_rgba_fnv1a64 &&
                        codec.scene_revision == e.scene_revision && codec.metrics == e.metrics,
                    "opaque codec and raw readbacks are not the same stable control scene");
            const Json capture = Json::Object{
                {"path", name},
                {"sha256", gallery_demo_file_sha256(output / name)},
                {"raw", raw_name},
                {"raw_sha256", gallery_demo_file_sha256(output / raw_name)},
                {"width", integer(raw.width)},
                {"height", integer(raw.height)},
                {"dpr", metrics.display_scale},
                {"clear_rgba", Json::Array{e.clear_rgba[0], e.clear_rgba[1], e.clear_rgba[2], e.clear_rgba[3]}},
                {"readback_id", integer(e.readback_id)},
                {"fence_id", integer(e.fence_id)},
                {"fence_completed", e.fence_completed},
                {"raw_alpha_available", e.raw_alpha_available},
                {"raw_alpha_min", integer(e.raw_alpha_min)},
                {"raw_alpha_max", integer(e.raw_alpha_max)},
                {"raw_opaque_pixels", integer(e.raw_opaque_pixels)},
                {"raw_total_pixels", integer(e.raw_total_pixels)},
                {"source_channel_order", e.source_bgra ? "BGRA" : "RGBA"},
                {"framebuffer_format", e.source_bgra ? "BGRA8_UNORM" : "RGBA8_UNORM"},
                {"codec_fence_id", integer(codec.fence_id)},
                {"codec_readback_id", integer(codec.readback_id)},
                {"codec_raw_match", true},
                {"native_receipt", Json::Object{{"renderer_owner", integer(e.renderer_owner)},
                                                {"device_epoch", integer(e.device_epoch)},
                                                {"scene_revision", integer(e.scene_revision)},
                                                {"pixel_width", integer(e.scene_metrics.pixel_width)},
                                                {"pixel_height", integer(e.scene_metrics.pixel_height)},
                                                {"dpr", e.scene_metrics.display_scale}}},
                {"color_profile", "native-encoded-SDR"},
                {"alpha", "premultiplied"}};
            write_json(
                output / ("stream-" + std::to_string(index) + ".json"),
                Json::Object{
                    {"schema_version", 1},
                    {"calibration_kind", "pilot-independent-v1"},
                    {"side", "native"},
                    {"nonce", request.at("nonce").string() + "-" + std::to_string(index)},
                    {"process_nonce", process_nonce},
                    {"process", Json::Object{{"pid", static_cast<std::int64_t>(process_id)},
                                             {"launch_id", launch_id},
                                             {"started_ticks", static_cast<std::int64_t>(process_started)},
                                             {"ancestry", process_ancestry(process_id)}}},
                    {"request_sha256", request_sha256},
                    {"sample_ordinal", index},
                    {"snapshot_sha256", before},
                    {"snapshot_json",
                     Json{Json::Object{{"controls", board.observations}, {"commands", board.draw_commands()}}}.dump()},
                    {"board", request.at("board")},
                    {"fault", request.at("fault")},
                    {"restart", request.at("restart")},
                    {"provenance",
                     Json::Object{{"binding", request.at("binding")},
                                  {"source", gallery_demo_native_build_manifest()},
                                  {"backend", driver},
                                  {"preset", RYNUI_CONTROL_PRESET},
                                  {"toolchain", RYNUI_CONTROL_TOOLCHAIN},
                                  {"configuration", RYNUI_CONTROL_CONFIGURATION},
                                  {"shader", renderer.shader_format()},
#if defined(_WIN32)
                                  {"platform", "windows"},
#else
                                  {"platform", "linux"},
#endif
                                  {"metrics", Json::Object{{"logical_width", window.logical_width()},
                                                           {"logical_height", window.logical_height()},
                                                           {"pixel_width", window.pixel_width},
                                                           {"pixel_height", window.pixel_height},
                                                           {"dpr", window.display_scale}}},
                                  {"gpu", Json::Object{{"name", device.name},
                                                       {"driver_name", device.driver_name},
                                                       {"driver_version", device.driver_version},
                                                       {"driver_info", device.driver_info}}},
                                  {"binary_sha256", gallery_demo_file_sha256(std::filesystem::absolute(argv[0]))}}},
                    {"capture", capture},
                    {"submitted_commands", board.draw_commands()},
                    {"controls", board.observations},
                    {"complete", true}});
        }
        Json::Array alpha_cells;
        for (const auto& cell : request.at("cells").array()) {
            if (cell.at("kind").string() == "alpha") {
                alpha_cells.push_back(cell);
            }
        }
        Json transparent_request = request;
        std::get<Json::Object>(transparent_request.value)["panels"] = Json::Array{};
        std::get<Json::Object>(transparent_request.value)["cells"] = alpha_cells;
        renderer.set_clear_color(Color{0, 0, 0, 0});
        Board transparent(renderer, *runtime.runtime, engine, metrics);
        transparent.build(transparent_request);
        SceneRgbaReadback probe;
        require(renderer.read_frame_rgba(probe), "transparent raw alpha probe failed");
        write(output / "transparent-probe.rgba", probe.pixels);
        write_json(output / "transparent-probe.json",
                   Json::Object{{"raw", "transparent-probe.rgba"},
                                {"raw_sha256", gallery_demo_file_sha256(output / "transparent-probe.rgba")},
                                {"width", integer(probe.width)},
                                {"height", integer(probe.height)},
                                {"dpr", metrics.display_scale},
                                {"readback_id", integer(probe.evidence.readback_id)},
                                {"fence_id", integer(probe.evidence.fence_id)},
                                {"fence_completed", probe.evidence.fence_completed},
                                {"cells", alpha_cells},
                                {"clear_rgba", Json::Array{0, 0, 0, 0}},
                                {"storage", "native-encoded-SDR/premultiplied"}});
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 4;
    }
}
