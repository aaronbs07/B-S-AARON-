#pragma once
#include "BinaryWriter.hpp"
#include "BinaryReader.hpp"
#include "ecs/ecs.hpp"

namespace KumariEngine::Save {

class ComponentSerializer {
public:
    virtual ~ComponentSerializer() = default;
    virtual uint16_t GetComponentTypeID() const = 0;
    virtual void Serialize(BinaryWriter& writer, const void* componentData) const = 0;
    virtual void Deserialize(BinaryReader& reader, void* componentData) const = 0;
};

} // namespace KumariEngine::Save
