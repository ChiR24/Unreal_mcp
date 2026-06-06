#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_EditorFunctionHandlersShared.h"

#if WITH_EDITOR
#include "Editor.h"
#include "GameFramework/WorldSettings.h"

#if __has_include("Subsystems/UnrealEditorSubsystem.h")
  #include "Subsystems/UnrealEditorSubsystem.h"
#elif __has_include("UnrealEditorSubsystem.h")
  #include "UnrealEditorSubsystem.h"
#endif

#if __has_include("Subsystems/LevelEditorSubsystem.h")
  #include "Subsystems/LevelEditorSubsystem.h"
#elif __has_include("LevelEditorSubsystem.h")
  #include "LevelEditorSubsystem.h"
#endif

#if __has_include("EditorLoadingAndSavingUtils.h")
  #include "EditorLoadingAndSavingUtils.h"
#elif __has_include("FileHelpers.h")
  #include "FileHelpers.h"
#endif

namespace McpEditorFunctionHandlers {

bool HandleEditorControlFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FN, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (FN == TEXT("SET_VIEWPORT_CAMERA") ||
      FN == TEXT("SET_VIEWPORT_CAMERA_INFO") ||
      FN == TEXT("SET_CAMERA_POSITION")) {
    const TSharedPtr<FJsonObject> *Params = nullptr;
    FVector Loc(0, 0, 0);
    FRotator Rot(0, 0, 0);
    if (Payload->TryGetObjectField(TEXT("params"), Params) && Params &&
        (*Params).IsValid()) {
      ReadVectorField(*Params, TEXT("location"), Loc, Loc);
      ReadRotatorField(*Params, TEXT("rotation"), Rot, Rot);
    } else {
      ReadVectorField(Payload, TEXT("location"), Loc, Loc);
      ReadRotatorField(Payload, TEXT("rotation"), Rot, Rot);
    }
    if (!GEditor) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false, TEXT("Editor not available"),
          nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    if (UUnrealEditorSubsystem *UES =
            GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) {
      UES->SetLevelViewportCameraInfo(Loc, Rot);
      if (ULevelEditorSubsystem *LES =
              GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
        LES->EditorInvalidateViewports();
      }
      TSharedPtr<FJsonObject> R = McpHandlerUtils::CreateResultObject();
      R->SetBoolField(TEXT("success"), true);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                    TEXT("Camera set"), R, FString());
    } else {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("UnrealEditorSubsystem not available"), nullptr,
          TEXT("SUBSYSTEM_NOT_FOUND"));
    }
    return true;
  }

  if (FN == TEXT("BUILD_LIGHTING")) {
    FString Quality;
    Payload->TryGetStringField(TEXT("quality"), Quality);
    if (!GEditor) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false, TEXT("Editor not available"),
          nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UWorld *CurrentWorld = GEditor->GetEditorWorldContext().World();
    if (!CurrentWorld) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Editor world not available for build lighting"), nullptr,
          TEXT("EDITOR_WORLD_NOT_AVAILABLE"));
      return true;
    }

    if (ULevelEditorSubsystem *LES =
            GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
      ELightingBuildQuality QualityEnum =
          ELightingBuildQuality::Quality_Production;
      if (!Quality.IsEmpty()) {
        const FString LowerQuality = Quality.ToLower();
        if (LowerQuality == TEXT("preview")) {
          QualityEnum = ELightingBuildQuality::Quality_Preview;
        } else if (LowerQuality == TEXT("medium")) {
          QualityEnum = ELightingBuildQuality::Quality_Medium;
        } else if (LowerQuality == TEXT("high")) {
          QualityEnum = ELightingBuildQuality::Quality_High;
        } else if (LowerQuality == TEXT("production")) {
          QualityEnum = ELightingBuildQuality::Quality_Production;
        } else {
          TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
          Err->SetBoolField(TEXT("success"), false);
          Err->SetStringField(TEXT("error"), TEXT("unknown_quality"));
          Err->SetStringField(TEXT("quality"), Quality);
          Err->SetStringField(
              TEXT("validValues"),
              TEXT("preview, medium, high, production"));
          Bridge.SendAutomationResponse(
              RequestingSocket, RequestId, false,
              TEXT("Unknown lighting quality"), Err, TEXT("UNKNOWN_QUALITY"));
          return true;
        }
      }
      if (AWorldSettings *WS = CurrentWorld->GetWorldSettings()) {
        if (WS->bForceNoPrecomputedLighting) {
          TSharedPtr<FJsonObject> R = McpHandlerUtils::CreateResultObject();
          R->SetBoolField(TEXT("skipped"), true);
          R->SetStringField(TEXT("reason"),
                            TEXT("bForceNoPrecomputedLighting is true"));
          Bridge.SendAutomationResponse(
              RequestingSocket, RequestId, true,
              TEXT("Lighting build skipped (precomputed lighting disabled)"), R,
              FString());
          return true;
        }
      }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      LES->BuildLightMaps(QualityEnum, /*bWithReflectionCaptures*/ false);
      TSharedPtr<FJsonObject> R = McpHandlerUtils::CreateResultObject();
      R->SetBoolField(TEXT("requested"), true);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                    TEXT("Build lighting requested"), R,
                                    FString());
#else
      TSharedPtr<FJsonObject> R = McpHandlerUtils::CreateResultObject();
      R->SetBoolField(TEXT("requested"), false);
      R->SetStringField(
          TEXT("error"), TEXT("BuildLightMaps not available in UE 5.0"));
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Build lighting not available in UE 5.0"), R,
          TEXT("NOT_AVAILABLE"));
#endif
    } else {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("LevelEditorSubsystem not available"), nullptr,
          TEXT("SUBSYSTEM_NOT_FOUND"));
    }
    return true;
  }

  if (FN == TEXT("SAVE_CURRENT_LEVEL")) {
    if (!GEditor) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false, TEXT("Editor not available"),
          nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    bool bSaved = false;
#if __has_include("EditorLoadingAndSavingUtils.h")
    bSaved = UEditorLoadingAndSavingUtils::SaveCurrentLevel();
#elif __has_include("FileHelpers.h")
    bSaved = FEditorFileUtils::SaveCurrentLevel();
#endif

    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetBoolField(TEXT("success"), bSaved);
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, bSaved,
        bSaved ? TEXT("Level saved") : TEXT("Failed to save level"), Out,
        bSaved ? FString() : TEXT("SAVE_FAILED"));
    return true;
  }

  return false;
}

}
#endif
