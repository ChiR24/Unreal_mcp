#include "Misc/AutomationTest.h"
#include "Domains/PCG/Data/McpPCGDataRows.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "Data/PCGPointArrayData.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataRowsPointTest, "EngineWire.PCG.DataRows.ExactSelectedPointRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataRowsPointTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGPointArrayData> Data(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    Data->SetNumPoints(3);
    Data->GetSeedValueRange()[1] = -71;
    UPCGMetadata* Metadata = Data->MutableMetadata();
    Metadata->CreateInteger64Attribute(TEXT("Exact"), MIN_int64, false, false);
    auto* Domain = Metadata->GetMetadataDomain(PCGMetadataDomainID::Elements);
    auto* Attribute = Domain->GetMutableTypedAttribute<int64>(TEXT("Exact"));
    if (!TestNotNull(TEXT("Owned Int64 fixture exists"), Attribute)) return false;
    const PCGMetadataEntryKey Key = Domain->AddEntry();
    Attribute->SetValue(Key, MAX_int64);
    Data->GetMetadataEntryValueRange()[1] = Key;
    const auto Allocated = Data->GetAllocatedProperties();
    const bool bDirty = Data->GetOutermost()->IsDirty();
    FPCGDataProjection Projection;
    Projection.Offset = 1;
    Projection.Limit = 1;
    Projection.Fields = {TEXT("seed")};
    Projection.Attributes = {TEXT("Exact")};
    TArray<TSharedPtr<FJsonValue>> Rows;
    int64 Total = -1;
    FString Error;
    TestTrue(TEXT("One explicit range is projected"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    if (!TestEqual(TEXT("One selected row is returned"), Rows.Num(), 1)) return false;
    const auto Row = Rows[0]->AsObject();
    TestEqual(TEXT("Source index is not rebased to projection index"), Row->GetNumberField(TEXT("sourceIndex")), 1.0);
    TestEqual(TEXT("Native source total is separate from captured row count"), Total, static_cast<int64>(3));
    TestEqual(TEXT("Requested seed is exact"), Row->GetObjectField(TEXT("fields"))->GetNumberField(TEXT("seed")), -71.0);
    TestEqual(TEXT("Metadata follows the point's actual native entry key"), Row->GetObjectField(TEXT("attributes"))->GetStringField(TEXT("Exact")), FString(TEXT("9223372036854775807")));
    Projection.Offset = 2;
    TestTrue(TEXT("Invalid point entry keys resolve the attribute default"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    if (!TestEqual(TEXT("Default row exists"), Rows.Num(), 1)) return false;
    TestEqual(TEXT("Default MIN Int64 is not rounded"), Rows[0]->AsObject()->GetObjectField(TEXT("attributes"))->GetStringField(TEXT("Exact")), FString(TEXT("-9223372036854775808")));
    TestEqual(TEXT("Typed capture allocates no native columns"), Data->GetAllocatedProperties(), Allocated);
    TestEqual(TEXT("Typed capture does not dirty the package"), Data->GetOutermost()->IsDirty(), bDirty);
    Projection.Offset = 3;
    TestTrue(TEXT("Off-end bounded projection is valid and empty"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    TestEqual(TEXT("Off-end projection retains no rows"), Rows.Num(), 0);
    Projection.Fields = {TEXT("unknown")};
    TestFalse(TEXT("Invalid fields are refused even off-end"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    TestEqual(TEXT("Refusal clears source total"), Total, static_cast<int64>(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataRowsDomainTest, "EngineWire.PCG.DataRows.ParamDomainsAndFailures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataRowsDomainTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGParamData> Data(NewObject<UPCGParamData>(GetTransientPackage()));
    UPCGMetadata* Metadata = Data->MutableMetadata();
    Metadata->CreateBoolAttribute(TEXT("Shared"), true, false, false);
    auto* Elements = Metadata->GetMetadataDomain(PCGMetadataDomainID::Elements);
    auto* Dataset = Metadata->GetMetadataDomain(PCGMetadataDomainID::Data);
    Dataset->CopyAttribute(Elements->GetConstAttribute(TEXT("Shared")), TEXT("Shared"), false, false, false);
    Elements->AddEntry();
    Elements->AddEntry();
    FPCGDataProjection Projection;
    Projection.Domain = TEXT("Data");
    Projection.Attributes = {TEXT("Shared")};
    TArray<TSharedPtr<FJsonValue>> Rows;
    int64 Total = -1;
    FString Error;
    TestTrue(TEXT("Data domain has single default-value row semantics"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    if (!TestEqual(TEXT("One Data-domain row exists"), Rows.Num(), 1)) return false;
    TestEqual(TEXT("Data domain is not the Elements row count"), Total, static_cast<int64>(1));
    TestTrue(TEXT("Single-valued Data attribute reads its native default"), Rows[0]->AsObject()->GetObjectField(TEXT("attributes"))->GetBoolField(TEXT("Shared")));
    Projection.Domain = TEXT("Elements");
    TestTrue(TEXT("Param Elements rows use metadata entry keys"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    TestEqual(TEXT("Both Param element entries are visible"), Total, static_cast<int64>(2));
    Projection.Attributes = {TEXT("Shared"), TEXT("shared")};
    TestFalse(TEXT("Duplicate native names cannot masquerade as distinct columns"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    Projection.Attributes = {TEXT("Missing")};
    TestFalse(TEXT("Missing attribute is not an empty or zero column"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    Projection.Attributes = {TEXT("Shared")};
    Projection.Fields = {TEXT("seed")};
    TestFalse(TEXT("Param data is not implicitly converted to points"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    Projection.Fields.Reset();
    Projection.Domain = TEXT("Custom");
    TestFalse(TEXT("Unsupported domains are explicit"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    Projection.Domain = TEXT("Elements");
    Projection.Limit = FPCGDataSnapshot::MaxRows + 1;
    TestFalse(TEXT("Oversized explicit ranges are refused before copying"), BuildPCGDataRows(Data.Get(), Projection, Rows, Total, Error));
    TestEqual(TEXT("Refusal never exposes partial rows"), Rows.Num(), 0);
    return true;
}
#endif
