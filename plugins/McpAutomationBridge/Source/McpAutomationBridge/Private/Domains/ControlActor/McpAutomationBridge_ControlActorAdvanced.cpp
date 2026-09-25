#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

#include "Foundation/Reflection/McpReflectedInvoke.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetBlueprintVariables(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // actors: many actors, each with its own values, in one call (eight billboard
  // headlines were eight calls). Each item runs through this handler under a
  // captured id, so it behaves exactly like a single call.
  const TArray<TSharedPtr<FJsonValue>> *Items = nullptr;
  if (Payload->TryGetArrayField(TEXT("actors"), Items) && Items->Num() > 0) {
    FMcpResponseCaptureRegistry &Capture = FMcpResponseCaptureRegistry::Get();
    TArray<TSharedPtr<FJsonValue>> Results;
    TArray<FString> Failures;
    for (int32 Index = 0; Index < Items->Num(); ++Index) {
      const TSharedPtr<FJsonObject> *Item = nullptr;
      TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
      FString Name;
      int32 Wanted = 0;
      if ((*Items)[Index].IsValid() && (*Items)[Index]->TryGetObject(Item) && Item) {
        One->Values = (*Item)->Values;
        One->RemoveField(TEXT("actors"));
        One->TryGetStringField(TEXT("actorName"), Name);
        const TSharedPtr<FJsonObject> *Vars = nullptr;
        if (One->TryGetObjectField(TEXT("variables"), Vars) && Vars && Vars->IsValid())
          Wanted = (*Vars)->Values.Num();
      }
      const FString ItemId = FString::Printf(TEXT("%s#%d"), *RequestId, Index);
      Capture.Begin(ItemId);
      HandleControlActorSetBlueprintVariables(ItemId, One, Socket);
      const FMcpCapturedResponse Reply = Capture.End(ItemId);
      // Reply envelope: { data: { updated: [...] }, warnings: [...] }.
      const TSharedPtr<FJsonObject> *ReplyData = nullptr;
      const TArray<TSharedPtr<FJsonValue>> *Updated = nullptr;
      const TArray<TSharedPtr<FJsonValue>> *Warnings = nullptr;
      if (Reply.Result.IsValid()) {
        if (Reply.Result->TryGetObjectField(TEXT("data"), ReplyData))
          (*ReplyData)->TryGetArrayField(TEXT("updated"), Updated);
        Reply.Result->TryGetArrayField(TEXT("warnings"), Warnings);
      }
      const bool bAll = Reply.bSuccess && Updated && Wanted > 0 && Updated->Num() == Wanted;
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("actorName"), Name);
      Entry->SetBoolField(TEXT("success"), bAll);
      if (Updated)
        Entry->SetArrayField(TEXT("updated"), *Updated);
      if (!bAll) {
        FString Why = Reply.Message;
        if (Warnings) {
          for (const TSharedPtr<FJsonValue> &Warning : *Warnings)
            Why += TEXT(" ") + Warning->AsString();
        }
        Entry->SetStringField(TEXT("error"), Why);
        Failures.Add(FString::Printf(TEXT("%s: %s"), *Name, *Why));
      }
      Results.Add(MakeShared<FJsonValueObject>(Entry));
    }
    const int32 Done = Results.Num() - Failures.Num();
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetArrayField(TEXT("results"), Results);
    Data->SetNumberField(TEXT("updatedActors"), Done);
    if (Failures.Num() > 0) {
      SendAutomationResponse(Socket, RequestId, false,
                             FString::Printf(TEXT("Variables set on %d of %d actors; %s"), Done,
                                             Results.Num(), *FString::Join(Failures, TEXT("; "))),
                             Data, TEXT("VARIABLE_BATCH_INCOMPLETE"));
    } else {
      SendAutomationResponse(Socket, RequestId, true,
                             FString::Printf(TEXT("Variables set on %d actors"), Done), Data);
    }
    return true;
  }

  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  const TSharedPtr<FJsonObject> *VariablesPtr = nullptr;
  if (!(Payload->TryGetObjectField(TEXT("variables"), VariablesPtr) &&
        VariablesPtr && VariablesPtr->IsValid())) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("variables object required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  UClass *ActorClass = Found->GetClass();
  Found->Modify();
  TArray<FString> Applied;
  TArray<FString> Warnings;

  for (const auto &Pair : (*VariablesPtr)->Values) {
    const FString VariableName(*Pair.Key);
    FProperty *Property = ActorClass->FindPropertyByName(*VariableName);
    if (!Property) {
      Warnings.Add(FString::Printf(TEXT("Property not found: %s"), *VariableName));
      continue;
    }

    FString ApplyError;
    if (ApplyJsonValueToProperty(Found, Property, Pair.Value, ApplyError))
      Applied.Add(VariableName);
    else
      Warnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *VariableName,
                                   *ApplyError));
  }

  // A Details-panel edit reruns the construction script; without this an
  // instance kept the look its old values built until something else moved it.
  if (Applied.Num() > 0) {
    Found->PostEditChange();
  }
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  if (Applied.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> AppliedArray;
    for (const FString &Name : Applied)
      AppliedArray.Add(MakeShared<FJsonValueString>(Name));
    Data->SetArrayField(TEXT("updated"), AppliedArray);
  }

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Variables updated"), Data, Warnings);
  return true;
#else
  return false;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlActorExport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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

  FMcpOutputCapture OutputCapture;
  UExporter::ExportToOutputDevice(nullptr, Found, nullptr, OutputCapture,
                                  TEXT("T3D"), 0, 0, false);
  FString OutputString = FString::Join(OutputCapture.Consume(), TEXT("\n"));

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("t3d"), OutputString);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor exported"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCallFunction(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName, FunctionName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("functionName"), FunctionName);

  if (ActorName.IsEmpty() || FunctionName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName and functionName are required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  UFunction* Function = Actor->FindFunction(*FunctionName);
  if (!Function) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Function not found: %s"), *FunctionName), TEXT("FUNCTION_NOT_FOUND"));
    return true;
  }

  const TSharedPtr<FJsonObject> *ArgsPtr = nullptr;
  Payload->TryGetObjectField(TEXT("arguments"), ArgsPtr);
  const TSharedPtr<FJsonObject> Args =
      (ArgsPtr && ArgsPtr->IsValid()) ? *ArgsPtr : nullptr;

  TArray<TSharedPtr<FJsonValue>> Unset;
  TSharedPtr<FJsonObject> Outputs;

  if (Function->ParmsSize > 0) {
    // The parameter block must be constructed, bound and destroyed through the
    // function's own property chain. Handing ProcessEvent a merely zeroed buffer
    // -- what this did before -- discarded every argument the caller sent while
    // still reporting success, so a bool argument always arrived false.
    FMcpScopedParamBlock Params(Function);
    FString BindError;
    if (!McpBindJsonArgsToParams(Function, Args, Params.Data(), Unset, BindError)) {
      SendAutomationError(Socket, RequestId, BindError, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Actor->ProcessEvent(Function, Params.Data());
    Outputs = McpReadParamOutputs(Function, Params.Data());
  } else {
    Actor->ProcessEvent(Function, nullptr);
    Outputs = MakeShared<FJsonObject>();
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetStringField(TEXT("functionName"), FunctionName);
  // Name resolution alone does not prove the invocation reached the world instance: the sibling
  // reflected-invoke capability guards against templates because "invoking on a template ... never
  // reaches the running instance the caller meant". Report what was actually resolved, so a caller can
  // tell a live actor from a class default object instead of inferring it from an effect that never came.
  Data->SetStringField(TEXT("resolvedObject"), Actor->GetPathName());
  Data->SetBoolField(TEXT("resolvedIsTemplate"), Actor->IsTemplate());
  // Now that the function side is fully accounted for (owner, flags, native bit, non-null thunk,
  // ParmsSize), the remaining axis is the TARGET: a pointer whose GetPathName() is correct can still be
  // unfit to receive a reflected call. Report the validity and class-relationship facts directly rather
  // than inferring them from an effect that never came.
  Data->SetBoolField(TEXT("actorIsValid"), IsValid(Actor));
  Data->SetBoolField(TEXT("actorIsUnreachable"), Actor->IsUnreachable());
  Data->SetStringField(TEXT("actorClass"), Actor->GetClass()->GetName());
  if (UClass *FunctionOwner = Cast<UClass>(Function->GetOuter())) {
    Data->SetBoolField(TEXT("actorIsAOfFunctionOwner"), Actor->IsA(FunctionOwner));
  }
  // The function is resolved and the object is a live, non-template instance, yet the call has no effect.
  // Report what was found so the invocation itself can be diagnosed from the receipt instead of guessed at:
  // a native function with no thunk, or a function owned by an unexpected class, would both explain a
  // silent no-op.
  Data->SetStringField(TEXT("resolvedFunction"), Function->GetPathName());
  Data->SetStringField(TEXT("functionOwnerClass"),
                       Function->GetOuter() ? Function->GetOuter()->GetName() : FString());
  Data->SetStringField(TEXT("functionFlags"),
                       FString::Printf(TEXT("0x%08X"), (uint32)Function->FunctionFlags));
  Data->SetBoolField(TEXT("functionIsNative"), (Function->FunctionFlags & FUNC_Native) != 0);
  // A native UFunction with no thunk would make UFunction::Invoke a silent no-op -- the last remaining
  // explanation that fits "correct function, correct live instance, no effect". Report it directly.
  Data->SetBoolField(TEXT("functionHasNativeThunk"), Function->GetNativeFunc() != nullptr);
  Data->SetNumberField(TEXT("functionParmsSize"), Function->ParmsSize);
  Data->SetObjectField(TEXT("outputs"), Outputs);
  Data->SetArrayField(TEXT("unsetParameters"), Unset);

  // The published output schema declares `value` ("Property value (any type)") and the capability example
  // promises {"success":true,...,"value":true}, but only `outputs` was ever emitted -- so a caller that read
  // the documented field got nothing while the real return sat under outputs.ReturnValue. Surface the return
  // value under the documented name too; `outputs` stays for callers that already use it.
  if (Outputs.IsValid())
  {
    const TSharedPtr<FJsonValue>* ReturnValue = Outputs->Values.Find(TEXT("ReturnValue"));
    if (ReturnValue && ReturnValue->IsValid())
    {
      Data->SetField(TEXT("value"), *ReturnValue);
    }
  }
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Function called"), Data);
  return true;
#else
  return false;
#endif
}
