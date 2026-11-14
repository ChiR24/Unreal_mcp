#include "Domains/Render/McpRenderPrivate.h"
#include "Domains/Render/McpRenderSupport.h"
#include "Domains/Render/McpRenderSupportSettings.h"

#include "EngineWireSubsystem.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SceneCaptureCube.h"
#include "Engine/TextureRenderTarget2D.h"

namespace McpRenderHandlers
{
namespace
{
template <typename TActor>
bool CreateCaptureActor(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TActor* Actor = SpawnNamedRenderActor<TActor>(Subsystem, RequestId, SubAction, Payload, Socket, TEXT("Scene capture actor"));
    if (!Actor)
    {
        return true;
    }
    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
    McpHandlerUtils::AddVerification(Result, Actor);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Scene capture actor created."), Result);
    return true;
}

// Through the shared canonicalizer and the registry, which takes a package or an object path.
UTextureRenderTarget2D* LoadRenderTarget2D(const FString& Path)
{
    const FString SafePath = SanitizeProjectRelativePath(Path);
    return SafePath.IsEmpty() ? nullptr : Cast<UTextureRenderTarget2D>(McpLoadAsset(SafePath));
}

bool ApplyCaptureSource(
    UObject* Component,
    const FString& Source,
    FString& OutError)
{
    // An omitted captureSource used to change nothing and still report the source applied.
    if (Source.IsEmpty())
    {
        OutError = TEXT("captureSource is required, e.g. FinalColorLDR, SceneColorHDR, SceneDepth, BaseColor or Normal.");
        return false;
    }
    if (!Component)
    {
        return true;
    }
    static const TMap<FString, FString> Aliases = {
        {TEXT("FinalColorLDR"), TEXT("SCS_FinalColorLDR")},
        {TEXT("SceneColorHDR"), TEXT("SCS_SceneColorHDR")},
        {TEXT("SceneDepth"), TEXT("SCS_SceneDepth")},
        {TEXT("BaseColor"), TEXT("SCS_BaseColor")},
        {TEXT("Normal"), TEXT("SCS_Normal")}
    };
    const FString Value = Aliases.Contains(Source) ? Aliases[Source] : Source;
    FProperty* Property = Component->GetClass()->FindPropertyByName(TEXT("CaptureSource"));
    return !Property ||
        ApplyJsonValueToProperty(
            Component, Property, MakeShared<FJsonValueString>(Value), OutError);
}
}

bool HandleRenderSceneCaptureAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("create_scene_capture_2d"))
    {
        return CreateCaptureActor<ASceneCapture2D>(Subsystem, RequestId, SubAction, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("create_scene_capture_cube"))
    {
        return CreateCaptureActor<ASceneCaptureCube>(Subsystem, RequestId, SubAction, Payload, RequestingSocket);
    }

    const bool bCaptureSubAction =
        SubAction == TEXT("configure_capture_resolution") ||
        SubAction == TEXT("configure_capture_source") ||
        SubAction == TEXT("assign_render_target") ||
        SubAction == TEXT("capture_scene");

    FString Reference;
    ReadActorReference(Payload, Reference);
    AActor* Actor = FindRenderActor(Reference);
    FString ResolveError;
    FString ResolveErrorCode;
    if (!Actor && Reference.IsEmpty() && bCaptureSubAction)
    {
        // actorName is optional in the published schema: fall back to the
        // level's sole scene capture actor instead of failing on an empty name.
        Actor = FindSoleRenderActor(
            [](AActor* Candidate) { return Candidate->IsA<ASceneCapture2D>() || Candidate->IsA<ASceneCaptureCube>(); },
            TEXT("scene capture"),
            TEXT("Scene capture actor not found: no SceneCapture2D/SceneCaptureCube in the level (create one with create_scene_capture_2d or pass actorName)."),
            ResolveError, ResolveErrorCode);
    }
    if (!Actor)
    {
        if (bCaptureSubAction)
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId,
                ResolveError.IsEmpty() ? FString::Printf(TEXT("Scene capture actor not found: %s"), *Reference) : ResolveError,
                ResolveErrorCode.IsEmpty() ? FString(TEXT("ACTOR_NOT_FOUND")) : ResolveErrorCode);
            return true;
        }
        return false;
    }

    USceneCaptureComponent2D* Capture2D =
        Cast<ASceneCapture2D>(Actor) ? Cast<ASceneCapture2D>(Actor)->GetCaptureComponent2D() : nullptr;
    USceneCaptureComponentCube* CaptureCube =
        Cast<ASceneCaptureCube>(Actor) ? Cast<ASceneCaptureCube>(Actor)->GetCaptureComponentCube() : nullptr;
    if (!Capture2D && !CaptureCube)
    {
        if (bCaptureSubAction)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("%s is no scene capture (SceneCapture2D or SceneCaptureCube)."), *Actor->GetActorLabel()),
                TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }
        return false;
    }

    UObject* CaptureComponent = Capture2D ? static_cast<UObject*>(Capture2D) : static_cast<UObject*>(CaptureCube);
    // The settings below change the placed actor; without Modify its level was not marked changed and a save missed them.
    if (SubAction != TEXT("capture_scene"))
    {
        CaptureComponent->Modify();
    }
    if (SubAction == TEXT("configure_capture_source"))
    {
        FString Error;
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Capture Source")),
                                                EMcpMutationDurability::EditorStateOnly, {CaptureComponent});
        if (!ApplyCaptureSource(CaptureComponent, GetJsonStringField(Payload, TEXT("captureSource")), Error))
        {
            // Refused before anything was written: the step goes.
            Transaction.Cancel();
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
    }
    else if (SubAction == TEXT("assign_render_target"))
    {
        if (!Capture2D)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Only SceneCapture2D accepts TextureRenderTarget2D."), TEXT("UNSUPPORTED_TARGET"));
            return true;
        }
        const FString TargetPath = GetJsonStringField(Payload, TEXT("renderTargetPath"));
        UTextureRenderTarget2D* Target = LoadRenderTarget2D(TargetPath);
        if (!Target)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Render target not found: %s"), *TargetPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Assign Render Target")),
                                                EMcpMutationDurability::EditorStateOnly, {Capture2D});
        Capture2D->TextureTarget = Target;
    }
    else if (SubAction == TEXT("configure_capture_resolution"))
    {
        // No default: a call without resolution resized the render target to 512 and said it was asked for.
        double Resolution = 0.0;
        bool bSent = false;
        FString Error;
        if (!ReadBoundedNumberSetting(Payload, TEXT("resolution"), 1.0, 4096.0, true, Resolution, bSent, Error) || !bSent)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                bSent ? Error : FString(TEXT("resolution must be a whole number between 1 and 4096")), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (!Capture2D)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Only SceneCapture2D supports TextureRenderTarget2D resolution."), TEXT("UNSUPPORTED_TARGET"));
            return true;
        }
        if (!Capture2D->TextureTarget)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Assign a render target before configuring capture resolution."), TEXT("RENDER_TARGET_NOT_ASSIGNED"));
            return true;
        }
        // The asset is reached through the actor, not named by the request: a path-restricted caller resizes only one
        // inside its prefixes.
        if (!McpCapabilityPrincipal::ActiveRequestPermitsObject(Capture2D->TextureTarget.Get()))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                TEXT("The capture's render target is outside the caller's allowed paths; nothing was resized."),
                TEXT("PATH_NOT_PERMITTED"));
            return true;
        }
        // The render target asset is what changes (every capture drawing into it), so it is what undo must restore.
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Resize Capture Target")),
                                                EMcpMutationDurability::EditorStateOnly, {Capture2D->TextureTarget.Get()});
        Capture2D->TextureTarget->ResizeTarget(static_cast<uint32>(Resolution), static_cast<uint32>(Resolution));
    }
    else if (SubAction == TEXT("capture_scene"))
    {
        // Refused before the call: a capture into no render target ran and was refused after.
        if (!((Capture2D && Capture2D->TextureTarget) || (CaptureCube && CaptureCube->TextureTarget)))
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId,
                TEXT("Cannot capture scene: assign a render target to the capture component first."),
                TEXT("RENDER_TARGET_NOT_ASSIGNED"));
            return true;
        }
        if (Capture2D)
        {
            Capture2D->CaptureScene();
        }
        if (CaptureCube)
        {
            CaptureCube->CaptureScene();
        }
    }
    else
    {
        return false;
    }

    if (Capture2D)
    {
        Capture2D->MarkRenderStateDirty();
    }
    if (CaptureCube)
    {
        CaptureCube->MarkRenderStateDirty();
    }
    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
    if (SubAction == TEXT("configure_capture_resolution"))
    {
        Result->SetNumberField(TEXT("resolution"), Capture2D->TextureTarget->SizeX);
    }
    if (SubAction == TEXT("capture_scene"))
    {
        Result->SetBoolField(TEXT("captured"), true);
    }
    if (Capture2D && Capture2D->TextureTarget)
    {
        Result->SetStringField(TEXT("renderTargetPath"), Capture2D->TextureTarget->GetPathName());
    }
    McpHandlerUtils::AddVerification(Result, Actor);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Scene capture action applied."), Result);
    return true;
}
}
