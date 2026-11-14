#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace McpLevelStructure
{

bool HandleConfigureLevelStreaming(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    // CRITICAL: levelName is required - no default fallback
    const FString LevelName = GetJsonStringField(Payload, TEXT("levelName"));

    if (LevelName.IsEmpty())
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("levelName is required for configure_level_streaming"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString StreamingMethod = GetJsonStringField(Payload, TEXT("streamingMethod"), TEXT("Blueprint"));
    bool bShouldBeVisible = GetJsonBoolField(Payload, TEXT("bShouldBeVisible"), true);
    bool bShouldBlockOnLoad = GetJsonBoolField(Payload, TEXT("bShouldBlockOnLoad"), false);
    bool bDisableDistanceStreaming = GetJsonBoolField(Payload, TEXT("bDisableDistanceStreaming"), false);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("No editor world available"), TEXT("NO_EDITOR_WORLD"));
        return true;
    }

    // streamingMethod used to be echoed back without ever changing the streaming class. Resolved
    // before FindOrAddStreamingLevelOrReply, which may add the level to the world.
    UClass* StreamingClass = Payload->HasField(TEXT("streamingMethod")) ? ResolveLevelStreamingClass(StreamingMethod) : nullptr;
    if (Payload->HasField(TEXT("streamingMethod")) && !StreamingClass)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown streamingMethod '%s'; use Blueprint or AlwaysLoaded."), *StreamingMethod), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    ULevelStreaming* FoundLevel = FindOrAddStreamingLevelOrReply(Subsystem, RequestId, Socket, World, LevelName);
    if (!FoundLevel)
    {
        return true;
    }

    if (StreamingClass)
    {
        if (!FoundLevel->IsA(StreamingClass))
        {
            ULevelStreaming* Converted = UEditorLevelUtils::SetStreamingClassForLevel(FoundLevel, StreamingClass);
            if (!Converted)
            {
                Subsystem->SendAutomationError(Socket, RequestId,
                    FString::Printf(TEXT("Could not switch %s to %s streaming"), *LevelName, *StreamingMethod), TEXT("OPERATION_FAILED"));
                return true;
            }
            FoundLevel = Converted;
        }
    }

    {
        // One undo step, closed before the level is saved. A streaming level carries no RF_Transactional until the
        // editor's own visibility toggle gives it one (EditorLevelUtils::SetLevelVisibility), so it is given it here.
        // Only what was passed changes; an omitted flag keeps the level's current value.
        FoundLevel->SetFlags(RF_Transactional);
        const FMcpScopedEditorTransaction FlagsStep(FText::FromString(TEXT("Configure Level Streaming")),
                                                    EMcpMutationDurability::EditorStateOnly, TArray<UObject*>{FoundLevel});
        FoundLevel->Modify();
        if (Payload->HasField(TEXT("bShouldBeVisible"))) { FoundLevel->SetShouldBeVisible(bShouldBeVisible); }
        if (Payload->HasField(TEXT("bShouldBlockOnLoad"))) { FoundLevel->bShouldBlockOnLoad = bShouldBlockOnLoad; }
        if (Payload->HasField(TEXT("bDisableDistanceStreaming"))) { FoundLevel->bDisableDistanceStreaming = bDisableDistanceStreaming; }
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, FoundLevel);
    ResponseJson->SetStringField(TEXT("levelName"), LevelName);
    ResponseJson->SetStringField(TEXT("streamingMethod"), FoundLevel->IsA<ULevelStreamingAlwaysLoaded>() ? TEXT("AlwaysLoaded") : TEXT("Blueprint"));
    ResponseJson->SetBoolField(TEXT("shouldBeVisible"), FoundLevel->GetShouldBeVisibleFlag());

    FString Message = FString::Printf(TEXT("Configured streaming for level: %s"), *LevelName);
    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, World->PersistentLevel, Message, ResponseJson);
    return true;
}

}
