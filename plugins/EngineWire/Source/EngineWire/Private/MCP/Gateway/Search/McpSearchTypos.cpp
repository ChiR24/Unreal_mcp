// Typo correction for search (see the header). Built once (the catalog is compiled into the plugin): every folded word a
// record can be matched by, and among them the names a correction may land on, from the fields that name what a record
// does (action words, aliases, topics, family, domain, parent; not summary prose), counted by how many records carry them.

#include "MCP/Gateway/Search/McpSearchTypos.h"

#include "Dom/JsonValue.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewaySearchMatch.h"

namespace
{
constexpr int32 McpTypoSwapLength = 4; // shortest word corrected, and then only by a swap
constexpr int32 McpTypoMinLength = 5;  // shortest word any one edit corrects
constexpr int32 McpTypoLongLength = 8; // shortest word two edits correct

// A slip onto a neighbouring key costs one edit; any other substitution two, which is where real words meet ("carve" is
// one letter from "curve"). Keys by row and column, each row half a key right of the one above.
bool AreNeighbourKeys(TCHAR A, TCHAR B)
{
    static const TCHAR* const Rows[] = {TEXT("qwertyuiop"), TEXT("asdfghjkl"), TEXT("zxcvbnm")};
    double Where[2][2] = {{-9.0, -9.0}, {-9.0, -9.0}};
    for (int32 Row = 0; Row < 3; ++Row)
    {
        for (int32 Col = 0; Rows[Row][Col]; ++Col)
        {
            for (int32 Key = 0; Key < 2; ++Key)
            {
                if (Rows[Row][Col] == (Key == 0 ? A : B))
                {
                    Where[Key][0] = Row;
                    Where[Key][1] = Col + 0.5 * Row;
                }
            }
        }
    }
    return Where[0][0] >= 0.0 && Where[1][0] >= 0.0 && FMath::Abs(Where[0][0] - Where[1][0]) <= 1.0 &&
        FMath::Abs(Where[0][1] - Where[1][1]) <= 1.0;
}

struct FMcpSearchVocabulary
{
    TSet<FString> Known;
    TMap<FString, int32> Names;
};

void AddWords(const FString& Text, TSet<FString>& OutWords)
{
    TArray<FString> Words;
    McpSearchWords(Text, Words);
    OutWords.Append(Words);
}

const FMcpSearchVocabulary& Vocabulary(const FMcpCapabilityStore& Store)
{
    static const FMcpSearchVocabulary Built = [&Store]()
    {
        FMcpSearchVocabulary Out;
        for (const FMcpCapabilityRecord& Record : Store.GetRecords())
        {
            TSet<FString> Names;
            AddWords(McpLastDottedSegment(Record.Id), Names);
            for (const FString& Alias : Record.Aliases)
            {
                AddWords(McpLastDottedSegment(Alias), Names);
            }
            for (const FString& Topic : Record.Topics)
            {
                AddWords(Topic, Names);
            }
            AddWords(Record.Family, Names);
            AddWords(Record.Domain, Names);
            AddWords(Record.Parent, Names);
            for (const FString& Word : Names)
            {
                ++Out.Names.FindOrAdd(Word);
            }
            Out.Known.Append(Names);
            AddWords(Record.Summary, Out.Known);
        }
        return Out;
    }();
    return Built;
}

bool HasDigit(const FString& Word)
{
    for (const TCHAR Ch : Word)
    {
        if (Ch >= TEXT('0') && Ch <= TEXT('9'))
        {
            return true;
        }
    }
    return false;
}

// Whether B is A with two letters swapped (the only fix a 4-letter word gets: "lsit").
bool IsSwapOf(const FString& A, const FString& B)
{
    if (A.Len() != B.Len())
    {
        return false;
    }
    TArray<int32> Differ;
    for (int32 Index = 0; Index < A.Len() && Differ.Num() <= 2; ++Index)
    {
        if (A[Index] != B[Index])
        {
            Differ.Add(Index);
        }
    }
    return Differ.Num() == 2 && A[Differ[0]] == B[Differ[1]] && A[Differ[1]] == B[Differ[0]];
}

// The name nearest Folded within its edit budget, or empty: fewest edits, then the name more records carry, then byte order.
FString NearestName(const FString& Folded, const TMap<FString, int32>& Names)
{
    const int32 Limit = Folded.Len() >= McpTypoLongLength ? 2 : 1;
    FString Best;
    int32 BestDistance = Limit + 1;
    int32 BestCount = 0;
    for (const TPair<FString, int32>& Entry : Names)
    {
        const int32 Distance = McpEditDistance(Folded, Entry.Key, Limit);
        if (Distance > Limit || (Folded.Len() < McpTypoMinLength && !IsSwapOf(Folded, Entry.Key)))
        {
            continue;
        }
        if (Distance < BestDistance || (Distance == BestDistance && (Entry.Value > BestCount ||
            (Entry.Value == BestCount && Entry.Key.Compare(Best, ESearchCase::CaseSensitive) < 0))))
        {
            Best = Entry.Key;
            BestDistance = Distance;
            BestCount = Entry.Value;
        }
    }
    return Best;
}
} // namespace

int32 McpEditDistance(const FString& A, const FString& B, int32 Limit)
{
    const int32 N = A.Len();
    const int32 M = B.Len();
    if (FMath::Abs(N - M) > Limit)
    {
        return Limit + 1;
    }
    // Rows two back, one back and current of the optimal string alignment table.
    TArray<int32> Older, Prev, Row;
    Older.Init(0, M + 1);
    Prev.Init(0, M + 1);
    Row.Init(0, M + 1);
    for (int32 J = 0; J <= M; ++J)
    {
        Prev[J] = J;
    }
    for (int32 I = 1; I <= N; ++I)
    {
        Row[0] = I;
        int32 RowMin = Row[0];
        for (int32 J = 1; J <= M; ++J)
        {
            const int32 Substitute = A[I - 1] == B[J - 1] ? 0 : AreNeighbourKeys(A[I - 1], B[J - 1]) ? 1 : 2;
            Row[J] = FMath::Min3(Prev[J] + 1, Row[J - 1] + 1, Prev[J - 1] + Substitute);
            if (I > 1 && J > 1 && A[I - 1] == B[J - 2] && A[I - 2] == B[J - 1])
            {
                Row[J] = FMath::Min(Row[J], Older[J - 2] + 1);
            }
            RowMin = FMath::Min(RowMin, Row[J]);
        }
        if (RowMin > Limit)
        {
            return Limit + 1;
        }
        Swap(Older, Prev);
        Swap(Prev, Row);
    }
    return FMath::Min(Prev[M], Limit + 1);
}

FString McpCorrectQuery(const FString& LowerQuery, const FMcpCapabilityStore& Store, TArray<FMcpQueryCorrection>& OutCorrections)
{
    const FMcpSearchVocabulary& Words = Vocabulary(Store);
    FString Out;
    int32 Index = 0;
    while (Index < LowerQuery.Len())
    {
        int32 End = Index;
        while (End < LowerQuery.Len() && LowerQuery[End] < 128 && FChar::IsAlnum(LowerQuery[End]))
        {
            ++End;
        }
        if (End == Index)
        {
            Out.AppendChar(LowerQuery[Index++]);
            continue;
        }
        const FString Raw = LowerQuery.Mid(Index, End - Index);
        Index = End;
        TArray<FString> Folded;
        McpSearchWords(Raw, Folded);
        TArray<FString> Content;
        McpSearchContentWords(Folded, Content);
        const bool bCandidate = Content.Num() == 1 && Content[0].Len() >= McpTypoSwapLength && !HasDigit(Raw) &&
            !Words.Known.Contains(Content[0]);
        const FString Fix = bCandidate ? NearestName(Content[0], Words.Names) : FString();
        Out += Fix.IsEmpty() ? Raw : Fix;
        if (!Fix.IsEmpty())
        {
            OutCorrections.Add({Raw, Fix});
        }
    }
    return Out;
}

void McpNoteCorrections(const TSharedPtr<FJsonObject>& Reply, const TArray<FMcpQueryCorrection>& Corrections)
{
    if (!Reply.IsValid() || Corrections.Num() == 0)
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FMcpQueryCorrection& Correction : Corrections)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("from"), Correction.From);
        Row->SetStringField(TEXT("to"), Correction.To);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Reply->SetArrayField(TEXT("corrected"), Rows);
}
