#include "Misc/AutomationTest.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "InstanceDataPackers/PCGInstanceDataPackerByAttribute.h"
#include "MeshSelectors/PCGMeshSelectorByAttribute.h"
#include "MeshSelectors/PCGMeshSelectorWeightedByCategory.h"
#include "UObject/GarbageCollection.h"

namespace
{
struct FSpawnerFixture
{
    TStrongObjectPtr<UPCGGraph> Graph{NewObject<UPCGGraph>(GetTransientPackage())};
    UPCGNode* Node = nullptr;
    UPCGStaticMeshSpawnerSettings* Settings = nullptr;
    FSpawnerFixture()
    {
        UPCGSettings* Created = nullptr;
        Node = Graph->AddNodeOfType(UPCGStaticMeshSpawnerSettings::StaticClass(), Created);
        Settings = Cast<UPCGStaticMeshSpawnerSettings>(Created);
    }
};

TSharedPtr<FJsonObject> SwitchPayload()
{
    auto Fields = MakeShared<FJsonObject>();
    Fields->SetStringField(TEXT("MeshSelectorParameters.AttributeName"), TEXT("ChosenMesh"));
    Fields->SetArrayField(TEXT("InstanceDataPackerParameters.AttributeSelectors"), {MakeShared<FJsonValueString>(TEXT("PCGBegin($Density)PCGEnd"))});
    Fields->SetStringField(TEXT("InstanceDataPackerType"), TEXT("/Script/PCG.PCGInstanceDataPackerByAttribute"));
    Fields->SetStringField(TEXT("MeshSelectorType"), TEXT("/Script/PCG.PCGMeshSelectorByAttribute"));
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetObjectField(TEXT("settings"), Fields);
    return Payload;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSpawnerTypeInitTest, "EngineWire.PCG.SpawnerTypes.InitializeBeforeDependentFields",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSpawnerTypeInitTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    FSpawnerFixture Fixture;
    auto* Settings = Fixture.Settings;
    if (!TestNotNull(TEXT("Owned native spawner"), Settings)) return false;
    UPCGMeshSelectorBase* Original = Settings->MeshSelectorParameters;
    const FString Before = GetPCGGraphRevision(Fixture.Graph.Get());
    if (!TestFalse(TEXT("Complete original revision"), Before.IsEmpty())) return false;
    FString Error;
    auto Payload = SwitchPayload();
    TestTrue(TEXT("Class-dependent fields prevalidate on the new native type"),
        ValidatePCGNodeSettings(Fixture.Graph.Get(), Fixture.Node, TEXT("set_pcg_node_settings"), Payload, Error));
    TestEqual(TEXT("Preflight does not replace the source selector"), Settings->MeshSelectorParameters.Get(), Original);
    TestEqual(TEXT("Preflight preserves original authored state"), GetPCGGraphRevision(Fixture.Graph.Get()), Before);
    int32 Applied = 0;
    TestTrue(TEXT("Types initialize before dependent paths regardless of field insertion order"),
        ApplyPCGSettingsPayload(Settings, TEXT("set_pcg_node_settings"), Payload, Error, Applied));
    TestEqual(TEXT("Every type and dependent field counted once"), Applied, 4);
    auto* Selector = Cast<UPCGMeshSelectorByAttribute>(Settings->MeshSelectorParameters);
    auto* Packer = Cast<UPCGInstanceDataPackerByAttribute>(Settings->InstanceDataPackerParameters);
    if (!TestNotNull(TEXT("Actual native selector"), Selector) || !TestNotNull(TEXT("Actual native packer"), Packer)) return false;
    TestEqual(TEXT("Selector class and object agree"), Settings->MeshSelectorType.Get(), Selector->GetClass());
    TestEqual(TEXT("Packer class and object agree"), Settings->InstanceDataPackerType.Get(), Packer->GetClass());
    TestTrue(TEXT("Selector is privately owned"), Selector->GetOuter() == Settings);
    TestTrue(TEXT("Packer is privately owned"), Packer->GetOuter() == Settings);
    TestEqual(TEXT("Dependent attribute persisted"), Selector->AttributeName, FName(TEXT("ChosenMesh")));
    if (!TestEqual(TEXT("One selector imported through native export text"), Packer->AttributeSelectors.Num(), 1)) return false;
    TestEqual(TEXT("Density property selector imported accurately"), Packer->AttributeSelectors[0].GetPropertyName(), FName(TEXT("Density")));
    TestEqual(TEXT("Replacement does not inherit prototype-only transient flags"), Selector->HasAnyFlags(RF_Transient), Settings->HasAnyFlags(RF_Transient));
    TestTrue(TEXT("Original live object was not garbage-marked"), IsValid(Original));
    auto SameFields = MakeShared<FJsonObject>();
    SameFields->SetStringField(TEXT("MeshSelectorType"), TEXT("PCGMeshSelectorByAttribute"));
    SameFields->SetStringField(TEXT("InstanceDataPackerType"), TEXT("PCGInstanceDataPackerByAttribute"));
    TestTrue(TEXT("Same native types can be restated"), ApplySettingsObject(Settings, SameFields, Error, Applied));
    TestTrue(TEXT("Same selector object preserved"), Settings->MeshSelectorParameters == Selector);
    TestTrue(TEXT("Same packer object preserved"), Settings->InstanceDataPackerParameters == Packer);
    TestEqual(TEXT("Same type does not reset dependent values"), Selector->AttributeName, FName(TEXT("ChosenMesh")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSpawnerTypeRefusalTest, "EngineWire.PCG.SpawnerTypes.InvalidPreflightLeavesSourceUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSpawnerTypeRefusalTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    FSpawnerFixture Fixture;
    if (!TestNotNull(TEXT("Native spawner"), Fixture.Settings)) return false;
    UPCGMeshSelectorBase* Original = Fixture.Settings->MeshSelectorParameters;
    const FString Before = GetPCGGraphRevision(Fixture.Graph.Get());
    const bool bDirty = Fixture.Graph->GetOutermost()->IsDirty();
    if (!TestFalse(TEXT("Nonempty source revision"), Before.IsEmpty())) return false;
    TArray<TSharedPtr<FJsonObject>> Invalid;
    for (const TCHAR* Class : {TEXT("/Script/Engine.Actor"), TEXT("/Script/PCG.PCGMeshSelectorBase"),
        TEXT("/Game/MCP_NeverLoaded.MCP_NeverLoaded_C")})
    {
        auto Payload = SwitchPayload();
        Payload->GetObjectField(TEXT("settings"))->SetStringField(TEXT("MeshSelectorType"), Class);
        Invalid.Add(Payload);
    }
    auto Missing = SwitchPayload();
    Missing->GetObjectField(TEXT("settings"))->SetNumberField(TEXT("MeshSelectorParameters.MissingAfterSwitch"), 7);
    Invalid.Add(Missing);
    auto Pointer = SwitchPayload();
    Pointer->GetObjectField(TEXT("settings"))->SetField(TEXT("MeshSelectorParameters"), MakeShared<FJsonValueNull>());
    Invalid.Add(Pointer);
    auto Duplicate = SwitchPayload();
    Duplicate->GetObjectField(TEXT("settings"))->SetStringField(TEXT("meshselectortype"), TEXT("PCGMeshSelectorWeighted"));
    Invalid.Add(Duplicate);
    auto BadElement = MakeShared<FJsonObject>();
    auto WeightedFields = MakeShared<FJsonObject>();
    WeightedFields->SetStringField(TEXT("MeshSelectorType"), TEXT("PCGMeshSelectorWeighted"));
    WeightedFields->SetArrayField(TEXT("MeshSelectorParameters.MeshEntries"), {MakeShared<FJsonValueBoolean>(false)});
    BadElement->SetObjectField(TEXT("settings"), WeightedFields);
    Invalid.Add(BadElement);
    auto Conflict = SwitchPayload();
    Conflict->SetStringField(TEXT("meshPath"), TEXT("/Engine/BasicShapes/Cube.Cube"));
    Invalid.Add(Conflict);
    for (const auto& Payload : Invalid)
    {
        FString Error;
        TestFalse(TEXT("Invalid class, pointer, element or dependent setting refused before source mutation"),
            ValidatePCGNodeSettings(Fixture.Graph.Get(), Fixture.Node, TEXT("set_pcg_node_settings"), Payload, Error));
        TestFalse(TEXT("Refusal explains its boundary"), Error.IsEmpty());
        TestEqual(TEXT("Authored revision unchanged"), GetPCGGraphRevision(Fixture.Graph.Get()), Before);
        TestEqual(TEXT("Selector identity unchanged"), Fixture.Settings->MeshSelectorParameters.Get(), Original);
        TestEqual(TEXT("Dirty state unchanged"), Fixture.Graph->GetOutermost()->IsDirty(), bDirty);
        TestTrue(TEXT("Original remains valid"), IsValid(Original));
        TestNull(TEXT("No packer created on source"), Fixture.Settings->InstanceDataPackerParameters.Get());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSpawnerTypeRecoveryTest, "EngineWire.PCG.SpawnerTypes.RecoveryPreservesOriginalObjectsAndGC",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSpawnerTypeRecoveryTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    FSpawnerFixture Fixture;
    auto* Settings = Fixture.Settings;
    if (!TestNotNull(TEXT("Native spawner"), Settings)) return false;
    UPCGMeshSelectorBase* Original = Settings->MeshSelectorParameters;
    const FString Before = GetPCGGraphRevision(Fixture.Graph.Get());
    const bool bDirty = Fixture.Graph->GetOutermost()->IsDirty();
    if (!TestFalse(TEXT("Nonempty revision"), Before.IsEmpty())) return false;
    TWeakObjectPtr<UPCGMeshSelectorBase> Discarded;
    {
        FPCGEditState State(Fixture.Graph.Get());
        FString Error;
        int32 Applied = 0;
        if (!TestTrue(TEXT("Native switch applied for explicit recovery probe"),
            ApplyPCGSettingsPayload(Settings, TEXT("set_pcg_node_settings"), SwitchPayload(), Error, Applied))) return false;
        Discarded = Settings->MeshSelectorParameters;
        TestTrue(TEXT("Different owned object initialized"), Discarded.Get() != Original);
        TestTrue(TEXT("Old object still usable before restore"), IsValid(Original));
        TestTrue(TEXT("Existing authored recovery restores type changes"), State.Restore());
        TestTrue(TEXT("Original exact object restored"), Settings->MeshSelectorParameters.Get() == Original);
        TestEqual(TEXT("Original native class restored"), Settings->MeshSelectorType.Get(), UPCGMeshSelectorWeighted::StaticClass());
        TestNull(TEXT("Original null packer restored"), Settings->InstanceDataPackerParameters.Get());
        TestTrue(TEXT("Discarded child removed from target ownership"), Discarded.IsValid() && Discarded->GetOuter() != Settings);
        TestEqual(TEXT("Authored revision fully restored"), GetPCGGraphRevision(Fixture.Graph.Get()), Before);
        TestEqual(TEXT("Package dirty state restored"), Fixture.Graph->GetOutermost()->IsDirty(), bDirty);
    }
    CollectGarbage(RF_NoFlags);
    TestTrue(TEXT("Restored original survives GC through graph ownership"), IsValid(Original));
    TestFalse(TEXT("Discarded replacement is not retained"), Discarded.IsValid());
    FString Error;
    int32 Applied = 0;
    if (!TestTrue(TEXT("Switch can be applied after recovery and GC"),
        ApplyPCGSettingsPayload(Settings, TEXT("set_pcg_node_settings"), SwitchPayload(), Error, Applied))) return false;
    const FString Switched = GetPCGGraphRevision(Fixture.Graph.Get());
    if (!TestFalse(TEXT("Complete switched revision"), Switched.IsEmpty())) return false;
    CollectGarbage(RF_NoFlags);
    TestEqual(TEXT("Retired parameter GC does not change authored revision"), GetPCGGraphRevision(Fixture.Graph.Get()), Switched);
    auto ClearFields = MakeShared<FJsonObject>();
    ClearFields->SetField(TEXT("InstanceDataPackerType"), MakeShared<FJsonValueNull>());
    TestTrue(TEXT("Packer can be explicitly disabled"), ApplySettingsObject(Settings, ClearFields, Error, Applied));
    TestNull(TEXT("Packer class cleared"), Settings->InstanceDataPackerType.Get());
    TestNull(TEXT("Packer object cleared"), Settings->InstanceDataPackerParameters.Get());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSpawnerTypeOwnerTest, "EngineWire.PCG.SpawnerTypes.RefusesSharedAndDefaultParameters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSpawnerTypeOwnerTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    FSpawnerFixture Fixture;
    auto* Settings = Fixture.Settings;
    if (!TestNotNull(TEXT("Native spawner"), Settings)) return false;
    TStrongObjectPtr<UPCGMeshSelectorWeighted> Foreign(NewObject<UPCGMeshSelectorWeighted>(GetTransientPackage()));
    auto* Default = GetMutableDefault<UPCGMeshSelectorWeighted>();
    const auto Fields = SwitchPayload()->GetObjectField(TEXT("settings"));
    for (UPCGMeshSelectorBase* Unowned : {static_cast<UPCGMeshSelectorBase*>(Foreign.Get()), static_cast<UPCGMeshSelectorBase*>(Default)})
    {
        Settings->MeshSelectorParameters = Unowned;
        FString Error;
        int32 Applied = 0;
        TestFalse(TEXT("Shared/CDO parameters cannot enter a native type switch"), ApplySettingsObject(Settings, Fields, Error, Applied));
        TestTrue(TEXT("Source pointer unchanged"), Settings->MeshSelectorParameters == Unowned);
        TestEqual(TEXT("Source class unchanged"), Settings->MeshSelectorType.Get(), UPCGMeshSelectorWeighted::StaticClass());
        TestNull(TEXT("No packer initialized on refusal"), Settings->InstanceDataPackerParameters.Get());
        TestTrue(TEXT("Unowned source remains valid"), IsValid(Unowned));
    }
    auto* Unsupported = NewObject<UPCGMeshSelectorWeightedByCategory>(Settings);
    Settings->MeshSelectorType = UPCGMeshSelectorWeightedByCategory::StaticClass();
    Settings->MeshSelectorParameters = Unsupported;
    FString Error;
    int32 Applied = 0;
    TestFalse(TEXT("Unproved owned native parameter classes also refuse replacement"), ApplySettingsObject(Settings, Fields, Error, Applied));
    TestTrue(TEXT("Unsupported owned pointer unchanged"), Settings->MeshSelectorParameters.Get() == Unsupported);
    TestTrue(TEXT("Unsupported owned object was not detached"), Unsupported->GetOuter() == Settings);
    return true;
}
#endif
