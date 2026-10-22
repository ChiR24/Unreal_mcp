#include "Misc/AutomationTest.h"
#include "Runtime/Launch/Resources/Version.h"

#ifndef MCP_HAS_PCG
#define MCP_HAS_PCG 0
#endif

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Domains/PCG/Data/McpPCGDataSchema.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGParamData.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
TSharedPtr<FJsonObject> ReadSchema(FAutomationTestBase& Test, const UPCGData* Data, const FString& Domain, int32 Offset, int32 Limit)
{
    TSharedPtr<FJsonObject> Schema;
    FString Error;
    const bool bRead = McpPCGHandlers::ReadPCGDataSchema(Data, Domain, Offset, Limit, Schema, Error);
    Test.TestTrue(TEXT("Bounded const schema read succeeds"), bRead);
    if (!bRead) Test.AddError(Error);
    return Schema;
}

TSharedPtr<FJsonObject> FindColumn(const TSharedPtr<FJsonObject>& Schema, const FString& Name)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Schema.IsValid() || !Schema->TryGetArrayField(TEXT("attributes"), Values) || !Values) return nullptr;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> Column = Value->AsObject();
        if (Column.IsValid() && Column->GetStringField(TEXT("name")) == Name) return Column;
    }
    return nullptr;
}

const TArray<TSharedPtr<FJsonValue>>* Columns(const TSharedPtr<FJsonObject>& Schema)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    return Schema.IsValid() && Schema->TryGetArrayField(TEXT("attributes"), Values) ? Values : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataSchemaReadTest, "EngineWire.PCG.DataSchema.MetadataDomainsPagingAndExactDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataSchemaReadTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGParamData> Data(NewObject<UPCGParamData>(GetTransientPackage()));
    UPCGMetadata* Metadata = Data->MutableMetadata();
    FPCGMetadataDomain* Elements = Metadata->GetMetadataDomain(PCGMetadataDomainID::Elements);
    FPCGMetadataDomain* Dataset = Metadata->GetMetadataDomain(PCGMetadataDomainID::Data);
    if (!TestNotNull(TEXT("Param element domain exists"), Elements) || !TestNotNull(TEXT("Param data domain exists"), Dataset)) return false;
    Metadata->CreateInteger32Attribute(TEXT("Alpha"), 7, false, false);
    Metadata->CreateInteger32Attribute(TEXT("Beta"), 9, true, false);
    Metadata->CreateInteger64Attribute(TEXT("MaxExact"), MAX_int64, false, false);
    Metadata->CreateInteger64Attribute(TEXT("MinExact"), MIN_int64, false, false);
    Elements->AddEntry();
    Dataset->CopyAttribute(Elements->GetConstAttribute(TEXT("Alpha")), TEXT("DataOnly"), false, false, false);
    const bool bDirty = Data->GetOutermost()->IsDirty();
    const int32 ElementAttributes = Elements->GetAttributeCount();
    const int32 DataAttributes = Dataset->GetAttributeCount();
    const int64 Entries = Elements->GetLocalItemCount();

    const TSharedPtr<FJsonObject> Schema = ReadSchema(*this, Data.Get(), TEXT("Elements"), 0, 4);
    if (!TestTrue(TEXT("Elements schema is returned"), Schema.IsValid())) return false;
    TestEqual(TEXT("Param data class is identified"), Schema->GetStringField(TEXT("dataClass")), FString(TEXT("PCGParamData")));
    TestEqual(TEXT("Queried domain is explicit"), Schema->GetStringField(TEXT("domainName")), FString(TEXT("Elements")));
    TestEqual(TEXT("Param defaults to Elements"), Schema->GetNumberField(TEXT("defaultDomainFlag")), static_cast<double>(EPCGMetadataDomainFlag::Elements));
    TestEqual(TEXT("Local metadata entry count is explicit"), Schema->GetNumberField(TEXT("localEntryCount")), 1.0);
    TestEqual(TEXT("Metadata key count is explicit"), Schema->GetNumberField(TEXT("entryKeyCount")), 1.0);
    TestEqual(TEXT("Elements row semantics do not claim point count"), Schema->GetStringField(TEXT("rowSemantics")), FString(TEXT("metadata_entry_keys")));
    TestEqual(TEXT("Attribute total is complete"), Schema->GetNumberField(TEXT("attributeTotal")), 4.0);
    TestEqual(TEXT("Page offset is reported"), Schema->GetNumberField(TEXT("offset")), 0.0);
    TestEqual(TEXT("Page limit is reported"), Schema->GetNumberField(TEXT("limit")), 4.0);
    TestFalse(TEXT("Complete first page is not truncated"), Schema->GetBoolField(TEXT("truncated")));
    TestEqual(TEXT("Default domain custom flag is retained"), Schema->GetNumberField(TEXT("defaultDomainCustomFlag")), -1.0);
    const TSharedPtr<FJsonObject> QueriedDomain = Schema->GetObjectField(TEXT("domain"));
    TestEqual(TEXT("Queried domain numeric flag is retained"), QueriedDomain->GetNumberField(TEXT("flag")), static_cast<double>(EPCGMetadataDomainFlag::Elements));
    TestEqual(TEXT("Queried domain custom flag is retained"), QueriedDomain->GetNumberField(TEXT("customFlag")), -1.0);
    TestTrue(TEXT("Queried domain is available"), QueriedDomain->GetBoolField(TEXT("available")));
    const TSharedPtr<FJsonObject> DefaultDomain = Schema->GetObjectField(TEXT("defaultDomain"));
    TestTrue(TEXT("Default domain identity is available"), DefaultDomain->GetBoolField(TEXT("available")));
    const TArray<TSharedPtr<FJsonValue>>* DomainIds = nullptr;
    if (!TestTrue(TEXT("Advertised domain identities are returned"), Schema->TryGetArrayField(TEXT("domains"), DomainIds) && DomainIds)) return false;
    bool bElementsMarked = false;
    for (const TSharedPtr<FJsonValue>& DomainValue : *DomainIds)
    {
        const TSharedPtr<FJsonObject> Id = DomainValue->AsObject();
        TestTrue(TEXT("Domain identity exposes numeric flag and custom flag"), Id->HasField(TEXT("flag")) && Id->HasField(TEXT("customFlag")));
        TestTrue(TEXT("Domain identity exposes descriptive name and availability"), Id->HasField(TEXT("debugName")) && Id->HasField(TEXT("available")));
        if (Id->GetStringField(TEXT("name")) == TEXT("Elements")) bElementsMarked = Id->GetBoolField(TEXT("queried"));
    }
    TestTrue(TEXT("Queried identity is marked in domain listing"), bElementsMarked);
    const TArray<TSharedPtr<FJsonValue>>* Values = Columns(Schema);
    if (!TestNotNull(TEXT("Attribute page exists"), Values)) return false;
    TestEqual(TEXT("Attribute names sort case-sensitively"), (*Values)[0]->AsObject()->GetStringField(TEXT("name")), FString(TEXT("Alpha")));
    TestEqual(TEXT("Page contains the requested columns"), Values->Num(), 4);
    const TSharedPtr<FJsonObject> Minimum = FindColumn(Schema, TEXT("MinExact"));
    const TSharedPtr<FJsonObject> Maximum = FindColumn(Schema, TEXT("MaxExact"));
    if (!TestTrue(TEXT("Exact Int64 columns exist"), Minimum.IsValid() && Maximum.IsValid())) return false;
    TestEqual(TEXT("Legacy metadata type id is reported"), Minimum->GetNumberField(TEXT("typeId")), static_cast<double>(EPCGMetadataTypes::Integer64));
    TestEqual(TEXT("Scalar type name is reported"), Minimum->GetStringField(TEXT("typeName")), FString(TEXT("Integer64")));
    TestFalse(TEXT("Interpolation capability is reported"), Minimum->GetBoolField(TEXT("allowsInterpolation")));
    TestTrue(TEXT("Minimum default is available"), Minimum->GetBoolField(TEXT("defaultAvailable")));
    TestEqual(TEXT("Int64 minimum default remains exact"), Minimum->GetStringField(TEXT("defaultValue")), FString(TEXT("-9223372036854775808")));
    TestEqual(TEXT("Int64 maximum default remains exact"), Maximum->GetStringField(TEXT("defaultValue")), FString(TEXT("9223372036854775807")));
    TestEqual(TEXT("Schema marks its queried domain"), Schema->GetObjectField(TEXT("domain"))->GetBoolField(TEXT("queried")), true);
    TestEqual(TEXT("Const schema reading does not change Elements attributes"), Elements->GetAttributeCount(), ElementAttributes);
    TestEqual(TEXT("Const schema reading does not change Data attributes"), Dataset->GetAttributeCount(), DataAttributes);
    TestEqual(TEXT("Const schema reading does not add entry keys"), Elements->GetLocalItemCount(), Entries);
    TestEqual(TEXT("Const schema reading does not dirty the package"), Data->GetOutermost()->IsDirty(), bDirty);

    const TSharedPtr<FJsonObject> Page = ReadSchema(*this, Data.Get(), TEXT("Elements"), 1, 2);
    if (!TestTrue(TEXT("Middle page is returned"), Page.IsValid())) return false;
    TestTrue(TEXT("A partial page reports truncation"), Page->GetBoolField(TEXT("truncated")));
    TestEqual(TEXT("Paged names keep deterministic order"), (*Columns(Page))[0]->AsObject()->GetStringField(TEXT("name")), FString(TEXT("Beta")));
    const TSharedPtr<FJsonObject> PastEnd = ReadSchema(*this, Data.Get(), TEXT("Elements"), 99, 2);
    if (!TestTrue(TEXT("Off-end page is an empty schema page"), PastEnd.IsValid())) return false;
    TestEqual(TEXT("Off-end page contains no attributes"), Columns(PastEnd)->Num(), 0);
    TestFalse(TEXT("Off-end page does not claim more results"), PastEnd->GetBoolField(TEXT("truncated")));

    const TSharedPtr<FJsonObject> DataSchema = ReadSchema(*this, Data.Get(), TEXT("Data"), 0, 4);
    if (!TestTrue(TEXT("Data-domain schema is returned"), DataSchema.IsValid())) return false;
    TestEqual(TEXT("Data domain has single-value row semantics"), DataSchema->GetStringField(TEXT("rowSemantics")), FString(TEXT("single_default_value")));
    TestEqual(TEXT("Data and Elements attributes remain distinct"), DataSchema->GetNumberField(TEXT("attributeTotal")), 1.0);
    TestEqual(TEXT("Data-only attribute is named"), (*Columns(DataSchema))[0]->AsObject()->GetStringField(TEXT("name")), FString(TEXT("DataOnly")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataSchemaEmptyAndRefusalTest, "EngineWire.PCG.DataSchema.EmptyDomainAndUnsupportedRequests",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataSchemaEmptyAndRefusalTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGParamData> Empty(NewObject<UPCGParamData>(GetTransientPackage()));
    const UPCGMetadata* Metadata = Empty->ConstMetadata();
    const FPCGMetadataDomain* Elements = Metadata->GetConstMetadataDomain(PCGMetadataDomainID::Elements);
    if (!TestNotNull(TEXT("Empty Elements domain is real and available"), Elements)) return false;
    const TSharedPtr<FJsonObject> Schema = ReadSchema(*this, Empty.Get(), TEXT("Elements"), 0, 8);
    if (!TestTrue(TEXT("Empty real domain has a schema"), Schema.IsValid())) return false;
    TestEqual(TEXT("Real empty domain reports zero attributes"), Schema->GetNumberField(TEXT("attributeTotal")), 0.0);

    TStrongObjectPtr<UPCGData> Generic(NewObject<UPCGData>(GetTransientPackage()));
    TSharedPtr<FJsonObject> Missing;
    FString Error;
    TestFalse(TEXT("Unadvertised Elements is not reported as an empty schema"),
        McpPCGHandlers::ReadPCGDataSchema(Generic.Get(), TEXT("Elements"), 0, 8, Missing, Error));
    TestFalse(TEXT("Missing domain fails with no schema"), Missing.IsValid());
    TestTrue(TEXT("Missing domain explains its refusal"), !Error.IsEmpty());
    TestFalse(TEXT("Unknown requested domain is refused"),
        McpPCGHandlers::ReadPCGDataSchema(Empty.Get(), TEXT("Custom"), 0, 8, Missing, Error));
    TestFalse(TEXT("Unknown domain leaves no schema"), Missing.IsValid());
    TestFalse(TEXT("Negative offsets are refused"), McpPCGHandlers::ReadPCGDataSchema(Empty.Get(), TEXT("Elements"), -1, 8, Missing, Error));
    TestFalse(TEXT("Pages above the bounded limit are refused"), McpPCGHandlers::ReadPCGDataSchema(Empty.Get(), TEXT("Elements"), 0, 33, Missing, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataSchemaBudgetTest, "EngineWire.PCG.DataSchema.BoundsAndPointReadOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataSchemaBudgetTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGPointData> Points(NewObject<UPCGPointData>(GetTransientPackage()));
    Points->SetNumPoints(2);
    const EPCGPointNativeProperties Allocated = Points->GetAllocatedProperties();
    const bool bDirty = Points->GetOutermost()->IsDirty();
    const TSharedPtr<FJsonObject> PointSchema = ReadSchema(*this, Points.Get(), TEXT("Elements"), 0, 8);
    if (!TestTrue(TEXT("Point metadata schema is available"), PointSchema.IsValid())) return false;
    TestEqual(TEXT("Schema entry count is metadata, not native point count"), PointSchema->GetNumberField(TEXT("localEntryCount")), 0.0);
    TestEqual(TEXT("Schema read allocates no point columns"), Points->GetAllocatedProperties(), Allocated);
    TestEqual(TEXT("Schema read does not dirty point data"), Points->GetOutermost()->IsDirty(), bDirty);

    TStrongObjectPtr<UPCGParamData> TooMany(NewObject<UPCGParamData>(GetTransientPackage()));
    UPCGMetadata* Metadata = TooMany->MutableMetadata();
    for (int32 Index = 0; Index < 257; ++Index)
        Metadata->CreateInteger32Attribute(FName(*FString::Printf(TEXT("Budget_%03d"), Index)), 0, false, false);
    const bool bTooManyDirty = TooMany->GetOutermost()->IsDirty();
    TSharedPtr<FJsonObject> Schema;
    FString Error;
    TestFalse(TEXT("Attribute count is rejected before enumeration"),
        McpPCGHandlers::ReadPCGDataSchema(TooMany.Get(), TEXT("Elements"), 0, 32, Schema, Error));
    TestFalse(TEXT("Oversized attribute schema is not partially returned"), Schema.IsValid());
    TestEqual(TEXT("Budget refusal does not alter metadata"), Metadata->GetConstMetadataDomain(PCGMetadataDomainID::Elements)->GetAttributeCount(), 257);
    TestEqual(TEXT("Budget refusal does not dirty the package"), TooMany->GetOutermost()->IsDirty(), bTooManyDirty);

    TStrongObjectPtr<UPCGParamData> LongSchema(NewObject<UPCGParamData>(GetTransientPackage()));
    const FString Value = FString::ChrN(1000, TEXT('v'));
    UPCGMetadata* LongMetadata = LongSchema->MutableMetadata();
    for (int32 Index = 0; Index < 32; ++Index)
    {
        const FString Suffix = FString::Printf(TEXT("_%02d"), Index);
        LongMetadata->CreateStringAttribute(FName(*(FString::ChrN(NAME_SIZE - 1 - Suffix.Len(), TEXT('n')) + Suffix)), Value, false, false);
    }
    TestFalse(TEXT("Compact serialized schema has a hard character ceiling"),
        McpPCGHandlers::ReadPCGDataSchema(LongSchema.Get(), TEXT("Elements"), 0, 32, Schema, Error));
    TestFalse(TEXT("Output-budget refusal returns no partial schema"), Schema.IsValid());
    return true;
}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataSchemaDescriptorTest, "EngineWire.PCG.DataSchema.GenericAndContainerDescriptors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataSchemaDescriptorTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGParamData> Data(NewObject<UPCGParamData>(GetTransientPackage()));
    UPCGMetadata* Metadata = Data->MutableMetadata();
    FPCGMetadataAttributeDesc ByteDesc;
    ByteDesc.Name = TEXT("GenericByte");
    ByteDesc.ValueType = EPCGMetadataTypes::Byte;
    FPCGMetadataAttributeDesc ArrayDesc;
    ArrayDesc.Name = TEXT("DoubleArray");
    ArrayDesc.ValueType = EPCGMetadataTypes::Double;
    ArrayDesc.ContainerTypes.Add(EPCGMetadataAttributeContainerTypes::Array);
    if (!TestNotNull(TEXT("Generic Byte fixture uses the public descriptor factory"), Metadata->CreateAttribute(
        FPCGAttributeIdentifier(ByteDesc.Name, PCGMetadataDomainID::Elements), ByteDesc, false, false))) return false;
    if (!TestNotNull(TEXT("Double array fixture uses the public descriptor factory"), Metadata->CreateAttribute(
        FPCGAttributeIdentifier(ArrayDesc.Name, PCGMetadataDomainID::Elements), ArrayDesc, false, false))) return false;
    const TSharedPtr<FJsonObject> Schema = ReadSchema(*this, Data.Get(), TEXT("Elements"), 0, 8);
    if (!TestTrue(TEXT("Descriptor schema is returned"), Schema.IsValid())) return false;
    const TSharedPtr<FJsonObject> Byte = FindColumn(Schema, TEXT("GenericByte"));
    const TSharedPtr<FJsonObject> Array = FindColumn(Schema, TEXT("DoubleArray"));
    if (!TestTrue(TEXT("Generic and array descriptors exist"), Byte.IsValid() && Array.IsValid())) return false;
    TestEqual(TEXT("Generic type is accurately named"), Byte->GetStringField(TEXT("valueTypeName")), FString(TEXT("Byte")));
    TestFalse(TEXT("Unsupported generic default is unavailable"), Byte->GetBoolField(TEXT("defaultAvailable")));
    TestEqual(TEXT("Array type is not mislabeled as scalar Double"), Array->GetStringField(TEXT("typeName")), FString(TEXT("Array<Double>")));
    TestEqual(TEXT("Array value type remains Double"), Array->GetStringField(TEXT("valueTypeName")), FString(TEXT("Double")));
    const TArray<TSharedPtr<FJsonValue>>* Containers = nullptr;
    TestTrue(TEXT("Descriptor reports its container kind"), Array->TryGetArrayField(TEXT("containerTypes"), Containers));
    if (Containers && Containers->Num() == 1)
        TestEqual(TEXT("Container descriptor is Array"), (*Containers)[0]->AsString(), FString(TEXT("Array")));
    TestFalse(TEXT("Double array default is unavailable"), Array->GetBoolField(TEXT("defaultAvailable")));
    TestTrue(TEXT("Unsupported default has a bounded reason"), Array->GetStringField(TEXT("unavailableReason")).Len() <= 256);
    return true;
}
#endif
#endif
