// McpCompletionProvider.cpp
// Task 33 (native mirror): the completable-slot table, the bounded enum value
// sets, the safety gate, the deterministic ranking, and the fail-closed
// orchestration. DATA + pure logic only, mirroring
// src/server/mcp-primitives/completions/. No transport, no execution, no editor
// scan, and no secret/destructive/host-path suggestion. Compiled by a later
// combined BuildPlugin (Task 37). The source-contract test asserts TS/native parity.
#include "McpCompletionProvider.h"

#include "Containers/StringConv.h"

const TArray<FMcpCompletionSlot>& McpCompletionSlots()
{
	static const TArray<FMcpCompletionSlot> Slots = {
		{ TEXT("ref/resource"), TEXT("ue://capability/{capabilityId}"), TEXT("capabilityId"), TEXT("capability"), true },
		{ TEXT("ref/resource"), TEXT("ue://knowledge/{engineVersion}/{topic}"), TEXT("engineVersion"), TEXT("enum"), false },
		{ TEXT("ref/resource"), TEXT("ue://knowledge/{engineVersion}/{topic}"), TEXT("topic"), TEXT("enum"), false },
		{ TEXT("ref/resource"), TEXT("ue://object/{objectPath}"), TEXT("objectPath"), TEXT("project-handle"), false },
		{ TEXT("ref/resource"), TEXT("ue://asset/{assetPath}"), TEXT("assetPath"), TEXT("project-handle"), false },
		{ TEXT("ref/prompt"), TEXT("asset-import"), TEXT("sourceFormat"), TEXT("enum"), false },
		{ TEXT("ref/prompt"), TEXT("sequence-render"), TEXT("outputFormat"), TEXT("enum"), false },
	};
	return Slots;
}

const FMcpCompletionSlot* McpResolveCompletionSlot(const FString& RefType, const FString& RefId, const FString& ArgumentName)
{
	for (const FMcpCompletionSlot& Slot : McpCompletionSlots())
	{
		if (Slot.RefType == RefType && Slot.RefId == RefId && Slot.ArgumentName == ArgumentName)
		{
			return &Slot;
		}
	}
	return nullptr;
}

TArray<FString> McpCompletionEnumValues(const FMcpCompletionSlot& Slot)
{
	if (Slot.RefType == TEXT("ref/resource") && Slot.ArgumentName == TEXT("engineVersion"))
	{
		return { TEXT("5.0"), TEXT("5.1"), TEXT("5.2"), TEXT("5.3"), TEXT("5.4"), TEXT("5.5"), TEXT("5.6"), TEXT("5.7"), TEXT("5.8") };
	}
	if (Slot.RefType == TEXT("ref/resource") && Slot.ArgumentName == TEXT("topic"))
	{
		return { TEXT("overview"), TEXT("assets"), TEXT("actors"), TEXT("blueprints"), TEXT("levels"), TEXT("sequencer"), TEXT("niagara"), TEXT("physics") };
	}
	if (Slot.RefType == TEXT("ref/prompt") && Slot.RefId == TEXT("asset-import") && Slot.ArgumentName == TEXT("sourceFormat"))
	{
		return { TEXT("fbx"), TEXT("obj"), TEXT("gltf"), TEXT("png"), TEXT("wav") };
	}
	if (Slot.RefType == TEXT("ref/prompt") && Slot.RefId == TEXT("sequence-render") && Slot.ArgumentName == TEXT("outputFormat"))
	{
		return { TEXT("png"), TEXT("jpeg"), TEXT("exr"), TEXT("custom") };
	}
	return {};
}

namespace McpCompletionInternal
{
	bool NameMatchesAny(const FString& Lower, const TArray<FString>& Fragments)
	{
		for (const FString& Fragment : Fragments)
		{
			if (Lower.Contains(Fragment)) return true;
		}
		return false;
	}

	bool HostPathLike(const FString& Value)
	{
		if (Value.Len() >= 3 && FChar::IsAlpha(Value[0]) && Value[1] == TEXT(':') && (Value[2] == TEXT('\\') || Value[2] == TEXT('/')))
		{
			return true;
		}
		if (Value.Contains(TEXT("\\")) || Value.StartsWith(TEXT("~")))
		{
			return true;
		}
		static const TArray<FString> Roots = {
			TEXT("/home"), TEXT("/users"), TEXT("/etc"), TEXT("/var"),
			TEXT("/root"), TEXT("/tmp"), TEXT("/bin"), TEXT("/opt"), TEXT("/usr"),
		};
		const FString Lower = Value.ToLower();
		for (const FString& Root : Roots)
		{
			if (Lower.StartsWith(Root) && (Lower.Len() == Root.Len() || !FChar::IsAlnum(Lower[Root.Len()])))
			{
				return true;
			}
		}
		return false;
	}

	bool HasTraversal(const FString& Value)
	{
		FString Normalized = Value.Replace(TEXT("\\"), TEXT("/"));
		TArray<FString> Parts;
		Normalized.ParseIntoArray(Parts, TEXT("/"), false);
		return Parts.Contains(TEXT(".."));
	}

	int32 Utf8Bytes(const FString& Value)
	{
		FTCHARToUTF8 Converter(*Value);
		return Converter.Length();
	}

	// Mirror withinOneEdit: true when A is within edit distance 1 of B.
	bool WithinOneEdit(const FString& A, const FString& B)
	{
		if (A == B) return true;
		const int32 La = A.Len();
		const int32 Lb = B.Len();
		if (FMath::Abs(La - Lb) > 1) return false;
		int32 i = 0;
		int32 j = 0;
		int32 Edits = 0;
		while (i < La && j < Lb)
		{
			if (A[i] == B[j]) { ++i; ++j; continue; }
			if (++Edits > 1) return false;
			if (La > Lb) ++i;
			else if (Lb > La) ++j;
			else { ++i; ++j; }
		}
		if (i < La || j < Lb) ++Edits;
		return Edits <= 1;
	}

	bool IsSubsequence(const FString& Needle, const FString& Haystack)
	{
		int32 n = 0;
		for (int32 h = 0; h < Haystack.Len() && n < Needle.Len(); ++h)
		{
			if (Haystack[h] == Needle[n]) ++n;
		}
		return n == Needle.Len();
	}

	constexpr int32 TierExactPrefix = 0;
	constexpr int32 TierSubstring = 1;
	constexpr int32 TierSubsequence = 2;
	constexpr int32 TierTypo = 3;
	constexpr int32 TierNone = 99;

	int32 TierFor(const FString& Value, const FString& Prefix)
	{
		if (Prefix.IsEmpty() || Value.StartsWith(Prefix)) return TierExactPrefix;
		if (Value.Contains(Prefix)) return TierSubstring;
		if (IsSubsequence(Prefix, Value)) return TierSubsequence;
		const FString Head = Value.Left(FMath::Min(Prefix.Len() + 1, Value.Len()));
		if (WithinOneEdit(Prefix, Value.Left(FMath::Min(Prefix.Len(), Value.Len()))) || WithinOneEdit(Prefix, Head)) return TierTypo;
		return TierNone;
	}
}

FString McpClassifyUnsafeCompletion(const FString& ArgumentName, const FString& Value)
{
	using namespace McpCompletionInternal;
	const FString Lower = ArgumentName.ToLower();
	static const TArray<FString> SecretFragments = {
		TEXT("token"), TEXT("secret"), TEXT("password"), TEXT("passwd"),
		TEXT("apikey"), TEXT("api_key"), TEXT("credential"),
		TEXT("privatekey"), TEXT("private_key"), TEXT("bearer"), TEXT("auth"),
	};
	static const TArray<FString> DestructiveFragments = {
		TEXT("confirm"), TEXT("force"), TEXT("overwrite"), TEXT("purge"), TEXT("wipe"), TEXT("destroy"),
	};
	if (NameMatchesAny(Lower, SecretFragments)) return McpCompletionGuidance::SecretField;
	if (NameMatchesAny(Lower, DestructiveFragments)) return McpCompletionGuidance::DestructiveField;
	if (HostPathLike(Value) || HasTraversal(Value)) return McpCompletionGuidance::UnboundedPath;
	return FString();
}

TArray<FMcpCompletionCandidate> McpRankCompletionCandidates(const TArray<FMcpCompletionCandidate>& Pool, const FString& Prefix)
{
	using namespace McpCompletionInternal;
	const FString Lowered = Prefix.ToLower();
	struct FScored { FMcpCompletionCandidate Candidate; int32 Tier; };
	TArray<FScored> Scored;
	bool bHasStrongMatch = false;
	for (const FMcpCompletionCandidate& Candidate : Pool)
	{
		const int32 Tier = TierFor(Candidate.Value.ToLower(), Lowered);
		if (Tier == TierNone) continue;
		if (Tier < TierTypo) bHasStrongMatch = true;
		Scored.Add({ Candidate, Tier });
	}
	// Typos are a fallback: keep them only when no prefix/substring/subsequence matched.
	if (bHasStrongMatch)
	{
		Scored.RemoveAll([](const FScored& Entry) { return Entry.Tier >= TierTypo; });
	}
	Scored.Sort([](const FScored& A, const FScored& B)
	{
		if (A.Tier != B.Tier) return A.Tier < B.Tier;
		return A.Candidate.Value.Compare(B.Candidate.Value) < 0;
	});
	TArray<FMcpCompletionCandidate> Ranked;
	Ranked.Reserve(Scored.Num());
	for (const FScored& Entry : Scored) Ranked.Add(Entry.Candidate);
	return Ranked;
}

FMcpCompletionResult McpApplyCompletionBudget(const TArray<FMcpCompletionCandidate>& Ranked)
{
	using namespace McpCompletionInternal;
	FMcpCompletionResult Result;
	int32 Bytes = 0;
	bool bTruncated = false;
	for (const FMcpCompletionCandidate& Candidate : Ranked)
	{
		if (Result.Values.Num() >= McpMaxCompletionItems) { bTruncated = true; break; }
		const int32 Size = Utf8Bytes(Candidate.Value);
		if (Result.Values.Num() > 0 && Bytes + Size > McpMaxCompletionBytes) { bTruncated = true; break; }
		Result.Values.Add(Candidate.Value);
		Bytes += Size;
	}
	Result.Total = Ranked.Num();
	Result.bHasMore = bTruncated || Result.Values.Num() < Ranked.Num();
	return Result;
}

namespace
{
	FMcpCompletionOutcome CompletionSafeEmpty(const FString& GuidanceCode)
	{
		FMcpCompletionOutcome Outcome;
		Outcome.GuidanceCode = GuidanceCode;
		return Outcome;
	}
}

FMcpCompletionOutcome McpCompleteFromPool(
	const FString& RefType,
	const FString& RefId,
	const FString& ArgumentName,
	const FString& Value,
	const TArray<FMcpCompletionCandidate>& CapabilityPool,
	const TArray<FMcpCompletionCandidate>& ProjectHandlePool,
	const TSet<FString>& EnabledCapabilityIds)
{
	if (Value.Len() > McpMaxCompletionPrefixLength) return CompletionSafeEmpty(McpCompletionGuidance::UnboundedPrefix);

	const FString Unsafe = McpClassifyUnsafeCompletion(ArgumentName, Value);
	if (!Unsafe.IsEmpty()) return CompletionSafeEmpty(Unsafe);

	const FMcpCompletionSlot* Slot = McpResolveCompletionSlot(RefType, RefId, ArgumentName);
	if (Slot == nullptr) return CompletionSafeEmpty(McpCompletionGuidance::Unavailable);

	TArray<FMcpCompletionCandidate> Pool;
	if (Slot->Kind == TEXT("capability")) Pool = CapabilityPool;
	else if (Slot->Kind == TEXT("project-handle")) Pool = ProjectHandlePool;
	else
	{
		for (const FString& EnumValue : McpCompletionEnumValues(*Slot))
		{
			Pool.Add({ EnumValue, TEXT("enum"), FString() });
		}
	}

	if (Slot->bCapabilityScoped)
	{
		Pool = Pool.FilterByPredicate([&EnabledCapabilityIds](const FMcpCompletionCandidate& Candidate)
		{
			return Candidate.CapabilityId.IsEmpty() || EnabledCapabilityIds.Contains(Candidate.CapabilityId);
		});
	}

	const TArray<FMcpCompletionCandidate> Ranked = McpRankCompletionCandidates(Pool, Value);
	if (Ranked.Num() == 0) return CompletionSafeEmpty(McpCompletionGuidance::NoMatch);

	FMcpCompletionOutcome Outcome;
	Outcome.Result = McpApplyCompletionBudget(Ranked);
	return Outcome;
}
