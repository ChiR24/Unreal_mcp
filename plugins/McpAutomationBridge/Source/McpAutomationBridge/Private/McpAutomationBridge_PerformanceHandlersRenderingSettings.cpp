#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PerformanceHandlersPrivate.h"

#include "ContentStreaming.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Scalability.h"

#if WITH_EDITOR
#include "Camera/PlayerCameraManager.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#endif

namespace McpPerformanceHandlers
{
#if WITH_EDITOR
namespace
{
void SetRenderingCVarInt(const TCHAR* Name, int32 Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value);
    }
}

void SetRenderingCVarFloat(const TCHAR* Name, float Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value);
    }
}
}
#endif

bool HandleRenderingSettingsAction(const FPerformanceActionContext& Context)
{
#if !WITH_EDITOR
    return false;
#else
    if (Context.Lower == TEXT("set_scalability"))
    {
        int32 Level = 3;
        Context.Payload->TryGetNumberField(TEXT("level"), Level);

        Scalability::FQualityLevels Quals;
        Quals.SetFromSingleQualityLevel(Level);
        Scalability::SetQualityLevels(Quals);
        Scalability::SaveState(GEditorIni);

        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Scalability set"), nullptr);
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

        SetRenderingCVarFloat(TEXT("r.ScreenPercentage"), static_cast<float>(Scale));
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Resolution scale set"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("set_vsync"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        SetRenderingCVarInt(TEXT("r.VSync"), bEnabled ? 1 : 0);
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
        SetRenderingCVarInt(TEXT("r.Nanite"), bEnabled ? 1 : 0);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Nanite configured"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("configure_lod"))
    {
        double LODBias = 0.0;
        if (Context.Payload->TryGetNumberField(TEXT("lodBias"), LODBias))
        {
            SetRenderingCVarFloat(
                TEXT("r.MipMapLODBias"), static_cast<float>(LODBias));
        }

        double ForceLOD = -1.0;
        if (Context.Payload->TryGetNumberField(TEXT("forceLOD"), ForceLOD))
        {
            SetRenderingCVarInt(TEXT("r.ForceLOD"), static_cast<int32>(ForceLOD));
        }

        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("LOD settings configured"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("configure_texture_streaming"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

        double PoolSize = 0.0;
        if (Context.Payload->TryGetNumberField(TEXT("poolSize"), PoolSize))
        {
            SetRenderingCVarFloat(
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

        SetRenderingCVarInt(TEXT("r.TextureStreaming"), bEnabled ? 1 : 0);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Texture streaming configured"), nullptr);
        return true;
    }

    return false;
#endif
}
}
