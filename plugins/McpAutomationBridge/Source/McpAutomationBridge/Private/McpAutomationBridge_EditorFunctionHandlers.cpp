#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#if WITH_EDITOR
#include "Editor.h"
#include "GameFramework/Pawn.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
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
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#if __has_include("EditorLoadingAndSavingUtils.h")
#include "EditorLoadingAndSavingUtils.h"
#elif __has_include("FileHelpers.h")
#include "FileHelpers.h"
#endif
#include "Factories/Factory.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Base64.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"
#include "UObject/SoftObjectPath.h"
#if __has_include("Blueprint/UserWidget.h")
#include "Blueprint/UserWidget.h"
#endif
#if __has_include("GameFramework/PlayerController.h")
#include "GameFramework/PlayerController.h"
#endif
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/OutputDeviceNull.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleExecuteEditorFunction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  // Accept either the generic execute_editor_function action or the
  // more specific execute_console_command action. This allows the
  // server to use native console commands for health checks and diagnostics.
  if (!Lower.Equals(TEXT("execute_editor_function"), ESearchCase::IgnoreCase) &&
      !Lower.Contains(TEXT("execute_editor_function")) &&
      !Lower.Equals(TEXT("execute_console_command")) &&
      !Lower.Contains(TEXT("execute_console_command")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("execute_editor_function payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Handle native console command action first — console commands
  // carry a top-level `command` (or params.command) and should not
  // be treated as a generic execute_editor_function requiring a
  // functionName field.
  if (Lower.Equals(TEXT("execute_console_command")) ||
      Lower.Contains(TEXT("execute_console_command"))) {
    // Accept either a top-level 'command' string or nested params.command
    FString Cmd;
    if (!Payload->TryGetStringField(TEXT("command"), Cmd)) {
      const TSharedPtr<FJsonObject> *ParamsPtr = nullptr;
      if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr &&
          (*ParamsPtr).IsValid()) {
        (*ParamsPtr)->TryGetStringField(TEXT("command"), Cmd);
      }
    }
    if (Cmd.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("command required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    bool bExecCalled = false;
    bool bOk = false;

    // Prefer executing with a valid editor world context where possible to
    // avoid assertions inside engine helpers that require a proper world
    // (e.g. when running Open/Map commands).
    UWorld *TargetWorld = nullptr;
#if WITH_EDITOR
    if (GEditor) {
      if (UUnrealEditorSubsystem *UES =
              GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) {
        TargetWorld = UES->GetEditorWorld();
      }
      if (!TargetWorld) {
        TargetWorld = GEditor->GetEditorWorldContext().World();
      }
    }
#endif

    if (GEditor && TargetWorld) {
      bOk = GEditor->Exec(TargetWorld, *Cmd);
      bExecCalled = true;
    }

    // Fallback: try all known engine world contexts if the editor world
    // did not handle the command successfully.
    if (!bOk && GEngine) {
      for (const FWorldContext &Ctx : GEngine->GetWorldContexts()) {
        UWorld *World = Ctx.World();
        if (!World)
          continue;
        const bool bWorldOk = GEngine->Exec(World, *Cmd);
        bExecCalled = bExecCalled || bWorldOk;
        if (bWorldOk) {
          bOk = true;
          break;
        }
      }
    }

    // If we could not find any valid world to execute against, avoid
    // invoking the engine command path entirely and return a structured
    // error instead of risking an assertion.
    if (!bExecCalled && !TargetWorld) {
      TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
      Out->SetStringField(TEXT("command"), Cmd);
      Out->SetBoolField(TEXT("success"), false);
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor world not available for command"),
                             Out, TEXT("EDITOR_WORLD_NOT_AVAILABLE"));
      return true;
    }

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("command"), Cmd);
    Out->SetBoolField(TEXT("success"), bOk);
    SendAutomationResponse(RequestingSocket, RequestId, bOk,
                           bOk ? TEXT("Command executed")
                               : TEXT("Command not executed"),
                           Out, bOk ? FString() : TEXT("EXEC_FAILED"));
    return true;
  }

  // For other execute_editor_function cases require functionName
  FString FunctionName;
  Payload->TryGetStringField(TEXT("functionName"), FunctionName);
  if (FunctionName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("functionName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString FN = FunctionName.ToUpper();
  // Accept either a top-level 'command' string or nested params.command
  FString Cmd;
  if (!Payload->TryGetStringField(TEXT("command"), Cmd)) {
    const TSharedPtr<FJsonObject> *ParamsPtr = nullptr;
    if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr &&
        (*ParamsPtr).IsValid()) {
      (*ParamsPtr)->TryGetStringField(TEXT("command"), Cmd);
    }
  }
  // (Console handling moved earlier)

#if WITH_EDITOR
  // Dispatch a handful of well-known functions to native handlers
  if (FN == TEXT("GET_ALL_ACTORS") || FN == TEXT("GET_ALL_ACTORS_SIMPLE")) {
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("EditorActorSubsystem not available"),
                             nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
      return true;
    }
    TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Reserve(Actors.Num());
    for (AActor *A : Actors) {
      if (!A)
        continue;
      TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
      E->SetStringField(TEXT("name"), A->GetName());
      E->SetStringField(TEXT("label"), A->GetActorLabel());
      E->SetStringField(TEXT("path"), A->GetPathName());
      E->SetStringField(TEXT("class"), A->GetClass()
                                           ? A->GetClass()->GetPathName()
                                           : TEXT(""));
      Arr.Add(MakeShared<FJsonValueObject>(E));
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("actors"), Arr);
    Result->SetNumberField(TEXT("count"), Arr.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Actor list"), Result, FString());
    return true;
  }

  if (FN == TEXT("SPAWN_ACTOR") || FN == TEXT("SPAWN_ACTOR_AT_LOCATION")) {
    FString ClassPath;
    Payload->TryGetStringField(TEXT("class_path"), ClassPath);
    if (ClassPath.IsEmpty())
      Payload->TryGetStringField(TEXT("classPath"), ClassPath);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    FVector Loc(0, 0, 0);
    FRotator Rot(0, 0, 0);
    if (Payload->TryGetObjectField(TEXT("params"), LocObj) && LocObj &&
        (*LocObj).IsValid()) {
      const TSharedPtr<FJsonObject> &P = *LocObj;
      ReadVectorField(P, TEXT("location"), Loc, Loc);
      ReadRotatorField(P, TEXT("rotation"), Rot, Rot);
    } else {
      if (const TSharedPtr<FJsonValue> LocVal =
              Payload->TryGetField(TEXT("location"))) {
        if (LocVal->Type == EJson::Array) {
          const TArray<TSharedPtr<FJsonValue>> &A = LocVal->AsArray();
          if (A.Num() >= 3)
            Loc = FVector((float)A[0]->AsNumber(), (float)A[1]->AsNumber(),
                          (float)A[2]->AsNumber());
        } else if (LocVal->Type == EJson::Object) {
          const TSharedPtr<FJsonObject> LocObject = LocVal->AsObject();
          if (LocObject.IsValid())
            ReadVectorField(LocObject, TEXT("location"), Loc, Loc);
        }
      }
    }

    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("EditorActorSubsystem not available"),
                             nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
      return true;
    }
    UClass *Resolved = nullptr;
    if (!ClassPath.IsEmpty()) {
      Resolved = ResolveClassByName(ClassPath);
    }
    if (!Resolved) {
      TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("error"), TEXT("Class not found"));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Class not found"), Err,
                             TEXT("CLASS_NOT_FOUND"));
      return true;
    }
    AActor *Spawned = SpawnActorInActiveWorld<AActor>(Resolved, Loc, Rot);
    if (!Spawned) {
      TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("error"), TEXT("Spawn failed"));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Spawn failed"), Err, TEXT("SPAWN_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
    Out->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
    Out->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Actor spawned"), Out, FString());
    return true;
  }

  if (FN == TEXT("DELETE_ACTOR") || FN == TEXT("DESTROY_ACTOR")) {
    // ... (existing delete logic) ...
    FString Target;
    Payload->TryGetStringField(TEXT("actor_name"), Target);
    if (Target.IsEmpty())
      Payload->TryGetStringField(TEXT("actorName"), Target);
    if (Target.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("actor_name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("EditorActorSubsystem not available"),
                             nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
      return true;
    }
    AActor *Found = nullptr;
    for (AActor *A : ActorSS->GetAllLevelActors()) {
      if (!A)
        continue;
      if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
          A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
          A->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) {
        Found = A;
        break;
      }
    }
    if (!Found) {
      TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("error"), TEXT("Actor not found"));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Actor not found"), Err,
                             TEXT("ACTOR_NOT_FOUND"));
      return true;
    }
    const bool bDeleted = ActorSS->DestroyActor(Found);
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), bDeleted);
    if (bDeleted) {
      Out->SetStringField(TEXT("deleted"), Found->GetActorLabel());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Actor deleted"), Out, FString());
    } else {
      Out->SetStringField(TEXT("error"), TEXT("Delete failed"));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Delete failed"), Out, TEXT("DELETE_FAILED"));
    }
    return true;
  }

  if (FN == TEXT("POSSESS")) {
    FString TargetName;
    Payload->TryGetStringField(TEXT("actor_name"), TargetName);
    if (TargetName.IsEmpty())
      Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!GEditor || !GEditor->IsPlaySessionInProgress()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Possess only available during PIE session"),
                             nullptr, TEXT("NOT_IN_PIE"));
      return true;
    }

    UWorld *PlayWorld = GEditor->PlayWorld;
    if (!PlayWorld) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("PIE World not found"), nullptr,
                             TEXT("WORLD_NOT_FOUND"));
      return true;
    }

    APawn *FoundPawn = nullptr;
    for (TActorIterator<APawn> It(PlayWorld); It; ++It) {
      APawn *P = *It;
      if (!P)
        continue;
      if (P->GetActorLabel().Equals(TargetName, ESearchCase::IgnoreCase) ||
          P->GetName().Equals(TargetName, ESearchCase::IgnoreCase)) {
        FoundPawn = P;
        break;
      }
    }

    if (!FoundPawn) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Pawn not found in PIE world"), nullptr,
                             TEXT("PAWN_NOT_FOUND"));
      return true;
    }

    APlayerController *PC = PlayWorld->GetFirstPlayerController();
    if (!PC) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No PlayerController found in PIE"), nullptr,
                             TEXT("PC_NOT_FOUND"));
      return true;
    }

    PC->Possess(FoundPawn);

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), true);
    Out->SetStringField(TEXT("possessed"), FoundPawn->GetActorLabel());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Possessed pawn"), Out, FString());
    return true;
  }

  if (FN == TEXT("ASSET_EXISTS") || FN == TEXT("ASSET_EXISTS_SIMPLE")) {
    FString PathToCheck;
    // Accept either top-level 'path' or nested params.path
    if (!Payload->TryGetStringField(TEXT("path"), PathToCheck)) {
      const TSharedPtr<FJsonObject> *ParamsPtr = nullptr;
      if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr &&
          (*ParamsPtr).IsValid()) {
        (*ParamsPtr)->TryGetStringField(TEXT("path"), PathToCheck);
      }
    }
    if (PathToCheck.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Perform check on game thread
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    const bool bExists = UEditorAssetLibrary::DoesAssetExist(PathToCheck);
    Out->SetBoolField(TEXT("exists"), bExists);
    Out->SetStringField(TEXT("path"), PathToCheck);
    Out->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           bExists ? TEXT("Asset exists")
                                   : TEXT("Asset not found"),
                           Out, bExists ? FString() : TEXT("NOT_FOUND"));
    return true;
  }

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
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    if (UUnrealEditorSubsystem *UES =
            GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) {
      UES->SetLevelViewportCameraInfo(Loc, Rot);
      if (ULevelEditorSubsystem *LES =
              GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
        LES->EditorInvalidateViewports();
      }
      TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
      R->SetBoolField(TEXT("success"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Camera set"), R, FString());
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("UnrealEditorSubsystem not available"),
                             nullptr, TEXT("SUBSYSTEM_NOT_FOUND"));
    }
    return true;
  }

  if (FN == TEXT("BUILD_LIGHTING")) {
    FString Quality;
    Payload->TryGetStringField(TEXT("quality"), Quality);
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    // Guard against missing editor world; building lighting when there is
    // no active editor world can trigger engine assertions. If the world
    // is not available, report a structured error instead of proceeding.
    UWorld *CurrentWorld = nullptr;
    if (GEditor) {
      CurrentWorld = GEditor->GetEditorWorldContext().World();
    }
    if (!CurrentWorld) {
      SendAutomationResponse(
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
        } else {
          TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
          Err->SetBoolField(TEXT("success"), false);
          Err->SetStringField(TEXT("error"), TEXT("unknown_quality"));
          Err->SetStringField(TEXT("quality"), Quality);
          SendAutomationResponse(RequestingSocket, RequestId, false,
                                 TEXT("Unknown lighting quality"), Err,
                                 TEXT("UNKNOWN_QUALITY"));
          return true;
        }
      }
      if (AWorldSettings *WS = CurrentWorld->GetWorldSettings()) {
        if (WS->bForceNoPrecomputedLighting) {
          TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
          R->SetBoolField(TEXT("skipped"), true);
          R->SetStringField(TEXT("reason"),
                            TEXT("bForceNoPrecomputedLighting is true"));
          SendAutomationResponse(
              RequestingSocket, RequestId, true,
              TEXT("Lighting build skipped (precomputed lighting disabled)"), R,
              FString());
          return true;
        }
      }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      LES->BuildLightMaps(QualityEnum, /*bWithReflectionCaptures*/ false);
      TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
      R->SetBoolField(TEXT("requested"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Build lighting requested"), R, FString());
#else
      // UE 5.0 fallback - BuildLightMaps not available
      TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
      R->SetBoolField(TEXT("requested"), false);
      R->SetStringField(TEXT("error"), TEXT("BuildLightMaps not available in UE 5.0"));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Build lighting not available in UE 5.0"), R, TEXT("NOT_AVAILABLE"));
#endif
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("LevelEditorSubsystem not available"),
                             nullptr, TEXT("SUBSYSTEM_NOT_FOUND"));
    }
    return true;
  }

  if (FN == TEXT("SAVE_CURRENT_LEVEL")) {
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    bool bSaved = false;
#if __has_include("EditorLoadingAndSavingUtils.h")
    bSaved = UEditorLoadingAndSavingUtils::SaveCurrentLevel();
#elif __has_include("FileHelpers.h")
    bSaved = FEditorFileUtils::SaveCurrentLevel();
#endif

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), bSaved);
    SendAutomationResponse(RequestingSocket, RequestId, bSaved,
                           bSaved ? TEXT("Level saved")
                                  : TEXT("Failed to save level"),
                           Out, bSaved ? FString() : TEXT("SAVE_FAILED"));
    return true;
  }

  // RESOLVE_OBJECT: return basic object/asset discovery info
  if (FN == TEXT("RESOLVE_OBJECT")) {
    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);
    if (Path.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    bool bExists = false;
    FString ClassName;
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      bExists = true;
      if (UObject *Obj = UEditorAssetLibrary::LoadAsset(Path)) {
        if (UClass *Cls = Obj->GetClass()) {
          ClassName = Cls->GetPathName();
        }
      }
    } else if (UObject *Obj = FindObject<UObject>(nullptr, *Path)) {
      bExists = true;
      if (UClass *Cls = Obj->GetClass()) {
        ClassName = Cls->GetPathName();
      }
    }
    Out->SetBoolField(TEXT("exists"), bExists);
    Out->SetStringField(TEXT("path"), Path);
    Out->SetStringField(TEXT("class"), ClassName);
    Out->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           bExists ? TEXT("Object resolved")
                                   : TEXT("Object not found"),
                           Out, bExists ? FString() : TEXT("NOT_FOUND"));
    return true;
  }

  // LIST_ACTOR_COMPONENTS: provide a simple listing of components for a given
  // editor actor
  if (FN == TEXT("LIST_ACTOR_COMPONENTS")) {
    FString ActorPath;
    Payload->TryGetStringField(TEXT("actorPath"), ActorPath);
    if (ActorPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("actorPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("EditorActorSubsystem not available"),
                             nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
      return true;
    }
    AActor *Found = nullptr;
    for (AActor *A : ActorSS->GetAllLevelActors()) {
      if (!A)
        continue;
      if (A->GetActorLabel().Equals(ActorPath, ESearchCase::IgnoreCase) ||
          A->GetName().Equals(ActorPath, ESearchCase::IgnoreCase) ||
          A->GetPathName().Equals(ActorPath, ESearchCase::IgnoreCase)) {
        Found = A;
        break;
      }
    }
    if (!Found) {
      TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("error"), TEXT("Actor not found"));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Actor not found"), Err,
                             TEXT("ACTOR_NOT_FOUND"));
      return true;
    }
    TArray<UActorComponent *> Comps;
    Found->GetComponents(Comps);
    TArray<TSharedPtr<FJsonValue>> Arr;
    for (UActorComponent *C : Comps) {
      if (!C)
        continue;
      TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
      R->SetStringField(TEXT("name"), C->GetName());
      R->SetStringField(TEXT("class"), C->GetClass()
                                           ? C->GetClass()->GetPathName()
                                           : TEXT(""));
      R->SetStringField(TEXT("path"), C->GetPathName());
      Arr.Add(MakeShared<FJsonValueObject>(R));
    }
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetArrayField(TEXT("components"), Arr);
    Out->SetNumberField(TEXT("count"), Arr.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Components listed"), Out, FString());
    return true;
  }

  // GET_BLUEPRINT_CDO: best-effort CDO/class info for a Blueprint asset
  if (FN == TEXT("GET_BLUEPRINT_CDO")) {
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("blueprintPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint not found"), nullptr,
                             TEXT("NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    UObject *Obj = UEditorAssetLibrary::LoadAsset(BlueprintPath);
    if (!Obj) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint not found"), nullptr,
                             TEXT("NOT_FOUND"));
      return true;
    }

    if (UBlueprint *BP = Cast<UBlueprint>(Obj)) {
      if (BP->GeneratedClass) {
        UClass *Gen = BP->GeneratedClass;
        Out->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Out->SetStringField(TEXT("classPath"), Gen->GetPathName());
        Out->SetStringField(TEXT("className"), Gen->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Blueprint CDO info"), Out, FString());
        return true;
      }
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint/GeneratedClass not available"),
                             nullptr, TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    if (UClass *C = Cast<UClass>(Obj)) {
      Out->SetStringField(TEXT("classPath"), C->GetPathName());
      Out->SetStringField(TEXT("className"), C->GetName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Class info"), Out, FString());
      return true;
    }

    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Blueprint/GeneratedClass not available"),
                           nullptr, TEXT("INVALID_BLUEPRINT"));
    return true;
  }

  if (FN == TEXT("BLUEPRINT_ADD_COMPONENT")) {
    const TSharedPtr<FJsonObject> *Params = nullptr;
    TSharedPtr<FJsonObject> LocalParams = MakeShared<FJsonObject>();
    if (Payload->TryGetObjectField(TEXT("params"), Params) && Params &&
        (*Params).IsValid()) {
      LocalParams = *Params;
    } else if (Payload->HasField(TEXT("payloadBase64"))) {
      FString Enc;
      Payload->TryGetStringField(TEXT("payloadBase64"), Enc);
      if (!Enc.IsEmpty()) {
        TArray<uint8> DecodedBytes;
        if (FBase64::Decode(Enc, DecodedBytes) && DecodedBytes.Num() > 0) {
          DecodedBytes.Add(0);
          const ANSICHAR *Utf8 =
              reinterpret_cast<const ANSICHAR *>(DecodedBytes.GetData());
          FString Decoded = FString(UTF8_TO_TCHAR(Utf8));
          TSharedPtr<FJsonObject> Parsed = MakeShared<FJsonObject>();
          TSharedRef<TJsonReader<>> Reader =
              TJsonReaderFactory<>::Create(Decoded);
          if (FJsonSerializer::Deserialize(Reader, Parsed) &&
              Parsed.IsValid()) {
            LocalParams = Parsed;
          }
        }
      }
    }

    FString TargetBP;
    LocalParams->TryGetStringField(TEXT("blueprintPath"), TargetBP);
    if (TargetBP.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("blueprintPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TSharedPtr<FJsonObject> SCSPayload = MakeShared<FJsonObject>();
    SCSPayload->SetStringField(TEXT("blueprintPath"), TargetBP);

    TArray<TSharedPtr<FJsonValue>> Ops;
    TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
    Op->SetStringField(TEXT("type"), TEXT("add_component"));
    FString Name;
    LocalParams->TryGetStringField(TEXT("componentName"), Name);
    if (!Name.IsEmpty())
      Op->SetStringField(TEXT("componentName"), Name);
    FString Class;
    LocalParams->TryGetStringField(TEXT("componentClass"), Class);
    if (!Class.IsEmpty())
      Op->SetStringField(TEXT("componentClass"), Class);
    FString AttachTo;
    LocalParams->TryGetStringField(TEXT("attachTo"), AttachTo);
    if (!AttachTo.IsEmpty())
      Op->SetStringField(TEXT("attachTo"), AttachTo);
    Ops.Add(MakeShared<FJsonValueObject>(Op));
    SCSPayload->SetArrayField(TEXT("operations"), Ops);

    return HandleBlueprintAction(RequestId, TEXT("blueprint_modify_scs"),
                                 SCSPayload, RequestingSocket);
  }

  if (FN == TEXT("CREATE_ASSET")) {
    // Check if we have a nested "params" object, which is standard for
    // ExecuteEditorFunction
    const TSharedPtr<FJsonObject> *ParamsObj;
    const TSharedPtr<FJsonObject> SourceObj =
        (Payload->TryGetObjectField(TEXT("params"), ParamsObj)) ? *ParamsObj
                                                                : Payload;

    FString AssetName;
    SourceObj->TryGetStringField(TEXT("asset_name"), AssetName);
    FString PackagePath;
    SourceObj->TryGetStringField(TEXT("package_path"), PackagePath);
    FString AssetClass;
    SourceObj->TryGetStringField(TEXT("asset_class"), AssetClass);
    FString FactoryClass;
    SourceObj->TryGetStringField(TEXT("factory_class"), FactoryClass);

    if (AssetName.IsEmpty() || PackagePath.IsEmpty() ||
        FactoryClass.IsEmpty()) {
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("asset_name, package_path, and factory_class required"),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    IAssetTools &AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools")
            .Get();

    // Resolve factory
    UClass *FactoryUClass = ResolveClassByName(FactoryClass);
    if (!FactoryUClass) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      // Try finding by short name or full path
      FactoryUClass = UClass::TryFindTypeSlow<UClass>(FactoryClass);
#else
      // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
      FactoryUClass = ResolveClassByName(FactoryClass);
#endif
    }

    // Quick factory lookup by short name if full resolution failed
    if (!FactoryUClass) {
      for (TObjectIterator<UClass> It; It; ++It) {
        if (It->GetName().Equals(FactoryClass) ||
            It->GetName().Equals(FactoryClass + TEXT("Factory"))) {
          if (It->IsChildOf(UFactory::StaticClass())) {
            FactoryUClass = *It;
            break;
          }
        }
      }
    }

    if (!FactoryUClass) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Factory class '%s' not found"), *FactoryClass),
          nullptr, TEXT("FACTORY_NOT_FOUND"));
      return true;
    }

    UFactory *Factory =
        NewObject<UFactory>(GetTransientPackage(), FactoryUClass);
    if (!Factory) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to instantiate factory"), nullptr,
                             TEXT("FACTORY_CREATION_FAILED"));
      return true;
    }

    // Attempt creation
    UObject *NewAsset =
        AssetTools.CreateAsset(AssetName, PackagePath, nullptr, Factory);
    if (NewAsset) {
      // Use McpSafeAssetSave instead of modal PromptForCheckoutAndSave to avoid D3D12 crashes
      McpSafeAssetSave(NewAsset);

      TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
      Out->SetStringField(TEXT("name"), NewAsset->GetName());
      Out->SetStringField(TEXT("path"), NewAsset->GetPathName());
      Out->SetBoolField(TEXT("success"), true);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Asset created"), Out, FString());
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to create asset via AssetTools"),
                             nullptr, TEXT("ASSET_CREATION_FAILED"));
    }
    return true;
  }

  // PLAY_SOUND helpers
  if (FN == TEXT("PLAY_SOUND_AT_LOCATION") || FN == TEXT("PLAY_SOUND_2D")) {
    const TSharedPtr<FJsonObject> *Params = nullptr;
    if (!Payload->TryGetObjectField(TEXT("params"), Params) ||
        !(*Params).IsValid()) { /* allow top-level path fields */
    }
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("path"), SoundPath))
      Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
    if (SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath or path required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
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
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor world not available"), nullptr,
                             TEXT("EDITOR_WORLD_NOT_AVAILABLE"));
      return true;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(SoundPath)) {
      TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("error"), TEXT("Sound asset not found"));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Sound not found"), Err, TEXT("NOT_FOUND"));
      return true;
    }

    USoundBase *Snd =
        Cast<USoundBase>(UEditorAssetLibrary::LoadAsset(SoundPath));
    if (!Snd) {
      TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("error"), TEXT("Sound asset not found"));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Sound not found"), Err, TEXT("NOT_FOUND"));
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

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played"), Out, FString());
    return true;
  }

  // ADD_WIDGET_TO_VIEWPORT: implemented with proper widget creation and
  // viewport management
  if (FN == TEXT("ADD_WIDGET_TO_VIEWPORT")) {
    FString WidgetPath;
    Payload->TryGetStringField(TEXT("widget_path"), WidgetPath);
    if (WidgetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("widget_path required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    int32 zOrder = 0;
    Payload->TryGetNumberField(TEXT("z_order"), zOrder);
    int32 playerIndex = 0;
    Payload->TryGetNumberField(TEXT("player_index"), playerIndex);

    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available for widget creation"),
                             nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    // Load the widget blueprint class
    UClass *WidgetClass = LoadClass<UUserWidget>(nullptr, *WidgetPath);
    if (!WidgetClass) {
      TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("error"), TEXT("Widget class not found"));
      Err->SetStringField(TEXT("widget_path"), WidgetPath);
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Widget class not found"), Err,
                             TEXT("WIDGET_NOT_FOUND"));
      return true;
    }

    // Get the current world and player controller
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world available"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    APlayerController *PlayerController =
        UGameplayStatics::GetPlayerController(World, playerIndex);
    if (!PlayerController) {
      // Try to get the first available player controller if the specified one
      // doesn't exist
      PlayerController = UGameplayStatics::GetPlayerController(World, 0);
      if (!PlayerController) {
        TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
        Err->SetStringField(TEXT("error"),
                            TEXT("Player controller not available"));
        Err->SetNumberField(TEXT("player_index"), playerIndex);
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Player controller not available"), Err,
                               TEXT("NO_PLAYER_CONTROLLER"));
        return true;
      }
    }

    // Create and add the widget to viewport
    UUserWidget *Widget =
        CreateWidget<UUserWidget>(PlayerController, WidgetClass);
    if (!Widget) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to create widget instance"), nullptr,
                             TEXT("WIDGET_CREATION_FAILED"));
      return true;
    }

    Widget->AddToViewport(zOrder);

    // Verify widget is in viewport
    const bool bIsInViewport = Widget->IsInViewport();

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), bIsInViewport);
    Out->SetStringField(TEXT("widget_path"), WidgetPath);
    Out->SetStringField(TEXT("widget_class"), WidgetClass->GetPathName());
    Out->SetNumberField(TEXT("z_order"), zOrder);
    Out->SetNumberField(TEXT("player_index"),
                        PlayerController ? playerIndex : 0);

    if (!bIsInViewport) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to add widget to viewport"), Out,
                             TEXT("ADD_TO_VIEWPORT_FAILED"));
      return true;
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Widget added to viewport"), Out, FString());
    return true;
  }

  if (FN == TEXT("GENERATE_MEMORY_REPORT")) {
    FString OutputPath;
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
    bool bDetailed = false;
    Payload->TryGetBoolField(TEXT("detailed"), bDetailed);

    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    FString MemReportCmd =
        bDetailed ? TEXT("memreport -full") : TEXT("memreport");
    GEditor->Exec(nullptr, *MemReportCmd);

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), true);
    // Note: OutputPath is not fully supported by the native memreport command
    // (it auto-generates filenames), but we acknowledge the request.
    Out->SetStringField(
        TEXT("message"),
        TEXT("Memory report generated (check Saved/Profiling/MemReports)"));

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Memory report generated"), Out, FString());
    return true;
  }

  // CALL_SUBSYSTEM: generic reflection-based subsystem call
  if (FN == TEXT("CALL_SUBSYSTEM")) {
    FString SubsystemName;
    Payload->TryGetStringField(TEXT("subsystem"), SubsystemName);
    FString TargetFuncName;
    Payload->TryGetStringField(TEXT("function"), TargetFuncName);
    const TSharedPtr<FJsonObject> *Args = nullptr;
    Payload->TryGetObjectField(TEXT("args"), Args);

    if (SubsystemName.IsEmpty() || TargetFuncName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("subsystem and function required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UObject *TargetSubsystem = nullptr;

    // 1. Try Editor Subsystems
    if (!TargetSubsystem) {
      // We can't iterate types easily without object iterator or known list.
      // Try resolving class first.
      UClass *SubsystemClass = ResolveClassByName(SubsystemName);
      if (SubsystemClass) {
        if (SubsystemClass->IsChildOf(UEditorSubsystem::StaticClass())) {
          TargetSubsystem = GEditor->GetEditorSubsystemBase(SubsystemClass);
        } else if (SubsystemClass->IsChildOf(UEngineSubsystem::StaticClass())) {
          TargetSubsystem = GEngine->GetEngineSubsystemBase(SubsystemClass);
        }
      }
    }

    // 2. Fallback: string-based lookup if class resolve failed or returns null
    // (though GetEditorSubsystemBase handles null class) Iterate known
    // subsystem collections if we really need to, but resolving class is best.

    if (!TargetSubsystem) {
      TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(
          TEXT("error"),
          FString::Printf(TEXT("Subsystem '%s' not found or not initialized"),
                          *SubsystemName));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Subsystem not found"), Err,
                             TEXT("SUBSYSTEM_NOT_FOUND"));
      return true;
    }

    // Build command string
    FString CmdString = TargetFuncName;
    if (Args && (*Args).IsValid()) {
      for (const auto &Pair : (*Args)->Values) {
        CmdString += TEXT(" ");
        CmdString += Pair.Key;
        CmdString += TEXT("=");

        switch (Pair.Value->Type) {
        case EJson::String:
          CmdString += FString::Printf(TEXT("\"%s\""), *Pair.Value->AsString());
          break;
        case EJson::Number:
          CmdString += FString::Printf(TEXT("%f"), Pair.Value->AsNumber());
          break;
        case EJson::Boolean:
          CmdString += Pair.Value->AsBool() ? TEXT("True") : TEXT("False");
          break;
        default:
          // Object/Array support in command string is limited, pass
          // stringified? For now, skip complex types or rely on simple string
          // conversion
          break;
        }
      }
    }

    FOutputDeviceNull Ar;
    bool bResult = TargetSubsystem->CallFunctionByNameWithArguments(
        *CmdString, Ar, nullptr, true);

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), bResult);
    Out->SetStringField(TEXT("subsystem"), SubsystemName);
    Out->SetStringField(TEXT("function"), TargetFuncName);

    SendAutomationResponse(RequestingSocket, RequestId, bResult,
                           bResult ? TEXT("Function called")
                                   : TEXT("Function call failed"),
                           Out, bResult ? FString() : TEXT("CALL_FAILED"));
    return true;
  }

  if (FN == TEXT("CONFIGURE_TEXTURE_STREAMING")) {
    bool bEnabled = true;
    if (Payload->HasField(TEXT("enabled")))
      Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    double PoolSize = -1;
    if (Payload->HasField(TEXT("poolSize")))
      Payload->TryGetNumberField(TEXT("poolSize"), PoolSize);

    bool bBoost = false;
    if (Payload->HasField(TEXT("boostPlayerLocation")))
      Payload->TryGetBoolField(TEXT("boostPlayerLocation"), bBoost);

    if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.TextureStreaming"))) {
      CVar->Set(bEnabled ? 1 : 0, ECVF_SetByCode);
    }

    if (PoolSize >= 0) {
      if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
              TEXT("r.Streaming.PoolSize"))) {
        CVar->Set((int32)PoolSize, ECVF_SetByCode);
      }
    }

    // Boost logic would go here (e.g. forcing stream in for player view),
    // but basic CVar setting is the core requirement.

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), true);
    Out->SetBoolField(TEXT("enabled"), bEnabled);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Texture streaming configured"), Out,
                           FString());
    return true;
  }

  return false;
}

#endif
