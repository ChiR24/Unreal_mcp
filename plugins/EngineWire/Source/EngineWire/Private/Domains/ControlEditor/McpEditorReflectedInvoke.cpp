// Copyright (c) 2025 EngineWire Contributors

#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#include "Foundation/Reflection/McpReflectedInvoke.h"

#include "UObject/Class.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

// Defined in McpEditorReflectedApi.cpp: the object describe_reflected_api describes, so invoke calls the same one.
UObject* McpFindReflectedInstance(const FString& ClassName);

/**
 * Calls one reflected UFunction on a plugin's live object.
 *
 * Parameters are marshalled through the function's OWN property chain, so no
 * signature is hardcoded here: whatever describe_reflected_api reports for the
 * installed build is what this accepts. That keeps an integration correct
 * across plugin updates instead of silently passing a stale parameter list.
 *
 * This is arbitrary in-process invocation and is classed accordingly — it can
 * reach any reflected function on any resolvable object.
 */
bool UEngineWireSubsystem::HandleInvokeReflectedFunction(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString ClassName, FunctionName;
  if (!Payload->TryGetStringField(TEXT("className"), ClassName) || ClassName.IsEmpty() ||
      !Payload->TryGetStringField(TEXT("functionName"), FunctionName) || FunctionName.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("'className' and 'functionName' are both required."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UObject *Instance = McpFindReflectedInstance(ClassName);
  if (Instance == nullptr) {
    SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("No reflected class named '%s'."), *ClassName),
        TEXT("NOT_FOUND"));
    return true;
  }
  UFunction *Function = Instance->FindFunction(FName(*FunctionName));
  if (Function == nullptr) {
    SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("'%s' has no reflected function '%s'. Call describe_reflected_api for the current surface."),
                        *ClassName, *FunctionName),
        TEXT("NOT_FOUND"));
    return true;
  }
  // Invoking on a template -- the CDO or any subobject it owns -- mutates shared
  // default state and never reaches the running instance the caller meant, so
  // refuse rather than appear to work.
  if (Instance->IsTemplate()) {
    SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Only the class default object of '%s' exists. Open the owning window first so a live instance is created."),
                        *ClassName),
        TEXT("NO_LIVE_INSTANCE"));
    return true;
  }

  const TSharedPtr<FJsonObject> *ArgsPtr = nullptr;
  Payload->TryGetObjectField(TEXT("arguments"), ArgsPtr);
  const TSharedPtr<FJsonObject> Args =
      (ArgsPtr && ArgsPtr->IsValid()) ? *ArgsPtr : nullptr;

  FMcpScopedParamBlock Params(Function);
  TArray<TSharedPtr<FJsonValue>> Unset;
  FString BindError;
  if (!McpBindJsonArgsToParams(Function, Args, Params.Data(), Unset, BindError)) {
    SendAutomationError(Socket, RequestId, BindError,
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // An actor's ProcessEvent runs nothing in an editor world unless script execution is allowed.
  FEditorScriptExecutionGuard ScriptGuard;
  Instance->ProcessEvent(Function, Params.Data());

  // Return and out parameters carry the answer; read them back generically so
  // the caller sees whatever this build's signature produces.
  const TSharedPtr<FJsonObject> Outputs = McpReadParamOutputs(Function, Params.Data());

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("className"), ClassName);
  Result->SetStringField(TEXT("functionName"), FunctionName);
  Result->SetStringField(TEXT("resolvedObject"), Instance->GetPathName());
  if (Outputs.IsValid() && Outputs->Values.Num() > 0) {
    Result->SetObjectField(TEXT("outputs"), Outputs);
  }
  if (Unset.Num() > 0) {
    Result->SetArrayField(TEXT("unsetParameters"), Unset);
  }
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Invoked %s::%s."), *ClassName, *FunctionName), Result);
  return true;
}
