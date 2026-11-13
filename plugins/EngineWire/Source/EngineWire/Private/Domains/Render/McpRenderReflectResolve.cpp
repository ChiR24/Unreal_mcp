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

    int32 Resolution = UReflectionCaptureComponent::GetReflectionCaptureSize();
    FString Error;
    if (!ReadBoundedIntField(Payload, TEXT("resolution"), Resolution, 16, 4096, Resolution, Error))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<FString> Applied;
    TArray<FString> Unsupported;
    McpSetConsoleVariableNamed(
        TEXT("r.ReflectionCaptureResolution"),
        FString::FromInt(Resolution),
        Applied,
        Unsupported);

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

    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
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
