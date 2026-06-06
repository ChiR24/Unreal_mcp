#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_EditorFunctionHandlersShared.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
  #include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
  #include "EditorActorSubsystem.h"
#endif

namespace McpEditorFunctionHandlers {

bool HandleActorFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FN, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (FN == TEXT("GET_ALL_ACTORS") || FN == TEXT("GET_ALL_ACTORS_SIMPLE")) {
    if (!GEditor) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Editor not available"), nullptr,
                                    TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("EditorActorSubsystem not available"),
                                    nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
      return true;
    }
    TArray<TSharedPtr<FJsonValue>> Arr;
    for (AActor *A : ActorSS->GetAllLevelActors()) {
      if (!A)
        continue;
      TSharedPtr<FJsonObject> E = McpHandlerUtils::CreateResultObject();
      E->SetStringField(TEXT("name"), A->GetName());
      E->SetStringField(TEXT("label"), A->GetActorLabel());
      E->SetStringField(TEXT("path"), A->GetPathName());
      E->SetStringField(TEXT("class"), A->GetClass() ? A->GetClass()->GetPathName() : TEXT(""));
      Arr.Add(MakeShared<FJsonValueObject>(E));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("actors"), Arr);
    Result->SetNumberField(TEXT("count"), Arr.Num());
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
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
    if (Payload->TryGetObjectField(TEXT("params"), LocObj) && LocObj && (*LocObj).IsValid()) {
      ReadVectorField(*LocObj, TEXT("location"), Loc, Loc);
      ReadRotatorField(*LocObj, TEXT("rotation"), Rot, Rot);
    } else if (const TSharedPtr<FJsonValue> LocVal = Payload->TryGetField(TEXT("location"))) {
      if (LocVal->Type == EJson::Array) {
        const TArray<TSharedPtr<FJsonValue>> &A = LocVal->AsArray();
        if (A.Num() >= 3)
          Loc = FVector((float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber());
      } else if (LocVal->Type == EJson::Object) {
        const TSharedPtr<FJsonObject> LocObject = LocVal->AsObject();
        if (LocObject.IsValid())
          ReadVectorField(LocObject, TEXT("location"), Loc, Loc);
      }
    }

    if (!GEditor) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Editor not available"), nullptr,
                                    TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    UClass *Resolved = ClassPath.IsEmpty() ? nullptr : ResolveClassByName(ClassPath);
    if (!Resolved) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(TEXT("error"), TEXT("Class not found"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Class not found"), Err, TEXT("CLASS_NOT_FOUND"));
      return true;
    }
    AActor *Spawned = SpawnActorInActiveWorld<AActor>(Resolved, Loc, Rot);
    if (!Spawned) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(TEXT("error"), TEXT("Spawn failed"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Spawn failed"), Err, TEXT("SPAWN_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
    Out->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
    Out->SetBoolField(TEXT("success"), true);
    McpHandlerUtils::AddVerification(Out, Spawned);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Actor spawned"), Out, FString());
    return true;
  }

  if (FN == TEXT("DELETE_ACTOR") || FN == TEXT("DESTROY_ACTOR")) {
    FString Target;
    Payload->TryGetStringField(TEXT("actor_name"), Target);
    if (Target.IsEmpty())
      Payload->TryGetStringField(TEXT("actorName"), Target);
    if (Target.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                                 TEXT("actor_name required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!GEditor) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Editor not available"), nullptr,
                                    TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
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
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(TEXT("error"), TEXT("Actor not found"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Actor not found"), Err, TEXT("ACTOR_NOT_FOUND"));
      return true;
    }
    const FString DeletedActorLabel = Found->GetActorLabel();
    const bool bDeleted = ActorSS->DestroyActor(Found);
    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetBoolField(TEXT("success"), bDeleted);
    if (bDeleted) {
      Out->SetStringField(TEXT("deleted"), DeletedActorLabel);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                    TEXT("Actor deleted"), Out, FString());
    } else {
      Out->SetStringField(TEXT("error"), TEXT("Delete failed"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
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
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                                 TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!GEditor || !GEditor->IsPlaySessionInProgress()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Possess only available during PIE session"),
                                    nullptr, TEXT("NOT_IN_PIE"));
      return true;
    }
    UWorld *PlayWorld = GEditor->PlayWorld;
    if (!PlayWorld) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("PIE World not found"), nullptr, TEXT("WORLD_NOT_FOUND"));
      return true;
    }
    APawn *FoundPawn = nullptr;
    for (TActorIterator<APawn> It(PlayWorld); It; ++It) {
      APawn *P = *It;
      if (P && (P->GetActorLabel().Equals(TargetName, ESearchCase::IgnoreCase) ||
                P->GetName().Equals(TargetName, ESearchCase::IgnoreCase))) {
        FoundPawn = P;
        break;
      }
    }
    if (!FoundPawn) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Pawn not found in PIE world"), nullptr,
                                    TEXT("PAWN_NOT_FOUND"));
      return true;
    }
    APlayerController *PC = PlayWorld->GetFirstPlayerController();
    if (!PC) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("No PlayerController found in PIE"), nullptr,
                                    TEXT("PC_NOT_FOUND"));
      return true;
    }
    PC->Possess(FoundPawn);
    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetBoolField(TEXT("success"), true);
    Out->SetStringField(TEXT("possessed"), FoundPawn->GetActorLabel());
    McpHandlerUtils::AddVerification(Out, FoundPawn);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Possessed pawn"), Out, FString());
    return true;
  }

  return false;
}

} // namespace McpEditorFunctionHandlers
#endif
