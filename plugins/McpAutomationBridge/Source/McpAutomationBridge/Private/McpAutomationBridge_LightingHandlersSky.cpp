#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LightingHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Subsystems/EditorActorSubsystem.h"

#if WITH_EDITOR
namespace McpLightingHandlers
{

bool HandleSpawnSkyLight(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FVector Location(0.0f, 0.0f, 500.0f);
    const TSharedPtr<FJsonObject>* LocPtr;
    if (Payload->TryGetObjectField(TEXT("location"), LocPtr))
    {
        Location.X = GetJsonNumberField((*LocPtr), TEXT("x"));
        Location.Y = GetJsonNumberField((*LocPtr), TEXT("y"));
        Location.Z = GetJsonNumberField((*LocPtr), TEXT("z"));
    }
    else
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("spawn_sky_light: No location provided, using default (0, 0, 500)"));
    }

    FRotator Rotation = FRotator::ZeroRotator;
    const TSharedPtr<FJsonObject>* RotPtr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotPtr))
    {
        Rotation.Pitch = GetJsonNumberField((*RotPtr), TEXT("pitch"));
        Rotation.Yaw = GetJsonNumberField((*RotPtr), TEXT("yaw"));
        Rotation.Roll = GetJsonNumberField((*RotPtr), TEXT("roll"));
    }

    AActor* SkyLight = SpawnActorInActiveWorld<AActor>(ASkyLight::StaticClass(), Location, Rotation);
    if (!SkyLight)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn SkyLight"), TEXT("SPAWN_FAILED"));
        return true;
    }

    FString Name;
    if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
    {
        SkyLight->SetActorLabel(Name);
    }

    USkyLightComponent* SkyComp = SkyLight->FindComponentByClass<USkyLightComponent>();
    if (SkyComp)
    {
        FString SourceType;
        if (Payload->TryGetStringField(TEXT("sourceType"), SourceType))
        {
            if (SourceType == TEXT("SpecifiedCubemap"))
            {
                SkyComp->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
                FString CubemapPath;
                if (Payload->TryGetStringField(TEXT("cubemapPath"), CubemapPath) && !CubemapPath.IsEmpty())
                {
                    FString SanitizedCubemapPath = SanitizeProjectRelativePath(CubemapPath);
                    if (SanitizedCubemapPath.IsEmpty())
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("spawn_sky_light: Invalid cubemapPath rejected: %s"), *CubemapPath);
                    }
                    else if (UTextureCube* Cubemap = Cast<UTextureCube>(
                                 StaticLoadObject(UTextureCube::StaticClass(), nullptr, *SanitizedCubemapPath)))
                    {
                        SkyComp->Cubemap = Cubemap;
                    }
                }
            }
            else
            {
                SkyComp->SourceType = ESkyLightSourceType::SLS_CapturedScene;
            }
        }

        double Intensity;
        if (Payload->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            SkyComp->SetIntensity(static_cast<float>(Intensity));
        }

        bool bRecapture;
        if (Payload->TryGetBoolField(TEXT("recapture"), bRecapture) && bRecapture)
        {
            SkyComp->RecaptureSky();
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), SkyLight->GetActorLabel());
    McpHandlerUtils::AddVerification(Resp, SkyLight);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SkyLight spawned"), Resp);
    return true;
}

bool HandleEnsureSingleSkyLight(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UEditorActorSubsystem* ActorSS)
{
    TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
    TArray<AActor*> SkyLights;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->IsA<ASkyLight>())
        {
            SkyLights.Add(Actor);
        }
    }

    FString TargetName;
    Payload->TryGetStringField(TEXT("name"), TargetName);
    if (TargetName.IsEmpty())
    {
        TargetName = TEXT("SkyLight");
    }

    int32 RemovedCount = 0;
    AActor* KeptActor = nullptr;
    for (AActor* SkyLight : SkyLights)
    {
        if (SkyLight->GetActorLabel() == TargetName && !TargetName.IsEmpty())
        {
            KeptActor = SkyLight;
            break;
        }
    }

    for (AActor* SkyLight : SkyLights)
    {
        if (SkyLight == KeptActor)
        {
            continue;
        }
        if (!KeptActor)
        {
            KeptActor = SkyLight;
            if (!TargetName.IsEmpty())
            {
                SkyLight->SetActorLabel(TargetName);
            }
        }
        else
        {
            ActorSS->DestroyActor(SkyLight);
            RemovedCount++;
        }
    }

    if (!KeptActor)
    {
        KeptActor = SpawnActorInActiveWorld<AActor>(
            ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, TargetName);
    }

    if (KeptActor)
    {
        bool bRecapture;
        if (Payload->TryGetBoolField(TEXT("recapture"), bRecapture) && bRecapture)
        {
            if (USkyLightComponent* Comp = KeptActor->FindComponentByClass<USkyLightComponent>())
            {
                Comp->RecaptureSky();
            }
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetNumberField(TEXT("removed"), RemovedCount);
    if (KeptActor)
    {
        McpHandlerUtils::AddVerification(Resp, KeptActor);
    }
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ensured single SkyLight"), Resp);
    return true;
}

}
#endif
