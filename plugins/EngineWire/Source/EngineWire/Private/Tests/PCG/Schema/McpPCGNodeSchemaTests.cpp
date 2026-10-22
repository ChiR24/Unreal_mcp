#include "Misc/AutomationTest.h"
#include "Domains/PCG/Discovery/Schema/McpPCGNodeSchemaAction.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGNodeSchemaReadTest, "EngineWire.PCG.NodeSchema.OwnedSelectorMetadataAndReadInvariance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGNodeSchemaReadTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGStaticMeshSpawnerSettings"));
    if (!TestNotNull(TEXT("Fixture settings class is already loaded"), Class)) return false;
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(Class, Settings);
    if (!Node || !Settings) return false;
    auto* Spawner = static_cast<UPCGStaticMeshSpawnerSettings*>(Settings);
    UObject* Selector = Spawner->MeshSelectorParameters;
    if (!TestNotNull(TEXT("Fixture selector is actually initialized"), Selector)) return false;
    const FString Revision = McpPCGHandlers::GetPCGGraphRevision(Graph.Get());
    if (!TestFalse(TEXT("Initial complete authored fingerprint exists"), Revision.IsEmpty())) return false;
    const bool bDirty = Graph->GetOutermost()->IsDirty();
    FString Error;
    auto Result = McpPCGHandlers::ReadPCGNodeSchema(Graph.Get(), Node, TEXT("MeshSelectorParameters"), 0, 16, Error);
    if (!TestTrue(TEXT("Owned selector instance schema succeeds"), Result.IsValid())) { AddError(Error); return false; }
    TestEqual(TEXT("Actual settings class is explicit"), Result->GetStringField(TEXT("settingsClass")), Settings->GetClass()->GetPathName());
    TestEqual(TEXT("Actual selector class, not a CDO guess"), Result->GetStringField(TEXT("scopeType")), Selector->GetClass()->GetPathName());
    TestEqual(TEXT("Scope category is owned object"), Result->GetStringField(TEXT("scopeKind")), FString(TEXT("owned_object")));
    TestTrue(TEXT("Metadata only is explicit"), Result->GetBoolField(TEXT("metadataOnly")));
    TestFalse(TEXT("No instance values were exported"), Result->GetBoolField(TEXT("valuesIncluded")));
    TestTrue(TEXT("No object load occurred through this read"), Result->GetBoolField(TEXT("loadedObjectsOnly")));
    TestEqual(TEXT("Mutation boundary is not inferred from reflection"), Result->GetStringField(TEXT("editPolicy")), FString(TEXT("existing_native_staged_preflight_required")));
    const auto Schema = Result->GetObjectField(TEXT("schema"));
    bool bFoundEntries = false;
    for (const auto& Value : Schema->GetArrayField(TEXT("items")))
    {
        const auto Item = Value->AsObject();
        if (Item->GetStringField(TEXT("name")) != TEXT("MeshEntries")) continue;
        bFoundEntries = true;
        TestEqual(TEXT("Selector array shape is discoverable"), Item->GetObjectField(TEXT("typeDetails"))->GetStringField(TEXT("container")), FString(TEXT("array")));
    }
    TestTrue(TEXT("Actual initialized weighted selector exposes MeshEntries"), bFoundEntries);
    TestTrue(TEXT("Read leaves the exact selector object in place"), Spawner->MeshSelectorParameters.Get() == Selector);
    TestEqual(TEXT("Read leaves authored graph revision unchanged"), McpPCGHandlers::GetPCGGraphRevision(Graph.Get()), Revision);
    TestEqual(TEXT("Read leaves package dirty state unchanged"), Graph->GetOutermost()->IsDirty(), bDirty);
    Result = McpPCGHandlers::ReadPCGNodeSchema(Graph.Get(), Node, TEXT("InstanceDataPackerParameters"), 0, 16, Error);
    TestFalse(TEXT("Null packer scope is refused rather than created"), Result.IsValid());
    TestNull(TEXT("Refused read leaves the native packer null"), Spawner->InstanceDataPackerParameters.Get());
    Result = McpPCGHandlers::ReadPCGNodeSchema(Graph.Get(), Node, TEXT(""), 0, 33, Error);
    TestFalse(TEXT("Invalid page size cannot bypass descriptor bounds"), Result.IsValid());
    return true;
}
#endif
