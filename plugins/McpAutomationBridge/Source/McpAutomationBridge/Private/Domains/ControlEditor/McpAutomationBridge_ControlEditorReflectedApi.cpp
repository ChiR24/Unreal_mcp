// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace
{
/**
 * Live instance of a class named at runtime, preferring a real instance over
 * the CDO.
 *
 * Plugins that host a web view register a UObject with the page (Fab binds
 * UFabBrowserApi as `window.ue.fab`). That object is reflected, so it is
 * reachable by class name without linking the plugin — which also means the
 * surface discovered here is whatever the INSTALLED version exposes, not a
 * copy of it frozen at the time this was written.
 */
UObject* FindLiveInstanceByClassName(const FString& ClassName)
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
		if (Candidate->GetClass() == Target && !Candidate->HasAnyFlags(RF_ClassDefaultObject)
			&& IsValid(Candidate))
		{
			return Candidate;
		}
	}
	return Target->GetDefaultObject();
}

/** Describes one reflected parameter so a caller can build a valid request. */
TSharedPtr<FJsonObject> DescribeParameter(const FProperty* Property)
{
	TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("name"), Property->GetName());
	Entry->SetStringField(TEXT("cppType"), Property->GetCPPType());
	Entry->SetBoolField(TEXT("isReturn"), Property->HasAnyPropertyFlags(CPF_ReturnParm));
	Entry->SetBoolField(TEXT("isOut"),
		Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ReturnParm));
	return Entry;
}

TSharedPtr<FJsonObject> DescribeFunction(UFunction* Function)
{
	TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("name"), Function->GetName());
	TArray<TSharedPtr<FJsonValue>> Params;
	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
	{
		Params.Add(MakeShared<FJsonValueObject>(DescribeParameter(*ParamIt)));
	}
	Entry->SetArrayField(TEXT("parameters"), Params);
	Entry->SetNumberField(TEXT("parameterCount"), Params.Num());
	return Entry;
}
} // namespace

/**
 * Enumerates the reflected API of a plugin's live bridge object.
 *
 * This exists so an integration does not have to hardcode another plugin's
 * contract: the answer is read from the installed build at call time, so a
 * caller sees the surface that actually shipped rather than one recorded when
 * this handler was written.
 */
bool UMcpAutomationBridgeSubsystem::HandleDescribeReflectedApi(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString ClassName;
  if (!Payload->TryGetStringField(TEXT("className"), ClassName) || ClassName.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("'className' is required (for example 'FabBrowserApi')."), nullptr,
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UObject* Instance = FindLiveInstanceByClassName(ClassName);
  if (Instance == nullptr) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("No reflected class named '%s'. Its owning plugin may be disabled, or the object is not created until its window opens."),
                        *ClassName),
        nullptr, TEXT("NOT_FOUND"));
    return true;
  }

  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);

  TArray<TSharedPtr<FJsonValue>> Functions;
  for (TFieldIterator<UFunction> FunctionIt(Instance->GetClass(), EFieldIteratorFlags::IncludeSuper);
       FunctionIt; ++FunctionIt) {
    UFunction* Function = *FunctionIt;
    if (!Filter.IsEmpty() && !Function->GetName().Contains(Filter)) {
      continue;
    }
    Functions.Add(MakeShared<FJsonValueObject>(DescribeFunction(Function)));
  }
  Functions.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) {
    FString NameA, NameB;
    A->AsObject()->TryGetStringField(TEXT("name"), NameA);
    B->AsObject()->TryGetStringField(TEXT("name"), NameB);
    return NameA < NameB;
  });

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("className"), ClassName);
  Result->SetStringField(TEXT("resolvedObject"), Instance->GetPathName());
  Result->SetBoolField(TEXT("isDefaultObject"), Instance->HasAnyFlags(RF_ClassDefaultObject));
  Result->SetArrayField(TEXT("functions"), Functions);
  Result->SetNumberField(TEXT("functionCount"), Functions.Num());
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("'%s' exposes %d reflected function(s)."), *ClassName, Functions.Num()),
      Result);
  return true;
}
#else
bool UMcpAutomationBridgeSubsystem::HandleDescribeReflectedApi(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false, TEXT("Editor required."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif
