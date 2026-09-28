#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Domains/Performance/McpAutomationBridge_PerformanceHandlersPrivate.h"

#include "ContentStreaming.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Scalability.h"

#include "Camera/PlayerCameraManager.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

namespace McpPerformanceHandlers
{
bool HandleRenderingSettingsAction(const FPerformanceActionContext& Context)
{
    if (Context.Lower == TEXT("set_scalability"))
    {
        double RequestedLevel = 0.0;
        if (!Context.Payload->TryGetNumberField(TEXT("level"), RequestedLevel))
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                TEXT("level (0 low to 4 cinematic) is required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        const int32 Level = FMath::Clamp(FMath::RoundToInt(RequestedLevel), 0, 4);
        FString Category;
        Context.Payload->TryGetStringField(TEXT("category"), Category);
        Category.TrimStartAndEndInline();

        // category used to be ignored, so a per-group request moved every group.
        Scalability::FQualityLevels Quals = Scalability::GetQualityLevels();
        if (Category.IsEmpty() || Category.Equals(TEXT("Overall"), ESearchCase::IgnoreCase))
        {
            Quals.SetFromSingleQualityLevel(Level);
        }
        else
        {
            const TPair<const TCHAR*, int32*> Groups[] = {
                {TEXT("ViewDistance"), &Quals.ViewDistanceQuality}, {TEXT("AntiAliasing"), &Quals.AntiAliasingQuality},
                {TEXT("Shadow"), &Quals.ShadowQuality}, {TEXT("GlobalIllumination"), &Quals.GlobalIlluminationQuality},
                {TEXT("Reflection"), &Quals.ReflectionQuality}, {TEXT("PostProcess"), &Quals.PostProcessQuality},
                {TEXT("Texture"), &Quals.TextureQuality}, {TEXT("Effects"), &Quals.EffectsQuality},
                {TEXT("Foliage"), &Quals.FoliageQuality}, {TEXT("Shading"), &Quals.ShadingQuality}};
            int32* Target = nullptr;
            for (const TPair<const TCHAR*, int32*>& Group : Groups)
            {
                if (Category.Equals(Group.Key, ESearchCase::IgnoreCase))
                {
                    Target = Group.Value;
                }
            }
            if (!Target)
            {
                Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                    FString::Printf(TEXT("Unknown scalability category '%s'; use Overall, ViewDistance, AntiAliasing, Shadow, GlobalIllumination, Reflection, PostProcess, Texture, Effects, Foliage or Shading"), *Category),
                    TEXT("INVALID_ARGUMENT"));
                return true;
            }
            *Target = Level;
        }
        Scalability::SetQualityLevels(Quals);
        Scalability::SaveState(GEditorIni);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("category"), Category.IsEmpty() ? TEXT("Overall") : Category);
        Resp->SetNumberField(TEXT("level"), Level);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            FString::Printf(TEXT("Scalability %s set to %d"), Category.IsEmpty() ? TEXT("Overall") : *Category, Level), Resp);
        return true;
    }

    if (Context.Lower == TEXT("set_resolution_scale"))
    {
        double Scale = 100.0;
        if (!Context.Payload->TryGetNumberField(TEXT("scale"), Scale))
        {
            Context.Bridge.SendAutomationResponse(
                Context.RequestingSocket, Context.RequestId, false,
                TEXT("Scale required"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        SetCVarIfExists(TEXT("r.ScreenPercentage"), static_cast<float>(Scale));
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Resolution scale set"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("set_vsync"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        SetCVarIfExists(TEXT("r.VSync"), bEnabled ? 1 : 0);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("VSync configured"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("set_frame_rate_limit"))
    {
        double Limit = 0.0;
        if (!Context.Payload->TryGetNumberField(TEXT("maxFPS"), Limit))
        {
            Context.Bridge.SendAutomationResponse(
                Context.RequestingSocket, Context.RequestId, false,
                TEXT("maxFPS required"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        GEngine->SetMaxFPS(static_cast<float>(Limit));
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Max FPS set"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("configure_nanite"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        SetCVarIfExists(TEXT("r.Nanite"), bEnabled ? 1 : 0);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Nanite configured"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("configure_lod"))
    {
        TSharedPtr<FJsonObject> LodResult = McpHandlerUtils::CreateResultObject(); // dogfood #175: echo the CVars applied
        TSharedPtr<FJsonObject> AppliedCVars = MakeShared<FJsonObject>();
        double LODBias = 0.0;
        if (Context.Payload->TryGetNumberField(TEXT("lodBias"), LODBias))
        {
            SetCVarIfExists(TEXT("r.MipMapLODBias"), static_cast<float>(LODBias));
            AppliedCVars->SetNumberField(TEXT("r.MipMapLODBias"), LODBias);
        }
        double ForceLOD = -1.0;
        if (Context.Payload->TryGetNumberField(TEXT("forceLOD"), ForceLOD))
        {
            SetCVarIfExists(TEXT("r.ForceLOD"), static_cast<int32>(ForceLOD));
            AppliedCVars->SetNumberField(TEXT("r.ForceLOD"), static_cast<int32>(ForceLOD));
        }
        LodResult->SetObjectField(TEXT("appliedCVars"), AppliedCVars);
        LodResult->SetNumberField(TEXT("appliedCount"), AppliedCVars->Values.Num());
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            AppliedCVars->Values.Num() > 0 ? TEXT("LOD settings configured") : TEXT("No LOD setting supplied (pass lodBias and/or forceLOD)"), LodResult);
        return true;
    }

    if (Context.Lower == TEXT("configure_texture_streaming"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

        double PoolSize = 0.0;
        if (Context.Payload->TryGetNumberField(TEXT("poolSize"), PoolSize))
        {
            SetCVarIfExists(
                TEXT("r.Streaming.PoolSize"), static_cast<float>(PoolSize));
        }

        bool bBoost = false;
        if (Context.Payload->TryGetBoolField(TEXT("boostPlayerLocation"), bBoost) &&
            bBoost &&
            GEditor &&
            GEditor->GetEditorWorldContext().World())
        {
            APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(
                GEditor->GetEditorWorldContext().World(), 0);
            if (Cam)
            {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                IStreamingManager::Get().AddViewLocation(Cam->GetCameraLocation());
#endif
            }
        }

        SetCVarIfExists(TEXT("r.TextureStreaming"), bEnabled ? 1 : 0);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Texture streaming configured"), nullptr);
        return true;
    }

    return false;
}
}
