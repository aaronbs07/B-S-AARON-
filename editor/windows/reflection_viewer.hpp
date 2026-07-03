#pragma once
#include "editor/window_system.hpp"
#include "reflection/type_registry.hpp"
#include "reflection/serializer.hpp"
#include <string>
#include <vector>

namespace KumariEngine::Editor {

// ---------------------------------------------------------------------------
// ReflectionViewerWindow
//
// ImGui panel displaying all types registered in the TypeRegistry:
//   - Total type count, component count
//   - Filterable type list
//   - Per-type: name, typeId, size, alignment, category, property table
//   - Per-property: name, type, offset, size, flags, metadata keys
//   - Serialized-size estimator (sum of property sizes)
// ---------------------------------------------------------------------------
class ReflectionViewerWindow : public EditorWindow {
public:
    ReflectionViewerWindow();

    void Initialize() override;
    void Update(float deltaTime) override;
    void RenderUI() override;

private:
    void RenderTypeList();
    void RenderTypeDetail(const Reflection::TypeInfo* info);
    void RenderPropertyRow(const Reflection::PropertyDescriptor& prop);

    static const char* CategoryToString(Reflection::TypeCategory cat);
    static const char* KindToString(Reflection::PropertyKind kind);

    char m_filterBuf[256]    = {};
    bool m_showComponentsOnly = false;
    bool m_showHiddenProps    = false;

    const Reflection::TypeInfo* m_selectedType = nullptr;

    // Cached type list (refreshed on demand)
    std::vector<const Reflection::TypeInfo*> m_typeCache;
    bool m_cacheDirty = true;
    float m_refreshTimer = 0.0f;
};

} // namespace KumariEngine::Editor
