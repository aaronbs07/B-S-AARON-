#include "inspector_window_reflection.hpp"
#include "editor/window_system.hpp"
#include "editor/undo_redo.hpp"
#include "core/logger.hpp"
#include "scene/transform_component.hpp"
#include "camera/camera_component.hpp"
#include "lighting/light_component.hpp"
#include "renderer/mesh_renderer_component.hpp"
#include "animation/animation_system.hpp"
#include "ai/ai_system.hpp"
#include "scripting/visual_scripting.hpp"
#include "audio/audio_system.hpp"
#include "particle/particle_system.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/GameplayTags.hpp"
#include "physics/physics_components.hpp"
#include "scripting/script_component.hpp"
#include "timeline/timeline.hpp"
#include <unordered_map>
#include <functional>

namespace KumariEngine::Editor {

// Map of dynamic component getters
static std::unordered_map<std::string, std::function<void*(ECS::Registry*, ECS::Entity)>> s_componentGetters;

static void InitializeGettersIfNeeded() {
    if (!s_componentGetters.empty()) return;

    s_componentGetters["TransformComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Scene::TransformComponent>(e) ? &r->GetComponent<Scene::TransformComponent>(e) : nullptr;
    };
    s_componentGetters["CameraComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Camera::CameraComponent>(e) ? &r->GetComponent<Camera::CameraComponent>(e) : nullptr;
    };
    s_componentGetters["LightComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Lighting::LightComponent>(e) ? &r->GetComponent<Lighting::LightComponent>(e) : nullptr;
    };
    s_componentGetters["MeshRendererComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Renderer::MeshRendererComponent>(e) ? &r->GetComponent<Renderer::MeshRendererComponent>(e) : nullptr;
    };
    s_componentGetters["AnimationComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Animation::AnimationComponent>(e) ? &r->GetComponent<Animation::AnimationComponent>(e) : nullptr;
    };
    s_componentGetters["NavMeshComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<AI::NavMeshComponent>(e) ? &r->GetComponent<AI::NavMeshComponent>(e) : nullptr;
    };
    s_componentGetters["BehaviorTreeComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<AI::BehaviorTreeComponent>(e) ? &r->GetComponent<AI::BehaviorTreeComponent>(e) : nullptr;
    };
    s_componentGetters["BlackboardComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<AI::BlackboardComponent>(e) ? &r->GetComponent<AI::BlackboardComponent>(e) : nullptr;
    };
    s_componentGetters["PerceptionComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<AI::PerceptionComponent>(e) ? &r->GetComponent<AI::PerceptionComponent>(e) : nullptr;
    };
    s_componentGetters["NavigationAgentComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<AI::NavigationAgentComponent>(e) ? &r->GetComponent<AI::NavigationAgentComponent>(e) : nullptr;
    };
    s_componentGetters["AIComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<AI::AIComponent>(e) ? &r->GetComponent<AI::AIComponent>(e) : nullptr;
    };
    s_componentGetters["VisualScriptingComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Scripting::VisualScriptingComponent>(e) ? &r->GetComponent<Scripting::VisualScriptingComponent>(e) : nullptr;
    };
    s_componentGetters["AudioSourceComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Audio::AudioSourceComponent>(e) ? &r->GetComponent<Audio::AudioSourceComponent>(e) : nullptr;
    };
    s_componentGetters["AudioListenerComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Audio::AudioListenerComponent>(e) ? &r->GetComponent<Audio::AudioListenerComponent>(e) : nullptr;
    };
    s_componentGetters["ParticleSystemComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Particle::ParticleSystemComponent>(e) ? &r->GetComponent<Particle::ParticleSystemComponent>(e) : nullptr;
    };
    s_componentGetters["HealthComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Gameplay::HealthComponent>(e) ? &r->GetComponent<Gameplay::HealthComponent>(e) : nullptr;
    };
    s_componentGetters["DamageComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Gameplay::DamageComponent>(e) ? &r->GetComponent<Gameplay::DamageComponent>(e) : nullptr;
    };
    s_componentGetters["TeamComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Gameplay::TeamComponent>(e) ? &r->GetComponent<Gameplay::TeamComponent>(e) : nullptr;
    };
    s_componentGetters["InteractionComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Gameplay::InteractionComponent>(e) ? &r->GetComponent<Gameplay::InteractionComponent>(e) : nullptr;
    };
    s_componentGetters["GameplayTagsComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Gameplay::GameplayTagsComponent>(e) ? &r->GetComponent<Gameplay::GameplayTagsComponent>(e) : nullptr;
    };
    s_componentGetters["SpawnPointComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Gameplay::SpawnPointComponent>(e) ? &r->GetComponent<Gameplay::SpawnPointComponent>(e) : nullptr;
    };
    s_componentGetters["PlayerControllerComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Gameplay::PlayerControllerComponent>(e) ? &r->GetComponent<Gameplay::PlayerControllerComponent>(e) : nullptr;
    };
    s_componentGetters["PlayerStateComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Gameplay::PlayerStateComponent>(e) ? &r->GetComponent<Gameplay::PlayerStateComponent>(e) : nullptr;
    };
    s_componentGetters["GameStateComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Gameplay::GameStateComponent>(e) ? &r->GetComponent<Gameplay::GameStateComponent>(e) : nullptr;
    };
    s_componentGetters["PhysicsComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Physics::PhysicsComponent>(e) ? &r->GetComponent<Physics::PhysicsComponent>(e) : nullptr;
    };
    s_componentGetters["ScriptComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<ECS::ScriptComponent>(e) ? &r->GetComponent<ECS::ScriptComponent>(e) : nullptr;
    };
    s_componentGetters["CinematicPlayerComponent"] = [](ECS::Registry* r, ECS::Entity e) -> void* {
        return r->HasComponent<Timeline::CinematicPlayerComponent>(e) ? &r->GetComponent<Timeline::CinematicPlayerComponent>(e) : nullptr;
    };
}

void* ModifyPropertyCommand::GetComponentPtr() {
    InitializeGettersIfNeeded();
    if (!m_registry || !m_typeInfo) return nullptr;

    auto it = s_componentGetters.find(m_typeInfo->name);
    if (it != s_componentGetters.end()) {
        return it->second(m_registry, m_entity);
    }
    return nullptr;
}

void ModifyPropertyCommand::Execute() {
    void* comp = GetComponentPtr();
    if (comp) {
        uint8_t* dest = static_cast<uint8_t*>(comp) + m_propDesc.offset;
        std::memcpy(dest, m_newBytes.data(), m_propDesc.size);
        Core::Logger::Info("UndoSystem", "Executed: Set property '%s::%s'", m_typeInfo->name.c_str(), m_propDesc.name.c_str());
    }
}

void ModifyPropertyCommand::Undo() {
    void* comp = GetComponentPtr();
    if (comp) {
        uint8_t* dest = static_cast<uint8_t*>(comp) + m_propDesc.offset;
        std::memcpy(dest, m_oldBytes.data(), m_propDesc.size);
        Core::Logger::Info("UndoSystem", "Undone: Set property '%s::%s'", m_typeInfo->name.c_str(), m_propDesc.name.c_str());
    }
}

std::string ModifyPropertyCommand::GetDescription() const {
    return "Modify Property " + m_typeInfo->name + "::" + m_propDesc.name;
}

void ReflectionPropertyEditor::DrawComponentProperties(ECS::Registry* registry, ECS::Entity entity, const Reflection::TypeInfo* typeInfo, void* componentPtr) {
    if (!typeInfo || !componentPtr) return;

    Core::Logger::Info("Inspector", "Component: %s", typeInfo->name.c_str());
    for (const auto& prop : typeInfo->properties) {
        if (prop.IsHidden()) continue;
        DrawProperty(registry, entity, typeInfo, prop, componentPtr);
    }
}

void ReflectionPropertyEditor::DrawProperty(ECS::Registry* registry, ECS::Entity entity, const Reflection::TypeInfo* typeInfo, const Reflection::PropertyDescriptor& prop, void* componentPtr) {
    (void)registry;
    (void)entity;
    (void)typeInfo;
    std::string displayName = prop.name;
    if (prop.HasMeta(Reflection::Meta::DisplayName)) {
        displayName = prop.GetMeta(Reflection::Meta::DisplayName)->AsString();
    }

    uint8_t* propAddr = static_cast<uint8_t*>(componentPtr) + prop.offset;

    switch (prop.kind) {
        case Reflection::PropertyKind::Bool: {
            bool val = *reinterpret_cast<bool*>(propAddr);
            Core::Logger::Info("Inspector", "  %s [bool]: %s", displayName.c_str(), val ? "True" : "False");
            break;
        }
        case Reflection::PropertyKind::Int8:
        case Reflection::PropertyKind::Int16:
        case Reflection::PropertyKind::Int32:
        case Reflection::PropertyKind::Int64: {
            int64_t val = 0;
            if (prop.size == 1) val = *reinterpret_cast<int8_t*>(propAddr);
            else if (prop.size == 2) val = *reinterpret_cast<int16_t*>(propAddr);
            else if (prop.size == 4) val = *reinterpret_cast<int32_t*>(propAddr);
            else if (prop.size == 8) val = *reinterpret_cast<int64_t*>(propAddr);
            Core::Logger::Info("Inspector", "  %s [int]: %lld", displayName.c_str(), val);
            break;
        }
        case Reflection::PropertyKind::UInt8:
        case Reflection::PropertyKind::UInt16:
        case Reflection::PropertyKind::UInt32:
        case Reflection::PropertyKind::UInt64: {
            uint64_t val = 0;
            if (prop.size == 1) val = *reinterpret_cast<uint8_t*>(propAddr);
            else if (prop.size == 2) val = *reinterpret_cast<uint16_t*>(propAddr);
            else if (prop.size == 4) val = *reinterpret_cast<uint32_t*>(propAddr);
            else if (prop.size == 8) val = *reinterpret_cast<uint64_t*>(propAddr);
            Core::Logger::Info("Inspector", "  %s [uint]: %llu", displayName.c_str(), val);
            break;
        }
        case Reflection::PropertyKind::Float: {
            float val = *reinterpret_cast<float*>(propAddr);
            Core::Logger::Info("Inspector", "  %s [float]: %.4f", displayName.c_str(), val);
            break;
        }
        case Reflection::PropertyKind::Double: {
            double val = *reinterpret_cast<double*>(propAddr);
            Core::Logger::Info("Inspector", "  %s [double]: %.4f", displayName.c_str(), val);
            break;
        }
        case Reflection::PropertyKind::String: {
            std::string val = *reinterpret_cast<std::string*>(propAddr);
            Core::Logger::Info("Inspector", "  %s [string]: \"%s\"", displayName.c_str(), val.c_str());
            break;
        }
        case Reflection::PropertyKind::Vec2: {
            glm::vec2 val = *reinterpret_cast<glm::vec2*>(propAddr);
            Core::Logger::Info("Inspector", "  %s [vec2]: (%.2f, %.2f)", displayName.c_str(), val.x, val.y);
            break;
        }
        case Reflection::PropertyKind::Vec3: {
            glm::vec3 val = *reinterpret_cast<glm::vec3*>(propAddr);
            Core::Logger::Info("Inspector", "  %s [vec3]: (%.2f, %.2f, %.2f)", displayName.c_str(), val.x, val.y, val.z);
            break;
        }
        case Reflection::PropertyKind::Vec4: {
            glm::vec4 val = *reinterpret_cast<glm::vec4*>(propAddr);
            Core::Logger::Info("Inspector", "  %s [vec4]: (%.2f, %.2f, %.2f, %.2f)", displayName.c_str(), val.x, val.y, val.z, val.w);
            break;
        }
        case Reflection::PropertyKind::Quat: {
            glm::quat val = *reinterpret_cast<glm::quat*>(propAddr);
            glm::vec3 euler = glm::eulerAngles(val) * 57.29578f; // to degrees
            Core::Logger::Info("Inspector", "  %s [quat/euler]: (%.1f, %.1f, %.1f)", displayName.c_str(), euler.x, euler.y, euler.z);
            break;
        }
        case Reflection::PropertyKind::Enum: {
            int val = *reinterpret_cast<int*>(propAddr);
            Core::Logger::Info("Inspector", "  %s [enum]: %d", displayName.c_str(), val);
            break;
        }
        default:
            Core::Logger::Info("Inspector", "  %s [struct/other]", displayName.c_str());
            break;
    }
}

void ReflectionPropertyEditor::RenderEntityComponents(ECS::Registry* registry, ECS::Entity entity) {
    if (!registry || !registry->IsAlive(entity)) return;

    InitializeGettersIfNeeded();
    auto allComps = Reflection::TypeRegistry::Get().GetAllComponents();
    for (const auto* typeInfo : allComps) {
        if (!typeInfo) continue;
        auto it = s_componentGetters.find(typeInfo->name);
        if (it != s_componentGetters.end()) {
            void* compPtr = it->second(registry, entity);
            if (compPtr != nullptr) {
                DrawComponentProperties(registry, entity, typeInfo, compPtr);
            }
        }
    }
}

} // namespace KumariEngine::Editor
