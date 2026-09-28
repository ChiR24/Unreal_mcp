#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace McpLevelStructure
{

bool HandleConfigureLevelStreaming(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    // CRITICAL: levelName is required - no default fallback
    FString LevelName;
    if (Payload.IsValid())
    {
        // Accept every spelling the contracts use for the streaming level (dogfood #159).
        for (const TCHAR* Key : {TEXT("levelName"), TEXT("sublevelName"), TEXT("subLevelName"), TEXT("sublevelPath"), TEXT("subLevelPath"), TEXT("levelPath")})
        {
            if (Payload->TryGetStringField(Key, LevelName) && !LevelName.IsEmpty()) { break; }
        }
    }

    if (LevelName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("levelName is required for configure_level_streaming"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString StreamingMethod = GetJsonStringField(Payload, TEXT("streamingMethod"), TEXT("Blueprint"));
    bool bShouldBeVisible = GetJsonBoolField(Payload, TEXT("bShouldBeVisible"), true);
    bool bShouldBlockOnLoad = GetJsonBoolField(Payload, TEXT("bShouldBlockOnLoad"), false);
    bool bDisableDistanceStreaming = GetJsonBoolField(Payload, TEXT("bDisableDistanceStreaming"), false);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_EDITOR_WORLD"));
        return true;
    }

    ULevelStreaming* FoundLevel = FindOrAddStreamingLevel(World, LevelName);
    if (!FoundLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Streaming level not found: %s"), *LevelName), nullptr, TEXT("LEVEL_NOT_FOUND"));
        return true;
    }

    // streamingMethod used to be echoed back without ever changing the streaming class.
    if (Payload->HasField(TEXT("streamingMethod")))
    {
        UClass* StreamingClass = ResolveLevelStreamingClass(StreamingMethod);
        if (!StreamingClass)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Unknown streamingMethod '%s'; use Blueprint or AlwaysLoaded."), *StreamingMethod), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (!FoundLevel->IsA(StreamingClass))
        {
            ULevelStreaming* Converted = UEditorLevelUtils::SetStreamingClassForLevel(FoundLevel, StreamingClass);
            if (!Converted)
            {
                Subsystem->SendAutomationResponse(Socket, RequestId, false,
                    FString::Printf(TEXT("Could not switch %s to %s streaming"), *LevelName, *StreamingMethod), nullptr, TEXT("OPERATION_FAILED"));
                return true;
            }
            FoundLevel = Converted;
        }
    }

    // Only what was passed changes; an omitted flag keeps the level's current value.
    FoundLevel->Modify();
    if (Payload->HasField(TEXT("bShouldBeVisible"))) { FoundLevel->SetShouldBeVisible(bShouldBeVisible); }
    if (Payload->HasField(TEXT("bShouldBlockOnLoad"))) { FoundLevel->bShouldBlockOnLoad = bShouldBlockOnLoad; }
    if (Payload->HasField(TEXT("bDisableDistanceStreaming"))) { FoundLevel->bDisableDistanceStreaming = bDisableDistanceStreaming; }

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
