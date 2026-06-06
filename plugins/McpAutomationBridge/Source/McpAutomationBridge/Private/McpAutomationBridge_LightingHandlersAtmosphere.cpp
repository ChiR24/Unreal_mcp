#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LightingHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "Components/ExponentialHeightFogComponent.h"
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

    UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
    FogComp->bEnableVolumetricFog = true;

    double Distance;
    if (Payload->TryGetNumberField(TEXT("viewDistance"), Distance))
    {
        FogComp->VolumetricFogDistance = static_cast<float>(Distance);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), FogActor->GetActorLabel());
    Resp->SetBoolField(TEXT("enabled"), true);
    McpHandlerUtils::AddVerification(Resp, FogActor);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Volumetric fog enabled"), Resp);
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

    bool bValidMethod = false;
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
        bValidMethod = true;
    }
    else if (Method == TEXT("ScreenSpace"))
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
        {
            CVar->Set(2);
        }
        bValidMethod = true;
    }
    else if (Method == TEXT("None"))
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
        {
            CVar->Set(0);
        }
        bValidMethod = true;
    }
    else if (Method == TEXT("RayTraced"))
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
        {
            CVar->Set(3);
        }
        bValidMethod = true;
    }
    else if (Method == TEXT("Lightmass"))
    {
        if (IConsoleVariable* CVarGI = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
        {
            CVarGI->Set(0);
        }
        bValidMethod = true;
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
    Resp->SetBoolField(TEXT("success"), bValidMethod);
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
    bool bVirtual = false;
    if (Payload->TryGetBoolField(TEXT("virtualShadowMaps"), bVirtual) ||
        Payload->TryGetBoolField(TEXT("rayTracedShadows"), bVirtual))
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable")))
        {
            CVar->Set(bVirtual ? 1 : 0);
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("virtualShadowMaps"), bVirtual);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Shadows configured"), Resp);
    return true;
}

}
#endif
