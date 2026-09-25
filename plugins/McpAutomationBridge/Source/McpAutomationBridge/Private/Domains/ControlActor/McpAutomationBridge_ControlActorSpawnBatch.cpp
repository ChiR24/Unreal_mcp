#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"

// spawn_batch: place many actors in one request. Each item runs through the
// ordinary spawn handler under a synthetic request id whose reply is captured
// (FMcpResponseCaptureRegistry), so a batch item behaves exactly like a single
// spawn. Laying out a level was otherwise one round trip per block.

#if WITH_EDITOR
namespace {
constexpr int32 MaxSpawnBatchItems = 500;

// The item's own fields over the batch's shared `defaults`.
TSharedPtr<FJsonObject> MergeSpawnItem(const TSharedPtr<FJsonObject> &Defaults,
                                       const TSharedPtr<FJsonObject> &Item) {
  TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
  if (Defaults.IsValid()) {
    Out->Values = Defaults->Values;
  }
  for (const TPair<FString, TSharedPtr<FJsonValue>> &Pair : Item->Values) {
    Out->SetField(Pair.Key, Pair.Value);
  }
  return Out;
}

// Outliner folder and tags need no handler of their own.
void ApplySpawnOrganisation(AActor *Actor, const TSharedPtr<FJsonObject> &Item) {
  FString Folder;
  if (Item->TryGetStringField(TEXT("folder"), Folder) && !Folder.IsEmpty()) {
    Actor->SetFolderPath(FName(*Folder));
  }
  const TArray<TSharedPtr<FJsonValue>> *Tags = nullptr;
  if (Item->TryGetArrayField(TEXT("tags"), Tags)) {
    for (const TSharedPtr<FJsonValue> &Tag : *Tags) {
      const FString TagName = Tag.IsValid() ? Tag->AsString() : FString();
      if (!TagName.IsEmpty()) {
        Actor->Tags.AddUnique(FName(*TagName));
      }
    }
  }
}
} // namespace
#endif

bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawnBatch(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  const TArray<TSharedPtr<FJsonValue>> *Items = nullptr;
  if (!Payload->TryGetArrayField(TEXT("actors"), Items) || Items->Num() == 0 ||
      Items->Num() > MaxSpawnBatchItems) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
        FString::Printf(TEXT("spawn_batch needs an `actors` array of 1-%d items, each "
                             "a spawn payload ({classPath|blueprintPath|meshPath, "
                             "actorName, location, rotation, scale, materialPath, "
                             "folder, tags})."),
                        MaxSpawnBatchItems),
        nullptr);
    return true;
  }
  const TSharedPtr<FJsonObject> *DefaultsPtr = nullptr;
  Payload->TryGetObjectField(TEXT("defaults"), DefaultsPtr);
  const TSharedPtr<FJsonObject> Defaults = DefaultsPtr ? *DefaultsPtr : nullptr;

  FMcpResponseCaptureRegistry &Capture = FMcpResponseCaptureRegistry::Get();
  TArray<TSharedPtr<FJsonValue>> Results;
  TArray<FString> Failures;
  int32 SpawnedCount = 0;
  for (int32 Index = 0; Index < Items->Num(); ++Index) {
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetNumberField(TEXT("index"), Index);
    Results.Add(MakeShared<FJsonValueObject>(Entry));

    const TSharedPtr<FJsonObject> *ItemObj = nullptr;
    if (!(*Items)[Index].IsValid() || !(*Items)[Index]->TryGetObject(ItemObj)) {
      Entry->SetBoolField(TEXT("success"), false);
      Entry->SetStringField(TEXT("error"), TEXT("item is not an object"));
      Failures.Add(FString::Printf(TEXT("#%d: item is not an object"), Index));
      continue;
    }
    const TSharedPtr<FJsonObject> Item = MergeSpawnItem(Defaults, *ItemObj);

    const FString SpawnId = FString::Printf(TEXT("%s#spawn%d"), *RequestId, Index);
    Capture.Begin(SpawnId);
    HandleControlActorSpawn(SpawnId, Item, Socket);
    const FMcpCapturedResponse Reply = Capture.End(SpawnId);
    FString ActorPath;
    if (!Reply.bSuccess || !Reply.Result.IsValid() ||
        !Reply.Result->TryGetStringField(TEXT("actorPath"), ActorPath)) {
      const FString Error = Reply.Message.IsEmpty() ? TEXT("spawn failed") : Reply.Message;
      Entry->SetBoolField(TEXT("success"), false);
      Entry->SetStringField(TEXT("error"), Error);
      Entry->SetStringField(TEXT("errorCode"), Reply.ErrorCode);
      Failures.Add(FString::Printf(TEXT("#%d: %s"), Index, *Error));
      continue;
    }
    ++SpawnedCount;
    Entry->SetBoolField(TEXT("success"), true);
    Entry->SetStringField(TEXT("path"), ActorPath);
    AActor *Actor = FindActorByName(ActorPath, true);
    if (Actor) {
      Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
      ApplySpawnOrganisation(Actor, Item);
    }

    // Per-instance Blueprint variables (e.g. a block's Kind), same handler as
    // set_blueprint_variables. An empty object sets nothing and is not a
    // failure: generated layouts emit `variables: {}` for plain items, and the
    // whole batch used to come back SPAWN_BATCH_INCOMPLETE with every actor placed.
    const TSharedPtr<FJsonObject> *VariablesObj = nullptr;
    if (Item->TryGetObjectField(TEXT("variables"), VariablesObj) && VariablesObj != nullptr &&
        (*VariablesObj)->Values.Num() > 0) {
      TSharedPtr<FJsonObject> VariablesPayload = MakeShared<FJsonObject>();
      VariablesPayload->SetStringField(TEXT("actorName"), ActorPath);
      VariablesPayload->SetField(TEXT("variables"), Item->TryGetField(TEXT("variables")));
      const FString VariablesId = SpawnId + TEXT("#variables");
      Capture.Begin(VariablesId);
      HandleControlActorSetBlueprintVariables(VariablesId, VariablesPayload, Socket);
      const FMcpCapturedResponse VariablesReply = Capture.End(VariablesId);
      // Reply envelope: { data: { updated: [...] }, warnings: [...] }.
      const TSharedPtr<FJsonObject> *VariablesData = nullptr;
      const TArray<TSharedPtr<FJsonValue>> *Updated = nullptr;
      const TArray<TSharedPtr<FJsonValue>> *Warnings = nullptr;
      if (VariablesReply.Result.IsValid()) {
        if (VariablesReply.Result->TryGetObjectField(TEXT("data"), VariablesData)) {
          (*VariablesData)->TryGetArrayField(TEXT("updated"), Updated);
        }
        VariablesReply.Result->TryGetArrayField(TEXT("warnings"), Warnings);
      }
      if (Updated) {
        Entry->SetArrayField(TEXT("variablesSet"), *Updated);
      }
      const int32 Wanted = (*VariablesObj)->Values.Num();
      if (!VariablesReply.bSuccess || !Updated || Updated->Num() != Wanted) {
        FString Why = VariablesReply.Message;
        if (Warnings) {
          for (const TSharedPtr<FJsonValue> &Warning : *Warnings) {
            Why += TEXT(" ") + Warning->AsString();
          }
        }
        Entry->SetStringField(TEXT("variablesError"), Why);
        Failures.Add(FString::Printf(TEXT("#%d variables: set %d of %d (%s)"), Index,
                                     Updated ? Updated->Num() : 0, Wanted, *Why));
      }
    }

    // Same handler as set_material, aimed at the exact actor just spawned.
    FString MaterialPath;
    if (Item->TryGetStringField(TEXT("materialPath"), MaterialPath) && !MaterialPath.IsEmpty()) {
      TSharedPtr<FJsonObject> MaterialPayload = MakeShared<FJsonObject>();
      MaterialPayload->SetStringField(TEXT("actorName"), ActorPath);
      MaterialPayload->SetStringField(TEXT("materialPath"), MaterialPath);
      for (const TCHAR *Key : {TEXT("componentName"), TEXT("materialSlot"), TEXT("allComponents")}) {
        if (Item->HasField(Key)) {
          MaterialPayload->SetField(Key, Item->TryGetField(Key));
        }
      }
      const FString MaterialId = SpawnId + TEXT("#material");
      Capture.Begin(MaterialId);
      HandleControlActorSetMaterial(MaterialId, MaterialPayload, Socket);
      const FMcpCapturedResponse MaterialReply = Capture.End(MaterialId);
      Entry->SetBoolField(TEXT("materialApplied"), MaterialReply.bSuccess);
      if (!MaterialReply.bSuccess) {
        Entry->SetStringField(TEXT("materialError"), MaterialReply.Message);
        Failures.Add(FString::Printf(TEXT("#%d material: %s"), Index, *MaterialReply.Message));
      }
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  // report: "failures" keeps only the items that went wrong: a 130-actor
  // layout echoed every path and name back, ~15K characters of "success".
  FString Report;
  if (Payload->TryGetStringField(TEXT("report"), Report) && Report == TEXT("failures")) {
    Results.RemoveAll([](const TSharedPtr<FJsonValue> &Value) {
      const TSharedPtr<FJsonObject> Entry = Value->AsObject();
      return Entry->GetBoolField(TEXT("success")) && !Entry->HasField(TEXT("materialError")) &&
             !Entry->HasField(TEXT("variablesError"));
    });
    Data->SetStringField(TEXT("report"), Report);
  }
  Data->SetArrayField(TEXT("results"), Results);
  Data->SetNumberField(TEXT("spawned"), SpawnedCount);
  Data->SetNumberField(TEXT("failed"), Failures.Num());
  if (Failures.Num() > 0) {
    // Actors that did spawn stay in the level; the results say which.
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Spawned %d of %d actors; %d problem(s): %s"), SpawnedCount,
                        Items->Num(), Failures.Num(), *FString::Join(Failures, TEXT("; "))),
        Data, TEXT("SPAWN_BATCH_INCOMPLETE"));
    return true;
  }
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Spawned %d actors"), SpawnedCount), Data);
  return true;
#else
  return false;
#endif
}
