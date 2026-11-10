// What a search request asks for beyond its words (see the header).

#include "MCP/Gateway/Search/McpSearchIntent.h"

#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "MCP/Gateway/McpNativeGatewaySearchMatch.h"

namespace
{
// Words that open a request to READ something, matched against the first word as
// typed (before folding); the eval ranker (tests/eval/native-ranker.ts) reads this
// list from here. Verbs that also open changes (view, look, see, check, print) are
// left out, and so is "when", which opens event logic ("when the player overlaps, ...").
const TCHAR* const ReadIntentWords[] = {
    TEXT("get"), TEXT("read"), TEXT("list"), TEXT("inspect"), TEXT("query"), TEXT("describe"), TEXT("find"), TEXT("count"),
    TEXT("show"), TEXT("what"), TEXT("which"), TEXT("where"), TEXT("who"), TEXT("why"), TEXT("how"), TEXT("is"), TEXT("does"),
};

// Words that open a request to DELETE something (RETRIEVAL_DELETE_INTENT_WORDS):
// "delete the spawned actor" used to rank spawn first, since "spawned" folds to spawn.
const TCHAR* const DeleteIntentWords[] = { TEXT("delete"), TEXT("remove"), TEXT("destroy"), TEXT("erase") };

void AddWholeAction(const FString& Name, TSet<FString>& OutActions)
{
    TArray<FString> Words;
    McpSearchWords(McpLastDottedSegment(Name), Words);
    if (Words.Num() == 1)
    {
        OutActions.Add(Words[0]);
    }
}

bool HoldsLeadAndAnother(const FString& Text, const TArray<FString>& ContentWords)
{
    TArray<FString> Words;
    McpSearchWords(Text, Words);
    if (!Words.Contains(ContentWords[0]))
    {
        return false;
    }
    for (int32 Index = 1; Index < ContentWords.Num(); ++Index)
    {
        if (Words.Contains(ContentWords[Index]))
        {
            return true;
        }
    }
    return false;
}

// Whether the request holds a word that sets the alias apart: one that is neither generic, nor a function word, nor
// the verb of the record's own action. An alias with no such word is always asked for.
bool AliasAsked(const TArray<FString>& AliasWords, const TArray<FString>& Own, const TArray<FString>& ContentWords, const TSet<FString>& GenericWords)
{
    TArray<FString> Apart;
    McpSearchContentWords(AliasWords, Apart);
    bool bSetApart = false;
    for (const FString& Word : Apart)
    {
        if (GenericWords.Contains(Word) || (Own.Num() > 0 && Word == Own[0]))
        {
            continue;
        }
        if (ContentWords.Contains(Word))
        {
            return true;
        }
        bSetApart = true;
    }
    return !bSetApart;
}
} // namespace

FString McpSearchIntendedEffect(const FString& LowerQuery)
{
    FString First;
    for (const TCHAR Ch : LowerQuery)
    {
        if (Ch < 128 && FChar::IsAlnum(Ch))
        {
            First.AppendChar(Ch);
        }
        else if (!First.IsEmpty())
        {
            break;
        }
    }
    for (const TCHAR* Word : ReadIntentWords)
    {
        if (First.Equals(Word, ESearchCase::CaseSensitive))
        {
            return TEXT("read");
        }
    }
    for (const TCHAR* Word : DeleteIntentWords)
    {
        if (First.Equals(Word, ESearchCase::CaseSensitive))
        {
            return TEXT("destructive");
        }
    }
    return FString();
}

bool McpSearchIsWholeAction(const FString& Word, const FMcpCapabilityStore& Store)
{
    // Built once, as the typo vocabulary is: the catalog is compiled into the plugin.
    static const TSet<FString> Actions = [&Store]()
    {
        TSet<FString> Out;
        for (const FMcpCapabilityRecord& Record : Store.GetRecords())
        {
            AddWholeAction(Record.Id, Out);
            for (const FString& Alias : Record.Aliases)
            {
                AddWholeAction(Alias, Out);
            }
        }
        return Out;
    }();
    return Actions.Contains(Word);
}

const TSet<FString>& McpSearchGenericWords(const FMcpCapabilityStore& Store)
{
    // Built once, as the whole actions are: the catalog is compiled into the plugin.
    static const TSet<FString> Generic = [&Store]()
    {
        TMap<FString, TSet<FString>> Tools;
        for (const FMcpCapabilityRecord& Record : Store.GetRecords())
        {
            TArray<FString> Words;
            McpSearchWords(McpLastDottedSegment(Record.Id), Words);
            for (const FString& Alias : Record.Aliases)
            {
                McpSearchWords(McpLastDottedSegment(Alias), Words);
            }
            for (const FString& Word : Words)
            {
                Tools.FindOrAdd(Word).Add(Record.Parent);
            }
        }
        TSet<FString> Out;
        for (const TPair<FString, TSet<FString>>& Entry : Tools)
        {
            if (Entry.Value.Num() >= McpSearchGenericWordTools) Out.Add(Entry.Key);
        }
        return Out;
    }();
    return Generic;
}

bool McpSearchLeadTied(const FMcpCapabilityRecord& Record, const TArray<FString>& ContentWords)
{
    if (ContentWords.Num() < 2 || HoldsLeadAndAnother(McpLastDottedSegment(Record.Id), ContentWords))
    {
        return true;
    }
    for (const FString& Alias : Record.Aliases)
    {
        if (HoldsLeadAndAnother(McpLastDottedSegment(Alias), ContentWords))
        {
            return true;
        }
    }
    for (const FString& Topic : Record.Topics)
    {
        if (HoldsLeadAndAnother(Topic, ContentWords))
        {
            return true;
        }
    }
    return false;
}

void McpSearchActionWords(const FMcpCapabilityRecord& Record, const TArray<FString>& ContentWords, bool bLeadIsAction,
    const TSet<FString>& GenericWords, TArray<FString>& OutHeld, TArray<FString>& OutScored)
{
    TArray<FString> Own;
    McpSearchWords(McpLastDottedSegment(Record.Id), Own);
    TArray<TArray<FString>> Aliases;
    for (const FString& Alias : Record.Aliases)
    {
        McpSearchWords(McpLastDottedSegment(Alias), Aliases.AddDefaulted_GetRef());
    }
    TArray<FString> Subject;
    McpSearchWords(Record.Domain, Subject);
    McpSearchWords(Record.Family, Subject);
    for (const FString& Word : ContentWords)
    {
        const bool bOwn = Own.Contains(Word);
        const bool bAlias = Aliases.ContainsByPredicate([&Word](const TArray<FString>& Words) { return Words.Contains(Word); });
        if (bOwn || bAlias) OutHeld.Add(Word);
        if (bOwn || (bAlias && Subject.Contains(Word))) OutScored.Add(Word);
    }
    // The first alias that adds the most: a tie changes which words score, never how many. In a request that opens
    // with an action verb, an alias the request does not ask for adds only its own verb, its first word.
    TArray<FString> Best;
    for (const TArray<FString>& Words : Aliases)
    {
        const bool bAsked = !bLeadIsAction || AliasAsked(Words, Own, ContentWords, GenericWords);
        TArray<FString> Added;
        for (const FString& Word : ContentWords)
        {
            if (Words.Contains(Word) && !OutScored.Contains(Word) && (bAsked || Word == Words[0])) Added.Add(Word);
        }
        if (Added.Num() > Best.Num()) Best = MoveTemp(Added);
    }
    OutScored.Append(Best);
}
