// set_level_blueprint_pin_default: the Blueprint graph door's set_pin_default_value, run on the level Blueprint.
#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"

namespace McpLevelStructure
{

bool HandleSetLevelBlueprintPinDefault(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    UWorld* World = GetEditorWorld();
    FString LevelError;
    ULevel* Level = World ? ResolveTargetLevelForBlueprintRequest(World, Payload, LevelError) : nullptr;
    ULevelScriptBlueprint* LevelBP = Level ? Level->GetLevelScriptBlueprint(false) : nullptr;
    UEdGraph* EventGraph = LevelBP ? FBlueprintEditorUtils::FindEventGraph(LevelBP) : nullptr;
    if (!EventGraph)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            LevelError.IsEmpty() ? TEXT("Level Blueprint not available") : LevelError, nullptr, TEXT("LEVEL_NOT_OPEN"));
        return true;
    }

    // The graph door saves the asset it edits; here that is the level, which this door saves only on save: true.
    // So its saves are held back and its success reply goes through SendLevelEditResult.
    FMcpDeferAssetSaves DeferSaves;
    McpBlueprintGraphHandlers::FActionContext Context{
        Subsystem, RequestId, Payload, Socket, TEXT("set_pin_default_value"), LevelBP, EventGraph, /*bDeferCompile=*/true};
    Context.OnSuccess = [&](const FString& Message, const TSharedPtr<FJsonObject>& Result)
    {
        SendLevelEditResult(Subsystem, RequestId, Socket, Payload, Level, Message, Result);
    };
    return McpBlueprintGraphHandlers::SetPinDefaultValue(Context);
}

}
