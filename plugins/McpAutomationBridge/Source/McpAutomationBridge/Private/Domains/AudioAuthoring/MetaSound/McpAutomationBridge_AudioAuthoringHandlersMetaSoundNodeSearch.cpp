#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR && MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND && MCP_HAS_METASOUND_SEARCH_ENGINE
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
	const FString Variant = ClassName.Variant.ToString();
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
}
#endif
