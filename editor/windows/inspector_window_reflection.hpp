#pragma once
#include "editor/undo_redo.hpp"
#include "reflection/reflection.hpp"
#include "reflection/type_registry.hpp"
#include <string>
#include <vector>

namespace KumariEngine::Editor {

// Generic undo/redo command for setting properties on components
class ModifyPropertyCommand : public Command {
public:
    ModifyPropertyCommand(ECS::Registry* registry, ECS::Entity entity, const Reflection::TypeInfo* typeInfo, 
                          const Reflection::PropertyDescriptor& propDesc, const std::vector<uint8_t>& oldBytes, const std::vector<uint8_t>& newBytes)
        : m_registry(registry), m_entity(entity), m_typeInfo(typeInfo), m_propDesc(propDesc), m_oldBytes(oldBytes), m_newBytes(newBytes) {}

    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;

private:
    void* GetComponentPtr();

    ECS::Registry* m_registry;
    ECS::Entity m_entity;
    const Reflection::TypeInfo* m_typeInfo;
    Reflection::PropertyDescriptor m_propDesc;
    std::vector<uint8_t> m_oldBytes;
    std::vector<uint8_t> m_newBytes;
};

class ReflectionPropertyEditor {
public:
    static void RenderEntityComponents(ECS::Registry* registry, ECS::Entity entity);
    static void DrawComponentProperties(ECS::Registry* registry, ECS::Entity entity, const Reflection::TypeInfo* typeInfo, void* componentPtr);
    static void DrawProperty(ECS::Registry* registry, ECS::Entity entity, const Reflection::TypeInfo* typeInfo, const Reflection::PropertyDescriptor& prop, void* componentPtr);
};

} // namespace KumariEngine::Editor
