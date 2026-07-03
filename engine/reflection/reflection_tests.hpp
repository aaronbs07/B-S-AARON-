#pragma once
// ---------------------------------------------------------------------------
// reflection_tests.hpp  --  Automated test suite for Kumari Engine Reflection
//
// Call RunReflectionTests() from engine startup (in debug builds) or a test
// runner. Returns 0 on success, number of failures otherwise.
// ---------------------------------------------------------------------------
#include "reflection/reflection.hpp"
#include "reflection/serializer.hpp"
#include "reflection/deserializer.hpp"
#include "core/logger.hpp"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace KumariEngine::Reflection::Tests {

// ===========================================================================
// Test structs
// ===========================================================================

struct SimpleStruct {
    bool        flag        = false;
    int32_t     count       = 0;
    float       speed       = 1.0f;
    double      precision   = 3.14159265358979;
    std::string label       = "default";
    glm::vec3   position    {0.0f};
    glm::quat   rotation    {1.0f, 0.0f, 0.0f, 0.0f};
};

enum class SimpleEnum : int32_t {
    Alpha = 0,
    Beta  = 1,
    Gamma = 2,
};

struct NestedStruct {
    SimpleStruct inner;
    float        extra = 0.0f;
};

// ===========================================================================
// Registration
// ===========================================================================

REFLECT_TYPE_BEGIN(KumariEngine::Reflection::Tests::SimpleStruct, "SimpleStruct")
    REFLECT_PROP_BEGIN(KumariEngine::Reflection::Tests::SimpleStruct, flag)
        REFLECT_META(KumariEngine::Reflection::Meta::DisplayName, "Flag")
    REFLECT_PROP_COMMIT(KumariEngine::Reflection::Tests::SimpleStruct, flag)

    REFLECT_PROP_BEGIN(KumariEngine::Reflection::Tests::SimpleStruct, count)
        REFLECT_META(KumariEngine::Reflection::Meta::Min, int64_t(0))
        REFLECT_META(KumariEngine::Reflection::Meta::Max, int64_t(9999))
    REFLECT_PROP_COMMIT(KumariEngine::Reflection::Tests::SimpleStruct, count)

    REFLECT_FIELD(KumariEngine::Reflection::Tests::SimpleStruct, speed)
    REFLECT_FIELD(KumariEngine::Reflection::Tests::SimpleStruct, precision)
    REFLECT_FIELD(KumariEngine::Reflection::Tests::SimpleStruct, label)
    REFLECT_FIELD(KumariEngine::Reflection::Tests::SimpleStruct, position)
    REFLECT_FIELD(KumariEngine::Reflection::Tests::SimpleStruct, rotation)
REFLECT_END()

REFLECT_TYPE_BEGIN(KumariEngine::Reflection::Tests::NestedStruct, "NestedStruct")
    REFLECT_FIELD(KumariEngine::Reflection::Tests::NestedStruct, inner)
    REFLECT_FIELD(KumariEngine::Reflection::Tests::NestedStruct, extra)
REFLECT_END()

// ===========================================================================
// Test helpers
// ===========================================================================

#define KE_TEST_ASSERT(cond, msg)                                         \
    if (!(cond)) {                                                         \
        Core::Logger::Error("ReflectionTest",                             \
            "FAIL [%s]: %s", __func__, msg);                              \
        return false;                                                      \
    }

#define KE_TEST_APPROX(a, b, eps)                                         \
    KE_TEST_ASSERT(std::abs((a) - (b)) < (eps),                           \
        "Value mismatch: " #a " vs " #b)

static bool PassTest(const char* name) {
    Core::Logger::Info("ReflectionTest", "  PASS  %s", name);
    return true;
}

// ===========================================================================
// T1 -- TypeRegistry: registration and lookup
// ===========================================================================
static bool TestTypeRegistration() {
    auto* info = TypeRegistry::Get().FindType("SimpleStruct");
    KE_TEST_ASSERT(info != nullptr, "SimpleStruct not found by name");
    KE_TEST_ASSERT(info->name == "SimpleStruct", "Wrong type name");
    KE_TEST_ASSERT(info->size == sizeof(SimpleStruct), "Wrong size");
    KE_TEST_ASSERT(info->properties.size() == 7, "Wrong property count");

    auto* byId = TypeRegistry::Get().FindType(TypeId<SimpleStruct>());
    KE_TEST_ASSERT(byId != nullptr, "Lookup by typeId failed");
    KE_TEST_ASSERT(byId->typeId == info->typeId, "typeId mismatch");
    return PassTest(__func__);
}

// ===========================================================================
// T2 -- Property access (offset-based read/write)
// ===========================================================================
static bool TestPropertyAccess() {
    SimpleStruct s;
    s.count = 42;
    s.speed = 3.14f;
    s.label = "hello";

    auto* info = TypeRegistry::Get().FindType("SimpleStruct");
    KE_TEST_ASSERT(info != nullptr, "Type not registered");

    auto* countProp = info->FindProperty("count");
    KE_TEST_ASSERT(countProp != nullptr, "Property 'count' not found");
    KE_TEST_ASSERT(countProp->GetRef<int32_t>(&s) == 42, "'count' read mismatch");

    countProp->Set<int32_t>(&s, 100);
    KE_TEST_ASSERT(s.count == 100, "Set<int32_t> failed");

    auto* labelProp = info->FindProperty("label");
    KE_TEST_ASSERT(labelProp != nullptr, "Property 'label' not found");
    KE_TEST_ASSERT(labelProp->GetRef<std::string>(&s) == "hello", "'label' read mismatch");
    return PassTest(__func__);
}

// ===========================================================================
// T3 -- Property flags and metadata
// ===========================================================================
static bool TestPropertyFlags() {
    auto* info = TypeRegistry::Get().FindType("SimpleStruct");
    KE_TEST_ASSERT(info != nullptr, "SimpleStruct not registered");

    auto* countProp = info->FindProperty("count");
    KE_TEST_ASSERT(countProp != nullptr, "Property 'count' not found");
    KE_TEST_ASSERT(countProp->HasMeta(Meta::Min), "count missing Min metadata");
    KE_TEST_ASSERT(countProp->HasMeta(Meta::Max), "count missing Max metadata");
    return PassTest(__func__);
}

// ===========================================================================
// T4 -- JSON roundtrip
// ===========================================================================
static bool TestJSONRoundtrip() {
    SimpleStruct original;
    original.flag      = true;
    original.count     = 777;
    original.speed     = 9.81f;
    original.precision = 2.718281828;
    original.label     = "Kumari Engine";
    original.position  = glm::vec3(1.0f, 2.0f, 3.0f);
    original.rotation  = glm::quat(0.707f, 0.0f, 0.707f, 0.0f);

    auto* info = TypeRegistry::Get().FindType("SimpleStruct");
    KE_TEST_ASSERT(info != nullptr, "Type not registered");

    std::string json = Serializer::SerializeToJSON(&original, *info);
    KE_TEST_ASSERT(!json.empty(), "JSON output empty");
    KE_TEST_ASSERT(json.find("\"count\"") != std::string::npos, "JSON missing 'count'");

    SimpleStruct restored;
    auto result = Deserializer::DeserializeFromJSON(&restored, *info, json);
    KE_TEST_ASSERT(result.success, "Deserialize failed");
    KE_TEST_ASSERT(restored.flag    == original.flag,  "flag mismatch");
    KE_TEST_ASSERT(restored.count   == original.count, "count mismatch");
    KE_TEST_APPROX(restored.speed,  original.speed,    1e-4f);
    KE_TEST_ASSERT(restored.label   == original.label, "label mismatch");
    KE_TEST_APPROX(restored.position.x, original.position.x, 1e-4f);
    KE_TEST_APPROX(restored.rotation.w, original.rotation.w, 1e-4f);
    return PassTest(__func__);
}

// ===========================================================================
// T5 -- Binary roundtrip
// ===========================================================================
static bool TestBinaryRoundtrip() {
    SimpleStruct original;
    original.flag      = false;
    original.count     = -999;
    original.speed     = 0.001f;
    original.label     = "binary_test";
    original.position  = glm::vec3(-5.0f, 10.0f, 0.5f);

    auto* info = TypeRegistry::Get().FindType("SimpleStruct");
    KE_TEST_ASSERT(info != nullptr, "Type not registered");

    auto blob = Serializer::SerializeToBinaryBlob(&original, *info);
    KE_TEST_ASSERT(!blob.empty(), "Binary blob is empty");

    SimpleStruct restored;
    auto result = Deserializer::DeserializeFromBinaryBlob(&restored, *info, blob);
    KE_TEST_ASSERT(result.success, "Binary deserialize failed");
    KE_TEST_ASSERT(restored.flag  == original.flag,  "Binary flag mismatch");
    KE_TEST_ASSERT(restored.count == original.count, "Binary count mismatch");
    KE_TEST_APPROX(restored.speed, original.speed, 1e-6f);
    KE_TEST_ASSERT(restored.label == original.label, "Binary label mismatch");
    KE_TEST_APPROX(restored.position.y, original.position.y, 1e-5f);
    return PassTest(__func__);
}

// ===========================================================================
// T6 -- Unknown properties skipped (forward compat)
// ===========================================================================
static bool TestUnknownPropertySkip() {
    const char* json = R"({
        "__type": "SimpleStruct",
        "__version": 1,
        "unknownField": 12345,
        "count": 42,
        "anotherUnknown": "foo",
        "speed": 2.5
    })";

    auto* info = TypeRegistry::Get().FindType("SimpleStruct");
    SimpleStruct s;
    auto result = Deserializer::DeserializeFromJSON(&s, *info, json);
    KE_TEST_ASSERT(result.success, "Should succeed despite unknown fields");
    KE_TEST_ASSERT(result.unknownPropertiesSkipped >= 2, "Should have skipped 2 unknown props");
    KE_TEST_ASSERT(s.count == 42, "count should be 42");
    KE_TEST_APPROX(s.speed, 2.5f, 1e-4f);
    return PassTest(__func__);
}

// ===========================================================================
// T7 -- Missing field defaults preserved
// ===========================================================================
static bool TestMissingFieldDefaults() {
    const char* json = R"({"__type":"SimpleStruct","__version":1,"flag":true})";
    auto* info = TypeRegistry::Get().FindType("SimpleStruct");

    SimpleStruct s;
    float defaultSpeed = s.speed;

    auto result = Deserializer::DeserializeFromJSON(&s, *info, json);
    KE_TEST_ASSERT(result.success, "Deserialize should succeed with partial data");
    KE_TEST_ASSERT(s.flag == true, "flag should be set");
    KE_TEST_APPROX(s.speed, defaultSpeed, 1e-6f);
    return PassTest(__func__);
}

// ===========================================================================
// T8 -- TypeId stability (FNV-1a reproducibility)
// ===========================================================================
static bool TestTypeIdStability() {
    uint64_t id1 = TypeId<SimpleStruct>();
    uint64_t id2 = TypeId<SimpleStruct>();
    KE_TEST_ASSERT(id1 == id2, "TypeId must be deterministic");
    KE_TEST_ASSERT(TypeId<SimpleStruct>() != TypeId<NestedStruct>(),
                   "Different types must have different TypeIds");

    uint64_t h1 = ConstexprHash("hello");
    uint64_t h2 = ConstexprHash("hello");
    KE_TEST_ASSERT(h1 == h2, "FNV hash must be stable");
    KE_TEST_ASSERT(ConstexprHash("hello") != ConstexprHash("world"), "FNV collision");
    return PassTest(__func__);
}

// ===========================================================================
// T9 -- TypeRegistry enumeration
// ===========================================================================
static bool TestTypeEnumeration() {
    auto all = TypeRegistry::Get().GetAllTypes();
    KE_TEST_ASSERT(all.size() >= 2, "Should have at least 2 registered types");

    bool foundSimple = false, foundNested = false;
    for (auto* t : all) {
        if (t->name == "SimpleStruct") foundSimple = true;
        if (t->name == "NestedStruct") foundNested = true;
    }
    KE_TEST_ASSERT(foundSimple, "SimpleStruct not in enumeration");
    KE_TEST_ASSERT(foundNested, "NestedStruct not in enumeration");
    return PassTest(__func__);
}

// ===========================================================================
// T10 -- Nested struct JSON roundtrip
// ===========================================================================
static bool TestNestedStructJSON() {
    NestedStruct original;
    original.inner.count = 55;
    original.inner.label = "nested_label";
    original.extra       = 42.42f;

    auto* info = TypeRegistry::Get().FindType("NestedStruct");
    KE_TEST_ASSERT(info != nullptr, "NestedStruct not registered");

    std::string json = Serializer::SerializeToJSON(&original, *info);
    KE_TEST_ASSERT(json.find("nested_label") != std::string::npos,
                   "Nested string missing in JSON");

    NestedStruct restored;
    auto result = Deserializer::DeserializeFromJSON(&restored, *info, json);
    KE_TEST_ASSERT(result.success, "Nested deserialize failed");
    KE_TEST_ASSERT(restored.inner.count == 55, "nested count mismatch");
    KE_TEST_ASSERT(restored.inner.label == "nested_label", "nested label mismatch");
    KE_TEST_APPROX(restored.extra, 42.42f, 1e-3f);
    return PassTest(__func__);
}

// ===========================================================================
// T11 -- Performance micro-benchmark
// ===========================================================================
static bool TestPerformanceBenchmark() {
    auto* info = TypeRegistry::Get().FindType("SimpleStruct");
    KE_TEST_ASSERT(info != nullptr, "Type not registered");

    SimpleStruct s;
    s.count = 42;
    s.speed = 1.0f;
    s.label = "bench";

    constexpr int N = 100'000;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        volatile auto json = Serializer::SerializeToJSON(&s, *info);
        (void)json;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    Core::Logger::Info("ReflectionTest",
        "  Perf: %dx JSON serialize = %.1fms (%.3fus/op)",
        N, ms, ms * 1000.0 / N);
    KE_TEST_ASSERT(ms < 30000.0, "Serialization too slow");
    return PassTest(__func__);
}

// ===========================================================================
// Master runner
// ===========================================================================
inline int RunReflectionTests() {
    Core::Logger::Info("ReflectionTest",
        "======= Kumari Reflection System -- Automated Tests =======");

    using TestFn = bool(*)();
    struct TestCase { const char* name; TestFn fn; };

    TestCase tests[] = {
        { "T1  Type Registration",      TestTypeRegistration      },
        { "T2  Property Access",         TestPropertyAccess        },
        { "T3  Property Flags/Metadata", TestPropertyFlags         },
        { "T4  JSON Roundtrip",          TestJSONRoundtrip         },
        { "T5  Binary Roundtrip",        TestBinaryRoundtrip       },
        { "T6  Unknown Prop Skipping",   TestUnknownPropertySkip   },
        { "T7  Missing Field Defaults",  TestMissingFieldDefaults  },
        { "T8  TypeId Stability",        TestTypeIdStability       },
        { "T9  Type Enumeration",        TestTypeEnumeration       },
        { "T10 Nested Struct JSON",      TestNestedStructJSON      },
        { "T11 Performance Benchmark",   TestPerformanceBenchmark  },
    };

    int passed = 0, failed = 0;
    for (auto& tc : tests) {
        Core::Logger::Info("ReflectionTest", "--- %s ---", tc.name);
        if (tc.fn()) { ++passed; }
        else          { ++failed; }
    }

    Core::Logger::Info("ReflectionTest",
        "======= Results: %d/%d passed =======",
        passed, passed + failed);
    return failed;
}

} // namespace KumariEngine::Reflection::Tests
