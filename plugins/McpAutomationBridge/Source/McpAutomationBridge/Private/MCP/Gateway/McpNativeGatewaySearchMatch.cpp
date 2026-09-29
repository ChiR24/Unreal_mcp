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

// Words that open a request to READ something, matched against the first word as
// typed (before folding). Same list as RETRIEVAL_READ_INTENT_WORDS in the
// TypeScript retrieval constants; verbs that also open changes (view, look, see,
// check, print) are left out.
const TCHAR* const ReadIntentWords[] = {
	TEXT("get"), TEXT("read"), TEXT("list"), TEXT("inspect"), TEXT("query"), TEXT("describe"), TEXT("find"), TEXT("count"),
	TEXT("show"), TEXT("what"), TEXT("which"), TEXT("where"), TEXT("who"), TEXT("how"), TEXT("is"), TEXT("does"),
};

bool IsAsciiAlnum(TCHAR Ch)
{
	return (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('0') && Ch <= TEXT('9'));
}

bool OpensWithReadWord(const FString& LowerQuery)
{
	FString First;
	for (const TCHAR Ch : LowerQuery)
	{
		if (!IsAsciiAlnum(Ch))
		{
			if (!First.IsEmpty()) break;
			continue;
		}
		First.AppendChar(Ch);
	}
	for (const TCHAR* Word : ReadIntentWords)
	{
		if (First.Equals(Word, ESearchCase::CaseSensitive)) return true;
	}
	return false;
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
	return Word.EndsWith(Suffix, ESearchCase::CaseSensitive);
}

// Regular plurals and the two regular verb inflections, first matching rule
// wins (`foldInflection`). Deliberately not a stemmer: every rule is a suffix
// rewrite so both surfaces reproduce it exactly.
FString FoldInflection(const FString& Word)
{
	const int32 Len = Word.Len();
	if (Len > 4 && EndsWith(Word, TEXT("ies"))) return Word.Left(Len - 3) + FString(TEXT("y"));
	if (Len > 4 && (EndsWith(Word, TEXT("ses")) || EndsWith(Word, TEXT("xes")) || EndsWith(Word, TEXT("ches")) || EndsWith(Word, TEXT("shes"))))
	{
		return Word.Left(Len - 2);
	}
	if (Len > 3 && EndsWith(Word, TEXT("s")) && !EndsWith(Word, TEXT("ss"))) return Word.Left(Len - 1);
	if (Len > 5 && EndsWith(Word, TEXT("ing"))) return Word.Left(Len - 3);
	if (Len > 4 && EndsWith(Word, TEXT("ed"))) return Word.Left(Len - 2);
	return Word;
}

// Verbs a caller uses for "delete", folded on query AND record words alike, so
// "remove node" finds delete_node while remove_* actions still match "remove".
FString FoldSynonym(const FString& Word)
{
	return Word == TEXT("remove") || Word == TEXT("destroy") || Word == TEXT("erase") ? FString(TEXT("delete")) : Word;
}

/** " a b c ": words padded so a run matches whole words only. */
FString SpacedRun(const TArray<FString>& Words)
{
	return FString(TEXT(" ")) + FString::Join(Words, TEXT(" ")) + TEXT(" ");
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

FString JoinWords(const TArray<FString>& Words)
{
	return FString::Join(Words, TEXT("_"));
}

/** The folded action key of an id or alias: "blueprint.list_blueprint_variables" -> "list_blueprint_variable". */
FString ActionKey(const FString& Id)
{
	TArray<FString> Words;
	McpSearchWords(McpLastDottedSegment(Id), Words);
	return JoinWords(Words);
}

bool ActionHasWord(const FMcpCapabilityRecord& Record, const FString& Word)
{
	if (ContainsWord(McpLastDottedSegment(Record.Id), Word)) return true;
	for (const FString& Alias : Record.Aliases)
	{
		if (ContainsWord(McpLastDottedSegment(Alias), Word)) return true;
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
			OutWords.Add(FoldSynonym(FoldInflection(Current)));
			Current = FString();
		}
	}
	if (!Current.IsEmpty()) OutWords.Add(FoldSynonym(FoldInflection(Current)));
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
	TArray<FString> OwnAction;
	McpSearchWords(McpLastDottedSegment(Record.Id), OwnAction);
	bool bOwnCovered = OwnAction.Num() >= 2;
	for (const FString& Word : OwnAction)
	{
		bOwnCovered = bOwnCovered && ContentWords.Contains(Word);
	}
	// An alias counts only as a contiguous run of the query ("remove tag from
	// actor" names remove_tag), so scattered alias words never qualify.
	const FString QueryRun = SpacedRun(ContentWords);
	bool bAliasRun = false;
	for (const FString& Alias : Record.Aliases)
	{
		TArray<FString> AliasWords;
		McpSearchWords(McpLastDottedSegment(Alias), AliasWords);
		bAliasRun = bAliasRun || (AliasWords.Num() >= 2 && QueryRun.Contains(SpacedRun(AliasWords), ESearchCase::CaseSensitive));
	}
	if (Matched == ContentWords.Num() && (bOwnCovered || bAliasRun)) Score += McpSearchActionCoveredBonus;
	Out.Reasons.Empty();
	for (int32 Rule = 0; Rule < RuleCount; ++Rule)
	{
		if (Fired[Rule]) Out.Reasons.Add(MatchRules[Rule].Reason);
	}
	// Reorders matches only: a record no rule fired for is still not a result.
	if (Out.Reasons.Num() > 0 && Record.Effect.Equals(TEXT("read"), ESearchCase::CaseSensitive) && OpensWithReadWord(Query))
	{
		Score += McpSearchReadIntentBonus;
	}
	Out.Score = Score;
	return Out.Reasons.Num() > 0;
}
