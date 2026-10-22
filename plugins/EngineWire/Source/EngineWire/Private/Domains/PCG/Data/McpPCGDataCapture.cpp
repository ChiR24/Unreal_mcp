#include "Domains/PCG/Data/McpPCGDataActions.h"

#if MCP_HAS_PCG_EXECUTION
#include "Domains/PCG/Data/McpPCGDataRows.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGData.h"
#include "Misc/DateTime.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpPCGHandlers
{
TSharedPtr<FJsonObject> CapturePCGDataView(UPCGComponent* Component, const FString& Principal,
    const TSharedPtr<FJsonObject>& Payload, FPCGDataViewStore& Store, TArray<FString>& NewHandles, FString& OutError)
{
    FString DatasetId;
    if (!Payload->TryGetStringField(TEXT("datasetId"), DatasetId) || DatasetId.IsEmpty() || DatasetId.Len() > 64)
    {
        OutError = TEXT("An owner-bound datasetId from root discovery is required.");
        return nullptr;
    }
    FPCGDataProjection Projection;
    if (!ReadPCGDataDomain(Payload, Projection.Domain, OutError) ||
        !ReadPCGDataInt(Payload, TEXT("sourceOffset"), 0, 0, MAX_int32, Projection.Offset, OutError) ||
        !ReadPCGDataInt(Payload, TEXT("sourceLimit"), 64, 1, FPCGDataSnapshot::MaxRows, Projection.Limit, OutError) ||
        !ReadPCGDataStrings(Payload, TEXT("fields"), 64, Projection.Fields, OutError) ||
        !ReadPCGDataStrings(Payload, TEXT("attributes"), NAME_SIZE - 1, Projection.Attributes, OutError)) return nullptr;
    auto Root = Store.ResolveRoot(DatasetId, Component, Principal, OutError);
    if (!Root.IsValid()) return nullptr;
    const UPCGData* Data = Root->Resolve(Principal, Component, OutError);
    if (!Data) return nullptr;
    TArray<TSharedPtr<FJsonValue>> Rows;
    int64 SourceTotal = 0;
    if (!BuildPCGDataRows(Data, Projection, Rows, SourceTotal, OutError)) return nullptr;
    auto Description = MakeShared<FJsonObject>();
    Description->SetStringField(TEXT("domain"), Projection.Domain);
    Description->SetNumberField(TEXT("sourceOffset"), Projection.Offset);
    Description->SetNumberField(TEXT("sourceLimit"), Projection.Limit);
    Description->SetNumberField(TEXT("rowCount"), Rows.Num());
    if (SourceTotal <= 9007199254740991LL) Description->SetNumberField(TEXT("sourceTotal"), static_cast<double>(SourceTotal));
    else Description->SetStringField(TEXT("sourceTotal"), LexToString(SourceTotal));
    Description->SetBoolField(TEXT("completeSourceRowRange"), Projection.Offset == 0 && Rows.Num() == SourceTotal);
    Description->SetStringField(TEXT("createdAt"), FDateTime::UtcNow().ToIso8601());
    Description->SetStringField(TEXT("coordinateSpace"), TEXT("native_stored_no_conversion_bounds_are_point_local"));
    Description->SetStringField(TEXT("pointRotationEncoding"), TEXT("quaternion_xyzw"));
    Description->SetStringField(TEXT("attributeTransformRotationEncoding"), TEXT("native_quaternion_to_euler_degrees"));
    Description->SetStringField(TEXT("provenance"), TEXT("current_component_root_generation_unknown"));
    Description->SetStringField(TEXT("dataClass"), Data->GetClass()->GetPathName());
    TArray<TSharedPtr<FJsonValue>> Fields, Attributes, Columns;
    for (const FString& Field : Projection.Fields) Fields.Add(MakeShared<FJsonValueString>(Field));
    const UPCGMetadata* Metadata = Data->ConstMetadata();
    const FPCGMetadataDomain* Domain = Metadata ? Metadata->GetConstMetadataDomain(
        Projection.Domain == TEXT("Data") ? PCGMetadataDomainID::Data : PCGMetadataDomainID::Elements) : nullptr;
    for (const FString& Name : Projection.Attributes)
    {
        const auto* Attribute = Domain ? Domain->GetConstAttribute(FName(*Name, FNAME_Find)) : nullptr;
        if (!Attribute)
        {
            OutError = TEXT("Selected native schema became unavailable during projection construction.");
            return nullptr;
        }
        auto Column = MakeShared<FJsonObject>();
        Column->SetStringField(TEXT("name"), Name);
        Column->SetNumberField(TEXT("typeId"), Attribute->GetTypeId());
        UEnum* Types = StaticEnum<EPCGMetadataTypes>();
        Column->SetStringField(TEXT("typeName"), Types ? Types->GetNameStringByValue(Attribute->GetTypeId()) : TEXT("Unknown"));
        Columns.Add(MakeShared<FJsonValueObject>(Column));
        Attributes.Add(MakeShared<FJsonValueString>(Name));
    }
    Description->SetArrayField(TEXT("fields"), Fields);
    Description->SetArrayField(TEXT("attributes"), Attributes);
    Description->SetArrayField(TEXT("attributeTypes"), Columns);
    FString Encoded;
    auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded);
    if (!FJsonSerializer::Serialize(Description, Writer) || Encoded.Len() > FPCGDataViewStore::MaxDescriptionCharacters)
    {
        OutError = TEXT("The immutable typed projection description exceeds its explicit size bound.");
        return nullptr;
    }
    const FString Id = Store.RetainRows(DatasetId, Component, Principal, Rows, OutError, Encoded);
    if (Id.IsEmpty()) return nullptr;
    NewHandles.Add(Id);
    TSharedPtr<FJsonObject> Copy;
    double Remaining = 0;
    if (!Store.DescribeRows(Id, Component, Principal, Copy, Remaining, OutError)) return nullptr;
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("datasetId"), DatasetId);
    Result->SetStringField(TEXT("viewId"), Id);
    Result->SetObjectField(TEXT("projection"), Copy);
    Result->SetNumberField(TEXT("expiresInSeconds"), Remaining);
    Result->SetStringField(TEXT("consistency"), TEXT("immutable_historical_projection_current_inputs_unknown"));
    return Result;
}
}
#endif
