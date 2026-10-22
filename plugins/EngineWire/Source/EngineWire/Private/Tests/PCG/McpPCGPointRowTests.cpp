#include "Misc/AutomationTest.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Domains/PCG/Data/McpPCGPointRow.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Data/PCGPointArrayData.h"
#include "Data/PCGPointData.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#include <limits>

namespace
{
constexpr int64 MaxSafeJsonInteger = 9007199254740991LL;

TArray<FString> AllPointFields()
{
    return {TEXT("transform"), TEXT("density"), TEXT("seed"), TEXT("boundsMin"), TEXT("boundsMax"), TEXT("color"), TEXT("steepness"), TEXT("metadataEntry")};
}

double PointRowNumber(FAutomationTestBase& Test, const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
    double Value = 0.0;
    Test.TestTrue(TEXT("Expected numeric row field exists"), Object.IsValid() && Object->TryGetNumberField(Field, Value));
    return Value;
}

bool ExpectFailure(FAutomationTestBase& Test, const UPCGBasePointData* Data, int32 Index, const TArray<FString>& Fields)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetBoolField(TEXT("stale"), true);
    FString Error = TEXT("stale error");
    const bool bSuccess = McpPCGHandlers::ReadPCGPointRow(Data, Index, Fields, Row, Error);
    Test.TestFalse(TEXT("Invalid point-row request is refused"), bSuccess);
    Test.TestFalse(TEXT("Refusal clears stale or partial JSON"), Row.IsValid());
    Test.TestTrue(TEXT("Refusal reports an error"), !Error.IsEmpty());
    return !bSuccess && !Row.IsValid() && !Error.IsEmpty();
}

bool CheckNativeProjection(FAutomationTestBase& Test, UPCGBasePointData* Data)
{
    Data->SetNumPoints(6);
    const FQuat Rotation(FVector(1.0, 2.0, 3.0).GetSafeNormal(), FMath::DegreesToRadians(73.125));
    const FTransform Expected(Rotation, FVector(1234567890.1234567, -2.2345678901234567, 3.0), FVector(2.0, 3.0, 4.0));
    const FVector4 ExpectedColor(0.12500000000000003, 0.25000000000000006, 0.5000000000000001, 0.7500000000000002);
    Data->GetTransformValueRange()[1] = Expected;
    Data->GetDensityValueRange()[1] = 0.375f;
    Data->GetSeedValueRange()[1] = -1700000001;
    Data->GetBoundsMinValueRange()[1] = FVector(-1.125, -2.25, -3.5);
    Data->GetBoundsMaxValueRange()[1] = FVector(4.125, 5.25, 6.5);
    Data->GetColorValueRange()[1] = ExpectedColor;
    Data->GetSteepnessValueRange()[1] = 0.625f;
    const int64 MetadataEntries[] = {-9007199254740992LL, 9007199254740993LL, -9007199254740991LL,
        9007199254740991LL, MIN_int64, MAX_int64};
    for (int32 EntryIndex = 0; EntryIndex < UE_ARRAY_COUNT(MetadataEntries); ++EntryIndex)
        Data->GetMetadataEntryValueRange()[EntryIndex] = MetadataEntries[EntryIndex];

    const EPCGPointNativeProperties Allocated = Data->GetAllocatedProperties();
    const bool bDirty = Data->GetOutermost()->IsDirty();
    TSharedPtr<FJsonObject> Row;
    FString Error;
    const TArray<FString> Fields = AllPointFields();
    Test.TestTrue(TEXT("Native row is exported"), McpPCGHandlers::ReadPCGPointRow(Data, 1, Fields, Row, Error));
    if (!Test.TestTrue(TEXT("All eight requested fields are present"), Row.IsValid() && Row->Values.Num() == 8)) return false;

    const TSharedPtr<FJsonObject>* Transform = nullptr;
    const TSharedPtr<FJsonObject>* Location = nullptr;
    const TSharedPtr<FJsonObject>* JsonRotation = nullptr;
    const TSharedPtr<FJsonObject>* Scale = nullptr;
    if (!Test.TestTrue(TEXT("Transform contains location, quaternion rotation, and scale"),
        Row->TryGetObjectField(TEXT("transform"), Transform) && (*Transform)->TryGetObjectField(TEXT("location"), Location) &&
        (*Transform)->TryGetObjectField(TEXT("rotation"), JsonRotation) && (*Transform)->TryGetObjectField(TEXT("scale"), Scale))) return false;
    Test.TestEqual(TEXT("Location retains 17-digit native precision"), PointRowNumber(Test, *Location, TEXT("x")), 1234567890.1234567);
    Test.TestEqual(TEXT("Location y retains 17-digit native precision"), PointRowNumber(Test, *Location, TEXT("y")), -2.2345678901234567);
    Test.TestEqual(TEXT("Quaternion x is copied directly"), PointRowNumber(Test, *JsonRotation, TEXT("x")), Expected.GetRotation().X);
    Test.TestEqual(TEXT("Quaternion y is copied directly"), PointRowNumber(Test, *JsonRotation, TEXT("y")), Expected.GetRotation().Y);
    Test.TestEqual(TEXT("Quaternion z is copied directly"), PointRowNumber(Test, *JsonRotation, TEXT("z")), Expected.GetRotation().Z);
    Test.TestEqual(TEXT("Quaternion w is copied directly"), PointRowNumber(Test, *JsonRotation, TEXT("w")), Expected.GetRotation().W);
    Test.TestEqual(TEXT("Transform scale is preserved"), PointRowNumber(Test, *Scale, TEXT("z")), 4.0);
    Test.TestEqual(TEXT("Density stays a number"), PointRowNumber(Test, Row, TEXT("density")), 0.375);
    Test.TestEqual(TEXT("Int32 seed stays exact"), PointRowNumber(Test, Row, TEXT("seed")), -1700000001.0);
    Test.TestEqual(TEXT("Bounds minimum is native"), PointRowNumber(Test, Row->GetObjectField(TEXT("boundsMin")), TEXT("y")), -2.25);
    Test.TestEqual(TEXT("Bounds maximum is native"), PointRowNumber(Test, Row->GetObjectField(TEXT("boundsMax")), TEXT("z")), 6.5);
    const TSharedPtr<FJsonObject> JsonColor = Row->GetObjectField(TEXT("color"));
    Test.TestEqual(TEXT("Color r keeps 17-digit precision"), PointRowNumber(Test, JsonColor, TEXT("r")), ExpectedColor.X);
    Test.TestEqual(TEXT("Color g keeps 17-digit precision"), PointRowNumber(Test, JsonColor, TEXT("g")), ExpectedColor.Y);
    Test.TestEqual(TEXT("Color b keeps 17-digit precision"), PointRowNumber(Test, JsonColor, TEXT("b")), ExpectedColor.Z);
    Test.TestEqual(TEXT("Color a keeps 17-digit precision"), PointRowNumber(Test, JsonColor, TEXT("a")), ExpectedColor.W);
    Test.TestEqual(TEXT("Steepness stays a number"), PointRowNumber(Test, Row, TEXT("steepness")), 0.625);
    const TSharedPtr<FJsonValue>* MetadataEntry = Row->Values.Find(TEXT("metadataEntry"));
    if (!Test.TestTrue(TEXT("Unsafe Int64 metadata entry is exact decimal text"), MetadataEntry && (*MetadataEntry)->Type == EJson::String)) return false;
    Test.TestEqual(TEXT("Metadata entry text does not round"), (*MetadataEntry)->AsString(), FString(TEXT("9007199254740993")));
    for (int32 EntryIndex = 0; EntryIndex < UE_ARRAY_COUNT(MetadataEntries); ++EntryIndex)
    {
        TSharedPtr<FJsonObject> EntryRow;
        Test.TestTrue(TEXT("Each Int64 metadata boundary exports"), McpPCGHandlers::ReadPCGPointRow(Data, EntryIndex, {TEXT("metadataEntry")}, EntryRow, Error));
        const TSharedPtr<FJsonValue>* EntryValue = EntryRow.IsValid() ? EntryRow->Values.Find(TEXT("metadataEntry")) : nullptr;
        if (MetadataEntries[EntryIndex] < -MaxSafeJsonInteger || MetadataEntries[EntryIndex] > MaxSafeJsonInteger)
        {
            if (!Test.TestTrue(TEXT("Int64 outside JSON safe range is decimal text"), EntryValue && (*EntryValue)->Type == EJson::String)) return false;
            Test.TestEqual(TEXT("Unsafe Int64 keeps exact signed decimal digits"), (*EntryValue)->AsString(), LexToString(MetadataEntries[EntryIndex]));
        }
        else
        {
            if (!Test.TestTrue(TEXT("Safe Int64 remains a JSON number"), EntryValue && (*EntryValue)->Type == EJson::Number)) return false;
            Test.TestEqual(TEXT("Safe Int64 endpoint stays exact"), (*EntryValue)->AsNumber(), static_cast<double>(MetadataEntries[EntryIndex]));
        }
    }
    Test.TestEqual(TEXT("Const row export allocates no native columns"), Data->GetAllocatedProperties(), Allocated);
    Test.TestEqual(TEXT("Const row export does not dirty its package"), Data->GetOutermost()->IsDirty(), bDirty);

    Row.Reset();
    Test.TestTrue(TEXT("Single-field selection succeeds"), McpPCGHandlers::ReadPCGPointRow(Data, 1, {TEXT("density")}, Row, Error));
    Test.TestTrue(TEXT("Single-field selection emits no other properties"), Row.IsValid() && Row->Values.Num() == 1 && Row->Values.Contains(TEXT("density")));
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPointRowProjectionTest, "EngineWire.PCG.PointRow.NativeProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPointRowProjectionTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGPointData> Legacy(NewObject<UPCGPointData>(GetTransientPackage()));
    TStrongObjectPtr<UPCGPointArrayData> Array(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    return CheckNativeProjection(*this, Legacy.Get()) && CheckNativeProjection(*this, Array.Get());
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPointRowDefaultsTest, "EngineWire.PCG.PointRow.UnallocatedDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPointRowDefaultsTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGPointArrayData> Data(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    Data->SetNumPoints(1);
    TestTrue(TEXT("Unallocated transform has a logical range for each point"), Data->GetConstTransformValueRange().Num() == 1);
    const EPCGPointNativeProperties Allocated = Data->GetAllocatedProperties();
    TSharedPtr<FJsonObject> Row;
    FString Error;
    TestTrue(TEXT("Unallocated native columns export through their const defaults"),
        McpPCGHandlers::ReadPCGPointRow(Data.Get(), 0, AllPointFields(), Row, Error));
    if (!TestTrue(TEXT("Default row contains all fields"), Row.IsValid() && Row->Values.Num() == 8)) return false;
    const FPCGPoint Defaults;
    const TSharedPtr<FJsonObject> Transform = Row->GetObjectField(TEXT("transform"));
    TestEqual(TEXT("Default location x matches the native point default"), PointRowNumber(*this, Transform->GetObjectField(TEXT("location")), TEXT("x")), Defaults.Transform.GetLocation().X);
    TestEqual(TEXT("Default quaternion w matches the native point default"), PointRowNumber(*this, Transform->GetObjectField(TEXT("rotation")), TEXT("w")), Defaults.Transform.GetRotation().W);
    TestEqual(TEXT("Default density matches the native point default"), PointRowNumber(*this, Row, TEXT("density")), static_cast<double>(Defaults.Density));
    TestEqual(TEXT("Default seed matches the native point default"), PointRowNumber(*this, Row, TEXT("seed")), static_cast<double>(Defaults.Seed));
    TestEqual(TEXT("Default bounds minimum matches native point"), PointRowNumber(*this, Row->GetObjectField(TEXT("boundsMin")), TEXT("x")), Defaults.BoundsMin.X);
    TestEqual(TEXT("Default bounds maximum matches native point"), PointRowNumber(*this, Row->GetObjectField(TEXT("boundsMax")), TEXT("z")), Defaults.BoundsMax.Z);
    TestEqual(TEXT("Default color matches native point"), PointRowNumber(*this, Row->GetObjectField(TEXT("color")), TEXT("r")), Defaults.Color.X);
    TestEqual(TEXT("Default steepness matches the native point default"), PointRowNumber(*this, Row, TEXT("steepness")), static_cast<double>(Defaults.Steepness));
    TestEqual(TEXT("Default metadata entry matches native point"), PointRowNumber(*this, Row, TEXT("metadataEntry")), static_cast<double>(Defaults.MetadataEntry));
    TestEqual(TEXT("Reading defaults does not allocate native columns"), Data->GetAllocatedProperties(), Allocated);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPointRowRefusalTest, "EngineWire.PCG.PointRow.RejectsUnsafeReads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPointRowRefusalTest::RunTest(const FString& Parameters)
{
    FString FieldError = TEXT("stale");
    TestTrue(TEXT("Closed point fields validate without point data"), McpPCGHandlers::ValidatePCGPointFields(AllPointFields(), FieldError) && FieldError.IsEmpty());
    TestFalse(TEXT("Empty point fields are rejected independently"), McpPCGHandlers::ValidatePCGPointFields({}, FieldError));
    TestTrue(TEXT("Invalid field validation explains the refusal"), !FieldError.IsEmpty());
    TestTrue(TEXT("Successful validation clears the prior error"), McpPCGHandlers::ValidatePCGPointFields({TEXT("seed")}, FieldError) && FieldError.IsEmpty());

    TStrongObjectPtr<UPCGPointData> Empty(NewObject<UPCGPointData>(GetTransientPackage()));
    TestTrue(TEXT("An empty point dataset has an empty native range"), Empty->GetConstTransformValueRange().IsEmpty());
    if (!ExpectFailure(*this, nullptr, 0, {TEXT("seed")}) || !ExpectFailure(*this, Empty.Get(), 0, {TEXT("transform")}) ||
        !ExpectFailure(*this, Empty.Get(), -1, {TEXT("seed")})) return false;
    Empty->SetNumPoints(2);
    if (!ExpectFailure(*this, Empty.Get(), 2, {TEXT("seed")}) || !ExpectFailure(*this, Empty.Get(), 0, {}) ||
        !ExpectFailure(*this, Empty.Get(), 0, {TEXT("seed"), TEXT("seed")}) ||
        !ExpectFailure(*this, Empty.Get(), 0, {TEXT("unknown")})) return false;
    if (!ExpectFailure(*this, Empty.Get(), 0, {TEXT("transform"), TEXT("density"), TEXT("seed"), TEXT("boundsMin"), TEXT("boundsMax"),
        TEXT("color"), TEXT("steepness"), TEXT("metadataEntry"), TEXT("density")})) return false;

    Empty->GetDensityValueRange()[0] = std::numeric_limits<float>::infinity();
    const EPCGPointNativeProperties Allocated = Empty->GetAllocatedProperties();
    const bool bDirty = Empty->GetOutermost()->IsDirty();
    if (!ExpectFailure(*this, Empty.Get(), 0, {TEXT("transform"), TEXT("density")})) return false;
    TestEqual(TEXT("Failed export does not change allocated columns"), Empty->GetAllocatedProperties(), Allocated);
    TestEqual(TEXT("Failed export does not dirty the package"), Empty->GetOutermost()->IsDirty(), bDirty);
    Empty->GetBoundsMinValueRange()[0] = FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0);
    return ExpectFailure(*this, Empty.Get(), 0, {TEXT("boundsMin")});
}
#endif
