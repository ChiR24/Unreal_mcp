#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_EditorFunctionHandlersShared.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#if __has_include("Blueprint/UserWidget.h")
  #include "Blueprint/UserWidget.h"
#endif

#if __has_include("GameFramework/PlayerController.h")
  #include "GameFramework/PlayerController.h"
#endif

#if __has_include("Subsystems/UnrealEditorSubsystem.h")
  #include "Subsystems/UnrealEditorSubsystem.h"
#elif __has_include("UnrealEditorSubsystem.h")
  #include "UnrealEditorSubsystem.h"
#endif

namespace McpEditorFunctionHandlers {

bool HandleRuntimeFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FN, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (FN == TEXT("PLAY_SOUND_AT_LOCATION") || FN == TEXT("PLAY_SOUND_2D")) {
    const TSharedPtr<FJsonObject> *Params = nullptr;
    if (!Payload->TryGetObjectField(TEXT("params"), Params) ||
        !(*Params).IsValid()) { /* allow top-level path fields */
    }
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("path"), SoundPath))
      Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
    if (SoundPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                                 TEXT("soundPath or path required"),
                                 TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!GEditor) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Editor world not available"), nullptr,
                                    TEXT("EDITOR_WORLD_NOT_AVAILABLE"));
      return true;
    }
    UWorld *World = nullptr;
    if (UUnrealEditorSubsystem *UES =
            GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) {
      World = UES->GetEditorWorld();
    }
    if (!World) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Editor world not available"), nullptr,
                                    TEXT("EDITOR_WORLD_NOT_AVAILABLE"));
      return true;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(SoundPath)) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(TEXT("error"), TEXT("Sound asset not found"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Sound not found"), Err,
                                    TEXT("NOT_FOUND"));
      return true;
    }

    USoundBase *Snd =
        Cast<USoundBase>(UEditorAssetLibrary::LoadAsset(SoundPath));
    if (!Snd) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(TEXT("error"), TEXT("Sound asset not found"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Sound not found"), Err,
                                    TEXT("NOT_FOUND"));
      return true;
    }

    if (FN == TEXT("PLAY_SOUND_AT_LOCATION")) {
      float x = 0, y = 0, z = 0;
      const TSharedPtr<FJsonObject> *LocObj = nullptr;
      if (Payload->TryGetObjectField(TEXT("params"), LocObj) && LocObj &&
          (*LocObj).IsValid()) {
        (*LocObj)->TryGetNumberField(TEXT("x"), x);
        (*LocObj)->TryGetNumberField(TEXT("y"), y);
        (*LocObj)->TryGetNumberField(TEXT("z"), z);
      }
      FVector Loc(x, y, z);
      UGameplayStatics::SpawnSoundAtLocation(World, Snd, Loc);
    } else {
      UGameplayStatics::SpawnSoundAtLocation(World, Snd, FVector::ZeroVector);
    }

    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetBoolField(TEXT("success"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Sound played"), Out, FString());
    return true;
  }

  if (FN == TEXT("ADD_WIDGET_TO_VIEWPORT")) {
    FString WidgetPath;
    Payload->TryGetStringField(TEXT("widget_path"), WidgetPath);
    if (WidgetPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                                 TEXT("widget_path required"),
                                 TEXT("INVALID_ARGUMENT"));
      return true;
    }

    int32 zOrder = 0;
    Payload->TryGetNumberField(TEXT("z_order"), zOrder);
    int32 playerIndex = 0;
    Payload->TryGetNumberField(TEXT("player_index"), playerIndex);

    if (!GEditor) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Editor not available for widget creation"), nullptr,
          TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UClass *WidgetClass = LoadClass<UUserWidget>(nullptr, *WidgetPath);
    if (!WidgetClass) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(TEXT("error"), TEXT("Widget class not found"));
      Err->SetStringField(TEXT("widget_path"), WidgetPath);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Widget class not found"), Err,
                                    TEXT("WIDGET_NOT_FOUND"));
      return true;
    }

    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("No world available"), nullptr,
                                    TEXT("NO_WORLD"));
      return true;
    }

    APlayerController *PlayerController =
        UGameplayStatics::GetPlayerController(World, playerIndex);
    if (!PlayerController) {
      PlayerController = UGameplayStatics::GetPlayerController(World, 0);
      if (!PlayerController) {
        TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
        Err->SetStringField(TEXT("error"),
                            TEXT("Player controller not available"));
        Err->SetNumberField(TEXT("player_index"), playerIndex);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                      TEXT("Player controller not available"),
                                      Err, TEXT("NO_PLAYER_CONTROLLER"));
        return true;
      }
    }

    UUserWidget *Widget =
        CreateWidget<UUserWidget>(PlayerController, WidgetClass);
    if (!Widget) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Failed to create widget instance"),
                                    nullptr, TEXT("WIDGET_CREATION_FAILED"));
      return true;
    }

    Widget->AddToViewport(zOrder);
    const bool bIsInViewport = Widget->IsInViewport();

    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetBoolField(TEXT("success"), bIsInViewport);
    Out->SetStringField(TEXT("widget_path"), WidgetPath);
    Out->SetStringField(TEXT("widget_class"), WidgetClass->GetPathName());
    Out->SetNumberField(TEXT("z_order"), zOrder);
    Out->SetNumberField(TEXT("player_index"),
                        PlayerController ? playerIndex : 0);

    if (!bIsInViewport) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Failed to add widget to viewport"),
                                    Out, TEXT("ADD_TO_VIEWPORT_FAILED"));
      return true;
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Widget added to viewport"), Out,
                                  FString());
    return true;
  }

  return false;
}

}
#endif
