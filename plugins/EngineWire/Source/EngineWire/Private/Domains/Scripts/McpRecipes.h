#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// system_control save_recipe / run_recipe / list_recipes (S14): an agent's working multi-call script kept in the project as
// a named recipe with parameters, so a later agent runs it as one checked call. A recipe is a run_script script
// (Domains/Scripts) plus parameter names, stored under Saved/EngineWire/Recipes/<name>.json. save checks it statically
// (the same check run_script makes, its parameters bound) and runs nothing: a restore point cannot undo a deletion and
// leaves the assets a step created, so a trial run would have side effects. run binds the arguments as (let name value)
// forms on the script's first line and runs it exactly as run_script runs a script (the per-step gate, the consent
// list, the rollback); a run that succeeds marks the recipe verified with the engine version.
namespace McpRecipes
{
bool HandleSave(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                const TSharedPtr<FMcpBridgeWebSocket>& Socket);
bool HandleRun(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
               const TSharedPtr<FMcpBridgeWebSocket>& Socket);
bool HandleList(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                const TSharedPtr<FMcpBridgeWebSocket>& Socket);

/** Saved/EngineWire/Recipes, where every recipe is a <name>.json file. */
FString Dir();
/** A saved recipe, or null: no such file, a name that is no file name, or a file that does not parse. */
TSharedPtr<FJsonObject> Load(const FString& Name);
/**
 * The recipes the gateway's search lists beside its capabilities (McpRecipesSearch.cpp): those whose name and summary
 * name every content word of the request, or at least two of them, most words first, at most Max. Each row is
 * {name, summary, paramNames, verified?, nextCall} with nextCall the describe of system_control.run_recipe. Runs on
 * the socket thread that answers search: files and JSON only, no editor API.
 */
TArray<TSharedPtr<FJsonValue>> SearchRows(const TArray<FString>& ContentWords, int32 Max);
} // namespace McpRecipes
