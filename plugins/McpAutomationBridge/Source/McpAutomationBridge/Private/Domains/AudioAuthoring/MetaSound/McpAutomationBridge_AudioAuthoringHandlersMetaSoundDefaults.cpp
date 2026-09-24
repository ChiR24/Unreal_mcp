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

#if WITH_EDITOR && MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
#include "MetasoundFrontendDataTypeRegistry.h"
#include <type_traits>
#include <utility>
#endif

#if WITH_EDITOR
namespace McpAudioAuthoring
{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
namespace
{
// The UObject class an asset-typed input (WaveAsset, ...) is built from, else null.
UClass* MsObjectClassFor(const FString& TypeName)
{
	return TypeName.IsEmpty() ? nullptr
		: Metasound::Frontend::IDataTypeRegistry::Get().GetUClassForDataType(FName(*TypeName.Replace(TEXT(":Array"), TEXT(""))));
}

template <typename T, typename = void> struct TMsHasObjectCheck : std::false_type {};
template <typename T> struct TMsHasObjectCheck<T, std::void_t<decltype(std::declval<const T&>()
	.IsValidUObjectForDataType(FName(), static_cast<const UObject*>(nullptr)))>> : std::true_type {};

// IsA alone let a MetaSoundSource through as a WaveAsset (it derives from
// USoundWave), and the saved graph then played nothing. The registry's own check
// (5.7+) knows better; without it, only the exact class the data type names.
template <typename RegistryT>
bool MsObjectFitsType(const RegistryT& Registry, const FString& TypeName, const UObject* Object, const UClass* Class)
{
	if constexpr (TMsHasObjectCheck<RegistryT>::value) { return Registry.IsValidUObjectForDataType(FName(*TypeName.Replace(TEXT(":Array"), TEXT(""))), Object); }
	else { return Object->GetClass() == Class; }
}

UObject* MsLoadLiteralObject(const TSharedPtr<FJsonValue>& Value, const FString& TypeName, UClass* Class, FString& OutError)
{
	const FString Path = Value->Type == EJson::String ? Value->AsString() : FString();
	UObject* Object = Path.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *NormalizeAudioPath(Path));
	if (!Object || !MsObjectFitsType(Metasound::Frontend::IDataTypeRegistry::Get(), TypeName, Object, Class))
	{
		OutError = FString::Printf(TEXT("'%s' is not a %s asset; pass the path of one (e.g. /Game/Audio/MyWave)"), *Path, *Class->GetName());
		return nullptr;
	}
	return Object;
}
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
	// An asset-typed input takes an object reference. It used to fall through to
	// the JSON value's own type, so a SoundWave path on a Wave Player's Wave Asset
	// was saved as a STRING literal, and building the graph at playback asserted
	// (bExpectsNone) and took the editor down.
	if (UClass* ObjectClass = MsObjectClassFor(TypeName))
	{
		TArray<UObject*> Objects;
		const TArray<TSharedPtr<FJsonValue>> Items = bArray && Value->Type == EJson::Array
			? Value->AsArray() : TArray<TSharedPtr<FJsonValue>>{ Value };
		for (const TSharedPtr<FJsonValue>& Item : Items)
		{
			UObject* Object = MsLoadLiteralObject(Item, TypeName, ObjectClass, OutError);
			if (!Object) { return false; }
			Objects.Add(Object);
		}
		if (bArray) { Out.Set(Objects); }
		else { Out.Set(Objects[0]); }
		return true;
	}
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
	if (Params->HasField(TEXT("floatValue"))) { Out.Set(static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("floatValue"), 0.0))); }
	else if (Params->HasField(TEXT("intValue"))) { Out.Set(static_cast<int32>(McpHandlerUtils::GetOptionalInt(Params, TEXT("intValue"), 0))); }
	else if (Params->HasField(TEXT("boolValue"))) { Out.Set(McpHandlerUtils::GetOptionalBool(Params, TEXT("boolValue"), false)); }
	else if (Params->HasField(TEXT("stringValue"))) { Out.Set(McpHandlerUtils::GetOptionalString(Params, TEXT("stringValue"), TEXT(""))); }
	else
	{
		const TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("defaultValue"));
		if (!Value.IsValid())
		{
			OutError = TEXT("defaultValue is required (a number, boolean, string, an asset path for an asset input, or an array of them for an array input)");
			return false;
		}
		if (!MsBuildLiteral(Value, TypeName, Out, OutError)) { return false; }
	}
	// The document builder accepts any literal and the asset saves fine; a type
	// the input cannot be built from only fails when the graph is built for
	// playback, as an assertion that crashes the editor. Refuse it while the
	// caller can still fix the call.
	if (!TypeName.IsEmpty() && !Metasound::Frontend::IDataTypeRegistry::Get().IsLiteralTypeSupported(FName(*TypeName), Out.GetType()))
	{
		const UClass* ObjectClass = MsObjectClassFor(TypeName);
		const FString Hint = ObjectClass
			? FString::Printf(TEXT("pass the path of a %s asset as defaultValue"), *ObjectClass->GetName())
			: FString(TEXT("pass a value of the input's own type, or connect a node output to it instead"));
		OutError = FString::Printf(TEXT("input is %s and cannot be built from a %s value (playing the MetaSound would crash the editor); %s"),
			*TypeName, *StaticEnum<EMetasoundFrontendLiteralType>()->GetNameStringByValue(static_cast<int64>(Out.GetType())), *Hint);
		return false;
	}
	return true;
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
