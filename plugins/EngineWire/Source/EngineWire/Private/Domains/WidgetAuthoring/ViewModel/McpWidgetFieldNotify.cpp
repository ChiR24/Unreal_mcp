// set_field_notify (see McpWidgetViewModel.h): the FieldNotify metadata on Blueprint variables, which the variable's
// FieldNotify box writes (an empty value: this field alone). Only a Blueprint that sends field notifications (a
// viewmodel, a widget) uses it. One undo step; compiled and saved after it.

#include "Domains/WidgetAuthoring/ViewModel/McpWidgetViewModel.h"

#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileError.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Safety/McpSafeOperations.h"

namespace McpWidgetViewModel
{
bool HandleFieldNotify(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket)
{
	const auto Refuse = [&](const FString& Message, const TCHAR* Code)
	{
		Bridge.SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was changed."), Code);
		return true;
	};
	FString Normalized, Error;
	UBlueprint* Blueprint = LoadBlueprintAsset(GetJsonStringField(Payload, TEXT("blueprintPath")), Normalized, Error);
	const TArray<FString> Names = McpHandlerUtils::GetStringArrayField(Payload, TEXT("variableNames"));
	const bool bOn = GetJsonBoolField(Payload, TEXT("on"), true);
	if (!Blueprint)
	{
		return Refuse(Error, TEXT("NOT_FOUND"));
	}
	if (!Notifies(Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass.Get() : Blueprint->GeneratedClass.Get()))
	{
		return Refuse(FString::Printf(TEXT("%s sends no field notifications, so FieldNotify does nothing on its variables: it works on a viewmodel "
			"(parent class MVVMViewModelBase) and on a Widget Blueprint."), *Blueprint->GetName()), TEXT("NOT_NOTIFYING"));
	}
	for (const FString& Name : Names)
	{
		if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*Name)) == INDEX_NONE)
		{
			TArray<FString> Variables;
			for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
			{
				Variables.Add(Variable.VarName.ToString());
			}
			return Refuse(FString::Printf(TEXT("%s has no variable %s; its variables: %s."), *Blueprint->GetName(), *Name,
				Variables.Num() ? *FString::Join(Variables, TEXT(", ")) : TEXT("none")), TEXT("VARIABLE_NOT_FOUND"));
		}
	}
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	{
		FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set FieldNotify")), EMcpMutationDurability::EditorStateOnly, {Blueprint});
		for (const FString& Name : Names)
		{
			if (bOn)
			{
				FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, FName(*Name), nullptr, FName(TEXT("FieldNotify")), FString());
			}
			else
			{
				FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(Blueprint, FName(*Name), nullptr, FName(TEXT("FieldNotify")));
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		Transaction.DescribeInto(Result);
	}
	FString CompileError;
	Result->SetStringField(TEXT("blueprintPath"), Normalized);
	Result->SetArrayField(TEXT("variableNames"), McpHandlerUtils::ToJsonStringArray(Names));
	Result->SetBoolField(TEXT("fieldNotify"), bOn);
	Result->SetBoolField(TEXT("compiled"), McpCompileBlueprintWithError(Blueprint, CompileError));
	Result->SetBoolField(TEXT("saved"), McpSafeOperations::McpSafeAssetSave(Blueprint));
	Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("FieldNotify turned %s for %d variable(s) of %s."), bOn ? TEXT("on") : TEXT("off"),
		Names.Num(), *Blueprint->GetName()), Result);
	return true;
}
}
