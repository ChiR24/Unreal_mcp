#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
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

    ULevelStreaming* FoundLevel = nullptr;
    // Match by package name, object path or short name, case-insensitively (dogfood #159:
    // create_sublevel reported the level under /Game/.../Sub but the configurators could not find it).
    FString WantedPackage = LevelName;
    int32 DotIndex = INDEX_NONE;
    if (WantedPackage.FindChar(TEXT('.'), DotIndex)) { WantedPackage.LeftInline(DotIndex); }
    const FString WantedShort = FPackageName::GetShortName(WantedPackage);
    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (!StreamingLevel) { continue; }
        const FString PackageName = StreamingLevel->GetWorldAssetPackageName();
        if (PackageName.Equals(WantedPackage, ESearchCase::IgnoreCase) ||
            FPackageName::GetShortName(PackageName).Equals(WantedShort, ESearchCase::IgnoreCase))
        {
            FoundLevel = StreamingLevel;
            break;
        }
    }

    // This handles cases where the sublevel was created but the streaming reference wasn't loaded
    if (!FoundLevel)
    {
        TArray<FString> PotentialPaths;

        if (LevelName.StartsWith(TEXT("/Game/")))
        {
            PotentialPaths.Add(LevelName);
        }
        FString WorldPath = FPaths::GetPath(World->GetOutermost()->GetName());
        PotentialPaths.Add(WorldPath / LevelName);
        PotentialPaths.Add(FString(TEXT("/Game/")) / LevelName);
        PotentialPaths.Add(FString(TEXT("/Game/")) + LevelName);

        for (const FString& TestPath : PotentialPaths)
        {
            FString TestFullPath = TestPath;
            if (!TestFullPath.EndsWith(TEXT(".umap")))
            {
                // Already a package path, check if package exists
                if (FPackageName::DoesPackageExist(TestFullPath))
                {
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
