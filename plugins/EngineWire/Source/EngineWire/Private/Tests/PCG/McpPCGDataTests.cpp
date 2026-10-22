#include "Misc/AutomationTest.h"
#include "Runtime/Launch/Resources/Version.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Data/PCGPointArrayData.h"
#include "Data/PCGPointData.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGConstPointDataTest, "EngineWire.PCG.Data.ConstPointRangesPreserveAllocation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGConstPointDataTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGPointData> Legacy(NewObject<UPCGPointData>(GetTransientPackage()));
    TStrongObjectPtr<UPCGPointArrayData> Array(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    for (UPCGBasePointData* Data : {static_cast<UPCGBasePointData*>(Legacy.Get()), static_cast<UPCGBasePointData*>(Array.Get())})
    {
        Data->SetNumPoints(3);
        Data->GetTransformValueRange()[1] = FTransform(FRotator(0, 90, 0), FVector(11, 22, 33), FVector(2, 3, 4));
        Data->GetDensityValueRange()[1] = 0.25f;
        Data->GetSeedValueRange()[1] = -17;
        Data->GetBoundsMinValueRange()[1] = FVector(-2, -3, -4);
        Data->GetBoundsMaxValueRange()[1] = FVector(2, 3, 4);
        Data->GetMetadataEntryValueRange()[1] = PCGInvalidEntryKey;
        const UPCGBasePointData* View = Data;
        const EPCGPointNativeProperties Allocated = View->GetAllocatedProperties();
        const bool bDirty = View->GetOutermost()->IsDirty();
        const auto Transforms = View->GetConstTransformValueRange();
        const auto Densities = View->GetConstDensityValueRange();
        const auto Seeds = View->GetConstSeedValueRange();
        const auto Minima = View->GetConstBoundsMinValueRange();
        const auto Maxima = View->GetConstBoundsMaxValueRange();
        const auto Colors = View->GetConstColorValueRange();
        const auto Steepness = View->GetConstSteepnessValueRange();
        const auto Entries = View->GetConstMetadataEntryValueRange();
        TestEqual(TEXT("Const range has the logical point count"), Transforms.Num(), 3);
        TestTrue(TEXT("Transform comes from the requested native row"), Transforms[1].Equals(
            FTransform(FRotator(0, 90, 0), FVector(11, 22, 33), FVector(2, 3, 4))));
        TestEqual(TEXT("Density is read without conversion"), Densities[1], 0.25f);
        TestEqual(TEXT("Negative seed is exact"), Seeds[1], -17);
        TestEqual(TEXT("Minimum bounds are read from this point"), Minima[1], FVector(-2, -3, -4));
        TestEqual(TEXT("Maximum bounds are read from this point"), Maxima[1], FVector(2, 3, 4));
        TestEqual(TEXT("A missing metadata entry remains invalid"), Entries[1], PCGInvalidEntryKey);
        TestEqual(TEXT("Unallocated color is a logical three-row range"), Colors.Num(), 3);
        TestEqual(TEXT("Unallocated steepness is a logical three-row range"), Steepness.Num(), 3);
        TestEqual(TEXT("Unallocated color exposes the native default"), Colors[2], FPCGPoint().Color);
        TestEqual(TEXT("Unallocated steepness exposes the native default"), Steepness[2], FPCGPoint().Steepness);
        TestEqual(TEXT("Reading const ranges allocates no native columns"), View->GetAllocatedProperties(), Allocated);
        TestEqual(TEXT("Reading const ranges does not dirty the package"), View->GetOutermost()->IsDirty(), bDirty);
    }
    TStrongObjectPtr<UPCGPointArrayData> Empty(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    const UPCGBasePointData* EmptyView = Empty.Get();
    TestEqual(TEXT("A valid empty dataset has zero points"), EmptyView->GetNumPoints(), 0);
    TestTrue(TEXT("An empty const range is not a missing dataset"), EmptyView->GetConstTransformValueRange().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGMetadataDataTest, "EngineWire.PCG.Data.MetadataDomainsAndTypedValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGMetadataDataTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGParamData> Data(NewObject<UPCGParamData>(GetTransientPackage()));
    UPCGMetadata* Metadata = Data->MutableMetadata();
    if (!TestNotNull(TEXT("Param data owns metadata"), Metadata)) return false;
    FPCGMetadataDomain* Elements = Metadata->GetMetadataDomain(PCGMetadataDomainID::Elements);
    FPCGMetadataDomain* Dataset = Metadata->GetMetadataDomain(PCGMetadataDomainID::Data);
    if (!TestNotNull(TEXT("Element domain exists"), Elements) || !TestNotNull(TEXT("Data domain exists"), Dataset)) return false;
    // Exported factories avoid importing LogPCG data through CreateAttribute<T> into a delay-loaded module.
    Metadata->CreateInteger64Attribute(TEXT("Exact"), 9007199254740993LL, false, false);
    Metadata->CreateVectorAttribute(TEXT("Position"), FVector::ZeroVector, true, false);
    Metadata->CreateTransformAttribute(TEXT("Pose"), FTransform::Identity, true, false);
    Metadata->CreateBoolAttribute(TEXT("Enabled"), true, false, false);
    auto* Exact = Elements->GetMutableTypedAttribute<int64>(TEXT("Exact"));
    auto* Position = Elements->GetMutableTypedAttribute<FVector>(TEXT("Position"));
    auto* Transform = Elements->GetMutableTypedAttribute<FTransform>(TEXT("Pose"));
    const auto* BooleanSource = Elements->GetConstAttribute(TEXT("Enabled"));
    if (!TestNotNull(TEXT("Native Boolean source exists"), BooleanSource)) return false;
    auto* Enabled = Dataset->CopyAttribute(BooleanSource, TEXT("Enabled"), false, false, false);
    if (!TestNotNull(TEXT("Native Int64 attribute created"), Exact) || !TestNotNull(TEXT("Native vector attribute created"), Position) ||
        !TestNotNull(TEXT("Native transform attribute created"), Transform) || !TestNotNull(TEXT("Data-domain attribute created"), Enabled)) return false;
    const PCGMetadataEntryKey Key = Elements->AddEntry();
    Exact->SetValue(Key, MAX_int64);
    Position->SetValue(Key, FVector(1.25, -2.5, 3.75));
    const FTransform Expected(FRotator(15, 30, 45), FVector(100, 200, 300), FVector(1, 2, 3));
    Transform->SetValue(Key, Expected);
    const UPCGData* View = Data.Get();
    const UPCGMetadata* ConstMetadata = View->ConstMetadata();
    const FPCGMetadataDomain* ConstElements = ConstMetadata->GetConstMetadataDomain(PCGMetadataDomainID::Elements);
    const FPCGMetadataDomain* ConstDataset = ConstMetadata->GetConstMetadataDomain(PCGMetadataDomainID::Data);
    if (!TestNotNull(TEXT("Const element domain exists"), ConstElements) || !TestNotNull(TEXT("Const data domain exists"), ConstDataset)) return false;
    const auto* ConstExact = ConstElements->GetConstTypedAttribute<int64>(TEXT("Exact"));
    const auto* ConstPosition = ConstElements->GetConstTypedAttribute<FVector>(TEXT("Position"));
    const auto* ConstTransform = ConstElements->GetConstTypedAttribute<FTransform>(TEXT("Pose"));
    const auto* ConstEnabled = ConstDataset->GetConstTypedAttribute<bool>(TEXT("Enabled"));
    if (!TestNotNull(TEXT("Const typed integer exists"), ConstExact) || !TestNotNull(TEXT("Const vector exists"), ConstPosition) ||
        !TestNotNull(TEXT("Const transform exists"), ConstTransform) || !TestNotNull(TEXT("Const data attribute exists"), ConstEnabled)) return false;
    TestTrue(TEXT("Both domains are advertised"), View->GetAllSupportedMetadataDomainIDs().Contains(PCGMetadataDomainID::Data) &&
        View->GetAllSupportedMetadataDomainIDs().Contains(PCGMetadataDomainID::Elements));
    TestTrue(TEXT("Elements is the Param default domain"), View->GetDefaultMetadataDomainID() == PCGMetadataDomainID::Elements);
    TestEqual(TEXT("Element-domain row count is not graph-parameter count"), ConstElements->GetLocalItemCount(), static_cast<int64>(1));
    TestEqual(TEXT("Maximum integer is read losslessly"), ConstExact->GetValueFromItemKey(Key), MAX_int64);
    TestEqual(TEXT("Default integer is not rounded through double"), ConstExact->GetValueFromItemKey(PCGInvalidEntryKey), 9007199254740993LL);
    TestEqual(TEXT("Vector is read from the selected entry"), ConstPosition->GetValueFromItemKey(Key), FVector(1.25, -2.5, 3.75));
    TestTrue(TEXT("Transform is read without reflected text export"), ConstTransform->GetValueFromItemKey(Key).Equals(Expected));
    TestTrue(TEXT("A single-valued Data-domain default is readable"), ConstEnabled->GetValueFromItemKey(PCGInvalidEntryKey));
    TestNull(TEXT("A missing attribute is not an empty schema"), ConstElements->GetConstAttribute(TEXT("Missing")));
    TArray<FName> Names;
    TArray<EPCGMetadataTypes> Types;
    ConstElements->GetAttributes(Names, Types);
    TestEqual(TEXT("All element attributes are enumerated"), Names.Num(), 4);
    TestEqual(TEXT("Each attribute has a declared native type"), Types.Num(), Names.Num());
    const int32 ExactIndex = Names.Find(FName(TEXT("Exact")));
    if (!TestTrue(TEXT("Typed schema includes the exact integer column"), Types.IsValidIndex(ExactIndex))) return false;
    TestTrue(TEXT("The integer schema retains its Int64 type"), Types[ExactIndex] == EPCGMetadataTypes::Integer64);
    return true;
}
#endif
