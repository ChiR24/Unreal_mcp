#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Domains/Render/McpAutomationBridge_RenderSupport.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"

#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
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
        Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Scene capture actor already exists."), Result);
        return true;
    }
    TActor* Actor = SpawnActorInActiveWorld<TActor>(
        TActor::StaticClass(),
        ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector),
        ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator),
        Name);
    if (!Actor)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Failed to spawn scene capture actor."), TEXT("SPAWN_FAILED"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
    McpHandlerUtils::AddVerification(Result, Actor);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Scene capture actor created."), Result);
    return true;
}

UTextureRenderTarget2D* LoadRenderTarget2D(const FString& Path)
{
    UTextureRenderTarget2D* Target = LoadObject<UTextureRenderTarget2D>(nullptr, *Path);
    if (Target || Path.Contains(TEXT(".")))
    {
        return Target;
    }
    const FString Name = McpHandlerUtils::ExtractAssetName(Path);
    return LoadObject<UTextureRenderTarget2D>(nullptr, *(Path + TEXT(".") + Name));
}

bool ApplyCaptureSource(
    UObject* Component,
    const FString& Source,
    FString& OutError)
{
    if (!Component || Source.IsEmpty())
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
    UMcpAutomationBridgeSubsystem* Subsystem,
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

    FString Reference;
    ReadActorReference(Payload, Reference);
    AActor* Actor = FindRenderActor(Reference);
    if (!Actor)
    {
        if (SubAction == TEXT("configure_capture_resolution") ||
            SubAction == TEXT("configure_capture_source") ||
            SubAction == TEXT("assign_render_target") ||
            SubAction == TEXT("capture_scene"))
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId, FString::Printf(TEXT("Scene capture actor not found: %s"), *Reference), TEXT("ACTOR_NOT_FOUND"));
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
        return false;
    }

    if (SubAction == TEXT("configure_capture_source"))
    {
        FString Error;
        if (!ApplyCaptureSource(Capture2D ? static_cast<UObject*>(Capture2D) : static_cast<UObject*>(CaptureCube),
                GetJsonStringField(Payload, TEXT("captureSource")), Error))
        {
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
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Render target not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        Capture2D->TextureTarget = Target;
    }
    else if (SubAction == TEXT("configure_capture_resolution"))
    {
        int32 Resolution = 512;
        FString Error;
        if (!ReadBoundedIntField(Payload, TEXT("resolution"), 512, 1, 4096, Resolution, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (!Capture2D)
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId,
                TEXT("Only SceneCapture2D supports TextureRenderTarget2D resolution."),
                TEXT("UNSUPPORTED_TARGET"));
            return true;
        }
        if (!Capture2D->TextureTarget)
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId,
                TEXT("Assign a render target before configuring capture resolution."),
                TEXT("RENDER_TARGET_NOT_ASSIGNED"));
            return true;
        }
        Capture2D->TextureTarget->ResizeTarget(Resolution, Resolution);
    }
    else if (SubAction == TEXT("capture_scene"))
    {
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
    Result->SetBoolField(TEXT("captured"), false);
    if (SubAction == TEXT("configure_capture_resolution"))
    {
        Result->SetNumberField(TEXT("resolution"), GetJsonIntField(Payload, TEXT("resolution"), 512));
    }
    if (SubAction == TEXT("capture_scene"))
    {
        const bool bHasRenderTarget = (Capture2D && Capture2D->TextureTarget) || (CaptureCube && CaptureCube->TextureTarget);
        if (!bHasRenderTarget)
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId,
                TEXT("Cannot capture scene: assign a render target to the capture component first."),
                TEXT("RENDER_TARGET_NOT_ASSIGNED"));
            return true;
        }
        Result->SetBoolField(TEXT("captured"), true);
    }
    Result->SetStringField(TEXT("renderTargetPath"), Capture2D && Capture2D->TextureTarget
        ? Capture2D->TextureTarget->GetPathName()
        : TEXT(""));
    McpHandlerUtils::AddVerification(Result, Actor);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Scene capture action applied."), Result);
    return true;
}
}
#endif
