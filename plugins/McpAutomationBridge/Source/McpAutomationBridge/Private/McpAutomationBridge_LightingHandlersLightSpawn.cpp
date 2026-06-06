#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LightingHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "Components/LightComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Light.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RenderingThread.h"

#if WITH_EDITOR
namespace McpLightingHandlers
{

static bool TryGetRequestedLightClass(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    FString& OutLightClass)
{
    if (Payload->TryGetStringField(TEXT("lightClass"), OutLightClass) && !OutLightClass.IsEmpty())
    {
        return true;
    }

    FString LightType;
    const bool bHasLightType =
        Payload->TryGetStringField(TEXT("lightType"), LightType) && !LightType.IsEmpty();
    const bool bHasType =
        !bHasLightType && Payload->TryGetStringField(TEXT("type"), LightType) && !LightType.IsEmpty();
    if (!bHasLightType && !bHasType)
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("lightClass or lightType required"), TEXT("INVALID_ARGUMENT"));
        return false;
    }

    const FString LowerType = LightType.ToLower();
    if (LowerType == TEXT("point") || LowerType == TEXT("pointlight"))
    {
        OutLightClass = TEXT("PointLight");
    }
    else if (LowerType == TEXT("directional") || LowerType == TEXT("directionallight"))
    {
        OutLightClass = TEXT("DirectionalLight");
    }
    else if (LowerType == TEXT("spot") || LowerType == TEXT("spotlight"))
    {
        OutLightClass = TEXT("SpotLight");
    }
    else if (LowerType == TEXT("rect") || LowerType == TEXT("rectlight"))
    {
        OutLightClass = TEXT("RectLight");
    }
    else if (LowerType == TEXT("sky") || LowerType == TEXT("skylight"))
    {
        OutLightClass = TEXT("SkyLight");
    }
    else
    {
        const FString ErrorMessage = bHasLightType
            ? FString::Printf(
                  TEXT("Invalid lightType: %s. Must be one of: point, directional, spot, rect, sky"),
                  *LightType)
            : FString::Printf(
                  TEXT("Invalid type: %s. Must be one of: point, directional, spot, rect, sky"),
                  *LightType);
        Subsystem.SendAutomationError(
            RequestingSocket,
            RequestId,
            ErrorMessage,
            TEXT("INVALID_LIGHT_TYPE"));
        return false;
    }

    return true;
}

static UClass* ResolveLightClass(const FString& LightClassStr)
{
    const FString LowerClassStr = LightClassStr.ToLower();
    if (LowerClassStr == TEXT("pointlight") || LowerClassStr == TEXT("point"))
    {
        return APointLight::StaticClass();
    }
    if (LowerClassStr == TEXT("directionallight") || LowerClassStr == TEXT("directional"))
    {
        return ADirectionalLight::StaticClass();
    }
    if (LowerClassStr == TEXT("spotlight") || LowerClassStr == TEXT("spot"))
    {
        return ASpotLight::StaticClass();
    }
    if (LowerClassStr == TEXT("rectlight") || LowerClassStr == TEXT("rect"))
    {
        return ARectLight::StaticClass();
    }
    if (LowerClassStr == TEXT("skylight") || LowerClassStr == TEXT("sky"))
    {
        return ASkyLight::StaticClass();
    }

    UClass* LightClass = ResolveUClass(LightClassStr);
    return LightClass ? LightClass : ResolveUClass(TEXT("A") + LightClassStr);
}

bool HandleSpawnLight(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString LightClassStr;
    if (!TryGetRequestedLightClass(Subsystem, RequestId, Payload, RequestingSocket, LightClassStr))
    {
        return true;
    }

    UClass* LightClass = ResolveLightClass(LightClassStr);
    if (!LightClass || !LightClass->IsChildOf(ALight::StaticClass()))
    {
        Subsystem.SendAutomationError(
            RequestingSocket,
            RequestId,
            FString::Printf(TEXT("Invalid light class: %s"), *LightClassStr),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Log,
        TEXT("spawn_light: Resolved lightClass '%s' to %s (path: %s)"),
        *LightClassStr,
        *LightClass->GetName(),
        *LightClass->GetPathName());

    FVector Location(0.0f, 0.0f, 300.0f);
    const TSharedPtr<FJsonObject>* LocPtr;
    if (Payload->TryGetObjectField(TEXT("location"), LocPtr))
    {
        Location.X = GetJsonNumberField((*LocPtr), TEXT("x"));
        Location.Y = GetJsonNumberField((*LocPtr), TEXT("y"));
        Location.Z = GetJsonNumberField((*LocPtr), TEXT("z"));
    }
    else
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("spawn_light: No location provided, using default (0, 0, 300)"));
    }

    FRotator Rotation = FRotator::ZeroRotator;
    const TSharedPtr<FJsonObject>* RotPtr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotPtr))
    {
        Rotation.Pitch = GetJsonNumberField((*RotPtr), TEXT("pitch"));
        Rotation.Yaw = GetJsonNumberField((*RotPtr), TEXT("yaw"));
        Rotation.Roll = GetJsonNumberField((*RotPtr), TEXT("roll"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || !World->IsValidLowLevel())
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("No valid world available for spawning light"), TEXT("NO_WORLD"));
        return true;
    }

    FlushRenderingCommands();
    FTransform SpawnTransform(Rotation, Location);
    AActor* NewLight = World->SpawnActorDeferred<AActor>(
        LightClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (NewLight)
    {
        UGameplayStatics::FinishSpawningActor(NewLight, SpawnTransform);
        NewLight->SetActorLabel(LightClassStr);
        NewLight->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    }
    if (!NewLight)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn light actor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    FString Name;
    if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
    {
        NewLight->SetActorLabel(Name);
    }

    if (ULightComponent* BaseLightComp = NewLight->FindComponentByClass<ULightComponent>())
    {
        BaseLightComp->SetMobility(EComponentMobility::Movable);
    }

    const TSharedPtr<FJsonObject>* Props;
    if (Payload->TryGetObjectField(TEXT("properties"), Props))
    {
        ApplyLightProperties(*NewLight, *Props);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), NewLight->GetActorLabel());
    McpHandlerUtils::AddVerification(Resp, NewLight);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Light spawned"), Resp);
    return true;
}

}
#endif
