#include "Domains/PCG/Data/McpPCGDataRows.h"

#if MCP_HAS_PCG_EXECUTION
#include "Domains/PCG/Data/McpPCGDataValue.h"
#include "Domains/PCG/Data/McpPCGPointRow.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPointArrayData.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpPCGHandlers
{
namespace
{
bool FailProjection(FString& Error, const TCHAR* Message)
{
    Error = Message;
    return false;
}

bool CheckAttribute(const FPCGMetadataAttributeBase* Attribute, FString& Error)
{
    if (!Attribute) return FailProjection(Error, TEXT("A selected metadata attribute is absent from the requested domain."));
    const int16 Type = Attribute->GetTypeId();
    if (Type < static_cast<int16>(EPCGMetadataTypes::Float) || Type > static_cast<int16>(EPCGMetadataTypes::SoftClassPath))
        return FailProjection(Error, TEXT("The selected metadata type has no supported scalar codec."));
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    const auto& Desc = Attribute->GetAttributeDesc();
    if (!Desc.ContainerTypes.IsEmpty() || static_cast<int16>(Desc.ValueType) != Type || Desc.ValueTypeObject)
        return FailProjection(Error, TEXT("The selected metadata descriptor or container has no supported scalar codec."));
#endif
    return true;
}
}

bool BuildPCGDataRows(const UPCGData* Data, const FPCGDataProjection& Projection,
    TArray<TSharedPtr<FJsonValue>>& OutRows, int64& OutSourceTotal, FString& OutError)
{
    OutRows.Reset();
    OutSourceTotal = 0;
    OutError.Reset();
    FPCGReadBudget Budget;
    if (!IsInGameThread() || !IsValid(Data) || Projection.Offset < 0 || Projection.Limit < 1 || Projection.Limit > FPCGDataSnapshot::MaxRows)
        return FailProjection(OutError, TEXT("An existing game-thread dataset and bounded nonnegative projection range are required."));
    if (Data->GetClass() != UPCGPointData::StaticClass() && Data->GetClass() != UPCGPointArrayData::StaticClass() && Data->GetClass() != UPCGParamData::StaticClass())
        return FailProjection(OutError, TEXT("Typed row projections currently support native PointData, PointArrayData and ParamData only; no Spatial conversion is performed."));
    if (Projection.Fields.Num() + Projection.Attributes.Num() < 1 || Projection.Fields.Num() > 8 || Projection.Attributes.Num() > 8)
        return FailProjection(OutError, TEXT("Select explicit point fields and/or at most eight metadata attributes."));
    if (!Projection.Fields.IsEmpty() && !ValidatePCGPointFields(Projection.Fields, OutError)) return false;
    const bool bDataDomain = Projection.Domain == TEXT("Data");
    if (!bDataDomain && Projection.Domain != TEXT("Elements"))
        return FailProjection(OutError, TEXT("Only explicit Data and Elements domains have supported row semantics."));
    const UPCGBasePointData* Points = Cast<UPCGBasePointData>(Data);
    if (!Projection.Fields.IsEmpty() && (!Points || bDataDomain))
        return FailProjection(OutError, TEXT("Point built-ins are only available on the point Elements projection."));
    const UPCGMetadata* Metadata = Data->ConstMetadata();
    const auto DomainId = bDataDomain ? PCGMetadataDomainID::Data : PCGMetadataDomainID::Elements;
    const FPCGMetadataDomain* Domain = Metadata ? Metadata->GetConstMetadataDomain(DomainId) : nullptr;
    if ((bDataDomain || !Points || !Projection.Attributes.IsEmpty()) && !Domain)
        return FailProjection(OutError, TEXT("The requested native metadata domain is unavailable, not an empty table."));
    const int64 Total = bDataDomain ? 1 : Points ? Points->GetNumPoints() : Domain->GetItemCountForChild();
    if (Total < 0) return FailProjection(OutError, TEXT("Native row count is invalid."));
    TArray<const FPCGMetadataAttributeBase*> Attributes;
    TSet<FName> Names;
    for (const FString& Name : Projection.Attributes)
    {
        if (Name.IsEmpty() || Name.Len() >= NAME_SIZE)
            return FailProjection(OutError, TEXT("A selected attribute name is empty or exceeds the native name limit."));
        const FName Key(*Name, FNAME_Find);
        if (Key.IsNone() || !Key.ToString().Equals(Name, ESearchCase::IgnoreCase) || Names.Contains(Key))
            return FailProjection(OutError, TEXT("Selected attributes must resolve exactly and be unique in the native domain."));
        Names.Add(Key);
        const auto* Attribute = Domain->GetConstAttribute(Key);
        if (!CheckAttribute(Attribute, OutError)) return false;
        Attributes.Add(Attribute);
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    int64 Characters = 0;
    for (int64 Index = Projection.Offset; Index < Total && Rows.Num() < Projection.Limit; ++Index)
    {
        if (!Budget.IsWithinTime()) return FailProjection(OutError, TEXT("Projection construction exceeded its cooperative time budget; no partial view was captured."));
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("sourceIndex"), static_cast<double>(Index));
        if (!Projection.Fields.IsEmpty())
        {
            TSharedPtr<FJsonObject> Fields;
            if (!ReadPCGPointRow(Points, static_cast<int32>(Index), Projection.Fields, Fields, OutError)) return false;
            Row->SetObjectField(TEXT("fields"), Fields);
        }
        if (!Attributes.IsEmpty())
        {
            const PCGMetadataEntryKey Entry = bDataDomain ? PCGInvalidEntryKey : Points ?
                Points->GetConstMetadataEntryValueRange()[static_cast<int32>(Index)] : Index;
            auto Values = MakeShared<FJsonObject>();
            for (int32 Column = 0; Column < Attributes.Num(); ++Column)
            {
                TSharedPtr<FJsonValue> Value;
                if (!McpPCGDataValue::ExportPCGDataValue(Attributes[Column], Entry, Value, OutError)) return false;
                Values->SetField(Projection.Attributes[Column], Value);
            }
            Row->SetObjectField(TEXT("attributes"), Values);
        }
        auto Envelope = MakeShared<FJsonObject>();
        Envelope->SetObjectField(TEXT("row"), Row);
        FString Encoded;
        auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded);
        if (!FJsonSerializer::Serialize(Envelope, Writer)) return FailProjection(OutError, TEXT("A typed projection row could not be serialized."));
        Characters += Encoded.Len();
        if (Encoded.Len() > FPCGDataSnapshot::MaxRowCharacters || Characters > FPCGDataSnapshot::MaxCharacters || !Budget.IsWithinTime())
            return FailProjection(OutError, TEXT("The selected typed range exceeds row, snapshot or time bounds; narrow the explicit projection."));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (!Budget.IsWithinTime()) return FailProjection(OutError, TEXT("Projection validation exceeded its cooperative time budget."));
    OutRows = MoveTemp(Rows);
    OutSourceTotal = Total;
    return true;
}
}
#endif
