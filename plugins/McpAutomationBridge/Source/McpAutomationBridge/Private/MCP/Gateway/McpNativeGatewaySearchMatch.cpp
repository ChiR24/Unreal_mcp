// McpNativeGatewaySearchMatch.cpp — see header for the cross-surface contract.

#include "MCP/Gateway/McpNativeGatewaySearchMatch.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"

namespace
{
// Ordered highest-signal first; `matchReasons` lists the rules that fired in
// this order regardless of which pass fired them.
enum EMatchRule { RuleIdExact = 0, RuleId, RuleFamily, RuleDomain, RuleTopic, RuleSummary, RuleParent, RuleCount };
struct FMatchRule { const TCHAR* Reason; int32 Weight; };
const FMatchRule MatchRules[RuleCount] = {
	{ TEXT("id-exact"), 100 },
	{ TEXT("id"), 50 },
	{ TEXT("family"), 20 },
	{ TEXT("domain"), 15 },
	{ TEXT("topic"), 12 },
	{ TEXT("summary"), 8 },
	{ TEXT("parent"), 5 },
};

// Closed-class English function words dropped from the QUERY. Same set as
// SEARCH_FUNCTION_WORDS in the TypeScript reference; a grammatical category,
// never catalog vocabulary.
const TCHAR* const FunctionWords[] = {
	TEXT("a"), TEXT("an"), TEXT("the"), TEXT("this"), TEXT("that"), TEXT("these"), TEXT("those"), TEXT("of"), TEXT("in"), TEXT("on"), TEXT("at"),
	TEXT("to"), TEXT("for"), TEXT("from"), TEXT("by"), TEXT("with"), TEXT("into"), TEXT("onto"), TEXT("and"), TEXT("or"), TEXT("but"), TEXT("it"),
	TEXT("its"), TEXT("is"), TEXT("are"), TEXT("be"), TEXT("as"), TEXT("all"), TEXT("every"), TEXT("any"), TEXT("some"), TEXT("my"), TEXT("our"),
	TEXT("their"), TEXT("his"), TEXT("her"), TEXT("you"), TEXT("me"), TEXT("we"), TEXT("i"), TEXT("please"), TEXT("then"), TEXT("so"),
	TEXT("what"), TEXT("which"), TEXT("how"), TEXT("where"), TEXT("who"), TEXT("when"), TEXT("why"), TEXT("do"), TEXT("does"), TEXT("did"),
	TEXT("can"), TEXT("could"), TEXT("should"), TEXT("would"), TEXT("will"), TEXT("want"), TEXT("need"),
};

bool IsAsciiAlnum(TCHAR Ch)
{
	return (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('0') && Ch <= TEXT('9'));
}

bool IsFunctionWord(const FString& Word)
{
	for (const TCHAR* Candidate : FunctionWords)
	{
		if (Word.Equals(Candidate, ESearchCase::CaseSensitive)) return true;
	}
	return false;
}

bool EndsWith(const FString& Word, const TCHAR* Suffix)
{
	int32 SuffixLen = 0;
	while (Suffix[SuffixLen] != 0) ++SuffixLen;
	if (Word.Len() < SuffixLen) return false;
	for (int32 Index = 0; Index < SuffixLen; ++Index)
	{
		if (Word[Word.Len() - SuffixLen + Index] != Suffix[Index]) return false;
	}
	return true;
}

FString LeftPart(const FString& Word, int32 Count)
{
	FString Out;
	for (int32 Index = 0; Index < Count && Index < Word.Len(); ++Index) Out.AppendChar(Word[Index]);
	return Out;
}

// Regular plurals and the two regular verb inflections, first matching rule
// wins (`foldInflection`). Deliberately not a stemmer: every rule is a suffix
// rewrite so both surfaces reproduce it exactly.
FString FoldInflection(const FString& Word)
{
	const int32 Len = Word.Len();
	if (Len > 4 && EndsWith(Word, TEXT("ies"))) return LeftPart(Word, Len - 3) + FString(TEXT("y"));
	if (Len > 4 && (EndsWith(Word, TEXT("ses")) || EndsWith(Word, TEXT("xes")) || EndsWith(Word, TEXT("ches")) || EndsWith(Word, TEXT("shes"))))
	{
		return LeftPart(Word, Len - 2);
	}
	if (Len > 3 && EndsWith(Word, TEXT("s")) && !EndsWith(Word, TEXT("ss"))) return LeftPart(Word, Len - 1);
	if (Len > 5 && EndsWith(Word, TEXT("ing"))) return LeftPart(Word, Len - 3);
	if (Len > 4 && EndsWith(Word, TEXT("ed"))) return LeftPart(Word, Len - 2);
	return Word;
}

bool ContainsWord(const FString& Text, const FString& Word)
{
	TArray<FString> Words;
	McpSearchWords(Text, Words);
	for (const FString& Candidate : Words)
	{
		if (Candidate.Equals(Word, ESearchCase::CaseSensitive)) return true;
	}
	return false;
}

FString ActionSegment(const FString& Id)
{
	int32 Dot = -1;
	return Id.FindLastChar(TEXT('.'), Dot) ? Id.RightChop(Dot + 1) : Id;
}

FString JoinWords(const TArray<FString>& Words)
{
	FString Out;
	for (int32 Index = 0; Index < Words.Num(); ++Index)
	{
		if (Index > 0) Out.AppendChar(TEXT('_'));
		Out.Append(Words[Index]);
	}
	return Out;
}

/** The folded action key of an id or alias: "blueprint.list_blueprint_variables" -> "list_blueprint_variable". */
FString ActionKey(const FString& Id)
{
	TArray<FString> Words;
	McpSearchWords(ActionSegment(Id), Words);
	return JoinWords(Words);
}

bool ActionHasWord(const FMcpCapabilityRecord& Record, const FString& Word)
{
	if (ContainsWord(ActionSegment(Record.Id), Word)) return true;
	for (const FString& Alias : Record.Aliases)
	{
		if (ContainsWord(ActionSegment(Alias), Word)) return true;
	}
	return false;
}

bool ActionEquals(const FMcpCapabilityRecord& Record, const FString& Key)
{
	if (Key.IsEmpty()) return false;
	if (ActionKey(Record.Id).Equals(Key, ESearchCase::CaseSensitive)) return true;
	for (const FString& Alias : Record.Aliases)
	{
		if (ActionKey(Alias).Equals(Key, ESearchCase::CaseSensitive)) return true;
	}
	return false;
}

bool AnyTopicContainsPhrase(const TArray<FString>& Topics, const FString& Query)
{
	for (const FString& Topic : Topics)
	{
		if (Topic.ToLower().Contains(Query, ESearchCase::CaseSensitive)) return true;
	}
	return false;
}

bool AnyTopicContainsWord(const TArray<FString>& Topics, const FString& Word)
{
	for (const FString& Topic : Topics)
	{
		if (ContainsWord(Topic, Word)) return true;
	}
	return false;
}
}

void McpSearchWords(const FString& Text, TArray<FString>& OutWords)
{
	const FString Lower = Text.ToLower();
	FString Current;
	for (int32 Index = 0; Index < Lower.Len(); ++Index)
	{
		const TCHAR Ch = Lower[Index];
		if (IsAsciiAlnum(Ch))
		{
			Current.AppendChar(Ch);
			continue;
		}
		if (!Current.IsEmpty())
		{
			OutWords.Add(FoldInflection(Current));
			Current = FString();
		}
	}
	if (!Current.IsEmpty()) OutWords.Add(FoldInflection(Current));
}

void McpSearchContentWords(const TArray<FString>& AllWords, TArray<FString>& OutContent)
{
	for (const FString& Word : AllWords)
	{
		if (IsFunctionWord(Word)) continue;
		bool bSeen = false;
		for (const FString& Kept : OutContent)
		{
			if (Kept.Equals(Word, ESearchCase::CaseSensitive)) { bSeen = true; break; }
		}
		if (!bSeen) OutContent.Add(Word);
	}
}

bool McpSearchScoreRecord(
	const FMcpCapabilityRecord& Record, const FString& Query,
	const TArray<FString>& AllWords, const TArray<FString>& ContentWords,
	FMcpSearchMatch& Out)
{
	bool Fired[RuleCount] = {};
	int32 Score = 0;
	// Phrase pass: the whole query as an exact id, or as the exact action
	// spelling of the id or of a declared alias, with or without function words.
	if (Record.Id.ToLower().Equals(Query, ESearchCase::CaseSensitive)
		|| ActionEquals(Record, JoinWords(AllWords))
		|| ActionEquals(Record, JoinWords(ContentWords)))
	{
		Fired[RuleIdExact] = true;
		Score += MatchRules[RuleIdExact].Weight;
	}
	// Phrase hits in prose only count for multi-word queries; a single word is
	// already scored by the word pass and must not count twice.
	if (ContentWords.Num() >= 2)
	{
		if (AnyTopicContainsPhrase(Record.Topics, Query))
		{
			Fired[RuleTopic] = true;
			Score += MatchRules[RuleTopic].Weight;
		}
		if (Record.Summary.ToLower().Contains(Query, ESearchCase::CaseSensitive))
		{
			Fired[RuleSummary] = true;
			Score += MatchRules[RuleSummary].Weight;
		}
	}
	// Word pass: whole-word hits per content word. The id rule reads the ACTION
	// segment and the declared aliases; namespace words reach a record only
	// through its domain and parent, at their own weights.
	int32 Matched = 0;
	for (const FString& Word : ContentWords)
	{
		bool Hits[RuleCount] = {};
		Hits[RuleId] = ActionHasWord(Record, Word);
		Hits[RuleFamily] = ContainsWord(Record.Family, Word);
		Hits[RuleDomain] = ContainsWord(Record.Domain, Word);
		Hits[RuleTopic] = AnyTopicContainsWord(Record.Topics, Word);
		Hits[RuleSummary] = ContainsWord(Record.Summary, Word);
		Hits[RuleParent] = ContainsWord(Record.Parent, Word);
		bool bAny = false;
		for (int32 Rule = 0; Rule < RuleCount; ++Rule)
		{
			if (!Hits[Rule]) continue;
			bAny = true;
			Fired[Rule] = true;
			Score += MatchRules[Rule].Weight;
		}
		if (bAny) ++Matched;
	}
	Score += Matched * McpSearchWordCoverageBonus;
	Out.Score = Score;
	Out.Reasons.Empty();
	for (int32 Rule = 0; Rule < RuleCount; ++Rule)
	{
		if (Fired[Rule]) Out.Reasons.Add(MatchRules[Rule].Reason);
	}
	return Out.Reasons.Num() > 0;
}
