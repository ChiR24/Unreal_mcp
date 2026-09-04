#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpAutomationBridge_LightingBuildCompatibility.h"
#include "Domains/Lighting/McpAutomationBridge_LightingHandlersPrivate.h"

#include "Components/LightComponent.h"
#include "Engine/RendererSettings.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Lightmass/LightmassImportanceVolume.h"

#if __has_include("Subsystems/LevelEditorSubsystem.h")
  #include "Subsystems/LevelEditorSubsystem.h"
#elif __has_include("LevelEditorSubsystem.h")
  #include "LevelEditorSubsystem.h"
#endif

#if WITH_EDITOR
namespace McpLightingHandlers
{
namespace
{

bool TryParseLightingQuality(
    const FString& Quality,
    ELightingBuildQuality& OutQuality,
    FString& OutQualityName)
{
    const FString LowerQuality = Quality.IsEmpty() ? TEXT("production") : Quality.ToLower();
    if (LowerQuality == TEXT("preview") || LowerQuality == TEXT("0"))
    {
        OutQuality = ELightingBuildQuality::Quality_Preview;
        OutQualityName = TEXT("Preview");
        return true;
    }
    if (LowerQuality == TEXT("medium") || LowerQuality == TEXT("1"))
    {
        OutQuality = ELightingBuildQuality::Quality_Medium;
        OutQualityName = TEXT("Medium");
        return true;
    }
    if (LowerQuality == TEXT("high") || LowerQuality == TEXT("2"))
    {
        OutQuality = ELightingBuildQuality::Quality_High;
        OutQualityName = TEXT("High");
        return true;
    }
    if (LowerQuality == TEXT("production") || LowerQuality == TEXT("3"))
    {
        OutQuality = ELightingBuildQuality::Quality_Production;
        OutQualityName = TEXT("Production");
        return true;
    }
    return false;
}

}

bool HandleBuildLighting(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!GEditor || !GEditor->GetEditorWorldContext().World())
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_WORLD_NOT_AVAILABLE"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    FString Quality;
    bool bBuildOnlySelected = false;
    bool bBuildReflectionCaptures = false;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("quality"), Quality);
        Payload->TryGetBoolField(TEXT("buildOnlySelected"), bBuildOnlySelected);
        Payload->TryGetBoolField(TEXT("buildReflectionCaptures"), bBuildReflectionCaptures);
    }

    if (bBuildOnlySelected)
    {
        TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
        Err->SetStringField(TEXT("option"), TEXT("buildOnlySelected"));
        Subsystem.SendAutomationResponse(
            RequestingSocket,
            RequestId,
            false,
            TEXT("Selected-only lighting builds are not supported by this bridge"),
            Err,
            TEXT("UNSUPPORTED_BUILD_OPTION"));
        return true;
    }

    ELightingBuildQuality QualityEnum = ELightingBuildQuality::Quality_Production;
    FString QualityName;
    if (!TryParseLightingQuality(Quality, QualityEnum, QualityName))
    {
        TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
        Err->SetStringField(TEXT("error"), TEXT("unknown_quality"));
        Err->SetStringField(TEXT("quality"), Quality);
        Err->SetStringField(TEXT("validValues"), TEXT("preview/0, medium/1, high/2, production/3"));
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false, TEXT("Unknown lighting quality"), Err, TEXT("UNKNOWN_QUALITY"));
        return true;
    }

    if (AWorldSettings* WS = World->GetWorldSettings())
    {
        if (WS->bForceNoPrecomputedLighting)
        {
            TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetBoolField(TEXT("skipped"), true);
            Resp->SetStringField(TEXT("reason"), TEXT("bForceNoPrecomputedLighting is true"));
            Resp->SetStringField(
                TEXT("suggestion"),
                TEXT("Set WorldSettings.bForceNoPrecomputedLighting to false to enable lighting builds"));
            Subsystem.SendAutomationResponse(
                RequestingSocket,
                RequestId,
                true,
                TEXT("Lighting build skipped - precomputed lighting disabled in WorldSettings"),
                Resp);
            return true;
        }
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    ULevelEditorSubsystem* LevelEditorSubsystem =
        GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
    if (!LevelEditorSubsystem)
    {
        Subsystem.SendAutomationError(
            RequestingSocket,
            RequestId,
            TEXT("Level editor subsystem not available"),
            TEXT("SUBSYSTEM_NOT_FOUND"));
        return true;
    }
    const bool bBuildSucceeded =
        LevelEditorSubsystem->BuildLightMaps(QualityEnum, bBuildReflectionCaptures);
    const FString BuildApi = TEXT("LevelEditorSubsystem.BuildLightMaps");
#else
    const bool bBuildSucceeded =
        RunLegacyLightingBuild(*World, QualityEnum);
    if (bBuildSucceeded && bBuildReflectionCaptures)
    {
        GEditor->BuildReflectionCaptures();
    }
    const FString BuildApi = TEXT("EditorBuildUtils.EditorBuild");
#endif

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), bBuildSucceeded);
    Resp->SetStringField(TEXT("quality"), QualityName);
    FString ReasonText;
    if (!bBuildSucceeded)
    {
        // Say why a build can fail instead of a bare BUILD_FAILED (dogfood #156/#215).
        TArray<TSharedPtr<FJsonValue>> Reasons;
        if (!GetDefault<URendererSettings>()->bAllowStaticLighting)
        {
            Reasons.Add(MakeShared<FJsonValueString>(TEXT("Project setting r.AllowStaticLighting is disabled (Rendering > Lighting)")));
        }
        int32 StaticLights = 0;
        int32 ImportanceVolumes = 0;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->IsA<ALightmassImportanceVolume>())
            {
                ++ImportanceVolumes;
            }
            if (ULightComponent* Light = It->FindComponentByClass<ULightComponent>())
            {
                if (Light->Mobility != EComponentMobility::Movable)
                {
                    ++StaticLights;
                }
            }
        }
        if (StaticLights == 0)
        {
            Reasons.Add(MakeShared<FJsonValueString>(TEXT("No static or stationary lights in the level; nothing to bake")));
        }
        if (ImportanceVolumes == 0)
        {
            Reasons.Add(MakeShared<FJsonValueString>(TEXT("No LightmassImportanceVolume (create_lightmass_volume) - builds without one are slow or refused")));
        }
        if (GEditor->IsLightingBuildCurrentlyRunning())
        {
            Reasons.Add(MakeShared<FJsonValueString>(TEXT("A lighting build is already running")));
        }
        if (Reasons.Num() == 0)
        {
            Reasons.Add(MakeShared<FJsonValueString>(TEXT("Lightmass/Swarm reported failure; see the editor log (LogLightmassSolver, LogSwarm) for details")));
        }
        Resp->SetArrayField(TEXT("reasons"), Reasons);
        for (const TSharedPtr<FJsonValue>& Reason : Reasons)
        {
            ReasonText += (ReasonText.IsEmpty() ? TEXT("") : TEXT("; ")) + Reason->AsString();
        }
        Resp->SetNumberField(TEXT("staticLightCount"), StaticLights);
        Resp->SetNumberField(TEXT("lightmassImportanceVolumeCount"), ImportanceVolumes);
    }
    Resp->SetBoolField(TEXT("buildReflectionCaptures"), bBuildReflectionCaptures);
    Resp->SetStringField(TEXT("buildApi"), BuildApi);
    Subsystem.SendAutomationResponse(
        RequestingSocket,
        RequestId,
        bBuildSucceeded,
        bBuildSucceeded
            ? FString::Printf(TEXT("Lighting build completed with quality: %s"), *QualityName)
            : FString::Printf(TEXT("Lighting build failed with quality: %s. %s"), *QualityName, *ReasonText),
        Resp,
        bBuildSucceeded ? FString() : TEXT("BUILD_FAILED"));
    return true;
}

bool HandleCreateLightmassVolume(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* LocObj;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj))
    {
        Location.X = GetJsonNumberField((*LocObj), TEXT("x"));
        Location.Y = GetJsonNumberField((*LocObj), TEXT("y"));
        Location.Z = GetJsonNumberField((*LocObj), TEXT("z"));
    }

    FVector Size(1000, 1000, 1000);
    const TSharedPtr<FJsonObject>* SizeObj;
    if (Payload->TryGetObjectField(TEXT("size"), SizeObj))
    {
        Size.X = GetJsonNumberField((*SizeObj), TEXT("x"));
        Size.Y = GetJsonNumberField((*SizeObj), TEXT("y"));
        Size.Z = GetJsonNumberField((*SizeObj), TEXT("z"));
    }

    AActor* Volume = SpawnActorInActiveWorld<AActor>(
        ALightmassImportanceVolume::StaticClass(), Location, FRotator::ZeroRotator);
    if (!Volume)
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("Failed to spawn LightmassImportanceVolume"), TEXT("SPAWN_FAILED"));
        return true;
    }

    Volume->SetActorScale3D(Size / 200.0f);
    FString Name;
    if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
    {
        Volume->SetActorLabel(Name);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), Volume->GetActorLabel());
    McpHandlerUtils::AddVerification(Resp, Volume);
    Subsystem.SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("LightmassImportanceVolume created"), Resp);
    return true;
}

}
#endif
