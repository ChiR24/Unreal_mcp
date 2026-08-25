// find_by_class lives in its own file so McpAutomationBridge_ControlActorQuery.cpp
// stays under the 250-pure-line structure ceiling; the handler is otherwise unchanged.

#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ClassName;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("class"), ClassName);
  }
  if (ClassName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("classPath"), ClassName);
  }

  if (ClassName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("className, class, or classPath is required"), nullptr);
    return true;
  }

  // Security: Validate class name format - reject path traversal attempts
  // Valid formats: "/Script/Module.ClassName", "/Game/Path/ClassName.ClassName", "ClassName"
  // Invalid: Contains "..", "\" (Windows paths), or other traversal patterns
  if (ClassName.Contains(TEXT("..")) || ClassName.Contains(TEXT("\\"))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid class name format: '%s'. Path traversal characters are not allowed."), *ClassName), nullptr);
    return true;
  }

  // Additional security: Reject absolute filesystem paths
  if (ClassName.StartsWith(TEXT("/")) && !ClassName.StartsWith(TEXT("/Script/")) &&
      !ClassName.StartsWith(TEXT("/Game/")) && !ClassName.StartsWith(TEXT("/Engine/"))) {
    // Could be a path traversal attempt disguised as a valid path
    if (ClassName.Contains(TEXT("/etc/")) || ClassName.Contains(TEXT("/usr/")) ||
        ClassName.Contains(TEXT("/var/")) || ClassName.Contains(TEXT("/home/")) ||
        ClassName.Contains(TEXT("/root/")) || ClassName.Contains(TEXT("/tmp/")) ||
        ClassName.Contains(TEXT("C:\\")) || ClassName.Contains(TEXT("D:\\"))) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid class name format: '%s'. Filesystem paths are not allowed."), *ClassName), nullptr);
      return true;
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> ActorsArray;

  // Prefer the PIE world while a play session is active, matching spawn,
  // spawn_blueprint, list and the lookup helpers. Reading the editor world here
  // meant find_by_class returned EDITOR actors during PIE while control_actor.list
  // returned UEDPIE_0 ones — two queries disagreeing about which world they
  // describe, with nothing on either response to say which. Any mutation driven
  // off these paths hit the editor originals instead of the live instances.
  UWorld* QueryWorld = GEditor->PlayWorld
                           ? GEditor->PlayWorld.Get()
                           : GEditor->GetEditorWorldContext().World();
  if (UWorld* World = QueryWorld) {
    UClass* ClassToFind = nullptr;

    // CRITICAL FIX: Use ResolveClassByName for proper engine class resolution
    // This handles: full paths, short names like "StaticMeshActor", and loads classes if needed
    // Without this, FindObject only finds already-loaded classes, missing engine classes like
    // AStaticMeshActor, APawn, etc. that haven't been accessed yet
    ClassToFind = ResolveClassByName(ClassName);

    if (ClassToFind) {
      for (TActorIterator<AActor> It(World, ClassToFind); It; ++It) {
        if (AActor* Actor = *It) {
          TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
          ActorObj->SetStringField(TEXT("name"), Actor->GetActorLabel());
          ActorObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
        }
      }
    } else {
      // Class not found - return empty result (this is valid for searches)
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleControlActorFindByClass: Class '%s' not found"), *ClassName);
    }
  }

  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              FString::Printf(TEXT("Found %d actors"), ActorsArray.Num()), Data);
  return true;
#else
  return false;
#endif
}
