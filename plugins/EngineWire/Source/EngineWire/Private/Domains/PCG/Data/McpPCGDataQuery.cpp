#include "Domains/PCG/Data/McpPCGDataActions.h"

#if MCP_HAS_PCG_EXECUTION
#include "Domains/PCG/Data/McpPCGDataSchema.h"
#include "PCGData.h"

namespace McpPCGHandlers
{
TSharedPtr<FJsonObject> QueryPCGRootData(UPCGComponent* Component, const FString& Principal, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FPCGDataViewStore& Store, TArray<FString>& NewHandles, FString& OutError)
{
    auto Result = MakeShared<FJsonObject>();
    int32 Offset = 0, Limit = 0;
    if (Action == TEXT("list_pcg_data"))
    {
        if (!ReadPCGDataInt(Payload, TEXT("offset"), 0, 0, 256, Offset, OutError) ||
            !ReadPCGDataInt(Payload, TEXT("limit"), 4, 1, 8, Limit, OutError)) return nullptr;
        const auto& Output = Component->GetGeneratedGraphOutput().TaggedData;
        if (Output.Num() > FPCGRootDataView::MaxDatasets)
        {
            OutError = TEXT("Root inventory exceeds the bounded dataset limit; no partial inventory was returned.");
            return nullptr;
        }
        TArray<TSharedPtr<FJsonValue>> Items;
        for (int32 Index = Offset; Index < Output.Num() && Items.Num() < Limit; ++Index)
        {
            if (!IsValid(Output[Index].Data.Get()))
            {
                auto Item = MakeShared<FJsonObject>();
                Item->SetNumberField(TEXT("datasetIndex"), Index);
                Item->SetBoolField(TEXT("available"), false);
                Item->SetStringField(TEXT("unavailableReason"), TEXT("native_root_data_absent"));
                Items.Add(MakeShared<FJsonValueObject>(Item));
                continue;
            }
            const FString Id = Store.RetainRoot(Component, Principal, Index, OutError);
            if (Id.IsEmpty()) return nullptr;
            NewHandles.Add(Id);
            auto Root = Store.ResolveRoot(Id, Component, Principal, OutError);
            if (!Root.IsValid()) return nullptr;
            auto Item = Root->Describe();
            Item->SetStringField(TEXT("datasetId"), Id);
            Item->SetBoolField(TEXT("available"), true);
            Items.Add(MakeShared<FJsonValueObject>(Item));
        }
        Result->SetArrayField(TEXT("items"), Items);
        Result->SetNumberField(TEXT("total"), Output.Num());
        Result->SetNumberField(TEXT("offset"), Offset);
        Result->SetNumberField(TEXT("limit"), Limit);
        Result->SetBoolField(TEXT("truncated"), Offset + Items.Num() < Output.Num());
        Result->SetStringField(TEXT("consistency"), TEXT("per_call_live_root_inventory_not_cross_page_revision"));
        return Result;
    }
    const TCHAR* Key = Action == TEXT("get_pcg_data_schema") ? TEXT("datasetId") :
        Action == TEXT("release_pcg_data_view") ? TEXT("handleId") : TEXT("viewId");
    FString Id;
    if (!Payload->TryGetStringField(Key, Id) || Id.IsEmpty() || Id.Len() > 64)
    {
        OutError = TEXT("A bounded nonempty owner-bound dataset/view handle is required.");
        return nullptr;
    }
    Result->SetStringField(Key, Id);
    if (Action == TEXT("release_pcg_data_view"))
    {
        if (!Store.Release(Id, Component, Principal))
        {
            OutError = TEXT("The owned handle is unavailable for this exact component; no foreign retention was changed.");
            return nullptr;
        }
        Result->SetBoolField(TEXT("released"), true);
        return Result;
    }
    if (Action == TEXT("get_pcg_data_schema"))
    {
        FString Domain;
        if (!ReadPCGDataDomain(Payload, Domain, OutError) ||
            !ReadPCGDataInt(Payload, TEXT("offset"), 0, 0, MAX_int32, Offset, OutError) ||
            !ReadPCGDataInt(Payload, TEXT("limit"), 16, 1, 32, Limit, OutError)) return nullptr;
        auto Root = Store.ResolveRoot(Id, Component, Principal, OutError);
        if (!Root.IsValid()) return nullptr;
        const UPCGData* Data = Root->Resolve(Principal, Component, OutError);
        TSharedPtr<FJsonObject> Schema;
        if (!Data || !ReadPCGDataSchema(Data, Domain, Offset, Limit, Schema, OutError) || !Root->Resolve(Principal, Component, OutError)) return nullptr;
        Result->SetObjectField(TEXT("schema"), Schema);
        Result->SetStringField(TEXT("consistency"), TEXT("per_call_live_schema_not_saved_view"));
        return Result;
    }
    if (Action == TEXT("read_pcg_data"))
    {
        if (!ReadPCGDataInt(Payload, TEXT("offset"), 0, 0, FPCGDataSnapshot::MaxRows, Offset, OutError) ||
            !ReadPCGDataInt(Payload, TEXT("limit"), 16, 1, FPCGDataSnapshot::MaxPageRows, Limit, OutError)) return nullptr;
        TArray<TSharedPtr<FJsonValue>> Rows;
        int32 Total = 0;
        if (!Store.ReadRows(Id, Component, Principal, Offset, Limit, Rows, Total, OutError)) return nullptr;
        TSharedPtr<FJsonObject> Description;
        double Remaining = 0;
        if (!Store.DescribeRows(Id, Component, Principal, Description, Remaining, OutError)) return nullptr;
        Result->SetArrayField(TEXT("rows"), Rows);
        Result->SetObjectField(TEXT("projection"), Description);
        Result->SetNumberField(TEXT("rowCount"), Total);
        Result->SetNumberField(TEXT("offset"), Offset);
        Result->SetNumberField(TEXT("limit"), Limit);
        Result->SetBoolField(TEXT("hasMore"), Offset + Rows.Num() < Total);
        Result->SetNumberField(TEXT("expiresInSeconds"), Remaining);
        Result->SetStringField(TEXT("consistency"), TEXT("immutable_historical_projection_current_inputs_unknown"));
        return Result;
    }
    OutError = TEXT("Unsupported data query variant.");
    return nullptr;
}
}
#endif
