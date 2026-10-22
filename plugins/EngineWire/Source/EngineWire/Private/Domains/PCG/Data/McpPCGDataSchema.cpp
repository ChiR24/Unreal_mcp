#include "Domains/PCG/Data/McpPCGDataSchema.h"

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Domains/PCG/Data/McpPCGDataValue.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "PCGData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpPCGHandlers
{
namespace
{
constexpr int32 SchemaDomainLimit = 16;
constexpr int32 SchemaAttributeLimit = 256;
constexpr int32 SchemaPageLimit = 32;
constexpr int32 SchemaNameLimit = 1024;
constexpr int32 SchemaTotalNameCharacters = 32768;
constexpr int32 SchemaOutputLimit = 60000;
constexpr int32 SchemaContainerLimit = 8;
constexpr int64 SchemaSafeIntegerLimit = 9007199254740991LL;

bool FailDataSchema(TSharedPtr<FJsonObject>& OutSchema, FString& OutError, const FString& Reason)
{
    OutSchema.Reset();
    OutError = Reason;
    return false;
}

FString DataSchemaDomainName(const FPCGMetadataDomainID& Id)
{
    if (Id == PCGMetadataDomainID::Data) return TEXT("Data");
    if (Id == PCGMetadataDomainID::Elements) return TEXT("Elements");
    if (Id == PCGMetadataDomainID::Default) return TEXT("Default");
    return Id.DebugName.IsNone() ? TEXT("Custom") : Id.DebugName.ToString();
}

TSharedPtr<FJsonObject> DataSchemaDomainObject(const FPCGMetadataDomainID& Id, bool bAvailable, bool bQueried)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), DataSchemaDomainName(Id));
    Result->SetNumberField(TEXT("flag"), static_cast<int32>(Id.Flag));
    Result->SetNumberField(TEXT("customFlag"), Id.CustomFlag);
    Result->SetStringField(TEXT("debugName"), Id.DebugName.ToString());
    Result->SetBoolField(TEXT("available"), bAvailable);
    Result->SetBoolField(TEXT("queried"), bQueried);
    return Result;
}

void SetDataSchemaInt64(TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int64 Value)
{
    if (Value >= -SchemaSafeIntegerLimit && Value <= SchemaSafeIntegerLimit)
        Object->SetNumberField(Field, static_cast<double>(Value));
    else
        Object->SetStringField(Field, LexToString(Value));
}

FString DataSchemaMetadataTypeName(int16 TypeId)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    const TCHAR* Names[] = {TEXT("Float"), TEXT("Double"), TEXT("Integer32"), TEXT("Integer64"), TEXT("Vector2"), TEXT("Vector"),
        TEXT("Vector4"), TEXT("Quaternion"), TEXT("Transform"), TEXT("String"), TEXT("Boolean"), TEXT("Rotator"), TEXT("Name"),
        TEXT("SoftObjectPath"), TEXT("SoftClassPath"), TEXT("EndLegacyTypes"), TEXT("Byte"), TEXT("Text"), TEXT("Enum"), TEXT("Struct"),
        TEXT("Object"), TEXT("SoftObject"), TEXT("Class"), TEXT("SoftClass")};
#else
    const TCHAR* Names[] = {TEXT("Float"), TEXT("Double"), TEXT("Integer32"), TEXT("Integer64"), TEXT("Vector2"), TEXT("Vector"),
        TEXT("Vector4"), TEXT("Quaternion"), TEXT("Transform"), TEXT("String"), TEXT("Boolean"), TEXT("Rotator"), TEXT("Name"),
        TEXT("SoftObjectPath"), TEXT("SoftClassPath")};
#endif
    return TypeId >= 0 && TypeId < UE_ARRAY_COUNT(Names) ? FString(Names[TypeId]) : FString::Printf(TEXT("TypeId_%d"), TypeId);
}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
FString DataSchemaContainerTypeName(EPCGMetadataAttributeContainerTypes Type)
{
    switch (Type)
    {
    case EPCGMetadataAttributeContainerTypes::None: return TEXT("None");
    case EPCGMetadataAttributeContainerTypes::Array: return TEXT("Array");
    case EPCGMetadataAttributeContainerTypes::Set: return TEXT("Set");
    case EPCGMetadataAttributeContainerTypes::Map: return TEXT("Map");
    default: return TEXT("Unknown");
    }
}
#endif

void SetDataSchemaTypeObjectPath(TSharedPtr<FJsonObject>& Result, const TCHAR* Field, const UObject* TypeObject)
{
    if (!TypeObject)
    {
        Result->SetStringField(Field, TEXT(""));
        Result->SetBoolField(FString::Printf(TEXT("%sAvailable"), Field), true);
        return;
    }
    const FString Path = TypeObject->GetPathName();
    Result->SetBoolField(FString::Printf(TEXT("%sAvailable"), Field), Path.Len() <= SchemaNameLimit);
    if (Path.Len() <= SchemaNameLimit) Result->SetStringField(Field, Path);
}

struct FPCGDataSchemaAttribute
{
    FName Name;
    FString Text;
};

bool PopulateDataSchemaDescriptor(const FPCGMetadataAttributeBase* Attribute, TSharedPtr<FJsonObject>& Result, FString& OutReason)
{
    const int16 TypeId = Attribute->GetTypeId();
    Result->SetNumberField(TEXT("typeId"), TypeId);
    Result->SetBoolField(TEXT("allowsInterpolation"), Attribute->AllowsInterpolation());
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    const FPCGMetadataAttributeDesc& Desc = Attribute->GetAttributeDesc();
    Result->SetNumberField(TEXT("valueType"), static_cast<int32>(Desc.ValueType));
    Result->SetStringField(TEXT("valueTypeName"), DataSchemaMetadataTypeName(static_cast<int16>(Desc.ValueType)));
    const int32 ContainerCount = Desc.ContainerTypes.Num();
    TArray<TSharedPtr<FJsonValue>> Containers;
    for (int32 Index = 0; Index < FMath::Min(ContainerCount, SchemaContainerLimit); ++Index)
        Containers.Add(MakeShared<FJsonValueString>(DataSchemaContainerTypeName(Desc.ContainerTypes[Index])));
    Result->SetArrayField(TEXT("containerTypes"), Containers);
    Result->SetBoolField(TEXT("containerTypesTruncated"), ContainerCount > SchemaContainerLimit);
    SetDataSchemaTypeObjectPath(Result, TEXT("valueTypeObjectPath"), Desc.ValueTypeObject.Get());
    Result->SetNumberField(TEXT("keyType"), static_cast<int32>(Desc.KeyType));
    Result->SetStringField(TEXT("keyTypeName"), DataSchemaMetadataTypeName(static_cast<int16>(Desc.KeyType)));
    SetDataSchemaTypeObjectPath(Result, TEXT("keyTypeObjectPath"), Desc.KeyTypeObject.Get());
    const bool bSimpleContainer = ContainerCount <= 1;
    if (ContainerCount > SchemaContainerLimit)
    {
        Result->SetStringField(TEXT("typeName"), TEXT("UnsupportedDescriptor"));
        OutReason = TEXT("PCG descriptor exceeds the schema container bound");
        return false;
    }
    if (bSimpleContainer) Result->SetStringField(TEXT("typeName"), Desc.GetTypeString());
    else Result->SetStringField(TEXT("typeName"), TEXT("UnsupportedNestedDescriptor"));
    if (!bSimpleContainer)
    {
        OutReason = TEXT("Nested PCG metadata containers are not supported for default-value reads");
        return false;
    }
#else
    Result->SetStringField(TEXT("typeName"), DataSchemaMetadataTypeName(TypeId));
#endif
    OutReason.Reset();
    return true;
}
}

bool ReadPCGDataSchema(const UPCGData* Data, const FString& Domain, int32 Offset, int32 Limit,
    TSharedPtr<FJsonObject>& OutSchema, FString& OutError)
{
    OutSchema.Reset();
    OutError.Reset();
    if (Domain != TEXT("Data") && Domain != TEXT("Elements"))
        return FailDataSchema(OutSchema, OutError, TEXT("Only the explicit Data and Elements metadata domains are supported."));
    if (Offset < 0 || Limit < 1 || Limit > SchemaPageLimit)
        return FailDataSchema(OutSchema, OutError, TEXT("Schema offset must be non-negative and limit must be between 1 and 32."));
    if (!IsInGameThread()) return FailDataSchema(OutSchema, OutError, TEXT("PCG metadata schema reads require the existing game-thread dispatch."));
    if (!IsValid(Data)) return FailDataSchema(OutSchema, OutError, TEXT("A valid PCG data object is required."));
    FPCGReadBudget Budget;

    const UClass* DataClass = Data->GetClass();
    const UObject* ClassPackage = DataClass ? DataClass->GetOutermost() : nullptr;
    if (!DataClass || !DataClass->IsNative() || !ClassPackage || ClassPackage->GetName() != TEXT("/Script/PCG") ||
        DataClass->GetFName() == FName(TEXT("PCGDataViewData")))
        return FailDataSchema(OutSchema, OutError, TEXT("This PCG dataset class has an unsupported domain-list override."));
    if (!Budget.IsWithinTime()) return FailDataSchema(OutSchema, OutError, TEXT("PCG schema read budget expired."));

    const UPCGMetadata* Metadata = Data->ConstMetadata();
    if (!Metadata) return FailDataSchema(OutSchema, OutError, TEXT("This PCG dataset does not expose metadata."));
    TArray<FPCGMetadataDomainID> DomainIds = Data->GetAllSupportedMetadataDomainIDs();
    if (!Budget.IsWithinTime()) return FailDataSchema(OutSchema, OutError, TEXT("PCG supported-domain query exceeded its time budget."));
    if (DomainIds.Num() > SchemaDomainLimit) return FailDataSchema(OutSchema, OutError, TEXT("PCG dataset advertises too many metadata domains."));
    TSet<FPCGMetadataDomainID> SeenIds;
    for (const FPCGMetadataDomainID& Id : DomainIds)
    {
        if (!Id.IsValid() || SeenIds.Contains(Id)) return FailDataSchema(OutSchema, OutError, TEXT("PCG dataset advertises an invalid or duplicate metadata domain."));
        SeenIds.Add(Id);
    }
    DomainIds.Sort([](const FPCGMetadataDomainID& A, const FPCGMetadataDomainID& B) { return A < B; });
    const FPCGMetadataDomainID RequestedId = Domain == TEXT("Data") ? PCGMetadataDomainID::Data : PCGMetadataDomainID::Elements;
    if (!SeenIds.Contains(RequestedId)) return FailDataSchema(OutSchema, OutError, TEXT("The requested built-in metadata domain is not advertised by this dataset."));
    const FPCGMetadataDomain* RequestedDomain = Metadata->GetConstMetadataDomain(RequestedId);
    if (!RequestedDomain) return FailDataSchema(OutSchema, OutError, TEXT("The requested metadata domain is advertised but unavailable."));

    TArray<TSharedPtr<FJsonValue>> DomainValues;
    for (const FPCGMetadataDomainID& Id : DomainIds)
    {
        if (!Budget.IsWithinTime()) return FailDataSchema(OutSchema, OutError, TEXT("PCG schema domain listing exceeded its time budget."));
        const bool bAvailable = Metadata->GetConstMetadataDomain(Id) != nullptr;
        DomainValues.Add(MakeShared<FJsonValueObject>(DataSchemaDomainObject(Id, bAvailable, Id == RequestedId)));
    }
    const FPCGMetadataDomainID DefaultId = Data->GetDefaultMetadataDomainID();
    const int32 AttributeCount = RequestedDomain->GetAttributeCount();
    if (AttributeCount < 0 || AttributeCount > SchemaAttributeLimit)
        return FailDataSchema(OutSchema, OutError, TEXT("PCG metadata attribute count exceeds the bounded schema limit."));
    TArray<FName> Names;
    TArray<EPCGMetadataTypes> Types;
    RequestedDomain->GetAttributes(Names, Types);
    if (Names.Num() != AttributeCount || Types.Num() != AttributeCount)
        return FailDataSchema(OutSchema, OutError, TEXT("PCG metadata attribute enumeration did not match its bounded count."));

    int32 NameCharacters = 0;
    TArray<FPCGDataSchemaAttribute> Attributes;
    Attributes.Reserve(Names.Num());
    for (const FName& Name : Names)
    {
        if (!Budget.IsWithinTime() || Name.IsNone()) return FailDataSchema(OutSchema, OutError, TEXT("PCG metadata names exceeded the read budget or contained an invalid name."));
        FString Text = Name.ToString();
        NameCharacters += Text.Len();
        if (Text.Len() >= SchemaNameLimit || NameCharacters > SchemaTotalNameCharacters)
            return FailDataSchema(OutSchema, OutError, TEXT("PCG metadata names exceed the schema length budget."));
        Attributes.Add({Name, MoveTemp(Text)});
    }
    Attributes.Sort([](const FPCGDataSchemaAttribute& A, const FPCGDataSchemaAttribute& B)
    {
        return A.Text.Compare(B.Text, ESearchCase::CaseSensitive) < 0;
    });

    const int64 EntryCount = RequestedDomain->GetItemCountForChild();
    const int64 LocalEntryCount = RequestedDomain->GetLocalItemCount();
    if (EntryCount < 0 || LocalEntryCount < 0) return FailDataSchema(OutSchema, OutError, TEXT("PCG metadata returned an invalid entry count."));
    TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
    Schema->SetStringField(TEXT("dataClass"), DataClass->GetName());
    Schema->SetStringField(TEXT("domainName"), Domain);
    Schema->SetArrayField(TEXT("domains"), DomainValues);
    Schema->SetObjectField(TEXT("domain"), DataSchemaDomainObject(RequestedId, true, true));
    Schema->SetObjectField(TEXT("defaultDomain"), DataSchemaDomainObject(DefaultId, Metadata->GetConstMetadataDomain(DefaultId) != nullptr, false));
    Schema->SetNumberField(TEXT("defaultDomainFlag"), static_cast<int32>(DefaultId.Flag));
    Schema->SetNumberField(TEXT("defaultDomainCustomFlag"), DefaultId.CustomFlag);
    Schema->SetStringField(TEXT("rowSemantics"), Domain == TEXT("Data") ? TEXT("single_default_value") : TEXT("metadata_entry_keys"));
    SetDataSchemaInt64(Schema, TEXT("localEntryCount"), LocalEntryCount);
    SetDataSchemaInt64(Schema, TEXT("entryKeyCount"), EntryCount);
    Schema->SetNumberField(TEXT("attributeTotal"), AttributeCount);
    Schema->SetNumberField(TEXT("offset"), Offset);
    Schema->SetNumberField(TEXT("limit"), Limit);

    TArray<TSharedPtr<FJsonValue>> AttributeValues;
    const int32 End = static_cast<int32>(FMath::Min<int64>(AttributeCount, static_cast<int64>(Offset) + Limit));
    for (int32 Index = Offset; Index < End; ++Index)
    {
        if (!Budget.IsWithinTime()) return FailDataSchema(OutSchema, OutError, TEXT("PCG schema attribute projection exceeded its time budget."));
        const FPCGDataSchemaAttribute& Item = Attributes[Index];
        const FPCGMetadataAttributeBase* Attribute = RequestedDomain->GetConstAttribute(Item.Name);
        if (!Attribute) return FailDataSchema(OutSchema, OutError, TEXT("A listed PCG metadata attribute was unavailable on the const domain."));
        TSharedPtr<FJsonObject> Column = MakeShared<FJsonObject>();
        Column->SetStringField(TEXT("name"), Item.Text);
        FString DescriptorError;
        const bool bDescriptorReadable = PopulateDataSchemaDescriptor(Attribute, Column, DescriptorError);
        TSharedPtr<FJsonValue> DefaultValue;
        FString ValueError;
        const bool bDefaultAvailable = bDescriptorReadable && McpPCGDataValue::ExportPCGDataValue(
            Attribute, PCGInvalidEntryKey, DefaultValue, ValueError);
        Column->SetBoolField(TEXT("defaultAvailable"), bDefaultAvailable);
        if (bDefaultAvailable) Column->SetField(TEXT("defaultValue"), DefaultValue);
        else
        {
            const FString Reason = DescriptorError.IsEmpty() ? ValueError : DescriptorError;
            Column->SetStringField(TEXT("unavailableReason"), Reason.Len() > 0 && Reason.Len() <= 256 ? Reason : TEXT("PCG metadata default is unavailable."));
        }
        AttributeValues.Add(MakeShared<FJsonValueObject>(Column));
    }
    Schema->SetArrayField(TEXT("attributes"), AttributeValues);
    Schema->SetBoolField(TEXT("truncated"), static_cast<int64>(Offset) + AttributeValues.Num() < AttributeCount);
    FString Encoded;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded);
    if (!FJsonSerializer::Serialize(Schema, Writer) || Encoded.Len() > SchemaOutputLimit || !Budget.IsWithinTime())
        return FailDataSchema(OutSchema, OutError, TEXT("PCG schema exceeded its compact-output or time budget; no partial schema was returned."));
    OutSchema = MoveTemp(Schema);
    return true;
}
}
#endif
