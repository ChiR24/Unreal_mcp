// McpEngineRoles.h — the engine adapter: what plays each role (a class, struct, property or function the plugin reaches
// by reflection) on the running engine, found from the role tables in Resources/EngineRoles/*.json.
//
// A role names what to find, never a version: a class or struct by its path, a member by its owner role, its first-guess
// name, the shape of its type and a few words. Stages, each run only when the one before found nothing: the exact name
// (with its type), the engine's own rename data (FCoreRedirects), the one member of the owner whose type fits, and last
// the member whose name shares most words with the role's by a clear margin. Several that fit equally is AMBIGUOUS, never
// a guess. The same stages run over the engine header trees in .compat/check-roles.mjs, whose per-engine table sits
// beside the role files (resolved.generated.json).
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FProperty;
class UFunction;
class UStruct;

namespace McpEngineRoles
{
struct FRoleHit
{
    UStruct* Type = nullptr;       // a class or struct role
    FProperty* Property = nullptr; // a property role
    UFunction* Function = nullptr; // a function role
    FString Stage;                 // exact, redirect, shape or words; empty when nothing plays the role
    FString Evidence;              // what was found, or what was searched and why nothing fit
    bool IsResolved() const { return !Stage.IsEmpty(); }
};

/** The role on this engine (game thread: reflection is read there). Resolved at first use and kept until a module loads
 *  or unloads. An unknown id answers an unresolved hit saying so. A copy: resolving one role can resolve others. */
FRoleHit Resolve(FName RoleId);
UStruct* TypeOf(FName RoleId);
FProperty* PropertyOf(FName RoleId);
UFunction* FunctionOf(FName RoleId);

/** Game thread: the member of Owner that the role with this owner and first-guess name resolves to here, so code
 *  written with one engine's names finds the member where another engine moved it. Owner's own member of that name when
 *  no role names it (or the role finds nothing). */
FProperty* PropertyByName(const UStruct* Owner, FName Name);
UFunction* FunctionByName(const UClass* Owner, FName Name);

/** Any thread: whether the role resolved at the last refresh (unknown before the first, which the game thread makes once
 *  the engine has started). A table's domain ("MVVM") resolves when every role in it that is not optional does. OutWhy,
 *  when it did not, says what is missing. */
bool IsKnownResolved(FName RoleId, FString* OutWhy = nullptr);

/** Game thread: resolve every role again and publish the result for IsKnownResolved. */
void RefreshAll();

/** Any thread: RefreshAll on the next game-thread tick (one, however often it is asked for before then). */
void ScheduleRefresh();

/** Every role of one domain (all when Domain is empty) with what plays it here, for inspect.describe_engine_roles. */
TSharedRef<FJsonObject> Describe(const FString& Domain);
}
