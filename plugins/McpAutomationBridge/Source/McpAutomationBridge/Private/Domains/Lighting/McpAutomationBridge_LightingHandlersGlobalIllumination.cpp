#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpAutomationBridge_LightingHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Dom/JsonObject.h"
#include "Engine/PostProcessVolume.h"
#include "Foundation/Render/McpPostProcessVolumeResolution.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"

namespace McpLightingHandlers
{

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

    // r.DynamicGlobalIlluminationMethod per method (Lightmass is baked: no dynamic GI).
    static const TMap<FString, int32> GIMethods = {
        {TEXT("None"), 0}, {TEXT("LumenGI"), 1}, {TEXT("ScreenSpace"), 2}, {TEXT("RayTraced"), 3}, {TEXT("Lightmass"), 0}};
    const int32* GIMethod = GIMethods.Find(Method);
    if (!GIMethod)
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
    // quality, indirectLightingIntensity and bounces were declared and never read.
    static const TMap<FString, int32> QualityLevels = {
        {TEXT("Low"), 0}, {TEXT("Medium"), 1}, {TEXT("High"), 2}, {TEXT("Epic"), 3}};
    const FString Quality = GetJsonStringField(Payload, TEXT("quality"));
    const int32* QualityLevel = QualityLevels.Find(Quality);
    if (!Quality.IsEmpty() && !QualityLevel)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid GI quality: %s. Valid values: Low, Medium, High, Epic"), *Quality),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    APostProcessVolume* PPV = nullptr;
    if (Payload->HasTypedField<EJson::Number>(TEXT("indirectLightingIntensity")))
    {
        FString ResolveError;
        FString ResolveErrorCode;
        PPV = McpRenderHandlers::McpResolvePostProcessVolume(
            McpHandlerUtils::GetEditorWorld(), Payload, true, ResolveError, ResolveErrorCode);
        if (!PPV)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                ResolveError.IsEmpty() ? FString(TEXT("No post-process volume for indirectLightingIntensity")) : ResolveError,
                ResolveErrorCode.IsEmpty() ? FString(TEXT("ACTOR_NOT_FOUND")) : ResolveErrorCode);
            return true;
        }
        PPV->Modify();
        PPV->Settings.bOverride_IndirectLightingIntensity = true;
        PPV->Settings.IndirectLightingIntensity = static_cast<float>(Payload->GetNumberField(TEXT("indirectLightingIntensity")));
        PPV->MarkPackageDirty();
    }
    if (QualityLevel)
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.GlobalIlluminationQuality")))
        {
            CVar->Set(*QualityLevel);
        }
    }
    // Baked (Lightmass) bounces live on the World Settings.
    AWorldSettings* WorldSettings = McpHandlerUtils::GetEditorWorld() ? McpHandlerUtils::GetEditorWorld()->GetWorldSettings() : nullptr;
    if (WorldSettings && Payload->HasTypedField<EJson::Number>(TEXT("bounces")))
    {
        WorldSettings->Modify();
        WorldSettings->LightmassSettings.NumIndirectLightingBounces =
            FMath::Clamp(FMath::RoundToInt(Payload->GetNumberField(TEXT("bounces"))), 0, 100);
        WorldSettings->MarkPackageDirty();
    }
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
    {
        CVar->Set(*GIMethod);
    }
    // Lumen GI pairs with Lumen reflections.
    IConsoleVariable* CVarRefl = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ReflectionMethod"));
    if (*GIMethod == 1 && CVarRefl)
    {
        CVarRefl->Set(1);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("method"), Method);
    if (QualityLevel)
    {
        Resp->SetStringField(TEXT("quality"), Quality);
    }
    if (PPV)
    {
        Resp->SetNumberField(TEXT("indirectLightingIntensity"), PPV->Settings.IndirectLightingIntensity);
        Resp->SetStringField(TEXT("postProcessVolume"), McpActorRef(PPV));
    }
    if (WorldSettings && Payload->HasTypedField<EJson::Number>(TEXT("bounces")))
    {
        Resp->SetNumberField(TEXT("bounces"), WorldSettings->LightmassSettings.NumIndirectLightingBounces);
    }
    Subsystem.SendAutomationResponse(
        RequestingSocket,
        RequestId,
        true,
        FString::Printf(TEXT("GI method configured: %s"), *Method),
        Resp);
    return true;
}

}
