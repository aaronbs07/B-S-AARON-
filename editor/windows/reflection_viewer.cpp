#include "reflection_viewer.hpp"
#include "reflection/property.hpp"
#include "core/logger.hpp"

// ImGui forward-declaration guard: include only if IMGUI is available
// The editor uses ImGui but does not expose it via a central header.
// We guard the render body so it compiles cleanly even without a full ImGui setup.
#if defined(IMGUI_VERSION)
#   define KE_IMGUI_AVAILABLE 1
#else
#   define KE_IMGUI_AVAILABLE 0
#endif

#if KE_IMGUI_AVAILABLE
#   include <imgui.h>
#endif

#include <algorithm>
#include <cstdio>

namespace KumariEngine::Editor {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
ReflectionViewerWindow::ReflectionViewerWindow()
    : EditorWindow("Reflection Viewer", true) {}

// ---------------------------------------------------------------------------
// Initialize / Update
// ---------------------------------------------------------------------------
void ReflectionViewerWindow::Initialize() {
    Core::Logger::Info("Editor", "Reflection Viewer initialized");
    m_cacheDirty = true;
}

void ReflectionViewerWindow::Update(float deltaTime) {
    m_refreshTimer += deltaTime;
    if (m_refreshTimer >= 2.0f) {   // auto-refresh every 2 seconds
        m_refreshTimer = 0.0f;
        m_cacheDirty   = true;
    }
    if (m_cacheDirty) {
        m_typeCache  = Reflection::TypeRegistry::Get().GetAllTypes();
        m_cacheDirty = false;
    }
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------
const char* ReflectionViewerWindow::CategoryToString(Reflection::TypeCategory cat) {
    using C = Reflection::TypeCategory;
    switch (cat) {
        case C::Primitive:  return "Primitive";
        case C::Struct:     return "Struct";
        case C::Component:  return "Component";
        case C::Enum:       return "Enum";
        case C::Asset:      return "Asset";
        default:            return "Unknown";
    }
}

const char* ReflectionViewerWindow::KindToString(Reflection::PropertyKind kind) {
    using K = Reflection::PropertyKind;
    switch (kind) {
        case K::Bool:     return "bool";
        case K::Int8:     return "int8";
        case K::Int16:    return "int16";
        case K::Int32:    return "int32";
        case K::Int64:    return "int64";
        case K::UInt8:    return "uint8";
        case K::UInt16:   return "uint16";
        case K::UInt32:   return "uint32";
        case K::UInt64:   return "uint64";
        case K::Float:    return "float";
        case K::Double:   return "double";
        case K::String:   return "string";
        case K::Vec2:     return "vec2";
        case K::Vec3:     return "vec3";
        case K::Vec4:     return "vec4";
        case K::Quat:     return "quat";
        case K::Mat4:     return "mat4";
        case K::Enum:     return "enum";
        case K::Struct:   return "struct";
        case K::Entity:   return "Entity";
        case K::VectorOf: return "vector<>";
        default:          return "?";
    }
}

// ---------------------------------------------------------------------------
// RenderUI — main ImGui pass
// ---------------------------------------------------------------------------
void ReflectionViewerWindow::RenderUI() {
    if (!m_isOpen) return;

#if KE_IMGUI_AVAILABLE
    ImGui::SetNextWindowSize(ImVec2(820, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(m_title.c_str(), &m_isOpen,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // ---- Header stats bar ----
    size_t totalTypes  = Reflection::TypeRegistry::Get().TypeCount();
    size_t totalComps  = Reflection::TypeRegistry::Get().ComponentCount();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
    ImGui::Text("Registered Types: %zu   |   Components: %zu",
                totalTypes, totalComps);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 30);

    if (ImGui::SmallButton("Refresh")) m_cacheDirty = true;
    ImGui::Separator();

    // ---- Filters ----
    ImGui::SetNextItemWidth(260);
    ImGui::InputText("##filter", m_filterBuf, sizeof(m_filterBuf));
    ImGui::SameLine();
    ImGui::Checkbox("Components Only", &m_showComponentsOnly);
    ImGui::SameLine();
    ImGui::Checkbox("Show Hidden Props", &m_showHiddenProps);
    ImGui::Separator();

    // ---- Two-column layout: list | detail ----
    ImGui::Columns(2, "ReflectionColumns", true);
    ImGui::SetColumnWidth(0, 250.0f);

    RenderTypeList();

    ImGui::NextColumn();

    if (m_selectedType) {
        RenderTypeDetail(m_selectedType);
    } else {
        ImGui::TextDisabled("Select a type on the left to inspect it.");
    }

    ImGui::Columns(1);
    ImGui::End();

#else
    // No ImGui: emit to log once
    static bool logged = false;
    if (!logged) {
        logged = true;
        Core::Logger::Info("Editor",
            "ReflectionViewerWindow: ImGui not available — logging registry to console");
        for (auto* t : m_typeCache) {
            Core::Logger::Info("Reflection",
                "  Type: %s  Size=%zu  Props=%zu  Cat=%s",
                t->name.c_str(), t->size,
                t->properties.size(),
                CategoryToString(t->category));
        }
    }
#endif
}

// ---------------------------------------------------------------------------
// RenderTypeList — left column
// ---------------------------------------------------------------------------
void ReflectionViewerWindow::RenderTypeList() {
#if KE_IMGUI_AVAILABLE
    ImGui::BeginChild("TypeList", ImVec2(0, 0), false);

    std::string filterLower(m_filterBuf);
    std::transform(filterLower.begin(), filterLower.end(),
                   filterLower.begin(), ::tolower);

    for (auto* info : m_typeCache) {
        if (!info) continue;

        // Filter: component-only
        if (m_showComponentsOnly && !info->IsComponent()) continue;

        // Filter: name substring
        if (!filterLower.empty()) {
            std::string nameLower = info->name;
            std::transform(nameLower.begin(), nameLower.end(),
                           nameLower.begin(), ::tolower);
            if (nameLower.find(filterLower) == std::string::npos) continue;
        }

        // Category badge colour
        ImVec4 badgeColor = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
        if (info->IsComponent()) badgeColor = ImVec4(0.3f, 0.7f, 1.0f, 1.0f);

        // Selectable row
        bool isSelected = (m_selectedType == info);
        if (ImGui::Selectable(info->name.c_str(), isSelected,
                              ImGuiSelectableFlags_None,
                              ImVec2(0, 18))) {
            m_selectedType = info;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Size: %zu bytes  |  Align: %zu  |  Props: %zu",
                        info->size, info->alignment, info->properties.size());
            ImGui::EndTooltip();
        }
        ImGui::SameLine(200);
        ImGui::PushStyleColor(ImGuiCol_Text, badgeColor);
        ImGui::TextUnformatted(CategoryToString(info->category));
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
#endif
}

// ---------------------------------------------------------------------------
// RenderTypeDetail — right column
// ---------------------------------------------------------------------------
void ReflectionViewerWindow::RenderTypeDetail(const Reflection::TypeInfo* info) {
    if (!info) return;
#if KE_IMGUI_AVAILABLE
    // ---- Type header ----
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
    ImGui::Text("[ %s ]  %s",
                CategoryToString(info->category),
                info->name.c_str());
    ImGui::PopStyleColor();

    ImGui::Text("Size: %zu bytes  |  Alignment: %zu  |  TypeID: 0x%016llx",
                info->size, info->alignment,
                static_cast<unsigned long long>(info->typeId));

    // Serialized size estimate (sum of non-transient property sizes)
    size_t estSize = 0;
    for (const auto& p : info->properties) {
        if (!p.IsTransient()) estSize += p.size;
    }
    ImGui::Text("Est. serialized size: ~%zu bytes", estSize);

    // Type-level metadata
    if (!info->metadata.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("Type Metadata:");
        for (const auto& [k, v] : info->metadata) {
            ImGui::Text("  %s = ...", k.c_str());
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Properties:");
    ImGui::Spacing();

    // ---- Property table ----
    if (ImGui::BeginTable("PropTable", 6,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg   |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit,
        ImVec2(0, 0)))
    {
        ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type",    ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Offset",  ImGuiTableColumnFlags_WidthFixed, 58);
        ImGui::TableSetupColumn("Size",    ImGuiTableColumnFlags_WidthFixed, 48);
        ImGui::TableSetupColumn("Flags",   ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Metadata",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& prop : info->properties) {
            if (!m_showHiddenProps && prop.IsHidden()) continue;
            RenderPropertyRow(prop);
        }
        ImGui::EndTable();
    }
#endif
}

// ---------------------------------------------------------------------------
// RenderPropertyRow
// ---------------------------------------------------------------------------
void ReflectionViewerWindow::RenderPropertyRow(
    [[maybe_unused]] const Reflection::PropertyDescriptor& prop)
{
#if KE_IMGUI_AVAILABLE
    ImGui::TableNextRow();

    // Name
    ImGui::TableSetColumnIndex(0);
    ImVec4 nameColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    if (prop.IsReadOnly())  nameColor = ImVec4(0.7f, 0.7f, 1.0f, 1.0f);
    if (prop.IsTransient()) nameColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, nameColor);
    ImGui::TextUnformatted(prop.name.c_str());
    ImGui::PopStyleColor();

    // Type
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.6f, 1.0f));
    ImGui::TextUnformatted(KindToString(prop.kind));
    ImGui::PopStyleColor();

    // Offset
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("+%zu", prop.offset);

    // Size
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%zu", prop.size);

    // Flags
    ImGui::TableSetColumnIndex(4);
    {
        char flagStr[64] = {};
        int pos = 0;
        if (prop.IsReadOnly())  pos += std::snprintf(flagStr + pos, sizeof(flagStr) - pos, "RO ");
        if (prop.IsHidden())    pos += std::snprintf(flagStr + pos, sizeof(flagStr) - pos, "Hid ");
        if (prop.IsTransient()) pos += std::snprintf(flagStr + pos, sizeof(flagStr) - pos, "Transient");
        if (pos == 0) std::snprintf(flagStr, sizeof(flagStr), "-");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.6f, 0.3f, 1.0f));
        ImGui::TextUnformatted(flagStr);
        ImGui::PopStyleColor();
    }

    // Metadata
    ImGui::TableSetColumnIndex(5);
    {
        bool first = true;
        for (const auto& [k, v] : prop.metadata) {
            if (!first) ImGui::SameLine();
            first = false;
            ImGui::TextDisabled("[%s]", k.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                if (v.IsBool())   ImGui::Text("%s: %s", k.c_str(), v.AsBool() ? "true" : "false");
                else if (v.IsInt())    ImGui::Text("%s: %lld", k.c_str(), static_cast<long long>(v.AsInt()));
                else if (v.IsDouble()) ImGui::Text("%s: %.4f", k.c_str(), v.AsDouble());
                else if (v.IsString()) ImGui::Text("%s: %s", k.c_str(), v.AsString().c_str());
                ImGui::EndTooltip();
            }
        }
        if (prop.metadata.empty()) ImGui::TextDisabled("-");
    }
#endif
}

} // namespace KumariEngine::Editor
