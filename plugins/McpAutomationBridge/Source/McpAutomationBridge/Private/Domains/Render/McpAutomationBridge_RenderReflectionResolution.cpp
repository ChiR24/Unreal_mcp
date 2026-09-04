#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Domains/Render/McpAutomationBridge_RenderSupport.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"

#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Components/ReflectionCaptureComponent.h"

namespace McpRenderHandlers
{
// Reflection actions declare an optional actorName; when it is omitted and the
// level holds exactly one reflection capture actor, that actor is the target.
// More than one is reported as AMBIGUOUS with the candidate labels; none as
// ACTOR_NOT_FOUND.
AActor* FindSoleReflectionCaptureActor(FString& OutError, FString& OutErrorCode)
{
    OutError.Reset();
    OutErrorCode.Reset();
    TArray<AActor*> Captures;
    if (UWorld* World = GetRenderWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->FindComponentByClass<UReflectionCaptureComponent>())
            {
                Captures.Add(*It);
            }
        }
    }
    if (Captures.Num() == 1)
    {
        return Captures[0];
    }
    if (Captures.Num() > 1)
    {
        TArray<FString> Labels;
        for (AActor* Capture : Captures)
        {
            Labels.Add(Capture->GetActorLabel());
        }
        OutError = FString::Printf(
            TEXT("Multiple reflection capture actors found (%d); pass actorName to pick one. Candidates: %s"),
            Captures.Num(), *FString::Join(Labels, TEXT(", ")));
        OutErrorCode = TEXT("AMBIGUOUS");
        return nullptr;
    }
    OutError = TEXT("Reflection capture actor not found: no reflection capture in the level (create one or pass actorName).");
    OutErrorCode = TEXT("ACTOR_NOT_FOUND");
    return nullptr;
}

// configure_reflection_capture_resolution: the resolution is a project-wide
// CVar, so no actor is required. An explicit actorName marks that capture for
// recapture, otherwise every reflection capture in the level is marked.
// Requiring an actor made the action unreachable (its schema has no mandatory
// actor), surfacing as the dispatcher's generic "Unknown subAction".
bool HandleRenderReflectionResolutionAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
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
    SetConsoleVariable(
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
    else if (UWorld* World = GetRenderWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (UReflectionCaptureComponent* Capture = It->FindComponentByClass<UReflectionCaptureComponent>())
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
#endif
