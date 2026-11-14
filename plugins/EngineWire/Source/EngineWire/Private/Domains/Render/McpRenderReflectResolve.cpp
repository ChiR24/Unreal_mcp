#include "Domains/Render/McpRenderPrivate.h"
#include "Domains/Render/McpRenderSupport.h"
#include "Domains/Render/McpRenderSupportSettings.h"

#include "EngineWireSubsystem.h"

#include "Components/ReflectionCaptureComponent.h"

namespace McpRenderHandlers
{
// configure_reflection_capture_resolution: the resolution is a project-wide
// CVar, so no actor is required. An explicit actorName marks that capture for
// recapture, otherwise every reflection capture in the level is marked.
// Requiring an actor made the action unreachable (its schema has no mandatory
// actor), surfacing as the dispatcher's generic "Unknown subAction".
bool HandleRenderReflectionResolutionAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction != TEXT("configure_reflection_capture_resolution"))
    {
        return false;
    }

    FString Reference;
    ReadActorReference(Payload, Reference);
    AActor* Actor = FindRenderActor(Reference);
    if (!Actor && !Reference.IsEmpty())
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Reflection capture actor not found: %s"), *Reference),
            TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    double Resolution = UReflectionCaptureComponent::GetReflectionCaptureSize();
    bool bSent = false;
    FString Error;
    if (!ReadBoundedNumberSetting(Payload, TEXT("resolution"), 16.0, 4096.0, true, Resolution, bSent, Error))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // The variable's help reads 'Must be power of 2'; the engine only clamps it to the cube map limit.
    if (bSent && !FMath::IsPowerOfTwo(static_cast<int32>(Resolution)))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("resolution must be a power of two from 16 "
            "to 4096 (16, 32, 64, 128, 256, 512, 1024, 2048 or 4096), not %d."), static_cast<int32>(Resolution)), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<FString> Applied;
    TArray<FString> Unsupported;
    McpSetConsoleVariableNamed(
        TEXT("r.ReflectionCaptureResolution"),
        FString::FromInt(static_cast<int32>(Resolution)),
        Applied,
        Unsupported);
    if (RefuseWhenNoConsoleVariableTook(Subsystem, RequestId, Applied, Unsupported, RequestingSocket))
    {
        return true;
    }

    int32 MarkedCaptures = 0;
    if (Actor)
    {
        if (UReflectionCaptureComponent* Capture = Actor->FindComponentByClass<UReflectionCaptureComponent>())
        {
            Capture->MarkDirtyForRecaptureOrUpload();
            ++MarkedCaptures;
        }
    }
    else if (UWorld* World = McpHandlerUtils::GetPlayOrEditorWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            UReflectionCaptureComponent* Capture = It->FindComponentByClass<UReflectionCaptureComponent>();
            // The request named no capture: a path-restricted caller marks only those in a level inside its prefixes.
            if (Capture && McpCapabilityPrincipal::ActiveRequestPermitsObject(*It))
            {
                Capture->MarkDirtyForRecaptureOrUpload();
                ++MarkedCaptures;
            }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeRenderResult();
    Result->SetNumberField(TEXT("resolution"), Resolution);
    Result->SetNumberField(TEXT("markedCaptures"), MarkedCaptures);
    AddStringArray(Result, TEXT("appliedCVars"), Applied);
    AddStringArray(Result, TEXT("unsupportedCVars"), Unsupported);
    if (Actor)
    {
        McpHandlerUtils::AddVerification(Result, Actor);
    }
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("Reflection capture resolution configured."), Result);
    return true;
}
}
