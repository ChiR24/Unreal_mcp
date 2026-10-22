#include "Domains/PCG/Data/McpPCGRootData.h"

#if MCP_HAS_PCG_EXECUTION
#include "PCGData.h"
#include "PCGParamData.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGSpatialData.h"

namespace McpPCGHandlers
{
TSharedPtr<FPCGRootDataView> CreatePCGRootDataView(UPCGComponent* Component, const FString& Principal, int32 Slot, FString& OutError)
{
    OutError.Reset();
    if (!IsInGameThread() || !IsValid(Component) || Slot < 0 || Component->IsGenerating() ||
        Component->IsCleaningUp() || HasActivePCGOperation(Component))
    {
        OutError = TEXT("An exact idle game-thread component and root slot are required.");
        return nullptr;
    }
    const auto& Output = Component->GetGeneratedGraphOutput().TaggedData;
    if (Output.Num() > FPCGRootDataView::MaxDatasets || !Output.IsValidIndex(Slot) || !IsValid(Output[Slot].Data.Get()))
    {
        OutError = TEXT("The requested root dataset is absent or root inventory exceeds its bound.");
        return nullptr;
    }
    const auto& Tagged = Output[Slot];
    if (Tagged.Tags.Num() > FPCGRootDataView::MaxTags)
    {
        OutError = TEXT("Root dataset tags exceed the bounded locator size.");
        return nullptr;
    }
    int64 TagCharacters = 0;
    for (const FString& Tag : Tagged.Tags)
    {
        TagCharacters += Tag.Len();
        if (Tag.Len() > FPCGRootDataView::MaxTagCharacters || TagCharacters > FPCGRootDataView::MaxTagTotalCharacters)
        {
            OutError = TEXT("A root dataset tag exceeds the bounded locator size.");
            return nullptr;
        }
    }
    const UPCGData* Data = Tagged.Data.Get();
    auto Guard = CreatePCGDataSnapshot(Component, Data, Principal, {}, OutError);
    if (!Guard.IsValid()) return nullptr;
    auto View = MakeShared<FPCGRootDataView>();
    View->Guard = MoveTemp(Guard);
    View->Source = Data;
    View->Slot = Slot;
    View->Pin = Tagged.Pin;
    View->Tags = Tagged.Tags;
    View->DataClass = Data->GetClass()->GetPathName();
    if (View->DataClass.Len() > 1024)
    {
        OutError = TEXT("The root data class path exceeds the bounded descriptor size.");
        return nullptr;
    }
    if (const auto* Points = Cast<UPCGBasePointData>(Data))
    {
        View->Kind = TEXT("point");
        View->PointCount = Points->GetNumPoints();
    }
    else if (Data->IsA<UPCGParamData>()) View->Kind = TEXT("param");
    else if (Data->IsA<UPCGSpatialData>()) View->Kind = TEXT("spatial");
    else View->Kind = TEXT("other");
    return View;
}

const UPCGData* FPCGRootDataView::Resolve(const FString& Principal, UPCGComponent* Component, FString& OutError) const
{
    OutError.Reset();
    TArray<TSharedPtr<FJsonValue>> Empty;
    if (!Guard.IsValid() || !Guard->ReadPage(Principal, Component, 0, 1, Empty, OutError)) return nullptr;
    const auto& Output = Component->GetGeneratedGraphOutput().TaggedData;
    if (Output.Num() > MaxDatasets || !Output.IsValidIndex(Slot) || Output[Slot].Data.Get() != Source.Get() ||
        Output[Slot].Pin != Pin || Output[Slot].Tags.Num() != Tags.Num())
    {
        OutError = TEXT("The weak dataset no longer occupies its captured component root slot.");
        return nullptr;
    }
    for (const FString& Tag : Output[Slot].Tags)
    {
        if (!Tags.Contains(Tag))
        {
            OutError = TEXT("The root dataset tags changed after locator capture.");
            return nullptr;
        }
    }
    return Source.Get();
}

TSharedPtr<FJsonObject> FPCGRootDataView::Describe() const
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("datasetIndex"), Slot);
    Result->SetStringField(TEXT("pin"), Pin.ToString());
    Result->SetStringField(TEXT("dataClass"), DataClass);
    Result->SetStringField(TEXT("kind"), Kind);
    if (Kind == TEXT("point")) Result->SetNumberField(TEXT("pointCountAtDiscovery"), PointCount);
    TArray<FString> Sorted = Tags.Array();
    Sorted.Sort([](const FString& A, const FString& B) { return A.Compare(B, ESearchCase::CaseSensitive) < 0; });
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FString& Tag : Sorted) Values.Add(MakeShared<FJsonValueString>(Tag));
    Result->SetArrayField(TEXT("tags"), Values);
    Result->SetStringField(TEXT("provenance"), TEXT("current_component_root_generation_unknown"));
    Result->SetStringField(TEXT("consistency"), TEXT("weak_root_locator_not_content_revision"));
    return Result;
}
}
#endif
