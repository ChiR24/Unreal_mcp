#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Domains/Render/McpAutomationBridge_RenderSupport.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"

#include "McpAutomationBridgeSubsystem.h"

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
            {false, TEXT("MaxBounces"), 0.0, 16.0, true, TEXT("r.PathTracing.MaxBounces")}}}},
    };
    const FConsoleAction* ConsoleAction = Actions.Find(SubAction);
    if (!ConsoleAction)
    {
        return false;
    }

    const TSharedPtr<FJsonObject> Settings = GetSettingsObject(Payload);
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
    SendConsoleResult(Subsystem, RequestId, SubAction, Applied, Unsupported, RequestingSocket);
    return true;
}
}
