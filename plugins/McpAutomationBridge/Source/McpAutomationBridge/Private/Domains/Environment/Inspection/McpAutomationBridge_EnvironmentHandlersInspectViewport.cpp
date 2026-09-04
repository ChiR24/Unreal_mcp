#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
#include "EditorViewportClient.h"
#include "Engine/EngineBaseTypes.h"
#include "LevelEditorViewport.h"
#include "UnrealClient.h"

namespace McpEnvironmentHandlers {

// get_viewport_info: active viewport size, the level viewport client's camera
// (location/rotation/FOV), view mode, realtime flag, the inspected world type
// and, while PIE runs, the player view target and camera manager.
void McpAppendViewportInfo(TSharedPtr<FJsonObject> Resp)
{
    if (!Resp.IsValid())
    {
        return;
    }
    FViewport *ActiveViewport = GEditor ? GEditor->GetActiveViewport() : nullptr;
    Resp->SetBoolField(TEXT("hasActiveViewport"), ActiveViewport != nullptr);
    if (ActiveViewport)
    {
        const FIntPoint Size = ActiveViewport->GetSizeXY();
        Resp->SetNumberField(TEXT("width"), Size.X);
        Resp->SetNumberField(TEXT("height"), Size.Y);
    }
    else
    {
        Resp->SetStringField(TEXT("message"), TEXT("Viewport info not available in this context"));
    }

    // GCurrentLevelEditingViewportClient is the level viewport the user last
    // interacted with; fall back to the first registered level viewport so a
    // freshly opened editor still reports its camera.
    FLevelEditorViewportClient *Client = GCurrentLevelEditingViewportClient;
    int32 LevelViewportCount = 0;
    if (GEditor)
    {
        const TArray<FLevelEditorViewportClient *> &Clients = GEditor->GetLevelViewportClients();
        LevelViewportCount = Clients.Num();
        for (FLevelEditorViewportClient *Candidate : Clients)
        {
            if (Client)
            {
                break;
            }
            Client = Candidate;
        }
    }
    Resp->SetNumberField(TEXT("levelViewportCount"), LevelViewportCount);
    Resp->SetBoolField(TEXT("hasLevelViewportClient"), Client != nullptr);
    if (Client)
    {
        Resp->SetStringField(TEXT("viewportType"), Client->IsPerspective() ? TEXT("Perspective") : TEXT("Orthographic"));
        Resp->SetNumberField(TEXT("viewportTypeIndex"), static_cast<int32>(Client->GetViewportType()));
        Resp->SetObjectField(TEXT("cameraLocation"), McpMakeVectorObject(Client->GetViewLocation()));
        Resp->SetObjectField(TEXT("cameraRotation"), McpMakeRotatorObject(Client->GetViewRotation()));
        Resp->SetNumberField(TEXT("cameraFov"), Client->ViewFOV);
        Resp->SetNumberField(TEXT("orthoZoom"), Client->GetOrthoZoom());
        FString ViewMode = StaticEnum<EViewModeIndex>()->GetNameStringByValue(static_cast<int64>(Client->GetViewMode()));
        ViewMode.RemoveFromStart(TEXT("VMI_"));
        Resp->SetStringField(TEXT("viewMode"), ViewMode);
        Resp->SetBoolField(TEXT("isRealtime"), Client->IsRealtime());
        Resp->SetBoolField(TEXT("isLevelEditorClient"), Client->IsLevelEditorClient());
        Resp->SetBoolField(TEXT("isCurrentLevelEditingViewport"), Client == GCurrentLevelEditingViewportClient);
        UWorld *ClientWorld = Client->GetWorld();
        Resp->SetStringField(TEXT("viewportWorld"), ClientWorld ? ClientWorld->GetName() : TEXT(""));
    }

    UWorld *World = McpGetRuntimeInspectionWorld();
    Resp->SetStringField(TEXT("worldType"), McpGetWorldTypeName(World));
    Resp->SetStringField(TEXT("worldName"), World ? World->GetName() : TEXT(""));
    const bool bPie = GEditor && GEditor->PlayWorld != nullptr;
    Resp->SetBoolField(TEXT("isPIE"), bPie);
    APlayerController *PlayerController = bPie ? GEditor->PlayWorld->GetFirstPlayerController() : nullptr;
    if (!PlayerController)
    {
        return;
    }
    Resp->SetStringField(TEXT("playerController"), PlayerController->GetPathName());
    if (AActor *ViewTarget = PlayerController->GetViewTarget())
    {
        Resp->SetStringField(TEXT("viewTarget"), ViewTarget->GetPathName());
        Resp->SetStringField(TEXT("viewTargetClass"), ViewTarget->GetClass()->GetName());
    }
    if (APlayerCameraManager *CameraManager = PlayerController->PlayerCameraManager)
    {
        Resp->SetStringField(TEXT("cameraManager"), CameraManager->GetPathName());
        Resp->SetStringField(TEXT("cameraManagerClass"), CameraManager->GetClass()->GetName());
        Resp->SetObjectField(TEXT("pieCameraLocation"), McpMakeVectorObject(CameraManager->GetCameraLocation()));
        Resp->SetObjectField(TEXT("pieCameraRotation"), McpMakeRotatorObject(CameraManager->GetCameraRotation()));
        Resp->SetNumberField(TEXT("pieCameraFov"), CameraManager->GetFOVAngle());
    }
}

} // namespace McpEnvironmentHandlers
#endif
