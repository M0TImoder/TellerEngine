#include <Base/Platform/DebugPanels.hpp>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <typeinfo>

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

#ifdef TELLER_DEBUG_UI
#include <imgui.h>
#include <imgui_stdlib.h>
#endif

#include <Base/Rate.hpp>

#include <cstdint>
#include <cstring>

namespace TellerEngine::Base::Platform {

std::string TypeNameOf(const GameObject &object) {
    const char *raw = typeid(object).name();

#if defined(__GNUC__) || defined(__clang__)
    int status = 0;
    char *readable = abi::__cxa_demangle(raw, nullptr, nullptr, &status);
    if (status == 0 && readable != nullptr) {
        std::string name = readable;
        std::free(readable);
        return name;
    }
#endif

    return raw;
}

void DestroyInstance(Context &context, DebugSelection &selection, InstanceId id) {
    if (context.instances.Find(id) == nullptr) {
        return;
    }
    context.Destroy(id);
    if (selection.instance == id) {
        selection.instance = InstanceId::None;
    }
}

const char *NameOf(DrawKind kind) {
    switch (kind) {
    case DrawKind::Sprite:
        return "sprite";
    case DrawKind::Rectangle:
        return "rectangle";
    case DrawKind::RoundRectangle:
        return "roundrect";
    case DrawKind::Line:
        return "line";
    case DrawKind::Circle:
        return "circle";
    case DrawKind::Ellipse:
        return "ellipse";
    case DrawKind::Triangle:
        return "triangle";
    case DrawKind::Text:
        return "text";
    case DrawKind::Target:
        return "target";
    }
    return "unknown";
}

const char *NameOf(BlendMode blend) {
    switch (blend) {
    case BlendMode::Normal:
        return "normal";
    case BlendMode::None:
        return "none";
    case BlendMode::Add:
        return "add";
    case BlendMode::Subtract:
        return "subtract";
    case BlendMode::Multiply:
        return "multiply";
    }
    return "unknown";
}

std::vector<HierarchyGroup> GroupInstances(Instances &instances) {
    std::map<std::string, std::vector<InstanceId>> byType;
    instances.ForEachAll([&byType](GameObject &object) {
        byType[TypeNameOf(object)].push_back(object.id);
    });

    std::vector<HierarchyGroup> groups;
    groups.reserve(byType.size());
    for (auto &entry : byType) {
        groups.push_back(HierarchyGroup{entry.first, std::move(entry.second)});
    }
    return groups;
}

#ifdef TELLER_DEBUG_UI

void DrawHierarchy(const char *title, bool &open, Context &context, std::string_view room,
                   DebugSelection &selection) {
    if (!open) {
        return;
    }

    Instances &instances = context.instances;
    InstanceId doomed = InstanceId::None;

    bool showing = open;
    if (ImGui::Begin(title, &showing)) {
        const std::vector<HierarchyGroup> groups = GroupInstances(instances);

        const std::string name = room.empty() ? std::string{"(room)"} : std::string{room};
        if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const HierarchyGroup &group : groups) {
                ImGui::PushID(group.type.c_str());
                const std::string label =
                    group.type + " (" + std::to_string(group.instances.size()) + ")";

                if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (const InstanceId id : group.instances) {
                        const GameObject *object = instances.Find(id);
                        if (object == nullptr) {
                            continue;
                        }

                        const std::string leaf =
                            "#" + std::to_string(static_cast<std::uint32_t>(id));
                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                                                   ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                   ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (selection.instance == id) {
                            flags |= ImGuiTreeNodeFlags_Selected;
                        }

                        if (!object->active) {
                            ImGui::PushStyleColor(ImGuiCol_Text,
                                                  ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                        }
                        ImGui::TreeNodeEx(leaf.c_str(), flags);
                        if (!object->active) {
                            ImGui::PopStyleColor();
                        }

                        if (ImGui::IsItemClicked()) {
                            selection.instance = id;
                        }
                        if (ImGui::BeginPopupContextItem(leaf.c_str())) {
                            selection.instance = id;
                            if (ImGui::MenuItem("destroy")) {
                                doomed = id;
                            }
                            ImGui::EndPopup();
                        }

                        ImGui::SameLine();
                        ImGui::TextDisabled("x %.0f  y %.0f  depth %.0f", object->x, object->y,
                                            object->depth);
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
    ImGui::End();
    open = showing;

    if (doomed != InstanceId::None) {
        DestroyInstance(context, selection, doomed);
    }
}

bool Editable(VariableKind kind) {
    switch (kind) {
    case VariableKind::Boolean:
    case VariableKind::Integer:
    case VariableKind::Number:
    case VariableKind::Text:
    case VariableKind::Duration:
    case VariableKind::Velocity:
    case VariableKind::Acceleration:
        return true;
    case VariableKind::Enumeration:
    case VariableKind::Other:
        break;
    }
    return false;
}

namespace {

// 中身をそのまま符号なしの数として読む
std::uint64_t RawOf(const VariableRef &ref, std::size_t size) {
    std::uint64_t value = 0;
    std::memcpy(&value, ref.data, size > sizeof(value) ? sizeof(value) : size);
    return value;
}

template <int kExponent> bool EditRate(VariableRef ref) {
    auto *quantity = ref.As<RateQuantity<kExponent>>();
    if (quantity == nullptr) {
        return false;
    }
    double value = quantity->Value();
    if (ImGui::InputDouble("##value", &value)) {
        *quantity = RateQuantity<kExponent>{value};
        return true;
    }
    return false;
}

// 型ごとの書き口
// 扱えない型はfalseを返す
bool EditValue(const VariableDescriptor &descriptor, VariableRef ref) {
    if (auto *value = ref.As<bool>()) {
        return ImGui::Checkbox("##value", value);
    }
    if (auto *value = ref.As<double>()) {
        return ImGui::InputDouble("##value", value);
    }
    if (auto *value = ref.As<float>()) {
        return ImGui::InputFloat("##value", value);
    }
    if (auto *value = ref.As<std::string>()) {
        return ImGui::InputText("##value", value);
    }
    if (auto *value = ref.As<int>()) {
        return ImGui::InputInt("##value", value);
    }
    if (auto *value = ref.As<unsigned int>()) {
        return ImGui::InputScalar("##value", ImGuiDataType_U32, value);
    }
    if (auto *value = ref.As<long long>()) {
        return ImGui::InputScalar("##value", ImGuiDataType_S64, value);
    }
    if (auto *value = ref.As<unsigned long long>()) {
        return ImGui::InputScalar("##value", ImGuiDataType_U64, value);
    }
    if (auto *value = ref.As<short>()) {
        return ImGui::InputScalar("##value", ImGuiDataType_S16, value);
    }
    if (auto *value = ref.As<unsigned short>()) {
        return ImGui::InputScalar("##value", ImGuiDataType_U16, value);
    }
    if (auto *value = ref.As<signed char>()) {
        return ImGui::InputScalar("##value", ImGuiDataType_S8, value);
    }
    if (auto *value = ref.As<unsigned char>()) {
        return ImGui::InputScalar("##value", ImGuiDataType_U8, value);
    }
    if (EditRate<1>(ref) || EditRate<-1>(ref) || EditRate<-2>(ref)) {
        return true;
    }

    ImGui::TextDisabled("%zu bytes", descriptor.size);
    return false;
}

} // namespace

void DrawInspector(const char *title, bool &open, Context &context,
                   DebugSelection &selection) {
    if (!open) {
        return;
    }

    Instances &instances = context.instances;
    InstanceId doomed = InstanceId::None;

    bool showing = open;
    if (ImGui::Begin(title, &showing)) {
        GameObject *object = instances.Find(selection.instance);
        if (object == nullptr) {
            ImGui::TextDisabled("no selection");
        } else {
            ImGui::Text("%s  #%u", TypeNameOf(*object).c_str(),
                        static_cast<unsigned>(selection.instance));
            ImGui::SameLine();
            if (ImGui::SmallButton("destroy")) {
                doomed = selection.instance;
            }
            ImGui::Separator();

            if (ImGui::BeginTable("##variables", 2,
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                instances.WithVariables(
                    selection.instance,
                    [](const VariableDescriptor &descriptor, VariableRef ref) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::PushID(descriptor.name.data(),
                                      descriptor.name.data() + descriptor.name.size());

                        ImGui::TextUnformatted(descriptor.name.data(),
                                               descriptor.name.data() + descriptor.name.size());
                        if (descriptor.original != descriptor.name &&
                            ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%.*s",
                                              static_cast<int>(descriptor.original.size()),
                                              descriptor.original.data());
                        }

                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-1.0f);
                        if (Editable(descriptor.kind)) {
                            EditValue(descriptor, ref);
                        } else {
                            ImGui::TextDisabled("%llu", static_cast<unsigned long long>(
                                                            RawOf(ref, descriptor.size)));
                        }
                        ImGui::PopID();
                    });

                ImGui::EndTable();
            }
        }
    }
    ImGui::End();
    open = showing;

    if (doomed != InstanceId::None) {
        DestroyInstance(context, selection, doomed);
    }
}

namespace {

// 種類ごとに意味のある値だけを出す
void ShapeOf(const DrawList &list, const DrawCommand &command) {
    switch (command.kind) {
    case DrawKind::Sprite:
        ImGui::Text("image %u frame %u scale %.2f,%.2f rot %.1f",
                    static_cast<unsigned>(command.image), command.frame, command.scaleX,
                    command.scaleY, command.rotation);
        if (command.usePart) {
            ImGui::SameLine();
            ImGui::TextDisabled("part %.0f,%.0f %.0fx%.0f", command.partX, command.partY,
                                command.partWidth, command.partHeight);
        }
        break;
    case DrawKind::Rectangle:
    case DrawKind::RoundRectangle:
        ImGui::Text("%.0fx%.0f radius %.1f %s", command.width, command.height, command.radius,
                    command.filled ? "filled" : "outline");
        break;
    case DrawKind::Line:
        ImGui::Text("to %.0f,%.0f width %.1f", command.secondX, command.secondY,
                    command.lineWidth);
        break;
    case DrawKind::Circle:
        ImGui::Text("radius %.1f segments %u %s", command.radius, command.segments,
                    command.filled ? "filled" : "outline");
        break;
    case DrawKind::Ellipse:
        ImGui::Text("%.0fx%.0f segments %u %s", command.width, command.height,
                    command.segments, command.filled ? "filled" : "outline");
        break;
    case DrawKind::Triangle:
        ImGui::Text("%.0f,%.0f %.0f,%.0f %s", command.secondX, command.secondY,
                    command.thirdX, command.thirdY,
                    command.filled ? "filled" : "outline");
        break;
    case DrawKind::Text: {
        const std::string_view text = list.TextOf(command);
        ImGui::Text("\"%.*s\"", static_cast<int>(text.size()), text.data());
        break;
    }
    case DrawKind::Target:
        ImGui::Text("image %u", static_cast<unsigned>(command.image));
        break;
    }
}

} // namespace

void DrawCommandList(const char *title, bool &open, const DrawList &list,
                     const DebugSelection &selection, bool &onlySelected) {
    if (!open) {
        return;
    }

    bool showing = open;
    if (ImGui::Begin(title, &showing)) {
        ImGui::Text("%zu commands  hash %016llx", list.Size(),
                    static_cast<unsigned long long>(list.Hash()));
        ImGui::Checkbox("selected only", &onlySelected);
        ImGui::Separator();

        if (ImGui::BeginTable("##commands", 6,
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("kind", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("from", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("at", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("color", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("shape", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            const Span<const DrawCommand> commands = list.Commands();
            for (std::size_t i = 0; i < commands.size(); ++i) {
                const DrawCommand &command = commands[i];
                if (onlySelected && command.source != selection.instance) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(i));

                ImGui::TableNextColumn();
                ImGui::Text("%zu", i);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(NameOf(command.kind));

                ImGui::TableNextColumn();
                if (command.source == InstanceId::None) {
                    ImGui::TextDisabled("-");
                } else {
                    ImGui::Text("#%u", static_cast<unsigned>(command.source));
                }

                ImGui::TableNextColumn();
                ImGui::Text("%.0f,%.0f", command.x, command.y);

                ImGui::TableNextColumn();
                const ImVec4 shown{command.color.red / 255.0f, command.color.green / 255.0f,
                                   command.color.blue / 255.0f,
                                   static_cast<float>(command.alpha)};
                ImGui::ColorButton("##color", shown,
                                   ImGuiColorEditFlags_NoTooltip |
                                       ImGuiColorEditFlags_NoDragDrop,
                                   ImVec2{14.0f, 14.0f});
                ImGui::SameLine();
                ImGui::TextDisabled("%s", NameOf(command.blend));

                ImGui::TableNextColumn();
                ShapeOf(list, command);

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
    open = showing;
}

#else

void DrawHierarchy(const char *, bool &, Context &, std::string_view, DebugSelection &) {}
void DrawCommandList(const char *, bool &, const DrawList &, const DebugSelection &,
                     bool &) {}
void DrawInspector(const char *, bool &, Context &, DebugSelection &) {}

bool Editable(VariableKind kind) {
    switch (kind) {
    case VariableKind::Boolean:
    case VariableKind::Integer:
    case VariableKind::Number:
    case VariableKind::Text:
    case VariableKind::Duration:
    case VariableKind::Velocity:
    case VariableKind::Acceleration:
        return true;
    case VariableKind::Enumeration:
    case VariableKind::Other:
        break;
    }
    return false;
}

#endif

} // namespace TellerEngine::Base::Platform
