#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Domains/Render/McpAutomationBridge_RenderSupport.h"

#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
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
    UMcpAutomationBridgeSubsystem* Subsystem,
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

void SendConsoleResult(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    TArray<FString>& Applied,
    TArray<FString>& Unsupported,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
    Result->SetBoolField(TEXT("supported"), Applied.Num() > 0);
    AddStringArray(Result, TEXT("appliedCVars"), Applied);
    AddStringArray(Result, TEXT("unsupportedCVars"), Unsupported);
    Subsystem->SendAutomationResponse(
        Socket,
        RequestId,
        true,
        Applied.Num() > 0 ? TEXT("Render console settings applied.") : TEXT("Render console settings not available."),
        Result);
}
}

bool HandleRenderConsoleAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TArray<FString> Applied;
    TArray<FString> Unsupported;
    const TSharedPtr<FJsonObject> Settings = GetSettingsObject(Payload);

    if (SubAction == TEXT("configure_ray_traced_shadows"))
    {
        FBoundedConsoleSetting Samples;
        if (!ReadBoundedNumberSetting(Subsystem, RequestId, Settings, TEXT("SamplesPerPixel"), 1.0, 64.0, true, Samples, RequestingSocket))
        {
            return true;
        }
        AddEnabledCVar(Payload, TEXT("r.RayTracing.Shadows"), Applied, Unsupported);
        ApplyNumberSetting(Samples, TEXT("r.RayTracing.Shadows.SamplesPerPixel"), Applied, Unsupported);
    }
    else if (SubAction == TEXT("configure_ray_traced_gi"))
    {
        FBoundedConsoleSetting Samples;
        FBoundedConsoleSetting Bounces;
        if (!ReadBoundedNumberSetting(Subsystem, RequestId, Settings, TEXT("SamplesPerPixel"), 1.0, 64.0, true, Samples, RequestingSocket) ||
            !ReadBoundedNumberSetting(Subsystem, RequestId, Settings, TEXT("MaxBounces"), 0.0, 16.0, true, Bounces, RequestingSocket))
        {
            return true;
        }
        AddEnabledCVar(Payload, TEXT("r.RayTracing.GlobalIllumination"), Applied, Unsupported);
        ApplyNumberSetting(Samples, TEXT("r.RayTracing.GlobalIllumination.SamplesPerPixel"), Applied, Unsupported);
        ApplyNumberSetting(Bounces, TEXT("r.RayTracing.GlobalIllumination.MaxBounces"), Applied, Unsupported);
    }
    else if (SubAction == TEXT("configure_ray_traced_reflections"))
    {
        FBoundedConsoleSetting Samples;
        FBoundedConsoleSetting MaxRoughness;
        if (!ReadBoundedNumberSetting(Subsystem, RequestId, Settings, TEXT("SamplesPerPixel"), 1.0, 64.0, true, Samples, RequestingSocket) ||
            !ReadBoundedNumberSetting(Subsystem, RequestId, Settings, TEXT("MaxRoughness"), 0.0, 1.0, false, MaxRoughness, RequestingSocket))
        {
            return true;
        }
        AddEnabledCVar(Payload, TEXT("r.RayTracing.Reflections"), Applied, Unsupported);
        ApplyNumberSetting(Samples, TEXT("r.RayTracing.Reflections.SamplesPerPixel"), Applied, Unsupported);
        ApplyNumberSetting(MaxRoughness, TEXT("r.RayTracing.Reflections.MaxRoughness"), Applied, Unsupported);
    }
    else if (SubAction == TEXT("configure_ray_traced_ao"))
    {
        FBoundedConsoleSetting Intensity;
        FBoundedConsoleSetting Radius;
        if (!ReadBoundedNumberSetting(Subsystem, RequestId, Payload, TEXT("intensity"), 0.0, 10.0, false, Intensity, RequestingSocket) ||
            !ReadBoundedNumberSetting(Subsystem, RequestId, Settings, TEXT("Radius"), 0.0, 10000.0, false, Radius, RequestingSocket))
        {
            return true;
        }
        AddEnabledCVar(Payload, TEXT("r.RayTracing.AmbientOcclusion"), Applied, Unsupported);
        ApplyNumberSetting(Intensity, TEXT("r.RayTracing.AmbientOcclusion.Intensity"), Applied, Unsupported);
        ApplyNumberSetting(Radius, TEXT("r.RayTracing.AmbientOcclusion.Radius"), Applied, Unsupported);
    }
    else if (SubAction == TEXT("configure_path_tracing"))
    {
        FBoundedConsoleSetting Samples;
        FBoundedConsoleSetting Bounces;
        if (!ReadBoundedNumberSetting(Subsystem, RequestId, Settings, TEXT("SamplesPerPixel"), 1.0, 256.0, true, Samples, RequestingSocket) ||
            !ReadBoundedNumberSetting(Subsystem, RequestId, Settings, TEXT("MaxBounces"), 0.0, 16.0, true, Bounces, RequestingSocket))
        {
            return true;
        }
        AddEnabledCVar(Payload, TEXT("r.PathTracing"), Applied, Unsupported);
        ApplyNumberSetting(Samples, TEXT("r.PathTracing.SamplesPerPixel"), Applied, Unsupported);
        ApplyNumberSetting(Bounces, TEXT("r.PathTracing.MaxBounces"), Applied, Unsupported);
    }
    else
    {
        return false;
    }

    SendConsoleResult(Subsystem, RequestId, SubAction, Applied, Unsupported, RequestingSocket);
    return true;
}
}
#endif
