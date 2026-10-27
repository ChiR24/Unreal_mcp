#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpLightingBuildCompatibility.h"
#include "Domains/Lighting/McpLightingPrivate.h"

#include "Components/LightComponent.h"
#include "Engine/RendererSettings.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Domains/Volume/McpVolumeGeometry.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Lightmass/LightmassImportanceVolume.h"

#include "LevelEditorSubsystem.h"

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
    UEngineWireSubsystem& Subsystem,
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
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("quality"), Quality);
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

    // It answered success with skipped: true, which read as a lit level.
    const AWorldSettings* WS = World->GetWorldSettings();
    if (WS && WS->bForceNoPrecomputedLighting)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Nothing was built: this level's World Settings force no precomputed lighting (bForceNoPrecomputedLighting). "
                 "Set it to false first: inspect set_property with actorName WorldSettings, propertyName bForceNoPrecomputedLighting, value false."),
            TEXT("PRECOMPUTED_LIGHTING_DISABLED"));
        return true;
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
        LevelEditorSubsystem->BuildLightMaps(QualityEnum, /*bWithReflectionCaptures*/ false);
#else
    const bool bBuildSucceeded =
        RunLegacyLightingBuild(*World, QualityEnum);
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
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FVector Size = ExtractVectorField(Payload, TEXT("size"), FVector(1000.0));
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world available"), TEXT("NO_WORLD"));
        return true;
    }
    if (Size.GetMin() <= 0.0)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("size must be above 0 on every axis. Nothing was created."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // A second actor under one label left every later call by that name on whichever came first.
    if (!Name.IsEmpty() && FindActorByNameInWorldForMcp(World, Name, true))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("An actor named %s is already in the level; pick another name. Nothing was created."), *Name), TEXT("NAME_TAKEN"));
        return true;
    }

    // Into the editor level, which Lightmass bakes (in Play it went into the Play world and was gone on stop), with a box
    // brush of that size: the scaled empty brush it had has bounds {0,0,0} and marked nothing for Lightmass.
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Create Lightmass Importance Volume")), EMcpMutationDurability::EditorStateOnly, TArray<UObject*>());
    ALightmassImportanceVolume* Volume = VolumeHelpers::SpawnVolumeActor<ALightmassImportanceVolume>(World, Name, Location, FRotator::ZeroRotator, Size * 0.5);
    if (!Volume)
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("Failed to spawn LightmassImportanceVolume"), TEXT("SPAWN_FAILED"));
        return true;
    }
    Volume->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), McpActorRef(Volume));
    Resp->SetStringField(TEXT("actorPath"), Volume->GetPathName());
    Resp->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Volume->GetActorLocation()));
    Resp->SetObjectField(TEXT("size"), McpHandlerUtils::VectorToJson(Volume->GetComponentsBoundingBox(true).GetSize()));
    Transaction.DescribeInto(Resp);
    McpHandlerUtils::AddVerification(Resp, Volume);
    Subsystem.SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("LightmassImportanceVolume created"), Resp);
    return true;
}

}
