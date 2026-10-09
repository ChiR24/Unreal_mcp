#include "Domains/Property/McpAutomationBridge_PropertyHandlersTarget.h"

#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpPropertyTarget
{
namespace
{
FMcpCapturedResponse RunCaptured(const FString& ItemId, const TSharedPtr<FJsonObject>& One, FSetBatchWrite RunOne)
{
  FMcpResponseCaptureRegistry::Get().Begin(ItemId);
  RunOne(ItemId, One);
  return FMcpResponseCaptureRegistry::Get().End(ItemId);
}

void Finish(FSetBatchReply& Out, const TSharedPtr<FJsonObject>& Data, const TCHAR* RowsField,
            const TArray<TSharedPtr<FJsonValue>>& Rows, const TArray<FString>& Failed, const FString& Done)
{
  Data->SetArrayField(RowsField, Rows);
  Data->SetNumberField(TEXT("applied"), Rows.Num() - Failed.Num());
  Out.Data = Data;
  Out.bSuccess = Failed.Num() == 0;
  Out.Message = Out.bSuccess ? Done : FString::Join(Failed, TEXT("; "));
  Out.ErrorCode = Out.bSuccess ? FString() : FString(TEXT("PROPERTY_BATCH_INCOMPLETE"));
}

// properties: several values on one target in one call ({BoxExtent, CollisionProfileName}).
void RunProperties(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, const FJsonObject& Properties,
                   FSetBatchWrite RunOne, FSetBatchReply& Out)
{
  TArray<TSharedPtr<FJsonValue>> Rows;
  TArray<FString> Failed;
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  double InstancesUpdated = -1.0;
  for (const TPair<FString, TSharedPtr<FJsonValue>> &Pair : Properties.Values) {
    TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
    One->Values = Payload->Values;
    for (const TCHAR *Field : {TEXT("properties"), TEXT("propertyPath"), TEXT("watch")}) One->RemoveField(Field);
    One->SetStringField(TEXT("propertyName"), Pair.Key);
    One->SetField(TEXT("value"), Pair.Value);
    const FMcpCapturedResponse Reply = RunCaptured(FString::Printf(TEXT("%s#prop%d"), *RequestId, Rows.Num()), One, RunOne);
    TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
    Row->SetStringField(TEXT("propertyName"), Pair.Key);
    Row->SetBoolField(TEXT("applied"), Reply.bSuccess);
    if (!Reply.bSuccess) Failed.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Reply.Message));
    // assetPath names the Blueprint or material a class-default or expression write saved: without it a batch on
    // a Blueprint's class defaults recompiled and saved the Blueprint while its receipt listed no change. Every row
    // saves the same package, so the last row's saved/saveSkippedReason is what reached disk.
    for (const TCHAR *Field : {TEXT("value"), TEXT("actorName"), TEXT("actorPath"), TEXT("packagePath"), TEXT("blueprintCompiled"),
                               TEXT("assetPath"), TEXT("materialRebuilt"), TEXT("saved"), TEXT("saveSkippedReason")}) {
      const TSharedPtr<FJsonValue> Value = Reply.Result.IsValid() ? Reply.Result->TryGetField(Field) : nullptr;
      if (Value.IsValid()) (FCString::Strcmp(Field, TEXT("value")) == 0 ? Row : Data)->SetField(Field, Value);
    }
    double Updated = 0.0;
    // The same placed copies follow every write of the batch, so the count is the most any one write moved, not a sum.
    if (Reply.Result.IsValid() && Reply.Result->TryGetNumberField(TEXT("instancesUpdated"), Updated)) InstancesUpdated = FMath::Max(InstancesUpdated, Updated);
    Rows.Add(MakeShared<FJsonValueObject>(Row));
  }
  if (InstancesUpdated >= 0.0) Data->SetNumberField(TEXT("instancesUpdated"), InstancesUpdated);
  Finish(Out, Data, TEXT("properties"), Rows, Failed, FString::Printf(TEXT("Set %d properties."), Rows.Num()));
}

// objectPaths: the same write on several targets (eight ambient sounds' VolumeMultiplier took eight calls). Each entry
// is the whole target of its write, so actorName and the other target fields are dropped; with properties, every
// target runs that batch. changedEntities names what the writes changed (the actor, else the package), which the
// receipt lists as changes.
void RunTargets(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, const TArray<TSharedPtr<FJsonValue>>& Paths,
                FSetBatchWrite RunOne, FSetBatchReply& Out)
{
  TArray<TSharedPtr<FJsonValue>> Rows;
  TArray<FString> Failed;
  TArray<FString> Changed;
  for (const TSharedPtr<FJsonValue>& Entry : Paths) {
    FString Path;
    if (Entry.IsValid()) Entry->TryGetString(Path);
    Path.TrimStartAndEndInline();
    TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
    One->Values = Payload->Values;
    for (const TCHAR *Field : {TEXT("objectPaths"), TEXT("actorName"), TEXT("name"), TEXT("blueprintPath"), TEXT("watch")}) One->RemoveField(Field);
    One->SetStringField(TEXT("objectPath"), Path);
    const FMcpCapturedResponse Reply = RunCaptured(FString::Printf(TEXT("%s#target%d"), *RequestId, Rows.Num()), One, RunOne);
    TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
    Row->SetStringField(TEXT("objectPath"), Path);
    Row->SetBoolField(TEXT("applied"), Reply.bSuccess);
    if (!Reply.bSuccess) Failed.Add(FString::Printf(TEXT("%s: %s"), *Path, *Reply.Message));
    FString Entity;
    if (Reply.bSuccess && Reply.Result.IsValid() && (Reply.Result->TryGetStringField(TEXT("actorPath"), Entity) || Reply.Result->TryGetStringField(TEXT("assetPath"), Entity))) Changed.AddUnique(Entity);
    for (const TCHAR *Field : {TEXT("value"), TEXT("properties"), TEXT("actorPath"), TEXT("instancesUpdated"), TEXT("saved"), TEXT("saveSkippedReason")}) {
      const TSharedPtr<FJsonValue> Value = Reply.Result.IsValid() ? Reply.Result->TryGetField(Field) : nullptr;
      if (Value.IsValid()) Row->SetField(Field, Value);
    }
    Rows.Add(MakeShared<FJsonValueObject>(Row));
  }
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> ChangedValues;
  for (const FString& Entity : Changed) ChangedValues.Add(MakeShared<FJsonValueString>(Entity));
  Data->SetArrayField(TEXT("changedEntities"), ChangedValues);
  Finish(Out, Data, TEXT("targets"), Rows, Failed, FString::Printf(TEXT("Wrote %d objects."), Rows.Num()));
}
}

bool RunSetBatch(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSetBatchWrite RunOne, FSetBatchReply& Out)
{
  if (!Payload.IsValid()) return false;
  const TArray<TSharedPtr<FJsonValue>>* Paths = nullptr;
  if (Payload->TryGetArrayField(TEXT("objectPaths"), Paths) && Paths->Num() > 0) {
    RunTargets(RequestId, Payload, *Paths, RunOne, Out);
    return true;
  }
  const TSharedPtr<FJsonObject>* Properties = nullptr;
  if (Payload->TryGetObjectField(TEXT("properties"), Properties) && (*Properties)->Values.Num() > 0) {
    RunProperties(RequestId, Payload, **Properties, RunOne, Out);
    return true;
  }
  return false;
}
}
