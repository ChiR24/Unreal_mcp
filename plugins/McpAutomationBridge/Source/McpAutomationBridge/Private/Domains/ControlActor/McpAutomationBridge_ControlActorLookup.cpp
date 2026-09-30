#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

AActor *UMcpAutomationBridgeSubsystem::FindActorByName(const FString &Target, bool bExactMatchOnly) {
  if (Target.IsEmpty() || !GEditor)
    return nullptr;

  // Priority: PIE World if active
  if (GEditor->PlayWorld) {
    if (AActor *PieActor = FindActorByNameInWorldForMcp(
            GEditor->PlayWorld.Get(), Target, true)) {
      return PieActor;
    }
    // Not in the running game. The editor-level fallback below goes through
    // UEditorActorSubsystem, which refuses every call during PIE and logs it,
    // and that log turned this miss into "stop play, then retry" although the
    // PIE world had been searched (the actor was in another level PIE had
    // loaded). The reply's worldName names the PIE world that was searched.
    // PlayerPawn, PlayerController, GameMode, HUD... reach the running game's
    // actors by role, so a test need not know that the pawn spawned as
    // BP_Hero_C_0.
    return Cast<AActor>(McpHandlerUtils::ResolveRuntimeRole(Target));
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS)
    return nullptr;

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  AActor *ExactMatch = nullptr;
  TArray<AActor *> FuzzyMatches;

  for (AActor *A : AllActors) {
    if (!A)
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
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
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

bool UMcpAutomationBridgeSubsystem::HandleControlActorGet(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

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
