// set_metasound_default: a graph input's default, or (nodeId) a node input's
// literal. Split out of McpAutomationBridge_AudioAuthoringHandlersMetaSoundInterfaces.cpp.
//
// The published contract has always been `defaultValue`, but the handler read
// only floatValue/intValue/boolValue/stringValue, so a documented call set the
// input to 0.0 and reported success. `defaultValue` is now converted to the
// target input's own data type (scalars and arrays); the explicit typed fields
// still work.
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
namespace
{
bool MsJsonToBool(const TSharedPtr<FJsonValue>& V)
{
	if (V->Type == EJson::Boolean) { return V->AsBool(); }
	if (V->Type == EJson::Number) { return V->AsNumber() != 0.0; }
	const FString S = V->AsString();
	return S.Equals(TEXT("true"), ESearchCase::IgnoreCase) || S == TEXT("1");
}

double MsJsonToNumber(const TSharedPtr<FJsonValue>& V)
{
	if (V->Type == EJson::Number) { return V->AsNumber(); }
	if (V->Type == EJson::Boolean) { return V->AsBool() ? 1.0 : 0.0; }
	return FCString::Atod(*V->AsString());
}

FString MsJsonToString(const TSharedPtr<FJsonValue>& V)
{
	return V->Type == EJson::Number ? FString::SanitizeFloat(V->AsNumber()) : V->AsString();
}

// Base kinds a literal can carry. Enums store their int32 value; Time is seconds.
enum class EMsLiteralKind { Float, Int, Bool, String, Unknown };

EMsLiteralKind MsKindForType(const FString& BaseType)
{
	if (BaseType == TEXT("Float") || BaseType == TEXT("Time")) { return EMsLiteralKind::Float; }
	if (BaseType == TEXT("Int32") || BaseType.StartsWith(TEXT("Enum"))) { return EMsLiteralKind::Int; }
	if (BaseType == TEXT("Bool")) { return EMsLiteralKind::Bool; }
	if (BaseType == TEXT("String")) { return EMsLiteralKind::String; }
	return EMsLiteralKind::Unknown;
}

EMsLiteralKind MsKindForJson(const TSharedPtr<FJsonValue>& V)
{
	switch (V->Type)
	{
	case EJson::Boolean: return EMsLiteralKind::Bool;
	case EJson::String: return EMsLiteralKind::String;
	default: return EMsLiteralKind::Float;
	}
}

// `defaultValue` rendered in the target input's data type (TypeName such as
// "Float", "Int32", "Time", "Float:Array"); an unknown type falls back to the
// JSON value's own type.
bool MsBuildLiteral(const TSharedPtr<FJsonValue>& Value, const FString& TypeName,
	FMetasoundFrontendLiteral& Out, FString& OutError)
{
	const bool bArray = TypeName.EndsWith(TEXT(":Array")) || (TypeName.IsEmpty() && Value->Type == EJson::Array);
	const FString BaseType = TypeName.Replace(TEXT(":Array"), TEXT(""));
	if (bArray)
	{
		if (Value->Type != EJson::Array)
		{
			OutError = FString::Printf(TEXT("input is %s; defaultValue must be a JSON array"), *TypeName);
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>& Items = Value->AsArray();
		EMsLiteralKind Kind = MsKindForType(BaseType);
		if (Kind == EMsLiteralKind::Unknown) { Kind = Items.Num() > 0 ? MsKindForJson(Items[0]) : EMsLiteralKind::Float; }
		TArray<float> Floats; TArray<int32> Ints; TArray<bool> Bools; TArray<FString> Strings;
		for (const TSharedPtr<FJsonValue>& Item : Items)
		{
			Floats.Add(static_cast<float>(MsJsonToNumber(Item)));
			Ints.Add(FMath::RoundToInt(MsJsonToNumber(Item)));
			Bools.Add(MsJsonToBool(Item));
			Strings.Add(MsJsonToString(Item));
		}
		switch (Kind)
		{
		case EMsLiteralKind::Int: Out.Set(Ints); break;
		case EMsLiteralKind::Bool: Out.Set(Bools); break;
		case EMsLiteralKind::String: Out.Set(Strings); break;
		default: Out.Set(Floats); break;
		}
		return true;
	}
	if (Value->Type == EJson::Array || Value->Type == EJson::Object || Value->Type == EJson::Null)
	{
		OutError = FString::Printf(TEXT("input is %s; defaultValue must be a single value"), *TypeName);
		return false;
	}
	EMsLiteralKind Kind = MsKindForType(BaseType);
	if (Kind == EMsLiteralKind::Unknown) { Kind = MsKindForJson(Value); }
	switch (Kind)
	{
	case EMsLiteralKind::Int: Out.Set(static_cast<int32>(FMath::RoundToInt(MsJsonToNumber(Value)))); break;
	case EMsLiteralKind::Bool: Out.Set(MsJsonToBool(Value)); break;
	case EMsLiteralKind::String: Out.Set(MsJsonToString(Value)); break;
	default: Out.Set(static_cast<float>(MsJsonToNumber(Value))); break;
	}
	return true;
}

}

// The legacy typed fields, then `defaultValue`. False with OutError when nothing usable was sent.
bool MetaSoundLiteralFromParams(const TSharedPtr<FJsonObject>& Params, const FString& TypeName,
	FMetasoundFrontendLiteral& Out, FString& OutError)
{
	if (Params->HasField(TEXT("floatValue"))) { Out.Set(static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("floatValue"), 0.0))); return true; }
	if (Params->HasField(TEXT("intValue"))) { Out.Set(static_cast<int32>(McpHandlerUtils::GetOptionalInt(Params, TEXT("intValue"), 0))); return true; }
	if (Params->HasField(TEXT("boolValue"))) { Out.Set(McpHandlerUtils::GetOptionalBool(Params, TEXT("boolValue"), false)); return true; }
	if (Params->HasField(TEXT("stringValue"))) { Out.Set(McpHandlerUtils::GetOptionalString(Params, TEXT("stringValue"), TEXT(""))); return true; }
	const TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("defaultValue"));
	if (!Value.IsValid())
	{
		OutError = TEXT("defaultValue is required (a number, boolean, string, or an array of them for an array input)");
		return false;
	}
	return MsBuildLiteral(Value, TypeName, Out, OutError);
}

namespace
{
TSharedPtr<FJsonObject> MsListNodeInputs(FMetaSoundFrontendDocumentBuilder& Builder, const FGuid& NodeGuid, TSharedPtr<FJsonObject> Error)
{
	TArray<TSharedPtr<FJsonValue>> Inputs;
	for (const FMetasoundFrontendVertex* Input : Builder.FindNodeInputs(NodeGuid))
	{
		Inputs.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s (%s)"), *Input->Name.ToString(), *Input->TypeName.ToString())));
	}
	Error->SetArrayField(TEXT("availableInputs"), Inputs);
	return Error;
}
}
#endif

TSharedPtr<FJsonObject> HandleMetaSoundDefaultAction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
	const FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
	const FString InputName = McpHandlerUtils::GetOptionalString(Params, TEXT("inputName"), TEXT(""));
	const FString NodeRef = McpHandlerUtils::GetOptionalString(Params, TEXT("nodeId"), TEXT(""));
	if (AssetPath.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_PATH"), TEXT("Asset path is required")); }
	if (InputName.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_INPUT_NAME"), TEXT("Input name is required")); }

	UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
	if (!MetaSound)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath));
	}
	TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
#if MCP_HAS_METASOUND_FRONTEND_V2
	FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
	FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif

	// Resolve the target first: its data type decides how defaultValue converts.
	FGuid NodeGuid;
	const FMetasoundFrontendVertex* NodeInput = nullptr;
	FString TypeName;
	if (!NodeRef.IsEmpty())
	{
		if (!FGuid::Parse(NodeRef, NodeGuid) || !Builder.FindNode(NodeGuid))
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_GUID"), FString::Printf(
				TEXT("nodeId '%s' is not a node in this MetaSound; pass the nodeId add_metasound_node returned"), *NodeRef));
		}
		NodeInput = Builder.FindNodeInput(NodeGuid, FName(*InputName));
		if (!NodeInput)
		{
			return MsListNodeInputs(Builder, NodeGuid, McpHandlerUtils::BuildErrorResponse(TEXT("INPUT_NOT_FOUND"),
				FString::Printf(TEXT("Node has no input '%s'"), *InputName)));
		}
		TypeName = NodeInput->TypeName.ToString();
	}
	else if (const FMetasoundFrontendClassInput* GraphInput = Builder.FindGraphInput(FName(*InputName)))
	{
		TypeName = GraphInput->TypeName.ToString();
	}
	else
	{
		TSharedPtr<FJsonObject> Error = McpHandlerUtils::BuildErrorResponse(TEXT("INPUT_NOT_FOUND"),
			FString::Printf(TEXT("Graph input '%s' not found (pass nodeId to set a node's input instead)"), *InputName));
#if MCP_HAS_METASOUND_FRONTEND_V2
		TArray<TSharedPtr<FJsonValue>> InputArray;
		for (const FMetasoundFrontendClassInput& Input : Builder.GetConstDocumentChecked().RootGraph.GetDefaultInterface().Inputs)
		{
			InputArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s (%s)"), *Input.Name.ToString(), *Input.TypeName.ToString())));
		}
		Error->SetArrayField(TEXT("availableInputs"), InputArray);
#endif
		return Error;
	}

	FMetasoundFrontendLiteral Literal;
	FString LiteralError;
	if (!MetaSoundLiteralFromParams(Params, TypeName, Literal, LiteralError))
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_VALUE"), FString::Printf(TEXT("'%s': %s"), *InputName, *LiteralError));
	}

	const bool bSuccess = NodeInput
		? Builder.SetNodeInputDefault(NodeGuid, NodeInput->VertexID, Literal)
		: Builder.SetGraphInputDefault(FName(*InputName), Literal);
#if MCP_HAS_METASOUND_FRONTEND_V2
	Builder.FinishBuilding();
#endif
	if (!bSuccess)
	{
		TSharedPtr<FJsonObject> Error = McpHandlerUtils::BuildErrorResponse(TEXT("SET_DEFAULT_FAILED"), FString::Printf(
			TEXT("Failed to set '%s' - value could not be applied (expected '%s')"), *InputName, *TypeName));
		Error->SetStringField(TEXT("expectedDataType"), TypeName);
		return Error;
	}
	McpSafeAssetSave(MetaSound);
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound %s '%s' set"), NodeInput ? TEXT("node input") : TEXT("default for"), *InputName));
	Response->SetStringField(TEXT("dataType"), TypeName);
	Response->SetStringField(TEXT("appliedValue"), Literal.ToString());
	McpHandlerUtils::AddVerification(Response, MetaSound);
	return Response;
#else
	return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_NOT_AVAILABLE"), TEXT("MetaSound Frontend Builder not available (UE 5.3+)"));
#endif
}
}
#endif
