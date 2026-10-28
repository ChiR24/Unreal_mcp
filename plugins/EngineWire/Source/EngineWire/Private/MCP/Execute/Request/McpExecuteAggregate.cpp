// options.aggregate (see the header): validated beside the other execute options, applied where the receipt is built,
// before the size check, so a list too large for one reply is counted rather than paged.

#include "MCP/Execute/Request/McpExecuteAggregate.h"

#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "Dom/JsonValue.h"

namespace
{
constexpr int32 McpAggregateMaxGroups = 50;
constexpr int32 McpAggregateMaxSample = 10;
constexpr int32 McpAggregateDefaultSample = 3;

FMcpSemanticError AggregateOptionError(const FString& Message, const FString& Pointer)
{
    return McpValidationError(TEXT("INVALID_OPTIONS"), Message, Pointer);
}

// The array to count: the one named, else the largest declared output array, else the largest top-level array.
FString ChooseField(const TSharedPtr<FJsonObject>& Spec, const TSharedPtr<FJsonObject>& OutputSchema, const TSharedPtr<FJsonObject>& Result)
{
    FString Named;
    if (Spec->TryGetStringField(TEXT("field"), Named))
    {
        return Named;
    }
    const TSharedPtr<FJsonObject>* Declared = nullptr;
    const bool bDeclared = OutputSchema.IsValid() && OutputSchema->TryGetObjectField(TEXT("properties"), Declared) && Declared;
    FString Best;
    int32 BestCount = -1;
    bool bBestDeclared = false;
    for (const auto& Entry : Result->Values)
    {
        const FString Key(*Entry.Key); // 5.8 keys are not FString
        if (!Entry.Value.IsValid() || Entry.Value->Type != EJson::Array)
        {
            continue;
        }
        const bool bIsDeclared = bDeclared && (*Declared)->HasField(Key);
        const int32 Count = Entry.Value->AsArray().Num();
        if ((bIsDeclared && !bBestDeclared) || (bIsDeclared == bBestDeclared &&
            (Count > BestCount || (Count == BestCount && Key.Compare(Best, ESearchCase::CaseSensitive) < 0))))
        {
            Best = Key;
            BestCount = Count;
            bBestDeclared = bIsDeclared;
        }
    }
    return Best;
}

TSharedPtr<FJsonValue> Handle(const TSharedPtr<FJsonValue>& Item)
{
    static const TCHAR* const Handles[] = {TEXT("name"), TEXT("label"), TEXT("actorName"), TEXT("assetPath"), TEXT("path"), TEXT("id")};
    const TSharedPtr<FJsonObject>* Object = nullptr;
    FString Value;
    for (const TCHAR* Field : Handles)
    {
        if (Item.IsValid() && Item->TryGetObject(Object) && Object && (*Object)->TryGetStringField(Field, Value))
        {
            return MakeShared<FJsonValueString>(Value);
        }
    }
    return Item;
}

FString GroupValue(const TSharedPtr<FJsonValue>& Item, const FString& GroupBy)
{
    const TSharedPtr<FJsonObject>* Object = nullptr;
    const TSharedPtr<FJsonValue> Field = Item.IsValid() && Item->TryGetObject(Object) && Object ? (*Object)->TryGetField(GroupBy) : nullptr;
    if (!Field.IsValid() || Field->Type == EJson::Null)
    {
        return TEXT("(none)");
    }
    FString Text;
    return Field->TryGetString(Text) ? Text : Field->Type == EJson::Boolean ? (Field->AsBool() ? TEXT("true") : TEXT("false")) : TEXT("(other)");
}
} // namespace

namespace McpExecuteAggregate
{
bool Validate(const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError)
{
    const TSharedPtr<FJsonValue> Value = Options.IsValid() ? Options->TryGetField(TEXT("aggregate")) : nullptr;
    if (!Value.IsValid())
    {
        return true;
    }
    const TSharedPtr<FJsonObject>* Spec = nullptr;
    if (!Value->TryGetObject(Spec) || !Spec)
    {
        OutError = AggregateOptionError(TEXT("options.aggregate must be an object: {field?, groupBy?, sample?}."), TEXT("/options/aggregate"));
        return false;
    }
    for (const auto& Entry : (*Spec)->Values)
    {
        const FString Key(*Entry.Key);
        const bool bText = (Key == TEXT("field") || Key == TEXT("groupBy")) && Entry.Value->Type == EJson::String &&
            !Entry.Value->AsString().IsEmpty() && Entry.Value->AsString().Len() <= 64;
        const double Sample = Entry.Value->Type == EJson::Number ? Entry.Value->AsNumber() : -1.0;
        const bool bSample = Key == TEXT("sample") && Sample >= 0.0 && Sample <= McpAggregateMaxSample && Sample == FMath::TruncToDouble(Sample);
        if (!bText && !bSample)
        {
            OutError = AggregateOptionError(FString::Printf(
                TEXT("options.aggregate.%s is not valid: field and groupBy are names of 1-64 characters, sample a whole number 0-10."), *Key),
                TEXT("/options/aggregate/") + Key);
            return false;
        }
    }
    return true;
}

FString FingerprintPart(const TSharedPtr<FJsonObject>& Options)
{
    const TSharedPtr<FJsonObject>* Spec = nullptr;
    if (!Options.IsValid() || !Options->TryGetObjectField(TEXT("aggregate"), Spec) || !Spec)
    {
        return FString();
    }
    FString Field, GroupBy;
    double Sample = McpAggregateDefaultSample;
    (*Spec)->TryGetStringField(TEXT("field"), Field);
    (*Spec)->TryGetStringField(TEXT("groupBy"), GroupBy);
    (*Spec)->TryGetNumberField(TEXT("sample"), Sample);
    return FString::Printf(TEXT(" aggregate%d:%s%d:%s:%d"), Field.Len(), *Field, GroupBy.Len(), *GroupBy, static_cast<int32>(Sample));
}

TSharedPtr<FJsonObject> Apply(const TSharedPtr<FJsonObject>& Spec, const TSharedPtr<FJsonObject>& OutputSchema, TSharedPtr<FJsonObject>& InOutResult)
{
    TSharedPtr<FJsonObject> Block = MakeShared<FJsonObject>();
    const FString Field = InOutResult.IsValid() ? ChooseField(Spec, OutputSchema, InOutResult) : FString();
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (Field.IsEmpty() || !InOutResult->TryGetArrayField(Field, Items) || !Items)
    {
        Block->SetStringField(TEXT("note"), Field.IsEmpty() ? TEXT("The reply has no list to aggregate.")
            : FString::Printf(TEXT("The reply has no list named '%s'."), *Field));
        return Block;
    }
    double SampleSize = McpAggregateDefaultSample;
    Spec->TryGetNumberField(TEXT("sample"), SampleSize);
    const int32 Sample = static_cast<int32>(SampleSize);
    FString GroupBy;
    const bool bGroup = Spec->TryGetStringField(TEXT("groupBy"), GroupBy);
    Block->SetStringField(TEXT("field"), Field);
    Block->SetNumberField(TEXT("total"), Items->Num());
    // A paged list counts its page only: say so, or a caller reads the page's counts as the whole.
    bool bMore = false;
    double Whole = 0.0;
    if ((InOutResult->TryGetBoolField(TEXT("hasMore"), bMore) && bMore) ||
        (InOutResult->TryGetNumberField(TEXT("totalCount"), Whole) && Whole > Items->Num()))
    {
        Block->SetStringField(TEXT("note"), FString::Printf(
            TEXT("Counted the %d on this page only; the reply has more. Raise the call's limit, or read the next pages, to count them all."),
            Items->Num()));
    }
    TArray<TPair<FString, TArray<TSharedPtr<FJsonValue>>>> Groups;
    TMap<FString, int32> Where;
    TArray<int32> Counts;
    for (const TSharedPtr<FJsonValue>& Item : *Items)
    {
        const FString Value = bGroup ? GroupValue(Item, GroupBy) : FString();
        const int32* Found = Where.Find(Value);
        const int32 At = Found ? *Found : Groups.Emplace(Value, TArray<TSharedPtr<FJsonValue>>());
        Where.Add(Value, At);
        Counts.SetNumZeroed(Groups.Num());
        ++Counts[At];
        if (Groups[At].Value.Num() < Sample)
        {
            Groups[At].Value.Add(Handle(Item));
        }
    }
    TArray<int32> Order;
    for (int32 At = 0; At < Groups.Num(); ++At)
    {
        Order.Add(At);
    }
    Order.Sort([&](int32 L, int32 R) { return Counts[L] != Counts[R] ? Counts[L] > Counts[R] : Groups[L].Key.Compare(Groups[R].Key, ESearchCase::CaseSensitive) < 0; });
    if (!bGroup)
    {
        Block->SetArrayField(TEXT("sample"), Groups.Num() > 0 ? Groups[0].Value : TArray<TSharedPtr<FJsonValue>>());
    }
    else
    {
        Block->SetStringField(TEXT("groupBy"), GroupBy);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (int32 Index = 0; Index < Order.Num() && Index < McpAggregateMaxGroups; ++Index)
        {
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("value"), Groups[Order[Index]].Key);
            Row->SetNumberField(TEXT("count"), Counts[Order[Index]]);
            Row->SetArrayField(TEXT("sample"), Groups[Order[Index]].Value);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Block->SetArrayField(TEXT("groups"), Rows);
        if (Order.Num() > McpAggregateMaxGroups)
        {
            Block->SetNumberField(TEXT("otherGroups"), Order.Num() - McpAggregateMaxGroups);
        }
    }
    TSharedPtr<FJsonObject> Without = MakeShared<FJsonObject>();
    Without->Values = InOutResult->Values;
    Without->SetArrayField(Field, TArray<TSharedPtr<FJsonValue>>());
    InOutResult = Without;
    return Block;
}
} // namespace McpExecuteAggregate
