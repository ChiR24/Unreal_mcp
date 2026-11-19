#pragma once

#include "Foundation/BridgeHelpers/Properties/McpNestedPropElements.h"

// The path parser and element picker behind McpResolveElementPath (McpNestedPropElementsPick.cpp).
namespace McpPropElems
{
struct FSegment
{
    FString Name;
    bool bSelector = false;
    bool bKeyed = false; // [Field=Value]
    FString Key;         // the index, the map or set key, or the Field of [Field=Value]
    FString Value;
};

inline bool Fail(FMcpElementPath& Out, const TCHAR* Code, const FString& Error)
{
    Out.Code = Code;
    Out.Error = Error;
    return false;
}

// Splits at the dots outside brackets. A bracket holds an index, a key or Field=Value; "..." quotes, and a backslash
// takes the next character as it is (\" \] \\).
bool Split(const FString& Path, TArray<FSegment>& OutSegments, FString& OutError);

// Picks one element of the list, map or set Property holds at Container; OutProperty and OutContainer become the
// element's. A set's element is its own key: bOutReadOnly.
bool Pick(FProperty* Property, void* Container, const FSegment& Segment, FProperty*& OutProperty, void*& OutContainer,
          bool& bOutReadOnly, FMcpElementPath& Out);
} // namespace McpPropElems
