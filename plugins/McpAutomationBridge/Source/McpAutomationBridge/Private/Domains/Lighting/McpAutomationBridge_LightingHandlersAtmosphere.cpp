#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpAutomationBridge_LightingHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/ExponentialHeightFog.h"
#include "HAL/IConsoleManager.h"
#include "Subsystems/EditorActorSubsystem.h"

namespace McpLightingHandlers
{

bool HandleSetupVolumetricFog(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UEditorActorSubsystem* ActorSS)
{
    AExponentialHeightFog* FogActor = nullptr;
    for (AActor* Actor : ActorSS->GetAllLevelActors())
    {
        if (Actor && Actor->IsA<AExponentialHeightFog>())
        {
            FogActor = Cast<AExponentialHeightFog>(Actor);
            break;
        }
    }

    if (!FogActor)
    {
        FogActor = Cast<AExponentialHeightFog>(
            SpawnActorInActiveWorld<AActor>(AExponentialHeightFog::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
    }

    if (!FogActor || !FogActor->GetComponent())
    {
        Subsystem.SendAutomationError(
            RequestingSocket,
            RequestId,
            TEXT("Failed to find or spawn ExponentialHeightFog"),
            TEXT("EXECUTION_ERROR"));
        return true;
    }

    // `enabled` was hard-coded true here while the TS handler ran
    // `r.VolumetricFog 0` first for enabled:false, so disabling fog turned the
    // cvar off and the component flag straight back on.
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
    FogComp->bEnableVolumetricFog = bEnabled;

    double Distance;
    if (Payload->TryGetNumberField(TEXT("viewDistance"), Distance))
    {
        FogComp->VolumetricFogDistance = static_cast<float>(Distance);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), McpActorRef(FogActor));
    Resp->SetBoolField(TEXT("enabled"), bEnabled);
    McpHandlerUtils::AddVerification(Resp, FogActor);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        bEnabled ? TEXT("Volumetric fog enabled") : TEXT("Volumetric fog disabled"), Resp);
    return true;
}

bool HandleConfigureShadows(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // The published contract declares `settings` and `actorName`; the handler
    // read NEITHER. It read two undeclared top-level fields instead, folded
    // rayTracedShadows onto the VIRTUAL shadow-map cvar (a different feature
    // with its own configure_ray_tracing action), and answered "Shadows
    // configured" with success:true even when it had touched nothing at all.
    const TSharedPtr<FJsonObject>* SettingsObj = nullptr;
    Payload->TryGetObjectField(TEXT("settings"), SettingsObj);
    auto ReadBool = [&Payload, SettingsObj](const TCHAR* Key, bool& Out)
    {
        return (SettingsObj && (*SettingsObj)->TryGetBoolField(Key, Out)) ||
               Payload->TryGetBoolField(Key, Out);
    };
    auto ReadNumber = [&Payload, SettingsObj](const TCHAR* Key, double& Out)
    {
        return (SettingsObj && (*SettingsObj)->TryGetNumberField(Key, Out)) ||
               Payload->TryGetNumberField(Key, Out);
    };

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Applied;
    // shadowQuality, shadowDistance, contactShadows and rayTracedShadows were declared and
    // never applied (rayTracedShadows only returned a note). Each drives its engine-wide cvar.
    auto SetCVar = [&Applied](const TCHAR* CVarName, const FString& Value, const TCHAR* Key)
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName))
        {
            CVar->Set(*Value);
            Applied.Add(MakeShared<FJsonValueString>(Key));
        }
    };
    FString ShadowQuality;
    if ((SettingsObj && (*SettingsObj)->TryGetStringField(TEXT("shadowQuality"), ShadowQuality)) ||
        Payload->TryGetStringField(TEXT("shadowQuality"), ShadowQuality))
    {
        static const TMap<FString, int32> ShadowLevels = {
            {TEXT("Low"), 0}, {TEXT("Medium"), 1}, {TEXT("High"), 2}, {TEXT("Epic"), 3}, {TEXT("Cinematic"), 4}};
        const int32* Level = ShadowLevels.Find(ShadowQuality);
        if (!Level)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid shadowQuality: %s. Valid values: Low, Medium, High, Epic, Cinematic"), *ShadowQuality),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        SetCVar(TEXT("sg.ShadowQuality"), FString::FromInt(*Level), TEXT("shadowQuality"));
    }
    double ShadowDistance = 0.0;
    if (ReadNumber(TEXT("shadowDistance"), ShadowDistance))
    {
        SetCVar(TEXT("r.Shadow.DistanceScale"), FString::SanitizeFloat(ShadowDistance), TEXT("shadowDistance"));
    }
    bool bContactShadows = false;
    if (ReadBool(TEXT("contactShadows"), bContactShadows))
    {
        SetCVar(TEXT("r.ContactShadows"), bContactShadows ? TEXT("1") : TEXT("0"), TEXT("contactShadows"));
    }
    bool bRayTraced = false;
    if (ReadBool(TEXT("rayTracedShadows"), bRayTraced))
    {
        SetCVar(TEXT("r.RayTracing.Shadows"), bRayTraced ? TEXT("1") : TEXT("0"), TEXT("rayTracedShadows"));
    }

    bool bVirtual = false;
    if (ReadBool(TEXT("virtualShadowMaps"), bVirtual))
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable")))
        {
            CVar->Set(bVirtual ? 1 : 0);
            Resp->SetBoolField(TEXT("virtualShadowMaps"), bVirtual);
            Applied.Add(MakeShared<FJsonValueString>(TEXT("virtualShadowMaps")));
        }
    }

    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (!ActorName.IsEmpty())
    {
        AActor* TargetActor = McpHandlerUtils::FindActorByName(ActorName);
        ULightComponent* LightComp = TargetActor ? TargetActor->FindComponentByClass<ULightComponent>() : nullptr;
        if (!LightComp)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("No actor named '%s' with a LightComponent was found in the editor world"), *ActorName),
                TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        bool bCastShadows = false;
        if (ReadBool(TEXT("castShadows"), bCastShadows))
        {
            LightComp->SetCastShadows(bCastShadows);
            Applied.Add(MakeShared<FJsonValueString>(TEXT("castShadows")));
        }
        double NumberValue = 0.0;
        if (ReadNumber(TEXT("shadowBias"), NumberValue))
        {
            LightComp->SetShadowBias(static_cast<float>(NumberValue));
            Applied.Add(MakeShared<FJsonValueString>(TEXT("shadowBias")));
        }
        if (ReadNumber(TEXT("shadowSlopeBias"), NumberValue))
        {
            LightComp->SetShadowSlopeBias(static_cast<float>(NumberValue));
            Applied.Add(MakeShared<FJsonValueString>(TEXT("shadowSlopeBias")));
        }
        if (ReadNumber(TEXT("shadowResolutionScale"), NumberValue))
        {
            LightComp->Modify();
            LightComp->ShadowResolutionScale = static_cast<float>(NumberValue);
            LightComp->MarkRenderStateDirty();
            Applied.Add(MakeShared<FJsonValueString>(TEXT("shadowResolutionScale")));
        }
        Resp->SetStringField(TEXT("actorName"), McpActorRef(TargetActor));
    }

    if (Applied.Num() == 0)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            TEXT("No shadow settings supplied. Pass shadowQuality, shadowDistance, contactShadows, rayTracedShadows or virtualShadowMaps, or actorName plus one of castShadows, shadowBias, shadowSlopeBias or shadowResolutionScale (top level or inside `settings`)."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("appliedSettings"), Applied);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Shadows configured (%d setting(s) applied)"), Applied.Num()), Resp);
    return true;
}

}
