// McpInstancedStructView.h - the type and memory of an FInstancedStruct, read without calling any of
// its members.
//
// Before UE 5.5 FInstancedStruct belongs to the Experimental StructUtils module, which the plugin
// does not link. From 5.3 even its inline getters return through a TObjectPtr and are not always
// inlined, so they compile to imports from that module and the link fails. Its data is the same two
// pointers on every version from 5.0 to 5.8: the script struct (a raw pointer before 5.3, a
// TObjectPtr after, the same bits for a loaded type) and the struct memory.
#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include MCP_INSTANCED_STRUCT_HEADER

struct FMcpInstancedStructView
{
    const UScriptStruct* Type = nullptr;
    uint8* Memory = nullptr;

    explicit FMcpInstancedStructView(const FInstancedStruct& Value)
    {
        static_assert(sizeof(FMcpInstancedStructView) == sizeof(FInstancedStruct) &&
                      alignof(FMcpInstancedStructView) == alignof(FInstancedStruct),
                      "FInstancedStruct is no longer {ScriptStruct, StructMemory}");
        FMemory::Memcpy(this, &Value, sizeof(*this));
    }

    bool IsValid() const { return Type && Memory; }
};
