// What a search request asks for beyond its words (see the header).

#include "MCP/Gateway/Search/McpSearchIntent.h"

#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
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
