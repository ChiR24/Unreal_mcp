#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

// Element segments in a property path, for inspect get_property and set_property only (McpNestedPropElements.cpp):
// Items[2].Damage, WeaponProfiles[WeaponId=Axe].BaseDamage, ScalarParameterValues[ParameterInfo.Name=Roughness].
// ParameterValue, Map[key]. A value may be quoted ([Name="Stone Axe"]) and \] escapes a bracket. Matching is exact and
// case-sensitive. Every other caller of the property resolver keeps refusing such a segment as a name not found.
struct FMcpElementPath
{
    /** The leaf; Property->ContainerPtrToValuePtr(Container) is its value. */
    FProperty* Property = nullptr;
    void* Container = nullptr;
    /** The object whose properties Chain walks: the root, or the last object a reference led into. */
    UObject* Owner = nullptr;
    /** Owner's member property down to the leaf, as the details panel reports an edit (PostEditChangeChainProperty). */
    TArray<FProperty*> Chain;
    /** On failure: PROPERTY_NOT_FOUND, PROPERTY_ELEMENT_NOT_FOUND, AMBIGUOUS_KEY_MATCH or ELEMENT_KEY_READ_ONLY. */
    FString Code;
    FString Error;
};

/** True when a path names an element (it holds a bracket). */
inline bool McpPathHasElements(const FString& Path) { return Path.Contains(TEXT("[")); }

/** Walks Path from Root; bWrite refuses a set element (its hash would break). False with Code and Error set on a miss. */
bool McpResolveElementPath(UObject* Root, const FString& Path, bool bWrite, FMcpElementPath& Out);

/** Reports a write through Path to its owner as the details panel does: a chain event from the member down. */
void McpNotifyElementWrite(const FMcpElementPath& Path);

/** The property name a path starts with: Items for Items[2].Damage, Mesh for Mesh.Material. */
FString McpPathHead(const FString& Path);
