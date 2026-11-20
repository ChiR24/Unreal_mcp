// McpEngineRolesMatch.h — how a role's type spec and words are matched against a reflected property. Shared by the
// resolver (McpEngineRoles.cpp) and the same rules as .compat/check-roles.mjs, which applies them to header text.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/Reflect/McpEngineRoles.h"

class FProperty;
class UStruct;

namespace McpEngineRolesMatch
{
/** A class or struct role: the object at its path, then the engine's rename of that path (McpEngineRolesResolve.cpp). */
McpEngineRoles::FRoleHit ResolveType(const TSharedPtr<FJsonObject>& Row, bool bClass);

/** A property or function role of its owner role: its name, the engine's rename of it, the one member whose type fits,
 *  then the closest name by a margin; several equally close is AMBIGUOUS (McpEngineRolesResolve.cpp). */
McpEngineRoles::FRoleHit ResolveMember(const TSharedPtr<FJsonObject>& Row, const FString& Kind);

/** Whether Property's type fits Spec: {"array": spec}, {"cpp": "FGuid"}, {"role": id}, {"enumWords": [..]},
 *  {"structWords": [..]} or {"anyOf": [spec, ..]}. A missing spec fits anything. RoleType finds a {"role"} entry's type. */
bool Fits(const TSharedPtr<FJsonObject>& Spec, const FProperty* Property, TFunctionRef<UStruct*(FName)> RoleType);

/** The C++ name of a property's type as a header spells it, wrappers off: FGuid, UClass, bool, TArray<FMVVMBinding>. */
FString CppName(const FProperty* Property);

/** A member name in lower-case words: "bDeprecatedSource" -> deprecated, source; "ViewModelName" -> view, model, name. */
TArray<FString> Words(const FString& Name);

/** The share of Want that Name's words cover, 0 to 1. */
float Overlap(const FString& Name, const TArray<FString>& Want);
}
