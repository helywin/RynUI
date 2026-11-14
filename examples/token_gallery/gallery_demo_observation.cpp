#include "gallery_demo_observation.hpp"
#include "gallery_demo_hash.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "component/select_component.hpp"
#include "component/flex_component.hpp"
#include "component/space_component.hpp"
#include "icons/bundled_icon_catalog.hpp"
#include "theme/theme_runtime.hpp"
#include "text/text_scene_service.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace rynui::example {
namespace {
using Json = GalleryDemoJson;

bool hash(const std::string& value) {
    return value.size() == 64 &&
           std::ranges::all_of(value, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

bool same_face(const Json& left, const Json& right) {
    return left.at("sha256").string() == right.at("sha256").string() &&
           left.at("face_index").integer() == right.at("face_index").integer();
}
} // namespace

GalleryDemoJson gallery_demo_input_paint_snapshot(const ryn::detail::InputPaintSnapshot& paint) {
    using namespace ryn;
    const auto rectangle = [](runtime::Rect r) -> Json {
        return Json::Object{{"x", r.x}, {"y", r.y}, {"width", r.width}, {"height", r.height}};
    };
    const auto identity = [](auto id) -> Json {
        return Json::Object{{"index", static_cast<std::int64_t>(id.index)},
                            {"generation", static_cast<std::int64_t>(id.generation)}};
    };
    Json::Array layers;
    Json::Array published_commands;
    for (const auto& command : paint.commands) {
        published_commands.emplace_back(
            Json::Object{{"kind", static_cast<std::int64_t>(command.kind)},
                         {"first_instance", static_cast<std::int64_t>(command.first_instance)},
                         {"instance_count", static_cast<std::int64_t>(command.instance_count)},
                         {"atlas_page", static_cast<std::int64_t>(command.atlas_page)}});
    }
    for (const auto& item : paint.layers) {
        const auto role = item.role == ryn::detail::InputPaintLayerRole::outer_shadow   ? "outer-shadow"
                          : item.role == ryn::detail::InputPaintLayerRole::border       ? "border"
                          : item.role == ryn::detail::InputPaintLayerRole::background   ? "background"
                          : item.role == ryn::detail::InputPaintLayerRole::inset_shadow ? "inset-shadow"
                                                                                        : "focus";
        Json::Object layer{
            {"primitive", identity(item.primitive)},
            {"role", role},
            {"corner", static_cast<std::int64_t>(item.corner)},
            {"exists", item.exists},
            {"in_fragment", item.in_fragment},
            {"clipped", item.clipped},
            {"hidden", item.hidden},
            {"packed_index", item.packed_index ? Json{static_cast<std::int64_t>(*item.packed_index)} : Json{}}};
        layer.emplace("command_ordinal",
                      item.command_ordinal ? Json{static_cast<std::int64_t>(*item.command_ordinal)} : Json{});
        if (item.retained) {
            const auto& geometry = item.retained->geometry;
            const auto& material = item.retained->material;
            layer.emplace(
                "actual",
                Json::Object{{"bounds", rectangle(geometry.shape.rect)},
                             {"radius", geometry.shape.radius},
                             {"kind", static_cast<std::int64_t>(geometry.kind)},
                             {"offset", Json::Array{geometry.offset.x, geometry.offset.y}},
                             {"blur", geometry.blur},
                             {"spread", geometry.spread},
                             {"outline_width", geometry.outline_width},
                             {"outline_offset", geometry.outline_offset},
                             {"translation", Json::Array{geometry.translation.x, geometry.translation.y}},
                             {"clip", geometry.ancestor_clip ? rectangle(geometry.ancestor_clip->bounds) : Json{}},
                             {"clip_identity", geometry.ancestor_clip
                                                   ? Json{static_cast<std::int64_t>(geometry.ancestor_clip->identity)}
                                                   : Json{}},
                             {"color", Json::Array{material.color.red(), material.color.green(), material.color.blue(),
                                                   material.color.alpha()}},
                             {"opacity", material.opacity},
                             {"visible", material.visible},
                             {"paint_present", material.paint.has_value()}});
        } else {
            layer.emplace("actual", Json{});
        }
        layers.emplace_back(std::move(layer));
    }
    return Json::Object{{"source", "published-retained-input-fragment"},
                        {"status", paint.status == ryn::detail::InputPaintStatus::published        ? "published"
                                   : paint.status == ryn::detail::InputPaintStatus::unsynchronized ? "unsynchronized"
                                                                                                   : "absent"},
                        {"owner", identity(paint.owner)},
                        {"branch_active", paint.branch_active},
                        {"editor", identity(paint.editor)},
                        {"container_fragment", identity(paint.container_fragment)},
                        {"selection_surface", identity(paint.selection_surface)},
                        {"overlay_surface", identity(paint.overlay_surface)},
                        {"fragment_clip", paint.fragment_clip ? rectangle(*paint.fragment_clip) : Json{}},
                        {"publication_revision", static_cast<std::int64_t>(paint.publication_revision)},
                        {"published_commands", std::move(published_commands)},
                        {"layers", std::move(layers)}};
}

GalleryDemoJson gallery_demo_paint_stack(ryn::detail::ButtonComponentHost& app,
                                         std::span<const ryn::runtime::ComponentId> roots,
                                         const ryn::graphics::OrderedScene& submitted) {
    using namespace ryn;
    const auto identity = [](auto id) -> Json {
        return Json::Object{{"index", static_cast<std::int64_t>(id.index)},
                            {"generation", static_cast<std::int64_t>(id.generation)}};
    };
    const auto rectangle = [](runtime::Rect r) -> Json {
        return Json::Object{{"x", r.x}, {"y", r.y}, {"width", r.width}, {"height", r.height}};
    };
    const auto array = [](const auto& values) -> Json {
        Json::Array result;
        for (const auto value : values) {
            result.emplace_back(value);
        }
        return result;
    };
    const auto in_roots = [&](runtime::ComponentId owner) {
        for (auto current = std::optional{owner}; current; current = app.components().parent(*current)) {
            if (std::ranges::find(roots, *current) != roots.end()) {
                return true;
            }
        }
        return false;
    };
    const auto& commands = submitted.commands();
    Json::Array final_commands;
    for (std::size_t ordinal = 0; ordinal < commands.size(); ++ordinal) {
        const auto& command = commands[ordinal];
        final_commands.emplace_back(Json::Object{{"ordinal", static_cast<std::int64_t>(ordinal)},
                                                 {"kind", static_cast<std::int64_t>(command.kind)},
                                                 {"first", static_cast<std::int64_t>(command.first_instance)},
                                                 {"count", static_cast<std::int64_t>(command.instance_count)},
                                                 {"atlas_page", static_cast<std::int64_t>(command.atlas_page)}});
    }
    Json::Array composition;
    Json::Array requests;
    std::vector<std::vector<std::uint32_t>> command_groups(commands.size());
    std::vector<graphics::DrawCoverageRole> command_coverage(commands.size(), graphics::DrawCoverageRole::Normal);
    if (const auto& program = submitted.composition()) {
        for (const auto& request : program->requests) {
            requests.emplace_back(Json::Object{{"owner", identity(request.owner)},
                                               {"hard_clip", rectangle(request.hard_clip)},
                                               {"shape", rectangle(request.shape.bounds)},
                                               {"radius", request.shape.radius},
                                               {"backdrop", request.backdrop},
                                               {"sigma", request.sigma},
                                               {"activation", request.activation}});
        }
        std::vector<std::uint32_t> open;
        for (const auto& op : program->operations) {
            composition.emplace_back(Json::Object{{"kind", static_cast<std::int64_t>(op.kind)},
                                                  {"first", static_cast<std::int64_t>(op.first)},
                                                  {"count", static_cast<std::int64_t>(op.count)},
                                                  {"request", static_cast<std::int64_t>(op.request)},
                                                  {"coverage", static_cast<std::int64_t>(op.coverage)}});
            if (op.kind == graphics::SceneCompositionOpKind::BeginMaskedGroup) {
                open.push_back(op.request);
            } else if (op.kind == graphics::SceneCompositionOpKind::EndMaskedGroup) {
                if (open.empty() || open.back() != op.request) {
                    throw std::runtime_error("actual submitted composition group is unbalanced");
                }
                open.pop_back();
            } else if (op.kind == graphics::SceneCompositionOpKind::DrawRange) {
                for (std::uint32_t ordinal = op.first; ordinal < op.first + op.count; ++ordinal) {
                    if (ordinal >= commands.size()) {
                        throw std::runtime_error("actual composition range escapes submitted commands");
                    }
                    command_groups[ordinal] = open;
                    command_coverage[ordinal] = op.coverage;
                }
            }
        }
        if (!open.empty()) {
            throw std::runtime_error("actual submitted composition group remains open");
        }
    }

    // Resolve identities from existing owning records, never from packed indices alone.
    struct EffectSource final {
        graphics::RoundedEffectId id;
        std::string role;
    };

    std::map<std::uint32_t, EffectSource> effect_sources;
    std::map<std::uint32_t, component::RetainedSurfaceId> quad_sources;
    std::map<std::uint32_t, std::string> quad_roles;
    const auto add_surface = [&](component::RetainedSurfaceId id) {
        const auto snapshot = app.services().surfaces().paint_snapshot(id);
        if (!snapshot) {
            return;
        }
        for (std::uint32_t offset = 0; offset < snapshot->range.count; ++offset) {
            quad_sources.emplace(snapshot->range.first + offset, id);
        }
        const auto add = [&](const auto& effects, const char* role) {
            for (const auto& effect : effects) {
                if (effect.packed_index) {
                    effect_sources.emplace(*effect.packed_index, EffectSource{effect.id, role});
                }
            }
        };
        add(snapshot->shadows, "shadow");
        add(snapshot->focus, "focus");
        add(snapshot->clipped_fills, "fill");
        add(snapshot->content_effects, "content-effect");
    };
    for (const auto& button : app.mounted_buttons()) {
        if (in_roots(button.component)) {
            add_surface(button.scene);
        }
    }
    if (const auto* inputs = app.services().input_runtime()) {
        for (const auto& input : inputs->mounted_inputs()) {
            if (!in_roots(input.component)) {
                continue;
            }
            const auto paint = inputs->paint_snapshot(input.component);
            add_surface(paint.selection_surface);
            add_surface(paint.overlay_surface);
            if (const auto selection = app.services().surfaces().paint_snapshot(paint.selection_surface)) {
                for (std::uint32_t offset = 0; offset < selection->range.count; ++offset) {
                    quad_roles.emplace(selection->range.first + offset, "selection");
                }
            }
            if (const auto overlay = app.services().surfaces().paint_snapshot(paint.overlay_surface)) {
                for (std::uint32_t offset = 0; offset < overlay->range.count; ++offset) {
                    quad_roles.emplace(
                        overlay->range.first + offset,
                        paint.caret_surface_offset && offset == *paint.caret_surface_offset ? "caret" : "composition");
                }
            }
            for (const auto& layer : paint.layers) {
                if (!layer.packed_index) {
                    continue;
                }
                const auto role = layer.role == ryn::detail::InputPaintLayerRole::outer_shadow   ? "shadow"
                                  : layer.role == ryn::detail::InputPaintLayerRole::border       ? "border"
                                  : layer.role == ryn::detail::InputPaintLayerRole::background   ? "fill"
                                  : layer.role == ryn::detail::InputPaintLayerRole::inset_shadow ? "inset-shadow"
                                                                                                 : "focus";
                effect_sources.emplace(*layer.packed_index, EffectSource{layer.primitive, role});
            }
        }
    }
    for (const auto owner : app.services().select().mounted()) {
        if (in_roots(owner)) {
            if (const auto paint = app.services().select().paint_owners(owner)) {
                add_surface(paint->popup.surface);
                add_surface(paint->clear.surface);
                for (const auto& row : paint->rows) {
                    add_surface(row.surface);
                }
            }
        }
    }
    Json::Array layers;
    for (const auto& entry : app.components().paint_traversal()) {
        if (!in_roots(entry.component)) {
            continue;
        }
        const auto binding = app.scene_composer().paint_snapshot(entry.fragment);
        if (!binding) {
            continue;
        }
        for (std::size_t local = 0; local < binding->commands.size(); ++local) {
            const auto& command = binding->commands[local];
            for (std::uint32_t offset = 0; offset < command.instance_count; ++offset) {
                const auto instance = command.first_instance + offset;
                Json::Array membership;
                for (std::size_t final = 0; final < commands.size(); ++final) {
                    const auto& draw = commands[final];
                    if (draw.kind == command.kind && draw.atlas_page == command.atlas_page &&
                        draw.first_instance <= instance && instance - draw.first_instance < draw.instance_count) {
                        Json::Array groups;
                        for (const auto group : command_groups[final]) {
                            groups.emplace_back(static_cast<std::int64_t>(group));
                        }
                        membership.emplace_back(
                            Json::Object{{"command", static_cast<std::int64_t>(final)},
                                         {"instance", static_cast<std::int64_t>(instance - draw.first_instance)},
                                         {"composition_group_path", std::move(groups)},
                                         {"coverage_role", static_cast<std::int64_t>(command_coverage[final])}});
                    }
                }
                const auto source_id =
                    std::to_string(entry.component.index) + ":" + std::to_string(entry.component.generation) + "/" +
                    std::to_string(entry.fragment.index) + ":" + std::to_string(entry.fragment.generation) + "/" +
                    std::to_string(binding->publication_revision) + "/" + std::to_string(local) + "/" +
                    std::to_string(offset);
                Json::Object layer{
                    {"source_material_id", source_id},
                    {"owner", identity(entry.component)},
                    {"fragment", identity(entry.fragment)},
                    {"publication_revision", static_cast<std::int64_t>(binding->publication_revision)},
                    {"fragment_command", static_cast<std::int64_t>(local)},
                    {"kind", static_cast<std::int64_t>(command.kind)},
                    {"packed_index", static_cast<std::int64_t>(instance)},
                    {"atlas_page", static_cast<std::int64_t>(command.atlas_page)},
                    {"in_fragment", true},
                    {"in_submitted_scene", !membership.empty()},
                    {"submitted_membership", std::move(membership)},
                    {"outside_own_mask", binding->outside_own_mask},
                    {"coverage_role", static_cast<std::int64_t>(binding->coverage)},
                    {"fragment_clip", binding->interaction_clip ? rectangle(*binding->interaction_clip) : Json{}},
                    {"window_layer", app.components().in_window_layer(entry.component)}};
                if (command.kind == graphics::SceneDrawKind::quad) {
                    const auto& quad = app.services().surfaces().instances().at(instance);
                    const auto role = quad_roles.find(instance);
                    layer.emplace("role", role != quad_roles.end() ? role->second : "fill");
                    layer.emplace("actual", Json::Object{{"bounds", rectangle({quad.bounds[0], quad.bounds[1],
                                                                               quad.bounds[2], quad.bounds[3]})},
                                                         {"radius", quad.corner_radius},
                                                         {"translation", array(quad.translation)},
                                                         {"color", array(quad.color)},
                                                         {"opacity", quad.opacity},
                                                         {"visible", quad.opacity > 0},
                                                         {"paint_present", false}});
                    if (const auto found = quad_sources.find(instance); found != quad_sources.end()) {
                        layer.emplace("surface", identity(found->second));
                        layer.emplace("content_range", found->second.content_range);
                    }
                } else if (command.kind == graphics::SceneDrawKind::rounded_effect) {
                    const auto& effect = app.rounded_effects().packed_instances()[instance];
                    const auto& g = effect.geometry;
                    const auto& m = effect.material;
                    layer.emplace(
                        "actual",
                        Json::Object{
                            {"bounds", rectangle(g.shape.rect)},
                            {"radius", g.shape.radius},
                            {"effect_kind", static_cast<std::int64_t>(g.kind)},
                            {"offset", Json::Array{g.offset.x, g.offset.y}},
                            {"blur", g.blur},
                            {"spread", g.spread},
                            {"outline_width", g.outline_width},
                            {"outline_offset", g.outline_offset},
                            {"translation", Json::Array{g.translation.x, g.translation.y}},
                            {"clip", g.ancestor_clip ? rectangle(g.ancestor_clip->bounds) : Json{}},
                            {"clip_identity",
                             g.ancestor_clip ? Json{static_cast<std::int64_t>(g.ancestor_clip->identity)} : Json{}},
                            {"color", Json::Array{m.color.red(), m.color.green(), m.color.blue(), m.color.alpha()}},
                            {"opacity", m.opacity},
                            {"visible", m.visible},
                            {"paint_present", m.paint.has_value()}});
                    std::get<Json::Object>(layer.at("actual").value)
                        .emplace("path_present", !std::holds_alternative<std::monostate>(g.path));
                    if (const auto found = effect_sources.find(instance); found != effect_sources.end()) {
                        layer.emplace("primitive", identity(found->second.id));
                        layer.emplace("role", found->second.role);
                    } else {
                        layer.emplace("role", "unmapped-effect");
                    }
                } else if (command.kind == graphics::SceneDrawKind::glyph) {
                    const auto& glyph = app.text().scene_service().glyph_scene().instances().at(instance);
                    layer.emplace("role", "glyph");
                    layer.emplace(
                        "actual",
                        Json::Object{
                            {"bounds", rectangle({glyph.position_size[0], glyph.position_size[1],
                                                  glyph.position_size[2], glyph.position_size[3]})},
                            {"clip_bounds", array(glyph.clip_bounds)},
                            {"translation", Json::Array{glyph.translation_opacity[0], glyph.translation_opacity[1]}},
                            {"color", array(glyph.color)},
                            {"opacity", glyph.translation_opacity[2]},
                            {"uv_rect", array(glyph.uv_rect)},
                            {"angle", glyph.transform.angle_degrees},
                            {"pivot", Json::Array{glyph.transform.pivot.x, glyph.transform.pivot.y}}});
                } else {
                    layer.emplace("role", "unsupported-image");
                }
                layers.emplace_back(std::move(layer));
            }
        }
    }
    Json::Object stack{
        {"status", app.scene_composer().needs_rebuild() ? "unsynchronized" : "published"},
        {"source", "actual-submitter-visible"},
        {"scene_revision", static_cast<std::int64_t>(submitted.revision())},
        {"composition_version",
         static_cast<std::int64_t>(submitted.composition() ? submitted.composition()->version : 0)},
        {"composition_scene_revision",
         static_cast<std::int64_t>(submitted.composition() ? submitted.composition()->scene_revision : 0)},
        {"commands", std::move(final_commands)},
        {"composition", std::move(composition)},
        {"composition_requests", std::move(requests)},
        {"layers", std::move(layers)}};
    const auto serialized = Json{stack}.dump();
    stack.emplace(
        "snapshot_digest",
        gallery_demo_sha256(std::span{reinterpret_cast<const unsigned char*>(serialized.data()), serialized.size()}));
    return stack;
}

void gallery_demo_validate_capture(const GalleryDemoCaptureEvidence& evidence, std::uint64_t previous_readback,
                                   std::uint64_t revision, const Json& metrics, const Json& fixture,
                                   const Json& provenance) {
    if (evidence.readback_id <= previous_readback || !evidence.fence_id || !evidence.fence_completed ||
        !evidence.renderer_owner || !evidence.device_epoch || evidence.scene_revision != revision) {
        throw std::runtime_error("checkpoint has stale/missing renderer frame or completed fence evidence");
    }
    for (const auto field : {"pixel_width", "pixel_height", "dpr", "logical_width", "logical_height"}) {
        if (evidence.metrics.at(field).number() != metrics.at(field).number()) {
            throw std::runtime_error("checkpoint readback and actual window metrics differ");
        }
    }
    const auto dpr = metrics.at("dpr").number();
    if (!std::isfinite(dpr) || dpr <= 0 || metrics.at("pixel_width").integer() <= 0 ||
        metrics.at("pixel_height").integer() <= 0 ||
        std::abs(metrics.at("logical_width").number() * dpr - metrics.at("pixel_width").number()) > .01 ||
        std::abs(metrics.at("logical_height").number() * dpr - metrics.at("pixel_height").number()) > .01) {
        throw std::runtime_error("checkpoint metrics have inconsistent logical/physical extent or DPR");
    }
    const auto& viewport = fixture.at("viewport");
    for (const auto axis : {"width", "height"}) {
        if (viewport.contains(axis) &&
            std::abs(metrics.at(std::string{"logical_"} + axis).number() - viewport.at(axis).number()) > .001) {
            throw std::runtime_error("checkpoint actual logical viewport differs from declared fixture");
        }
    }
    const auto& requested = viewport.at("dpr");
    if ((std::holds_alternative<std::string>(requested.value) && requested.string() != "native") ||
        (!std::holds_alternative<std::string>(requested.value) && requested.number() != dpr)) {
        throw std::runtime_error("requested DPR differs from actual native DPR");
    }
    if ((evidence.framebuffer_format != "RGBA8_UNORM" && evidence.framebuffer_format != "BGRA8_UNORM") ||
        (evidence.source_channel_order != "RGBA" && evidence.source_channel_order != "BGRA") ||
        evidence.channel_order != "RGBA" || evidence.alpha != "premultiplied" ||
        (evidence.framebuffer_format == "BGRA8_UNORM") != (evidence.source_channel_order == "BGRA") ||
        evidence.color_profile != "native-encoded-SDR") {
        throw std::runtime_error("checkpoint has unsupported or unknown framebuffer encoding");
    }
    if (!hash(evidence.source_hash) || !hash(evidence.binary_sha256) ||
        evidence.source_hash != provenance.at("source").at("source_hash").string() ||
        evidence.binary_sha256 != provenance.at("binary_sha256").string()) {
        throw std::runtime_error("checkpoint source/binary provenance differs from hello");
    }
}

void gallery_demo_validate_font_chain(const GalleryDemoScenario& scenario) {
    const auto& fixture = scenario.definition.at("fixture");
    const auto policy = fixture.at("font_policy").string();
    if (policy != "pinned" && policy != "system") {
        throw std::invalid_argument("native font policy is unknown");
    }
    if (policy == "system") {
        return; // The host resolves its ordinary system chain; files are not injected.
    }
    const auto& declared = fixture.at("fonts").array();
    if (declared.empty() || declared.size() != scenario.font_files.array().size()) {
        throw std::invalid_argument("pinned font declarations and explicit font files differ");
    }
    std::set<std::tuple<std::string, std::int64_t, std::int64_t>> faces;
    for (std::size_t i = 0; i < declared.size(); ++i) {
        const auto& expected = declared[i];
        const auto& supplied = scenario.font_files.array()[i];
        if (!hash(supplied.at("sha256").string()) || !same_face(expected, supplied) ||
            expected.at("identity").string() != supplied.at("identity").string() ||
            expected.at("weight").integer() != supplied.at("weight").integer() ||
            supplied.at("face_index").integer() < 0 || supplied.at("weight").integer() < 1 ||
            supplied.at("weight").integer() > 1000 || supplied.at("path").string().empty() ||
            !faces
                 .emplace(supplied.at("identity").string(), supplied.at("face_index").integer(),
                          supplied.at("weight").integer())
                 .second) {
            throw std::invalid_argument("pinned font identity/hash/face/weight/file mismatch");
        }
    }
}

void gallery_demo_validate_font_source(const GalleryDemoScenario& scenario, const Json& font, bool icon) {
    if (!hash(font.at("sha256").string()) || font.at("face_index").integer() < 0) {
        throw std::runtime_error("actual font source bytes/face are unknown");
    }
    if (icon) {
        if (font.at("source").string() != "bundled-icon") {
            throw std::runtime_error("icon text is not using the actual bundled icon font");
        }
        return;
    }
    const auto policy = scenario.definition.at("fixture").at("font_policy").string();
    if (policy == "pinned") {
        if (font.at("source").string() != "pinned-file" ||
            std::ranges::none_of(scenario.font_files.array(),
                                 [&](const auto& file) { return same_face(file, font); })) {
            throw std::runtime_error("pinned text used an undeclared actual face or incorrect font source marker");
        }
    } else if (font.at("source").string() != "system-file" ||
               std::ranges::none_of(scenario.definition.at("fixture").at("fonts").array(),
                                    [&](const auto& file) { return same_face(file, font); })) {
        throw std::runtime_error("system font profile used a custom/bundled/unobservable text face");
    }
}

void gallery_demo_verify_source_files(const Json& manifest, const std::filesystem::path& repository) {
    if (!hash(manifest.at("source_hash").string()) || manifest.at("files").array().empty()) {
        throw std::runtime_error("embedded native source closure is absent");
    }
    const auto root = std::filesystem::weakly_canonical(repository);
    std::string previous;
    for (const auto& file : manifest.at("files").array()) {
        const auto name = file.at("path").string();
        const auto path = std::filesystem::weakly_canonical(root / name);
        const auto relative = path.lexically_relative(root);
        if (name.empty() || name <= previous || relative.empty() || *relative.begin() == ".." ||
            !hash(file.at("sha256").string()) || gallery_demo_file_sha256(path) != file.at("sha256").string()) {
            throw std::runtime_error("native source changed without rebuilding the binary closure: " + name);
        }
        previous = name;
    }
}

namespace {
using namespace ryn;

Json retained_text(detail::ButtonComponentHost& app, runtime::ComponentId id) {
    for (const auto& text : app.text().mounted_texts()) {
        if (text.component != id) {
            continue;
        }
        try {
            const auto icon = app.text().icon_snapshot(id);
            if (const auto name = icon.source.bundled_name()) {
                return Json::Object{{"icon", std::string{detail::bundled_icon_entry(*name).name}}};
            }
        } catch (const std::out_of_range&) {
        }
        return std::string{std::as_const(app.text().scene_service()).text_state(text.scene).content().bytes()};
    }
    return {};
}

Json flex_props(const layout::FlexLayout& model) {
    const char* align = model.align == layout::FlexAlign::center     ? "center"
                        : model.align == layout::FlexAlign::end      ? "end"
                        : model.align == layout::FlexAlign::stretch  ? "stretch"
                        : model.align == layout::FlexAlign::baseline ? "baseline"
                                                                     : "start";
    return Json::Object{{"vertical", model.direction == layout::FlexDirection::vertical},
                        {"gap", model.main_gap},
                        {"cross_gap", model.cross_gap},
                        {"wrap", model.wrap != layout::FlexWrap::no_wrap},
                        {"align", align}};
}

std::string hex_color(Color color) {
    const char* digits = "0123456789abcdef";
    std::string value{"#"};
    for (const auto channel : {color.red(), color.green(), color.blue()}) {
        const auto byte = static_cast<unsigned>(std::lround(channel * 255));
        value.push_back(digits[(byte >> 4) & 15]);
        value.push_back(digits[byte & 15]);
    }
    return value;
}
} // namespace

Json gallery_demo_retained_content(detail::ButtonComponentHost& app, runtime::ComponentId root,
                                   std::optional<runtime::ComponentId> exclude) {
    Json::Array result;
    const auto visit = [&](auto&& self, runtime::ComponentId id) -> void {
        if (exclude == id || !app.components().contains(id)) {
            return;
        }
        const auto text = retained_text(app, id);
        if (!std::holds_alternative<std::nullptr_t>(text.value)) {
            result.push_back(text);
        }
        for (const auto child : app.components().children(id)) {
            self(self, child);
        }
    };
    visit(visit, root);
    return result;
}

Json gallery_demo_retained_tree(detail::ButtonComponentHost& app, std::span<const runtime::ComponentId> roots,
                                const std::vector<std::pair<std::string, runtime::ComponentId>>& targets,
                                std::optional<runtime::ComponentId> origin,
                                std::span<const std::pair<std::string, runtime::ComponentId>> feedback_targets) {
    const auto visit = [&](auto&& self, runtime::ComponentId id) -> Json {
        for (const auto& [identity, target] : feedback_targets) {
            if (id == target) {
                const auto content = retained_text(app, id);
                if (!std::holds_alternative<std::string>(content.value)) {
                    throw std::runtime_error("authored feedback target is not retained text");
                }
                return Json::Object{{"component", "source-feedback"}, {"identity", identity}, {"content", content}};
            }
        }
        if (origin == id) {
            return Json::Object{{"component", "origin"}};
        }
        for (const auto& [name, target] : targets) {
            if (id == target) {
                return Json::Object{{"component", "control"}, {"target", name}};
            }
        }
        Json::Array children;
        for (const auto child : app.components().children(id)) {
            children.push_back(self(self, child));
        }
        const auto text = retained_text(app, id);
        if (std::holds_alternative<std::string>(text.value)) {
            return Json::Object{{"component", "Typography.Text"}, {"content", text}};
        }
        const auto& store = std::as_const(app.components());
        if (const auto* state = store.state<detail::SpaceComponentState>(id)) {
            Json::Object result{{"component", "Space"}, {"props", flex_props(state->model)}, {"children", children}};
            if (const auto width = app.nodes().require(state->node).external_layout.width) {
                result.emplace("width", *width);
            }
            return result;
        }
        if (const auto* state = store.state<detail::FlexComponentState>(id)) {
            return Json::Object{{"component", "Flex"}, {"props", flex_props(state->model)}, {"children", children}};
        }
        return Json::Object{{"component", "retained"}, {"children", children}};
    };
    Json::Array result;
    for (const auto root : roots) {
        result.push_back(visit(visit, root));
    }
    Json host;
    if (!roots.empty()) {
        if (const auto parent = app.components().parent(roots.front())) {
            if (const auto* state = std::as_const(app.components()).state<detail::FlexComponentState>(*parent)) {
                host = flex_props(state->model);
            }
        }
    }
    return Json::Object{
        {"source", "retained component tree and applied layout models"}, {"host", host}, {"roots", result}};
}

Json gallery_demo_applied_theme(detail::ButtonComponentHost& app, runtime::ComponentId id) {
    const auto& scope = *app.components().theme_scope(id);
    const auto& snapshot = scope.snapshot();
    Json::Array algorithms;
    for (const auto algorithm : snapshot.algorithms()) {
        algorithms.emplace_back(algorithm == ThemeAlgorithm::Dark      ? "dark"
                                : algorithm == ThemeAlgorithm::Compact ? "compact"
                                                                       : "default");
    }
    const auto& config = scope.applied_config();
    Json::Object select{{"algorithm", config.select.algorithm}};
    if (config.select.seed.color_primary) {
        select.emplace("colorPrimary", hex_color(*config.select.seed.color_primary));
    }
    if (config.select.tokens.option_height) {
        select.emplace("optionHeight", config.select.tokens.option_height->value());
    }
    return Json::Object{{"source", "retained ThemeScope applied config and resolved snapshot"},
                        {"algorithms", algorithms},
                        {"colorBgContainer", hex_color(snapshot.alias().color_background_container)},
                        {"Select", select},
                        {"select_option_height", snapshot.select().metrics.option_height}};
}
} // namespace rynui::example
