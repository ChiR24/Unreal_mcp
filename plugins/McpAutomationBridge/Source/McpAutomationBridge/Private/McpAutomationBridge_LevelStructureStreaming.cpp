#include "McpAutomationBridge_LevelStructureActions.h"
#include "McpAutomationBridge_LevelStructureEditorWorld.h"

#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
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
        Payload->TryGetStringField(TEXT("levelName"), LevelName);
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

    // Find the streaming level in the world's streaming levels array
    ULevelStreaming* FoundLevel = nullptr;
    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (StreamingLevel && StreamingLevel->GetWorldAssetPackageFName().ToString().Contains(LevelName))
        {
            FoundLevel = StreamingLevel;
            break;
        }
    }

    // If not found in streaming levels, check if the level exists on disk and create a streaming reference
    // This handles cases where the sublevel was created but the streaming reference wasn't loaded
    if (!FoundLevel)
    {
        // Build potential full paths for the level
        TArray<FString> PotentialPaths;

        // Try as-is first (might be a full path)
        if (LevelName.StartsWith(TEXT("/Game/")))
        {
            PotentialPaths.Add(LevelName);
        }
        // Try under the current world's path
        FString WorldPath = FPaths::GetPath(World->GetOutermost()->GetName());
        PotentialPaths.Add(WorldPath / LevelName);
        // Try under /Game/ directly
        PotentialPaths.Add(FString(TEXT("/Game/")) / LevelName);
        // Try with the level name as a full path under /Game/
        PotentialPaths.Add(FString(TEXT("/Game/")) + LevelName);

        for (const FString& TestPath : PotentialPaths)
        {
            FString TestFullPath = TestPath;
            if (!TestFullPath.EndsWith(TEXT(".umap")))
            {
                // Already a package path, check if package exists
                if (FPackageName::DoesPackageExist(TestFullPath))
                {
                    // Found the level on disk - create a streaming reference
                    ULevelStreamingDynamic* NewStreamingLevel = NewObject<ULevelStreamingDynamic>(World, ULevelStreamingDynamic::StaticClass());
                    if (NewStreamingLevel)
                    {
                        NewStreamingLevel->SetWorldAssetByPackageName(FName(*TestFullPath));
                        NewStreamingLevel->LevelTransform = FTransform::Identity;
                        NewStreamingLevel->SetShouldBeVisible(true);
                        NewStreamingLevel->SetShouldBeLoaded(true);

                        World->AddStreamingLevel(NewStreamingLevel);
                        FoundLevel = NewStreamingLevel;

                        UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("Created streaming reference for existing level: %s"), *TestFullPath);
                        break;
                    }
                }
            }
        }
    }

    if (!FoundLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Streaming level not found: %s"), *LevelName), nullptr, TEXT("LEVEL_NOT_FOUND"));
        return true;
    }

    // Configure streaming settings
    FoundLevel->SetShouldBeVisible(bShouldBeVisible);
    FoundLevel->bShouldBlockOnLoad = bShouldBlockOnLoad;
    FoundLevel->bDisableDistanceStreaming = bDisableDistanceStreaming;

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, FoundLevel);
    ResponseJson->SetStringField(TEXT("levelName"), LevelName);
    ResponseJson->SetStringField(TEXT("streamingMethod"), StreamingMethod);
    ResponseJson->SetBoolField(TEXT("shouldBeVisible"), bShouldBeVisible);

    FString Message = FString::Printf(TEXT("Configured streaming for level: %s"), *LevelName);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
#endif
