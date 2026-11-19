// options.aggregate on a list inside an object (get_widget_info's widgetInfo.widgets): the dotted field is walked, and the
// reply copied along the way, so the list can be counted and emptied there as at the top.

#include "MCP/Execute/Request/McpExecuteAggregate.h"

#include "Dom/JsonValue.h"

namespace McpExecuteAggregate
{
TSharedPtr<FJsonObject> CopyToList(TSharedPtr<FJsonObject>& InOutResult, const FString& Field, FString& OutLeaf)
{
    TArray<FString> Parts;
    Field.ParseIntoArray(Parts, TEXT("."), true);
    if (Parts.Num() == 0 || !InOutResult.IsValid())
    {
        return nullptr;
    }
    OutLeaf = Parts.Pop();
    TSharedPtr<FJsonObject> Holder = MakeShared<FJsonObject>();
    Holder->Values = InOutResult->Values;
    InOutResult = Holder;
    for (const FString& Part : Parts)
    {
        const TSharedPtr<FJsonObject>* Inner = nullptr;
        if (!Holder->TryGetObjectField(Part, Inner) || !Inner || !(*Inner).IsValid())
        {
            return nullptr;
        }
        TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
        Copy->Values = (*Inner)->Values;
        Holder->SetObjectField(Part, Copy);
        Holder = Copy;
    }
    return Holder;
}

FString NestedListField(const TSharedPtr<FJsonObject>& Result)
{
    FString Best;
    int32 BestCount = -1;
    for (const auto& Outer : Result->Values)
    {
        const TSharedPtr<FJsonObject>* Inner = nullptr;
        if (!Outer.Value.IsValid() || !Outer.Value->TryGetObject(Inner) || !Inner || !(*Inner).IsValid())
        {
            continue;
        }
        for (const auto& Entry : (*Inner)->Values)
        {
            const FString Field = FString(*Outer.Key) + TEXT(".") + FString(*Entry.Key); // 5.8 keys are not FString
            const int32 Count = Entry.Value.IsValid() && Entry.Value->Type == EJson::Array ? Entry.Value->AsArray().Num() : -1;
            if (Count > BestCount || (Count == BestCount && Count >= 0 && Field.Compare(Best, ESearchCase::CaseSensitive) < 0))
            {
                Best = Count >= 0 ? Field : Best;
                BestCount = FMath::Max(Count, BestCount);
            }
        }
    }
    return Best;
}
} // namespace McpExecuteAggregate
