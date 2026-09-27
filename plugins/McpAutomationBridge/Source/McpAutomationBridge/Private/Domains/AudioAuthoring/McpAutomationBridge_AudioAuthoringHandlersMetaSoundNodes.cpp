#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

namespace McpAudioAuthoring
{
static FString BuildMetaSoundClassName(const FString& ActualNamespace, const FString& ActualName, const FString& ActualVariant)
{
	return ActualNamespace.IsEmpty()
		? ActualName
		: (ActualVariant.IsEmpty()
			? FString::Printf(TEXT("%s.%s"), *ActualNamespace, *ActualName)
			: FString::Printf(TEXT("%s.%s.%s"), *ActualNamespace, *ActualName, *ActualVariant));
}

#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
// nodeType shorthands: "alias[/alias]" -> UE.<Name>.<Variant>.
struct FMcpMetaSoundNodeAlias { const TCHAR* Aliases; const TCHAR* Name; const TCHAR* Variant; };
static const FMcpMetaSoundNodeAlias MetaSoundNodeAliases[] = {
	{TEXT("oscillator/sine"), TEXT("Sine"), TEXT("Audio")},
	{TEXT("gain/multiply"), TEXT("Multiply"), TEXT("Float")},
	{TEXT("multiply_audio"), TEXT("Multiply"), TEXT("Audio")},
	{TEXT("add"), TEXT("Add"), TEXT("Float")},
	{TEXT("add_audio"), TEXT("Add"), TEXT("Audio")},
	{TEXT("waveplayer/wave_player"), TEXT("Wave Player"), TEXT("Mono")},
};
#endif

TSharedPtr<FJsonObject> HandleMetaSoundNodeActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("add_metasound_node"))
	{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		FString NodeClassName = GetJsonStringField(Params, TEXT("nodeClassName"), TEXT(""));
		FString NodeType = GetJsonStringField(Params, TEXT("nodeType"), TEXT(""));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

		if (AssetPath.IsEmpty())
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_PATH"), TEXT("Asset path is required"));
		}

		UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
		if (!MetaSound)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath));
		}

		IMetaSoundDocumentInterface* DocInterface = Cast<IMetaSoundDocumentInterface>(MetaSound);
		if (!DocInterface)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INTERFACE_ERROR"), TEXT("MetaSound does not implement document interface"));
		}

		TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
		MCP_METASOUND_BUILDER(Builder, ScriptInterface);

		FString ActualNamespace;
		FString ActualName;
		FString ActualVariant;

		if (!NodeClassName.IsEmpty())
		{
			TArray<FString> Parts;
			NodeClassName.ParseIntoArray(Parts, TEXT("."));
			if (Parts.Num() == 3)
			{
				ActualNamespace = Parts[0];
				ActualName = Parts[1];
				ActualVariant = Parts[2];
			}
			else if (Parts.Num() == 2)
			{
				ActualNamespace = Parts[0];
				ActualName = Parts[1];
			}
			else
			{
				ActualName = NodeClassName;
			}
		}
		else if (!NodeType.IsEmpty())
		{
			ActualName = NodeType;
			for (const FMcpMetaSoundNodeAlias& Alias : MetaSoundNodeAliases)
			{
				TArray<FString> Spellings;
				FString(Alias.Aliases).ParseIntoArray(Spellings, TEXT("/"));
				if (Spellings.Contains(NodeType.ToLower()))
				{
					ActualNamespace = TEXT("UE");
					ActualName = Alias.Name;
					ActualVariant = Alias.Variant;
					break;
				}
			}
		}

		if (ActualName.IsEmpty())
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NODE_TYPE"), TEXT("Node class name or type is required"));
		}

		TArray<FString> RegistryCandidates;
#if MCP_HAS_METASOUND_SEARCH_ENGINE
		// Every spelling is resolved against the live node registry before the add,
		// case-insensitively: a bare or partial name ("Sine", "UE.Multiply") gets its
		// full Namespace.Name.Variant (dogfood #115), and a wrong namespace guess
		// ("UE.AD Envelope.Audio" for "AD Envelope.AD Envelope.Audio" -- standard
		// nodes do not share one namespace) is retried by name and variant alone,
		// so the engine is never asked for a class it will log as unregistered.
		{
			const FString Requested = BuildMetaSoundClassName(ActualNamespace, ActualName, ActualVariant);
			FMetasoundFrontendClassName Resolved;
			if (ResolveMetaSoundNodeClassName(ActualNamespace, ActualName, ActualVariant, Resolved, RegistryCandidates) ||
				(!ActualNamespace.IsEmpty() &&
					ResolveMetaSoundNodeClassName(FString(), ActualName, ActualVariant, Resolved, RegistryCandidates)))
			{
				ActualNamespace = Resolved.Namespace.ToString();
				ActualName = Resolved.Name.ToString();
				ActualVariant = Resolved.Variant.ToString();
				if (!BuildMetaSoundClassName(ActualNamespace, ActualName, ActualVariant).Equals(Requested))
				{
					Response->SetStringField(TEXT("nodeClassResolvedBy"), TEXT("registry-name-match"));
				}
			}
		}
#endif
		FMetasoundFrontendClassName ClassName = FMetasoundFrontendClassName(FName(*ActualNamespace), FName(*ActualName), FName(*ActualVariant));
		const FMetasoundFrontendNode* NewNode = Builder.AddNodeByClassName(ClassName, 1, FGuid::NewGuid());
		FString FullClassName = BuildMetaSoundClassName(ActualNamespace, ActualName, ActualVariant);

		if (NewNode)
		{
			if (bSave) { McpSafeAssetSave(MetaSound); }
			Response->SetStringField(TEXT("nodeId"), NewNode->GetID().ToString());
			Response->SetStringField(TEXT("nodeClassName"), FullClassName);
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound node '%s' added"), *FullClassName));
			McpHandlerUtils::AddVerification(Response, MetaSound);
		}
		else
		{
			TArray<TSharedPtr<FJsonValue>> CandidateArray;
			FString CandidateText;
			for (int32 Index = 0; Index < RegistryCandidates.Num() && Index < 10; ++Index)
			{
				CandidateArray.Add(MakeShared<FJsonValueString>(RegistryCandidates[Index]));
				CandidateText += (Index > 0 ? TEXT(", ") : TEXT("")) + RegistryCandidates[Index];
			}
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("error"), FString::Printf(
				TEXT("Node class '%s' not found in MetaSound registry. nodeClassName is 'Namespace.Name.Variant' (e.g. UE.Sine.Audio)%s%s"),
				*FullClassName,
				CandidateText.IsEmpty() ? TEXT("") : TEXT("; matching classes: "),
				*CandidateText));
			Response->SetStringField(TEXT("code"), TEXT("NODE_CLASS_NOT_FOUND"));
			TArray<TSharedPtr<FJsonValue>> AcceptedArray;
			for (const FMcpMetaSoundNodeAlias& Alias : MetaSoundNodeAliases)
			{
				AcceptedArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s -> UE.%s.%s"), Alias.Aliases, Alias.Name, Alias.Variant)));
			}
			Response->SetArrayField(TEXT("acceptedNodeTypes"), AcceptedArray);
			Response->SetArrayField(TEXT("candidateNodeClasses"), CandidateArray);
		}

		MCP_METASOUND_FINISH(Builder);
		return Response;
#elif MCP_HAS_METASOUND
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		FString NodeType = GetJsonStringField(Params, TEXT("nodeType"), TEXT(""));
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Cannot add MetaSound node '%s' - Frontend Builder not available"), *NodeType));
		Response->SetStringField(TEXT("code"), TEXT("METASOUND_FRONTEND_NOT_SUPPORTED"));
		Response->SetStringField(TEXT("requiredVersion"), TEXT("UE 5.3+"));
		return Response;
#else
		return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_NOT_AVAILABLE"), TEXT("MetaSound support not available"));
#endif
	}

	if (SubAction == TEXT("connect_metasound_nodes"))
	{
		return HandleMetaSoundNodeConnect(Params, Response);
	}

	return nullptr;
}
}