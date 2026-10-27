#include "MCP/Primitives/McpCompletions.h"
#include "MCP/Primitives/McpPromptCatalog.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Resources/McpResourceUri.h"
#include "Containers/StringConv.h"

// Named, not anonymous: unity builds merge neighbouring files.
namespace McpCompletionsInternal
{
	constexpr int32 MaxItems = 100;
	constexpr int32 MaxBytes = 8192;
	constexpr int32 MaxPrefix = 128;

	bool LooksLikeHostPathOrTraversal(const FString& Value)
	{
		if (Value.Contains(TEXT("\\")) || Value.StartsWith(TEXT("~"))) return true;
		if (Value.Len() >= 3 && FChar::IsAlpha(Value[0]) && Value[1] == TEXT(':') && Value[2] == TEXT('/')) return true;
		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, TEXT("/"), false);
		if (Parts.Contains(TEXT(".."))) return true;
		const FString Lower = Value.ToLower();
		for (const TCHAR* Root : { TEXT("/home"), TEXT("/users"), TEXT("/etc"), TEXT("/var"), TEXT("/root"),
			TEXT("/tmp"), TEXT("/bin"), TEXT("/opt"), TEXT("/usr") })
		{
			const int32 Len = FCString::Strlen(Root);
			if (Lower.StartsWith(Root) && (Lower.Len() == Len || !FChar::IsAlnum(Lower[Len]))) return true;
		}
		return false;
	}

	TArray<FString> CapabilityCandidates(TFunctionRef<bool(const FString&)> IsParentEnabled)
	{
		TArray<FString> Out;
		for (const FMcpCapabilityRecord& Record : FMcpCapabilityStore::Get().GetRecords())
		{
			if (!IsParentEnabled(Record.Parent)) continue;
			Out.AddUnique(Record.Id);
			for (const FString& Alias : Record.Aliases) Out.AddUnique(Alias);
			for (const FMcpLegacyPair& Pair : Record.LegacyPairs) Out.AddUnique(Pair.Tool + TEXT(".") + Pair.Action);
		}
		return Out;
	}

	// Returns false when (ref, argument) is not a completable slot.
	bool CandidatesFor(const FString& RefType, const FString& RefId, const FString& Argument,
		TFunctionRef<bool(const FString&)> IsParentEnabled, TArray<FString>& Out)
	{
		if (RefType == TEXT("ref/prompt"))
		{
			for (const FMcpWorkflowPrompt& Prompt : McpWorkflowPrompts())
			{
				if (Prompt.Id != RefId) continue;
				for (const FMcpPromptArgumentSpec& Spec : Prompt.Arguments)
				{
					if (Spec.Name == Argument && Spec.Allowed.Num() > 0) { Out = Spec.Allowed; return true; }
				}
			}
			return false;
		}
		if (RefId == TEXT("ue://capability/{capabilityId}") && Argument == TEXT("capabilityId")) { Out = CapabilityCandidates(IsParentEnabled); return true; }
		if (RefId == TEXT("ue://knowledge/{topic}") && Argument == TEXT("topic"))
		{
			Out = { TEXT("gateway"), TEXT("paths"), TEXT("resources"), TEXT("safety"), TEXT("transports") };
			return true;
		}
		if (RefId == TEXT("ue://asset/{assetPath}") && Argument == TEXT("assetPath")) { Out = McpResourceUri::ContentRoots(); return true; }
		return false;
	}

	bool WithinOneEdit(const FString& A, const FString& B)
	{
		if (FMath::Abs(A.Len() - B.Len()) > 1) return false;
		int32 i = 0, j = 0, Edits = 0;
		while (i < A.Len() && j < B.Len())
		{
			if (A[i] == B[j]) { ++i; ++j; continue; }
			if (++Edits > 1) return false;
			if (A.Len() > B.Len()) ++i;
			else if (B.Len() > A.Len()) ++j;
			else { ++i; ++j; }
		}
		return Edits + ((i < A.Len() || j < B.Len()) ? 1 : 0) <= 1;
	}

	bool IsSubsequence(const FString& Needle, const FString& Haystack)
	{
		int32 n = 0;
		for (int32 h = 0; h < Haystack.Len() && n < Needle.Len(); ++h) if (Haystack[h] == Needle[n]) ++n;
		return n == Needle.Len();
	}

	// 0 prefix, 1 substring, 2 subsequence, 3 one typo, INDEX_NONE no match.
	int32 TierFor(const FString& Value, const FString& Prefix)
	{
		if (Prefix.IsEmpty() || Value.StartsWith(Prefix)) return 0;
		if (Value.Contains(Prefix)) return 1;
		if (IsSubsequence(Prefix, Value)) return 2;
		if (WithinOneEdit(Prefix, Value.Left(Prefix.Len())) || WithinOneEdit(Prefix, Value.Left(Prefix.Len() + 1))) return 3;
		return INDEX_NONE;
	}
}

FMcpCompletionResult McpComplete(
	const FString& RefType, const FString& RefId, const FString& ArgumentName, const FString& Prefix,
	TFunctionRef<bool(const FString&)> IsParentEnabled)
{
	using namespace McpCompletionsInternal;
	FMcpCompletionResult Result;
	TArray<FString> Pool;
	if (Prefix.Len() > MaxPrefix || LooksLikeHostPathOrTraversal(Prefix)
		|| !CandidatesFor(RefType, RefId, ArgumentName, IsParentEnabled, Pool))
	{
		return Result;
	}

	const FString Lowered = Prefix.ToLower();
	TArray<TPair<int32, FString>> Scored;
	for (const FString& Value : Pool)
	{
		const int32 Tier = TierFor(Value.ToLower(), Lowered);
		if (Tier != INDEX_NONE) Scored.Add({ Tier, Value });
	}
	// Typo matches only count when nothing better matched.
	if (Scored.ContainsByPredicate([](const TPair<int32, FString>& Entry) { return Entry.Key < 3; }))
	{
		Scored.RemoveAll([](const TPair<int32, FString>& Entry) { return Entry.Key >= 3; });
	}
	Scored.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B)
	{
		return A.Key != B.Key ? A.Key < B.Key : A.Value.Compare(B.Value) < 0;
	});

	int32 Bytes = 0;
	for (const TPair<int32, FString>& Entry : Scored)
	{
		const int32 Size = FTCHARToUTF8(*Entry.Value).Length();
		if (Result.Values.Num() >= MaxItems || (Result.Values.Num() > 0 && Bytes + Size > MaxBytes)) break;
		Result.Values.Add(Entry.Value);
		Bytes += Size;
	}
	Result.Total = Scored.Num();
	Result.bHasMore = Result.Values.Num() < Scored.Num();
	return Result;
}
