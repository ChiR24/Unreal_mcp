#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace {

TSharedPtr<FJsonObject> MakeLightingSnapshot(
    const double TimeOfDay, const double SunIntensity, const double SkylightIntensity)
{
    TSharedPtr<FJsonObject> Snapshot = MakeShared<FJsonObject>();
    Snapshot->SetNumberField(TEXT("timeOfDay"), TimeOfDay);
    Snapshot->SetNumberField(TEXT("sunIntensity"), SunIntensity);
    Snapshot->SetNumberField(TEXT("skylightIntensity"), SkylightIntensity);
    return Snapshot;
}

struct FParsedLighting
{
    bool bParsed = false;
    double TimeOfDay = 0.0;
    double SunIntensity = 0.0;
    double SkylightIntensity = 0.0;
    FRotator Rotation = FRotator::ZeroRotator;
};

FParsedLighting ParseLighting(const TSharedPtr<FJsonObject> &Snapshot)
{
    FParsedLighting Out;
    Out.bParsed = McpEnvironmentHandlers::McpParseEnvironmentSnapshotLighting(
        Snapshot, Out.TimeOfDay, Out.SunIntensity, Out.SkylightIntensity, Out.Rotation);
    return Out;
}

TSharedPtr<FJsonObject> MakeVersionedLightingSnapshot(const double Version)
{
    TSharedPtr<FJsonObject> Snapshot = MakeLightingSnapshot(6.0, 7.25, 1.5);
    Snapshot->SetNumberField(TEXT("version"), Version);
    TSharedPtr<FJsonObject> Rotation = MakeShared<FJsonObject>();
    Rotation->SetNumberField(TEXT("pitch"), 0.0);
    Rotation->SetNumberField(TEXT("yaw"), 45.0);
    Rotation->SetNumberField(TEXT("roll"), 0.0);
    Snapshot->SetObjectField(TEXT("directionalLightRotation"), Rotation);
    return Snapshot;
}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpEnvironmentSnapshotUnversionedSchemaTest,
    "McpAutomationBridge.Environment.SnapshotSchema.UnversionedDerivesRotation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpEnvironmentSnapshotUnversionedSchemaTest::RunTest(const FString &Parameters)
{
    (void)Parameters;
    const TSharedPtr<FJsonObject> Snapshot = MakeLightingSnapshot(9.5, 7.25, 1.5);
    const auto [bParsed, TimeOfDay, SunIntensity, SkylightIntensity, Rotation] = ParseLighting(Snapshot);

    TestTrue(TEXT("Unversioned lighting snapshot parses"), bParsed);
    TestTrue(TEXT("Time of day is preserved"), FMath::IsNearlyEqual(TimeOfDay, 9.5));
    TestTrue(TEXT("Sun intensity is preserved"), FMath::IsNearlyEqual(SunIntensity, 7.25));
    TestTrue(TEXT("Skylight intensity is preserved"), FMath::IsNearlyEqual(SkylightIntensity, 1.5));
    TestTrue(TEXT("Rotation pitch is derived from time of day"), FMath::IsNearlyEqual(Rotation.Pitch, 52.5));
    TestTrue(TEXT("Derived rotation yaw is zero"), FMath::IsNearlyZero(Rotation.Yaw));
    TestTrue(TEXT("Derived rotation roll is zero"), FMath::IsNearlyZero(Rotation.Roll));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpEnvironmentSnapshotVersionedMissingRotationSchemaTest,
    "McpAutomationBridge.Environment.SnapshotSchema.VersionedMissingRotationFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpEnvironmentSnapshotVersionedMissingRotationSchemaTest::RunTest(const FString &Parameters)
{
    (void)Parameters;
    const TSharedPtr<FJsonObject> Snapshot = MakeLightingSnapshot(12.0, 5.0, 1.0);
    Snapshot->SetNumberField(TEXT("version"), 1);
    TestFalse(TEXT("Versioned snapshot requires an explicit rotation"), ParseLighting(Snapshot).bParsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpEnvironmentSnapshotMalformedRotationSchemaTest,
    "McpAutomationBridge.Environment.SnapshotSchema.MalformedExplicitRotationFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpEnvironmentSnapshotMalformedRotationSchemaTest::RunTest(const FString &Parameters)
{
    (void)Parameters;
    const TSharedPtr<FJsonObject> Snapshot = MakeLightingSnapshot(12.0, 5.0, 1.0);
    TSharedPtr<FJsonObject> RotationObject = MakeShared<FJsonObject>();
    RotationObject->SetStringField(TEXT("pitch"), TEXT("not-a-number"));
    RotationObject->SetNumberField(TEXT("yaw"), 0.0);
    RotationObject->SetNumberField(TEXT("roll"), 0.0);
    Snapshot->SetObjectField(TEXT("directionalLightRotation"), RotationObject);
    TestFalse(TEXT("Malformed explicit rotation is rejected"), ParseLighting(Snapshot).bParsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpEnvironmentSnapshotSupportedVersionSchemaTest,
    "McpAutomationBridge.Environment.SnapshotSchema.SupportedVersionParses",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpEnvironmentSnapshotSupportedVersionSchemaTest::RunTest(const FString &Parameters)
{
    (void)Parameters;
    const TSharedPtr<FJsonObject> Snapshot = MakeVersionedLightingSnapshot(1.0);
    TestTrue(TEXT("Supported snapshot version parses"), ParseLighting(Snapshot).bParsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpEnvironmentSnapshotUnknownVersionSchemaTest,
    "McpAutomationBridge.Environment.SnapshotSchema.UnknownVersionFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpEnvironmentSnapshotUnknownVersionSchemaTest::RunTest(const FString &Parameters)
{
    (void)Parameters;
    const TSharedPtr<FJsonObject> Snapshot = MakeVersionedLightingSnapshot(2.0);
    TestFalse(TEXT("Unknown snapshot version is rejected"), ParseLighting(Snapshot).bParsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpEnvironmentSnapshotFractionalVersionSchemaTest,
    "McpAutomationBridge.Environment.SnapshotSchema.FractionalVersionFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpEnvironmentSnapshotFractionalVersionSchemaTest::RunTest(const FString &Parameters)
{
    (void)Parameters;
    const TSharedPtr<FJsonObject> Snapshot = MakeVersionedLightingSnapshot(1.5);
    TestFalse(TEXT("Fractional snapshot version is rejected"), ParseLighting(Snapshot).bParsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpEnvironmentSnapshotNonnumericVersionSchemaTest,
    "McpAutomationBridge.Environment.SnapshotSchema.NonnumericVersionFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpEnvironmentSnapshotNonnumericVersionSchemaTest::RunTest(const FString &Parameters)
{
    (void)Parameters;
    const TSharedPtr<FJsonObject> Snapshot = MakeVersionedLightingSnapshot(1.0);
    Snapshot->SetStringField(TEXT("version"), TEXT("1"));
    TestFalse(TEXT("Nonnumeric snapshot version is rejected"), ParseLighting(Snapshot).bParsed);
    return true;
}

#endif
