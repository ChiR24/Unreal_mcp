#include "Domains/ControlActor/McpActorSupport.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

AActor *UEngineWireSubsystem::FindActorByName(const FString &Target, bool bExactMatchOnly) {
  if (Target.IsEmpty() || !GEditor)
    return nullptr;

  // The running game first: its exact match, then the role names (PlayerPawn, PlayerController, GameMode, HUD...), so
  // a test need not know that the pawn spawned as BP_Hero_C_0. The editor scan below goes through UEditorActorSubsystem,
  // which refuses every call during PIE and logs it, and that log turned a miss into "stop play, then retry" although
  // the PIE world had been searched.
  if (GEditor->PlayWorld) {
    return McpHandlerUtils::FindActorByName(Target);
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS)
    return nullptr;

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  AActor *ExactMatch = nullptr;
  TArray<AActor *> FuzzyMatches;

  for (AActor *A : AllActors) {
    // A label names an actor in any level; a path-restricted caller finds only those in a level inside its prefixes.
    if (!A || !McpCapabilityPrincipal::ActiveRequestPermitsObject(A))
      continue;
    if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
        A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
        A->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) {
      ExactMatch = A;
      break;
    }
    // Collect fuzzy matches ONLY if exact matching is not required
    // CRITICAL FIX: Fuzzy matching can cause delete operations to delete wrong actors
    // (e.g., "TestActor_Copy" matches when searching for "TestActor")
    if (!bExactMatchOnly && A->GetActorLabel().Contains(Target, ESearchCase::IgnoreCase)) {
      FuzzyMatches.Add(A);
    }
  }

  if (ExactMatch) {
    return ExactMatch;
  }

  if (!bExactMatchOnly) {
    if (FuzzyMatches.Num() == 1) {
      return FuzzyMatches[0];
    } else if (FuzzyMatches.Num() > 1) {
      UE_LOG(LogEngineWireSubsystem, Warning,
             TEXT("FindActorByName: Ambiguous match for '%s'. Found %d matches."),
             *Target, FuzzyMatches.Num());
    }
  }

  // Fallback: try to load as asset if it looks like a path
  if (Target.StartsWith(TEXT("/"))) {
    const FString SafeTargetPath = SanitizeProjectRelativePath(Target);
    if (!SafeTargetPath.IsEmpty()) {
      if (UObject *Obj = McpLoadAsset(SafeTargetPath)) {
        return Cast<AActor>(Obj);
      }
    }
  }
  return nullptr;
}

// The payload's actorName as an actor; nullptr after the reply (INVALID_ARGUMENT when it is missing,
// ACTOR_NOT_FOUND when nothing goes by it), so a handler answers `if (!Found) return true;`.
AActor *UEngineWireSubsystem::FindActorOrReply(const TSharedPtr<FJsonObject> &Payload, const FString &RequestId,
                                               const TSharedPtr<FMcpBridgeWebSocket> &Socket) {
  FString TargetName;
  if (!McpReadRequiredField(this, Socket, RequestId, Payload, TEXT("actorName"), TargetName))
    return nullptr;
  AActor *Found = FindActorByName(TargetName);
  if (!Found)
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"), nullptr);
  return Found;
}

bool UEngineWireSubsystem::HandleControlActorGet(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  AActor *Found = FindActorOrReply(Payload, RequestId, Socket);
  if (!Found)
    return true;

  const FTransform Current = Found->GetActorTransform();
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("name"), Found->GetName());
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("path"), Found->GetPathName());
  Data->SetStringField(TEXT("class"), Found->GetClass()
                                          ? Found->GetClass()->GetPathName()
                                          : TEXT(""));

  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags) {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  Data->SetArrayField(TEXT("tags"), TagsArray);

  Data->SetArrayField(TEXT("location"), McpHandlerUtils::VectorToJsonArray(Current.GetLocation()));
  Data->SetArrayField(TEXT("scale"), McpHandlerUtils::VectorToJsonArray(Current.GetScale3D()));

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor retrieved"),
                              Data);
  return true;
}
