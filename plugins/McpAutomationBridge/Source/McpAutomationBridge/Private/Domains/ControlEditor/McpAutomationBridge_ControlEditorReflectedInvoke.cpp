// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Foundation/Reflection/McpReflectedInvoke.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace
{
/** Mirrors the resolver in describe_reflected_api: live instance over CDO. */
UObject* ResolveReflectedTarget(const FString& ClassName)
{
	UClass* Target = nullptr;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if (ClassIt->GetName() == ClassName)
		{
			Target = *ClassIt;
			break;
		}
	}
	if (Target == nullptr)
	{
		return nullptr;
	}
	for (TObjectIterator<UObject> ObjectIt; ObjectIt; ++ObjectIt)
	{
		UObject* Candidate = *ObjectIt;
		// IsTemplate walks the outer chain, so a component template owned by a
		// CDO -- Default__WheeledVehiclePawn:VehicleMovementComp, which carries
		// RF_DefaultSubObject rather than RF_ClassDefaultObject -- is rejected
		// here too. Checking only the object's own RF_ClassDefaultObject let
		// such an archetype pass as a "live instance".
		if (Candidate->GetClass() == Target && !Candidate->IsTemplate() && IsValid(Candidate))
		{
			return Candidate;
		}
	}
	return Target->GetDefaultObject();
}
} // namespace

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
bool UMcpAutomationBridgeSubsystem::HandleInvokeReflectedFunction(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString ClassName, FunctionName;
  if (!Payload->TryGetStringField(TEXT("className"), ClassName) || ClassName.IsEmpty() ||
      !Payload->TryGetStringField(TEXT("functionName"), FunctionName) || FunctionName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("'className' and 'functionName' are both required."), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UObject *Instance = ResolveReflectedTarget(ClassName);
  if (Instance == nullptr) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("No reflected class named '%s'."), *ClassName), nullptr,
        TEXT("NOT_FOUND"));
    return true;
  }
  UFunction *Function = Instance->FindFunction(FName(*FunctionName));
  if (Function == nullptr) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("'%s' has no reflected function '%s'. Call describe_reflected_api for the current surface."),
                        *ClassName, *FunctionName),
        nullptr, TEXT("NOT_FOUND"));
    return true;
  }
  // Invoking on a template -- the CDO or any subobject it owns -- mutates shared
  // default state and never reaches the running instance the caller meant, so
  // refuse rather than appear to work.
  if (Instance->IsTemplate()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Only the class default object of '%s' exists. Open the owning window first so a live instance is created."),
                        *ClassName),
        nullptr, TEXT("NO_LIVE_INSTANCE"));
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
    SendAutomationResponse(Socket, RequestId, false, BindError, nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  Instance->ProcessEvent(Function, Params.Data());

  // Return and out parameters carry the answer; read them back generically so
  // the caller sees whatever this build's signature produces.
  const TSharedPtr<FJsonObject> Outputs = McpReadParamOutputs(Function, Params.Data());

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("className"), ClassName);
  Result->SetStringField(TEXT("functionName"), FunctionName);
  Result->SetStringField(TEXT("resolvedObject"), Instance->GetPathName());
  Result->SetObjectField(TEXT("outputs"), Outputs);
  Result->SetArrayField(TEXT("unsetParameters"), Unset);
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Invoked %s::%s."), *ClassName, *FunctionName), Result);
  return true;
}
#else
bool UMcpAutomationBridgeSubsystem::HandleInvokeReflectedFunction(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false, TEXT("Editor required."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif
