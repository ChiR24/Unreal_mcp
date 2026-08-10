#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Domains/Render/McpAutomationBridge_RenderSupport.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"

#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Components/PlanarReflectionComponent.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Engine/BoxReflectionCapture.h"
#include "Engine/PlanarReflection.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"
#include "Engine/SphereReflectionCapture.h"

namespace McpRenderHandlers
{
namespace
{
template <typename TActor>
bool CreateReflectionActor(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString Name = GetJsonStringField(Payload, TEXT("actorName"));
    if (Name.IsEmpty())
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("actorName is required."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (AActor* Existing = FindRenderActor(Name))
    {
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        Result->SetBoolField(TEXT("alreadyExists"), true);
        McpHandlerUtils::AddVerification(Result, Existing);
        Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Reflection actor already exists."), Result);
        return true;
    }
    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    TActor* Actor = SpawnActorInActiveWorld<TActor>(TActor::StaticClass(), Location, Rotation, Name);
    if (!Actor)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn reflection actor."), TEXT("SPAWN_FAILED"));
        return true;
    }
    TArray<FString> Applied;
    TArray<FString> Unsupported;
    FString Error;
    if (UActorComponent* Component = Actor->GetComponentByClass(UReflectionCaptureComponent::StaticClass()))
    {
        if (!ApplyJsonSettings(Component, Component->GetClass(), GetSettingsObject(Payload), false, Applied, Unsupported, Error))
        {
            Subsystem->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
    }
    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
    AddStringArray(Result, TEXT("appliedSettings"), Applied);
    AddStringArray(Result, TEXT("unsupportedSettings"), Unsupported);
    McpHandlerUtils::AddVerification(Result, Actor);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Reflection actor created."), Result);
    return true;
}

bool ApplyPostProcessReflectionSettings(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Reference;
    ReadActorReference(Payload, Reference);
    APostProcessVolume* Volume = Cast<APostProcessVolume>(FindRenderActor(Reference));
    if (!Volume)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("PostProcessVolume not found."), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    TArray<FString> Applied;
    TArray<FString> Unsupported;
    FString Error;
    if (!ApplyJsonSettings(
            &Volume->Settings, FPostProcessSettings::StaticStruct(), GetSettingsObject(Payload),
            true, Applied, Unsupported, Error))
    {
        Subsystem->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTING"));
        return true;
    }
    Volume->Modify();
    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
    AddStringArray(Result, TEXT("appliedSettings"), Applied);
    AddStringArray(Result, TEXT("unsupportedSettings"), Unsupported);
    McpHandlerUtils::AddVerification(Result, Volume);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Reflection post-process settings applied."), Result);
    return true;
}
}

bool HandleRenderReflectionAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("create_sphere_reflection_capture"))
    {
        return CreateReflectionActor<ASphereReflectionCapture>(Subsystem, RequestId, SubAction, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("create_box_reflection_capture"))
    {
        return CreateReflectionActor<ABoxReflectionCapture>(Subsystem, RequestId, SubAction, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("create_planar_reflection"))
    {
        return CreateReflectionActor<APlanarReflection>(Subsystem, RequestId, SubAction, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("configure_ssr_settings") ||
        SubAction == TEXT("configure_lumen_reflection_settings"))
    {
        return ApplyPostProcessReflectionSettings(Subsystem, RequestId, SubAction, Payload, RequestingSocket);
    }

    FString Reference;
    ReadActorReference(Payload, Reference);
    AActor* Actor = FindRenderActor(Reference);
    if (!Actor)
    {
        // These subActions act on an existing reflection capture actor. Falling
        // through to the dispatcher would surface its generic "Unknown
        // subAction" refusal, which names an internal dispatch concept the
        // client never declared (MCPBB-088). Name the real missing prerequisite
        // instead, mirroring RenderSceneCapture.cpp.
        if (SubAction == TEXT("configure_capture_offset") ||
            SubAction == TEXT("recapture_scene") ||
            SubAction == TEXT("configure_planar_reflection"))
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId,
                FString::Printf(TEXT("Reflection capture actor not found: %s"), *Reference),
                TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        return false;
    }

    if (SubAction == TEXT("configure_capture_offset"))
    {
        UReflectionCaptureComponent* Capture = Actor->FindComponentByClass<UReflectionCaptureComponent>();
        if (!Capture)
        {
            return false;
        }
        Capture->CaptureOffset = ExtractVectorField(Payload, TEXT("captureOffset"), Capture->CaptureOffset);
        Capture->MarkDirtyForRecaptureOrUpload();
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        McpHandlerUtils::AddVerification(Result, Actor);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Capture offset configured."), Result);
        return true;
    }
    if (SubAction == TEXT("recapture_scene"))
    {
        UReflectionCaptureComponent* Capture = Actor->FindComponentByClass<UReflectionCaptureComponent>();
        if (!Capture)
        {
            return false;
        }
        Capture->MarkDirtyForRecapture();
        if (UWorld* World = GetRenderWorld())
        {
            UReflectionCaptureComponent::UpdateReflectionCaptureContents(World, TEXT("MCP recapture"));
        }
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        Result->SetBoolField(TEXT("recaptured"), true);
        McpHandlerUtils::AddVerification(Result, Actor);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Reflection capture recaptured."), Result);
        return true;
    }
    if (SubAction == TEXT("configure_planar_reflection"))
    {
        APlanarReflection* Planar = Cast<APlanarReflection>(Actor);
        UPlanarReflectionComponent* Component = Planar ? Planar->GetPlanarReflectionComponent() : nullptr;
        if (!Component)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Planar reflection component not found."), TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }
        TArray<FString> Applied;
        TArray<FString> Unsupported;
        FString Error;
        const TSharedPtr<FJsonObject> Settings = GetSettingsObject(Payload);
        if (!ValidateBoundedNumberSetting(Settings, TEXT("ScreenPercentage"), 1.0, 200.0, Error) ||
            !ApplyJsonSettings(Component, Component->GetClass(), Settings, false, Applied, Unsupported, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
        Component->MarkRenderStateDirty();
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        AddStringArray(Result, TEXT("appliedSettings"), Applied);
        AddStringArray(Result, TEXT("unsupportedSettings"), Unsupported);
        McpHandlerUtils::AddVerification(Result, Actor);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Planar reflection configured."), Result);
        return true;
    }
    if (SubAction == TEXT("configure_reflection_capture_resolution"))
    {
        UReflectionCaptureComponent* Capture = Actor->FindComponentByClass<UReflectionCaptureComponent>();
        if (!Capture)
        {
            return false;
        }
        int32 Resolution = UReflectionCaptureComponent::GetReflectionCaptureSize();
        FString Error;
        if (!ReadBoundedIntField(Payload, TEXT("resolution"), Resolution, 16, 4096, Resolution, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        TArray<FString> Applied;
        TArray<FString> Unsupported;
        SetConsoleVariable(
            TEXT("r.ReflectionCaptureResolution"),
            FString::FromInt(Resolution),
            Applied,
            Unsupported);
        Capture->MarkDirtyForRecaptureOrUpload();
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        Result->SetNumberField(TEXT("resolution"), Resolution);
        AddStringArray(Result, TEXT("appliedCVars"), Applied);
        AddStringArray(Result, TEXT("unsupportedCVars"), Unsupported);
        McpHandlerUtils::AddVerification(Result, Actor);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Reflection capture resolution configured."), Result);
        return true;
    }

    return false;
}
}
#endif
