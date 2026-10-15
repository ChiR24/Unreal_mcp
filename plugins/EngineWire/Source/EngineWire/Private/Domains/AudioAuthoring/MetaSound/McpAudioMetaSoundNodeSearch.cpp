#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAudioAuthoringPrivate.h"

#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND && MCP_HAS_METASOUND_SEARCH_ENGINE
namespace McpAudioAuthoring
{
namespace
{
// "Wave Player" / "wave_player" / "WavePlayer" all name the same registry class.
FString SquashName(const FString& In)
{
	FString Out = In;
	Out.ReplaceInline(TEXT(" "), TEXT(""));
	Out.ReplaceInline(TEXT("_"), TEXT(""));
	return Out;
}

FString DottedName(const FMetasoundFrontendClassName& ClassName)
{
	const FString Namespace = ClassName.Namespace.ToString();
	// A class with no variant has NAME_None there, which printed as a ".None" add_node never needs.
	const FString Variant = ClassName.Variant.IsNone() ? FString() : ClassName.Variant.ToString();
	FString Result = Namespace.IsEmpty() ? ClassName.Name.ToString() : Namespace + TEXT(".") + ClassName.Name.ToString();
	if (!Variant.IsEmpty())
	{
		Result += TEXT(".") + Variant;
	}
	return Result;
}
}

// Resolves a partial MetaSound node class spelling against the live node registry
// (dogfood #115). Name matches case-insensitively in any namespace; an explicit
// Namespace/Variant narrows the set. A unique hit wins outright; among several hits the
// engine "UE" namespace wins when it alone disambiguates. OutCandidates always lists every
// hit as the dotted "Namespace.Name.Variant" spelling that nodeClassName accepts.
bool ResolveMetaSoundNodeClassName(
	const FString& Namespace,
	const FString& Name,
	const FString& Variant,
	FMetasoundFrontendClassName& OutClassName,
	TArray<FString>& OutCandidates)
{
	OutCandidates.Reset();
	if (Name.IsEmpty())
	{
		return false;
	}
	const FString WantedName = SquashName(Name);
	TArray<FMetasoundFrontendClassName> Matches;
	TArray<FString> SeenNames;
	const TArray<FMetasoundFrontendClass> Classes =
		Metasound::Frontend::ISearchEngine::Get().FindAllClasses(false);
	for (const FMetasoundFrontendClass& Class : Classes)
	{
		if (Class.Metadata.GetType() != EMetasoundFrontendClassType::External)
		{
			continue;
		}
		const FMetasoundFrontendClassName& ClassName = Class.Metadata.GetClassName();
		if (!SquashName(ClassName.Name.ToString()).Equals(WantedName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!Namespace.IsEmpty() && !ClassName.Namespace.ToString().Equals(Namespace, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!Variant.IsEmpty() && !ClassName.Variant.ToString().Equals(Variant, ESearchCase::IgnoreCase))
		{
			continue;
		}
		const FString Dotted = DottedName(ClassName);
		if (SeenNames.Contains(Dotted))
		{
			continue;
		}
		SeenNames.Add(Dotted);
		Matches.Add(ClassName);
		OutCandidates.Add(Dotted);
	}

	if (Matches.Num() == 0)
	{
		// Nothing is NAMED that: list what merely contains it ("Envelope" ->
		// "AD Envelope.AD Envelope.Audio", ...) so the caller can pick a spelling.
		// Standard nodes do not share one namespace ("AD Envelope", "UE", ...), so
		// guessing "UE." is the usual miss.
		for (const FMetasoundFrontendClass& Class : Classes)
		{
			const FMetasoundFrontendClassName& ClassName = Class.Metadata.GetClassName();
			const FString Dotted = DottedName(ClassName);
			if (Class.Metadata.GetType() == EMetasoundFrontendClassType::External &&
				SquashName(ClassName.Name.ToString()).Contains(WantedName, ESearchCase::IgnoreCase) &&
				!SeenNames.Contains(Dotted) && OutCandidates.Num() < 12)
			{
				SeenNames.Add(Dotted);
				OutCandidates.Add(Dotted);
			}
		}
		return false;
	}
	if (Matches.Num() == 1)
	{
		OutClassName = Matches[0];
		return true;
	}
	if (Matches.Num() > 1 && Namespace.IsEmpty())
	{
		int32 EngineMatchIndex = INDEX_NONE;
		int32 EngineMatchCount = 0;
		for (int32 Index = 0; Index < Matches.Num(); ++Index)
		{
			if (Matches[Index].Namespace.ToString().Equals(TEXT("UE"), ESearchCase::IgnoreCase))
			{
				EngineMatchIndex = Index;
				++EngineMatchCount;
			}
		}
		if (EngineMatchCount == 1)
		{
			OutClassName = Matches[EngineMatchIndex];
			return true;
		}
	}
	return false;
}

namespace
{
void ClassPins(const FMetasoundFrontendClass& Class, TArray<FString>& OutInputs, TArray<FString>& OutOutputs)
{
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 6
	const FMetasoundFrontendClassInterface& Interface = Class.GetDefaultInterface();
#else
	const FMetasoundFrontendClassInterface& Interface = Class.Interface;
#endif
	for (const FMetasoundFrontendClassInput& Input : Interface.Inputs)
	{
		OutInputs.Add(FString::Printf(TEXT("%s (%s)"), *Input.Name.ToString(), *Input.TypeName.ToString()));
	}
	for (const FMetasoundFrontendClassOutput& Output : Interface.Outputs)
	{
		OutOutputs.Add(FString::Printf(TEXT("%s (%s)"), *Output.Name.ToString(), *Output.TypeName.ToString()));
	}
}
}

void FindMetaSoundClassPins(const FMetasoundFrontendClassName& ClassName, TArray<FString>& OutInputs, TArray<FString>& OutOutputs)
{
	for (const FMetasoundFrontendClass& Class : Metasound::Frontend::ISearchEngine::Get().FindAllClasses(false))
	{
		if (Class.Metadata.GetType() == EMetasoundFrontendClassType::External && Class.Metadata.GetClassName() == ClassName)
		{
			ClassPins(Class, OutInputs, OutOutputs);
			return;
		}
	}
}
}
#endif

namespace McpAudioAuthoring
{
// list_metasound_nodes: the node classes the registry holds, by part of their name, display name or category, so a
// graph is built from names that exist: a node's class name was a guess until add_node refused it.
TSharedPtr<FJsonObject> ListMetaSoundNodeClasses(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND && MCP_HAS_METASOUND_SEARCH_ENGINE
	const FString Filter = SquashName(GetJsonStringField(Params, TEXT("filter"), TEXT("")));
	const bool bPins = GetJsonBoolField(Params, TEXT("includePins"), false);
	const int32 Limit = FMath::Clamp(GetJsonIntField(Params, TEXT("limit"), 50), 1, 200);
	TMap<FString, TSharedPtr<FJsonObject>> ByName;
	// Where the filter hits, best first: "Trigger" listed 100 classes led by enum conversions, sorted by name alone.
	TMap<FString, int32> Rank;
	for (const FMetasoundFrontendClass& Class : Metasound::Frontend::ISearchEngine::Get().FindAllClasses(false))
	{
		const FMetasoundFrontendClassMetadata& Meta = Class.Metadata;
		const FString Dotted = DottedName(Meta.GetClassName());
		TArray<FString> Parts;
		for (const FText& Part : Meta.GetCategoryHierarchy())
		{
			Parts.Add(Part.ToString());
		}
		const FString Category = FString::Join(Parts, TEXT("/"));
		if (Meta.GetType() != EMetasoundFrontendClassType::External || ByName.Contains(Dotted) ||
			!SquashName(Dotted + TEXT("|") + Meta.GetDisplayName().ToString() + TEXT("|") + Category).Contains(Filter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("nodeClassName"), Dotted);
		Row->SetStringField(TEXT("category"), Category);
		Row->SetStringField(TEXT("description"), Meta.GetDescription().ToString());
		if (bPins)
		{
			TArray<FString> Inputs;
			TArray<FString> Outputs;
			ClassPins(Class, Inputs, Outputs);
			Row->SetArrayField(TEXT("inputs"), McpHandlerUtils::ToJsonStringArray(Inputs));
			Row->SetArrayField(TEXT("outputs"), McpHandlerUtils::ToJsonStringArray(Outputs));
		}
		ByName.Add(Dotted, Row);
		// Within a tier the engine's own UE nodes come first: dozens of Crossfade "Trigger Route" variants hid Trigger Repeat.
		const FString Name = SquashName(Meta.GetClassName().Name.ToString());
		const int32 Tier = Name.Equals(Filter, ESearchCase::IgnoreCase) ? 0 : Name.StartsWith(Filter, ESearchCase::IgnoreCase) ? 1
			: Name.Contains(Filter, ESearchCase::IgnoreCase) ? 2 : 3;
		Rank.Add(Dotted, Tier * 2 + (Meta.GetClassName().Namespace.ToString().Equals(TEXT("UE"), ESearchCase::IgnoreCase) ? 0 : 1));
	}
	TArray<FString> Names;
	ByName.GetKeys(Names);
	Names.Sort([&Rank](const FString& A, const FString& B)
	{
		return Rank[A] != Rank[B] ? Rank[A] < Rank[B] : A.Compare(B, ESearchCase::CaseSensitive) < 0;
	});
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (int32 Index = 0; Index < Names.Num() && Index < Limit; ++Index)
	{
		Rows.Add(MakeShared<FJsonValueObject>(ByName[Names[Index]]));
	}
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("%d node classes match"), Names.Num()));
	Response->SetArrayField(TEXT("nodeClasses"), Rows);
	Response->SetNumberField(TEXT("matchCount"), Names.Num());
	Response->SetBoolField(TEXT("truncated"), Names.Num() > Limit);
	return Response;
#else
	return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_NOT_AVAILABLE"),
		TEXT("Listing MetaSound node classes needs the MetaSound plugin and its node registry search."));
#endif
}
}
