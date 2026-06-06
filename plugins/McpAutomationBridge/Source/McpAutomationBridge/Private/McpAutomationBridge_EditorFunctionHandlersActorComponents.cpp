#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_EditorFunctionHandlersShared.h"

#if WITH_EDITOR
#include "Editor.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
  #include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
  #include "EditorActorSubsystem.h"
#endif

namespace McpEditorFunctionHandlers {

static AActor *FindEditorActor(UEditorActorSubsystem &ActorSS, const FString &ActorPath) {
  for (AActor *A : ActorSS.GetAllLevelActors()) {
    if (!A)
      continue;
    if (A->GetActorLabel().Equals(ActorPath, ESearchCase::IgnoreCase) ||
        A->GetName().Equals(ActorPath, ESearchCase::IgnoreCase) ||
        A->GetPathName().Equals(ActorPath, ESearchCase::IgnoreCase)) {
      return A;
    }
  }
  return nullptr;
}

bool HandleAssetQueryFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FN, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleActorComponentFunction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FN, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (FN != TEXT("LIST_ACTOR_COMPONENTS"))
    return false;

  FString ActorPath;
  Payload->TryGetStringField(TEXT("actorPath"), ActorPath);
  if (ActorPath.IsEmpty()) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("actorPath required"), TEXT("INVALID_ARGUMENT"));
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
  AActor *Found = FindEditorActor(*ActorSS, ActorPath);
  if (!Found) {
    TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
    Err->SetStringField(TEXT("error"), TEXT("Actor not found"));
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  TEXT("Actor not found"), Err, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  TArray<UActorComponent *> Comps;
  Found->GetComponents(Comps);
  TArray<TSharedPtr<FJsonValue>> Arr;
  for (UActorComponent *C : Comps) {
    if (!C)
      continue;
    TSharedPtr<FJsonObject> R = McpHandlerUtils::CreateResultObject();
    R->SetStringField(TEXT("name"), C->GetName());
    R->SetStringField(TEXT("class"), C->GetClass() ? C->GetClass()->GetPathName() : TEXT(""));
    R->SetStringField(TEXT("path"), C->GetPathName());
    Arr.Add(MakeShared<FJsonValueObject>(R));
  }
  TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
  Out->SetArrayField(TEXT("components"), Arr);
  Out->SetNumberField(TEXT("count"), Arr.Num());
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                TEXT("Components listed"), Out, FString());
  return true;
}

} // namespace McpEditorFunctionHandlers
#endif
