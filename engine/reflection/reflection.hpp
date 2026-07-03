#pragma once
// ---------------------------------------------------------------------------
// reflection.hpp — Master include for the Kumari Engine Reflection System
//
// Usage example:
//
//   REFLECT_COMPONENT_BEGIN(KumariEngine::Gameplay::HealthComponent, "HealthComponent")
//       REFLECT_FIELD(KumariEngine::Gameplay::HealthComponent, currentHealth)
//       REFLECT_FIELD_META(KumariEngine::Gameplay::HealthComponent, maxHealth,
//           (MetaAttribute(0.0), MetaAttribute(9999.0)))   // not used in this form
//   REFLECT_END()
//
//   — OR — verbose form with per-property metadata:
//
//   REFLECT_COMPONENT_BEGIN(KumariEngine::Gameplay::HealthComponent, "HealthComponent")
//       REFLECT_PROP_BEGIN(KumariEngine::Gameplay::HealthComponent, currentHealth)
//           REFLECT_META("DisplayName", "Current Health")
//           REFLECT_META("Min", 0.0)
//       REFLECT_PROP_COMMIT(KumariEngine::Gameplay::HealthComponent, currentHealth)
//   REFLECT_END()
//
// ---------------------------------------------------------------------------
#include "reflection/metadata.hpp"
#include "reflection/type_info.hpp"
#include "reflection/property.hpp"
#include "reflection/type_registry.hpp"

// ===========================================================================
// Unique-name helper (avoids symbol collisions when header included multiple times)
// ===========================================================================
#define KUMARI_REFLECT_CONCAT_IMPL(a, b) a##b
#define KUMARI_REFLECT_CONCAT(a, b)      KUMARI_REFLECT_CONCAT_IMPL(a, b)
#define KUMARI_REFLECT_UNIQUE(base)      KUMARI_REFLECT_CONCAT(base, __LINE__)

// ===========================================================================
// REFLECT_TYPE_BEGIN / REFLECT_COMPONENT_BEGIN
// ===========================================================================
#define REFLECT_TYPE_BEGIN(FullType, StringName)                              \
    namespace {                                                               \
    struct KUMARI_REFLECT_UNIQUE(KRR_) {                                      \
        KUMARI_REFLECT_UNIQUE(KRR_)() {                                       \
            using _RO = FullType;                                             \
            auto _b = ::KumariEngine::Reflection::TypeRegistry::Get()         \
                          .RegisterType<FullType>(StringName);

#define REFLECT_COMPONENT_BEGIN(FullType, StringName)                         \
    namespace {                                                               \
    struct KUMARI_REFLECT_UNIQUE(KRR_) {                                      \
        KUMARI_REFLECT_UNIQUE(KRR_)() {                                       \
            using _RO = FullType;                                             \
            auto _b = ::KumariEngine::Reflection::TypeRegistry::Get()         \
                          .RegisterComponent<FullType>(StringName);

// ===========================================================================
// REFLECT_FIELD(FullType, fieldName)
//   One-liner: register a field with no flags, no extra metadata.
// ===========================================================================
#define REFLECT_FIELD(FullType, fieldName)                                    \
            _b.Property(::KumariEngine::Reflection::MakeProperty<            \
                FullType, decltype(FullType::fieldName)>(                     \
                    #fieldName,                                               \
                    offsetof(FullType, fieldName)));

// ===========================================================================
// REFLECT_FIELD_FLAGS(FullType, fieldName, flags)
//   One-liner with explicit flags.
// ===========================================================================
#define REFLECT_FIELD_FLAGS(FullType, fieldName, flags)                       \
            _b.Property(::KumariEngine::Reflection::MakeProperty<            \
                FullType, decltype(FullType::fieldName)>(                     \
                    #fieldName,                                               \
                    offsetof(FullType, fieldName),                            \
                    (flags)));

// ===========================================================================
// Verbose property form — allows per-property metadata:
//
//   REFLECT_PROP_BEGIN(FullType, fieldName)
//       REFLECT_META("DisplayName", "My Field")
//       REFLECT_META("Min", 0.0)
//   REFLECT_PROP_COMMIT(FullType, fieldName)
// ===========================================================================
#define REFLECT_PROP_BEGIN(FullType, fieldName)                               \
            {                                                                 \
                ::KumariEngine::Reflection::MetadataMap _meta;                \
                ::KumariEngine::Reflection::PropertyFlags _pf =               \
                    ::KumariEngine::Reflection::PropertyFlags::None;

#define REFLECT_PROP_BEGIN_FLAGS(FullType, fieldName, flags)                  \
            {                                                                 \
                ::KumariEngine::Reflection::MetadataMap _meta;                \
                ::KumariEngine::Reflection::PropertyFlags _pf = (flags);

#define REFLECT_META(key, value)                                              \
                _meta[std::string(key)] =                                     \
                    ::KumariEngine::Reflection::MetaAttribute(value);

#define REFLECT_PROP_COMMIT(FullType, fieldName)                              \
                _b.Property(::KumariEngine::Reflection::MakeProperty<        \
                    FullType, decltype(FullType::fieldName)>(                 \
                        #fieldName,                                           \
                        offsetof(FullType, fieldName),                        \
                        _pf, std::move(_meta)));                              \
            }

// ===========================================================================
// Type-level metadata (use inside REFLECT_TYPE_BEGIN / REFLECT_COMPONENT_BEGIN)
// ===========================================================================
#define REFLECT_TYPE_META(key, value)                                         \
            _b.Meta(key, ::KumariEngine::Reflection::MetaAttribute(value));

// ===========================================================================
// REFLECT_END — closes the registration struct
// ===========================================================================
#define REFLECT_END()                                                         \
        }                                                                     \
    } KUMARI_REFLECT_UNIQUE(s_krr_inst_);                                     \
    } /* anonymous namespace */

// ===========================================================================
// Legacy aliases — kept for backward compat with existing component files
// These map the REFLECT_PROPERTY/REFLECT_PROPERTY_END form to the new names.
// ===========================================================================
// REFLECT_PROPERTY(FullType, field) ... REFLECT_PROPERTY_END()
#define REFLECT_PROPERTY(FullType, fieldName) REFLECT_PROP_BEGIN(FullType, fieldName)
#define REFLECT_PROPERTY_FLAGS(FullType, fieldName, flags) REFLECT_PROP_BEGIN_FLAGS(FullType, fieldName, flags)
#define REFLECT_PROPERTY_END() /* closed by REFLECT_PROP_COMMIT — see note below */

// NOTE: The old REFLECT_PROPERTY/REFLECT_PROPERTY_END pattern required fieldName
// in REFLECT_PROPERTY_END, which is not possible in standard C++ macros.
// Use REFLECT_PROP_BEGIN + REFLECT_PROP_COMMIT instead for the verbose form.
// REFLECT_PROPERTY + REFLECT_PROPERTY_END are kept as aliases but REFLECT_PROP_COMMIT
// must be added where the field name is needed.
// The GameplayComponents, TransformComponent, and CameraComponent files use
// REFLECT_PROPERTY + explicit REFLECT_PROPERTY_END that must be updated.
