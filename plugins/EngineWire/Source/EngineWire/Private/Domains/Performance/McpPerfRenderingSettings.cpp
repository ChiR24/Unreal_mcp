#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Domains/Performance/McpPerfPrivate.h"

#include "CoreGlobals.h"
#include "Scalability.h"

namespace McpPerformanceHandlers
{
namespace
{
const TCHAR* McpCVarSwitch(bool bOn)
{
    return bOn ? TEXT("1") : TEXT("0");
}
}

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
        // The editor loads its scalability from the editor settings ini at startup (LaunchEngineLoop); saved to the
        // editor ini, it was never read back and the next session started at the old levels.
        Scalability::SaveState(GEditorSettingsIni);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("category"), Category.IsEmpty() ? TEXT("Overall") : Category);
        Resp->SetNumberField(TEXT("level"), Level);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            FString::Printf(TEXT("Scalability %s set to %d"), Category.IsEmpty() ? TEXT("Overall") : *Category, Level), Resp);
        return true;
    }

    // The rest set console variables as the console does and read them back: set at code priority, a value typed in
    // the console kept its place and the reply still said the setting was made.
    TArray<FString> Applied, NotSet;
    if (Context.Lower == TEXT("set_resolution_scale"))
    {
        double Scale = 0.0;
        if (!Context.Payload->TryGetNumberField(TEXT("scale"), Scale) || Scale < 0.0)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                TEXT("scale is required: the render resolution in percent (100 is full resolution), or 0 for the engine default"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        SetPerfCVar(TEXT("r.ScreenPercentage"), FString::SanitizeFloat(Scale), Applied, NotSet);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
        // Before 5.3 Play In Editor takes the level viewport's screen percentage over r.ScreenPercentage while this is on.
        SetPerfCVar(TEXT("r.Editor.Viewport.OverridePIEScreenPercentage"), TEXT("0"), Applied, NotSet);
#endif
        SendPerfCVarResult(Context, Scale > 0.0 ? FString::Printf(TEXT("Game views render at %g%% resolution"), Scale)
                                                : FString(TEXT("Game views back to the engine default resolution")), Applied, NotSet);
        return true;
    }

    if (Context.Lower == TEXT("set_vsync"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        // The editor's windows, Play In Editor included, present by r.VSyncEditor and never read r.VSync, so setting
        // only r.VSync changed nothing on screen.
        SetPerfCVar(TEXT("r.VSyncEditor"), McpCVarSwitch(bEnabled), Applied, NotSet);
        SetPerfCVar(TEXT("r.VSync"), McpCVarSwitch(bEnabled), Applied, NotSet);
        SendPerfCVarResult(Context, bEnabled ? TEXT("VSync on") : TEXT("VSync off"), Applied, NotSet);
        return true;
    }

    if (Context.Lower == TEXT("set_frame_rate_limit"))
    {
        double Limit = 0.0;
        if (!Context.Payload->TryGetNumberField(TEXT("maxFPS"), Limit) || Limit < 0.0)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                TEXT("maxFPS is required: frames per second, 0 for no cap"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        SetPerfCVar(TEXT("t.MaxFPS"), FString::SanitizeFloat(Limit), Applied, NotSet);
        SendPerfCVarResult(Context, Limit > 0.0 ? FString::Printf(TEXT("Frame rate capped at %g"), Limit)
                                                : FString(TEXT("Frame rate cap removed")), Applied, NotSet);
        return true;
    }

    if (Context.Lower == TEXT("configure_nanite"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        SetPerfCVar(TEXT("r.Nanite"), McpCVarSwitch(bEnabled), Applied, NotSet);
        SendPerfCVarResult(Context, bEnabled ? TEXT("Nanite meshes render with Nanite")
                                             : TEXT("Nanite meshes render their fallback meshes"), Applied, NotSet);
        return true;
    }

    if (Context.Lower == TEXT("configure_lod"))
    {
        double LODBias = 0.0;
        double ForceLOD = -1.0;
        const bool bHasBias = Context.Payload->TryGetNumberField(TEXT("lodBias"), LODBias);
        const bool bHasForce = Context.Payload->TryGetNumberField(TEXT("forceLOD"), ForceLOD);
        if (!bHasBias && !bHasForce)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                TEXT("Pass lodBias and/or forceLOD. Nothing was changed."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (bHasBias)
        {
            SetPerfCVar(TEXT("r.MipMapLODBias"), FString::SanitizeFloat(LODBias), Applied, NotSet);
        }
        if (bHasForce)
        {
            SetPerfCVar(TEXT("r.ForceLOD"), FString::FromInt(FMath::RoundToInt(ForceLOD)), Applied, NotSet);
        }
        SendPerfCVarResult(Context, TEXT("LOD settings set"), Applied, NotSet);
        return true;
    }

    if (Context.Lower == TEXT("configure_texture_streaming"))
    {
        bool bEnabled = true;
        double PoolSize = 0.0;
        const bool bHasEnabled = Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        const bool bHasPool = Context.Payload->TryGetNumberField(TEXT("poolSize"), PoolSize);
        if (!bHasEnabled && !bHasPool)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                TEXT("Pass enabled and/or poolSize. Nothing was changed."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        // A pool of 0 MB holds no streamed mip, so every streamed texture would drop to its lowest one.
        if (bHasPool && FMath::RoundToInt(PoolSize) != -1 && PoolSize < 1.0)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                TEXT("poolSize is megabytes, at least 1, or -1 for the engine's default pool. Nothing was changed."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (bHasEnabled)
        {
            SetPerfCVar(TEXT("r.TextureStreaming"), McpCVarSwitch(bEnabled), Applied, NotSet);
        }
        if (bHasPool)
        {
            SetPerfCVar(TEXT("r.Streaming.PoolSize"), FString::FromInt(FMath::RoundToInt(PoolSize)), Applied, NotSet);
        }
        SendPerfCVarResult(Context, TEXT("Texture streaming set"), Applied, NotSet);
        return true;
    }

    return false;
}
}
