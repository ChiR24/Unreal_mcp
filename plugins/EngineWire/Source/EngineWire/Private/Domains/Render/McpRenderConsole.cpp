#include "Domains/Render/McpRenderPrivate.h"
#include "Domains/Render/McpRenderSupport.h"
#include "Domains/Render/McpRenderSupportSettings.h"

#include "EngineWireSubsystem.h"
#include "Modules/ModuleManager.h"

namespace McpRenderHandlers
{
namespace
{
struct FBoundedConsoleSetting
{
    bool bPresent = false;
    double Value = 0.0;
};

bool AddEnabledCVar(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& CVar,
    TArray<FString>& Applied,
    TArray<FString>& Unsupported)
{
    return SetConsoleVariable(
        CVar,
        GetJsonBoolField(Payload, TEXT("enabled"), true) ? TEXT("1") : TEXT("0"),
        Applied,
        Unsupported);
}

bool ReadBoundedNumberSetting(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Settings,
    const FString& Field,
    double MinValue,
    double MaxValue,
    bool bRequireWholeNumber,
    FBoundedConsoleSetting& OutSetting,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    OutSetting.bPresent = false;
    OutSetting.Value = 0.0;
    // UE 5.8 changed FJsonObject::Values' key type to UE::TSharedString<TCHAR>, so an FString no
    // longer implicitly converts in Contains(); use the public HasField() accessor instead.
    if (!Settings.IsValid() || !Settings->HasField(Field))
    {
        return true;
    }

    double Value = 0.0;
    if (!Settings->TryGetNumberField(Field, Value) || !FMath::IsFinite(Value) ||
        Value < MinValue || Value > MaxValue ||
        (bRequireWholeNumber && Value != FMath::FloorToDouble(Value)))
    {
        const FString Requirement = bRequireWholeNumber ? TEXT("a whole number ") : TEXT("");
        Subsystem->SendAutomationError(
            Socket,
            RequestId,
            FString::Printf(
                TEXT("%s must be %sbetween %.0f and %.0f"),
                *Field,
                *Requirement,
                MinValue,
                MaxValue),
            TEXT("INVALID_ARGUMENT"));
        return false;
    }

    OutSetting.bPresent = true;
    OutSetting.Value = Value;
    return true;
}

void ApplyNumberSetting(
    const FBoundedConsoleSetting& Setting,
    const FString& CVar,
    TArray<FString>& Applied,
    TArray<FString>& Unsupported)
{
    if (Setting.bPresent)
    {
        SetConsoleVariable(CVar, FString::SanitizeFloat(Setting.Value), Applied, Unsupported);
    }
}

// The denoisers this editor has, by the name each plugin registers its denoiser under (the module is the plugin's name;
// both are the same in 5.5-5.8).
// ponytail: a loaded module, not proof its denoiser registered (NNEDenoiser and NFOR register only when their runtime and
// settings allow, and the renderer offers no query by name in any 5.x); read the registry if that gap ever matters.
TArray<FString> RenderLoadedDenoisers()
{
    static const TCHAR* const Plugins[][2] = {{TEXT("NNEDenoiser"), TEXT("NNEDenoiser")}, {TEXT("OpenImageDenoise"), TEXT("OIDN")},
                                              {TEXT("OptiXDenoise"), TEXT("OptiX")}, {TEXT("NFORDenoise"), TEXT("NFOR")}};
    TArray<FString> Out;
    for (const auto& Plugin : Plugins)
    {
        if (FModuleManager::Get().IsModuleLoaded(Plugin[0]))
        {
            Out.Add(Plugin[1]);
        }
    }
    return Out;
}

void SendConsoleResult(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    TArray<FString>& Applied,
    TArray<FString>& Unsupported,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TArray<FString>* Denoisers = nullptr)
{
    // Nothing taking answered success ("settings not available"), so a missing variable read as configured.
    if (Applied.Num() == 0)
    {
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Settings not available: no console variable took: %s. "
            "Nothing was changed."), *FString::Join(Unsupported, TEXT("; "))), TEXT("CVAR_UNAVAILABLE"));
        return;
    }
    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
    Result->SetBoolField(TEXT("supported"), true);
    AddStringArray(Result, TEXT("appliedCVars"), Applied);
    AddStringArray(Result, TEXT("unsupportedCVars"), Unsupported);
    if (Denoisers)
    {
        AddStringArray(Result, TEXT("denoisers"), *Denoisers);
    }
    // r.RayTracing is fixed at startup: with it off, every ray-traced setting is stored and draws nothing.
    const IConsoleVariable* RayTracing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));
    const bool bRayTracing = RayTracing && RayTracing->GetInt() != 0;
    Result->SetBoolField(TEXT("rayTracingEnabled"), bRayTracing);
    if (!bRayTracing)
    {
        TArray<TSharedPtr<FJsonValue>> Warnings;
        Warnings.Add(MakeShared<FJsonValueString>(TEXT("Hardware ray tracing is off for this project (r.RayTracing 0): these settings "
            "are stored but draw nothing until Support Hardware Ray Tracing is on (Project Settings > Rendering), the editor "
            "restarts, and the GPU supports it.")));
        Result->SetArrayField(TEXT("warnings"), Warnings);
    }
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set %s%s"), *FString::Join(Applied, TEXT(", ")),
                        Unsupported.Num() > 0 ? *(TEXT("; not set: ") + FString::Join(Unsupported, TEXT("; "))) : TEXT("")),
        Result);
}
}

bool HandleRenderConsoleAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Field, bounds and cvar per sub-action; fields live under settings except ray-traced AO's top-level intensity.
    struct FNumberCVar
    {
        bool bTopLevel;
        const TCHAR* Field;
        double Min;
        double Max;
        bool bWhole;
        const TCHAR* CVar;
    };
    struct FConsoleAction
    {
        const TCHAR* EnableCVar;
        TArray<FNumberCVar> Numbers;
    };
    static const TMap<FString, FConsoleAction> Actions = {
        {TEXT("configure_ray_traced_shadows"), {TEXT("r.RayTracing.Shadows"), {
            {false, TEXT("SamplesPerPixel"), 1.0, 64.0, true, TEXT("r.RayTracing.Shadows.SamplesPerPixel")}}}},
        {TEXT("configure_ray_traced_gi"), {TEXT("r.RayTracing.GlobalIllumination"), {
            {false, TEXT("SamplesPerPixel"), 1.0, 64.0, true, TEXT("r.RayTracing.GlobalIllumination.SamplesPerPixel")},
            {false, TEXT("MaxBounces"), 0.0, 16.0, true, TEXT("r.RayTracing.GlobalIllumination.MaxBounces")}}}},
        {TEXT("configure_ray_traced_reflections"), {TEXT("r.RayTracing.Reflections"), {
            {false, TEXT("SamplesPerPixel"), 1.0, 64.0, true, TEXT("r.RayTracing.Reflections.SamplesPerPixel")},
            {false, TEXT("MaxRoughness"), 0.0, 1.0, false, TEXT("r.RayTracing.Reflections.MaxRoughness")}}}},
        {TEXT("configure_ray_traced_ao"), {TEXT("r.RayTracing.AmbientOcclusion"), {
            {true, TEXT("intensity"), 0.0, 10.0, false, TEXT("r.RayTracing.AmbientOcclusion.Intensity")},
            {false, TEXT("Radius"), 0.0, 10000.0, false, TEXT("r.RayTracing.AmbientOcclusion.Radius")}}}},
        {TEXT("configure_path_tracing"), {TEXT("r.PathTracing"), {
            {false, TEXT("SamplesPerPixel"), 1.0, 256.0, true, TEXT("r.PathTracing.SamplesPerPixel")},
            {false, TEXT("MaxBounces"), 0.0, 16.0, true, TEXT("r.PathTracing.MaxBounces")},
            {false, TEXT("Denoiser"), 0.0, 1.0, true, TEXT("r.PathTracing.Denoiser")}}}},
    };
    const FConsoleAction* ConsoleAction = Actions.Find(SubAction);
    if (!ConsoleAction)
    {
        return false;
    }

    const TSharedPtr<FJsonObject> Settings = GetSettingsObject(Payload);
    const bool bPathTracing = SubAction == TEXT("configure_path_tracing");
    // A settings key the action does not read was dropped without a word, so {"Samples": 8} answered applied.
    TArray<FString> Known;
    for (const FNumberCVar& Number : ConsoleAction->Numbers)
    {
        if (!Number.bTopLevel)
        {
            Known.Add(Number.Field);
        }
    }
    if (bPathTracing)
    {
        Known.Add(TEXT("DenoiserName"));
    }
    TArray<FString> Unknown;
    if (Settings.IsValid())
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>> Pair : Settings->Values)
        {
            if (!Known.ContainsByPredicate([&Pair](const FString& Field) { return Field.Equals(Pair.Key, ESearchCase::IgnoreCase); }))
            {
                Unknown.Add(Pair.Key);
            }
        }
    }
    if (Unknown.Num() > 0)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s does not read settings %s; it reads %s. "
            "Nothing was changed."), *SubAction, *FString::Join(Unknown, TEXT(", ")), *FString::Join(Known, TEXT(", "))),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // An unknown name makes the renderer use whichever denoiser it has, so only one a loaded plugin registers is taken.
    const TArray<FString> Denoisers = RenderLoadedDenoisers();
    FString DenoiserName;
    if (bPathTracing && Settings.IsValid() && Settings->TryGetStringField(TEXT("DenoiserName"), DenoiserName) && !Denoisers.Contains(DenoiserName))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("DenoiserName '%s' is not a denoiser this editor "
            "has loaded (%s). Nothing was changed."), *DenoiserName, Denoisers.Num() > 0 ? *FString::Join(Denoisers, TEXT(", ")) : TEXT("none")),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    TArray<FBoundedConsoleSetting> Values;
    for (const FNumberCVar& Number : ConsoleAction->Numbers)
    {
        if (!ReadBoundedNumberSetting(Subsystem, RequestId, Number.bTopLevel ? Payload : Settings, Number.Field,
                Number.Min, Number.Max, Number.bWhole, Values.AddDefaulted_GetRef(), RequestingSocket))
        {
            return true;
        }
    }
    TArray<FString> Applied;
    TArray<FString> Unsupported;
    AddEnabledCVar(Payload, ConsoleAction->EnableCVar, Applied, Unsupported);
    for (int32 Index = 0; Index < Values.Num(); ++Index)
    {
        ApplyNumberSetting(Values[Index], ConsoleAction->Numbers[Index].CVar, Applied, Unsupported);
    }
    if (!DenoiserName.IsEmpty())
    {
        SetConsoleVariable(TEXT("r.PathTracing.Denoiser.Name"), DenoiserName, Applied, Unsupported);
    }
    SendConsoleResult(Subsystem, RequestId, SubAction, Applied, Unsupported, RequestingSocket, bPathTracing ? &Denoisers : nullptr);
    return true;
}
}
