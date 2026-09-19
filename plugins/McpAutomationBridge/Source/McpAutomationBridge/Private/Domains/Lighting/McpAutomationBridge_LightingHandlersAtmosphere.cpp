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

#if WITH_EDITOR
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
    Resp->SetStringField(TEXT("actorName"), FogActor->GetActorLabel());
    Resp->SetBoolField(TEXT("enabled"), bEnabled);
    McpHandlerUtils::AddVerification(Resp, FogActor);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        bEnabled ? TEXT("Volumetric fog enabled") : TEXT("Volumetric fog disabled"), Resp);
    return true;
}

bool HandleSetupGlobalIllumination(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString Method;
    if (!Payload->TryGetStringField(TEXT("method"), Method) || Method.IsEmpty())
    {
        Subsystem.SendAutomationError(
            RequestingSocket,
            RequestId,
            TEXT("method parameter is required. Valid values: LumenGI, ScreenSpace, None, RayTraced, Lightmass"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (Method == TEXT("LumenGI"))
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
        {
            CVar->Set(1);
        }
        if (IConsoleVariable* CVarRefl = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ReflectionMethod")))
        {
            CVarRefl->Set(1);
        }
    }
    else if (Method == TEXT("ScreenSpace"))
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
        {
            CVar->Set(2);
        }
    }
    else if (Method == TEXT("None"))
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
        {
            CVar->Set(0);
        }
    }
    else if (Method == TEXT("RayTraced"))
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
        {
            CVar->Set(3);
        }
    }
    else if (Method == TEXT("Lightmass"))
    {
        if (IConsoleVariable* CVarGI = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
        {
            CVarGI->Set(0);
        }
    }
    else
    {
        Subsystem.SendAutomationError(
            RequestingSocket,
            RequestId,
            FString::Printf(
                TEXT("Invalid GI method: %s. Valid values: LumenGI, ScreenSpace, None, RayTraced, Lightmass"),
                *Method),
            TEXT("INVALID_GI_METHOD"));
        return true;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("method"), Method);
    Subsystem.SendAutomationResponse(
        RequestingSocket,
        RequestId,
        true,
        FString::Printf(TEXT("GI method configured: %s"), *Method),
        Resp);
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
    bool bRayTraced = false;
    if (ReadBool(TEXT("rayTracedShadows"), bRayTraced))
    {
        Resp->SetStringField(TEXT("rayTracedShadowsNote"),
            TEXT("rayTracedShadows is a different feature from virtual shadow maps and was NOT applied here; use configure_ray_tracing with feature='shadows'."));
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
        Resp->SetStringField(TEXT("actorName"), TargetActor->GetActorLabel());
    }

    if (Applied.Num() == 0)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            TEXT("No shadow settings supplied. Pass virtualShadowMaps, or actorName plus one of castShadows/shadowBias/shadowSlopeBias/shadowResolutionScale (top level or inside `settings`)."),
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
#endif
