#include "Misc/AutomationTest.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectHash.h"

namespace
{
TSharedPtr<FJsonObject> ParseFields(const TCHAR* Json)
{
    TSharedPtr<FJsonObject> Fields;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Fields);
    return Fields;
}

TMap<FString, FString> CaptureAuthoredObjects(UPCGGraph* Graph)
{
    using namespace McpPCGHandlers;
    TArray<UObject*> Objects;
    GetObjectsWithOuter(Graph, Objects, true);
    Objects.Insert(Graph, 0);
    TMap<FString, FString> Result;
    for (UObject* Object : Objects)
    {
        if (!IsPCGAuthoredObject(Object, Graph)) continue;
        Result.Add(Object->GetPathName(Graph), Object->GetClass()->GetPathName() + TEXT(":") +
            FString::FromInt(Object->GetClass()->GetUniqueID()) + TEXT(":") +
            McpPropertyReflection::FingerprintEditableProperties(Object, PCGRevisionExtraProperties()));
    }
    return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSpawnerCompoundGCTest, "EngineWire.PCG.SpawnerTypes.CompoundPreflightPreservesRevisionAcrossGC",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSpawnerCompoundGCTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    FString Error;
    const auto Add = [&](const TCHAR* ClassName, const TCHAR* Json) -> UPCGNode*
    {
        UClass* Class = ResolvePCGSettingsClass(ClassName);
        if (!Class)
        {
            Error = TEXT("Required native compound-fixture class is unavailable.");
            return nullptr;
        }
        UPCGSettings* Settings = nullptr;
        UPCGNode* Node = Graph->AddNodeOfType(Class, Settings);
        if (!Node || !Settings) return nullptr;
        const auto Fields = ParseFields(Json);
        int32 Applied = 0;
        if (!Fields.IsValid() || !ApplySettingsObject(Settings, Fields, Error, Applied)) return nullptr;
        Node->UpdateAfterSettingsChangeDuringCreation();
        Settings->PostEditChange();
        return Node;
    };
    UPCGNode* Cube = Add(TEXT("PCGCreatePointsSettings"), TEXT(R"({"bCullPointsOutsideVolume":false,"PointsToCreate":[{"Density":0.25,"Seed":-17}]})"));
    UPCGNode* CubeAttribute = Add(TEXT("PCGAddAttributeSettings"), TEXT(R"({"OutputTarget":{"Selection":"Attribute","AttributeName":"MCP_Mesh"},"AttributeTypes":{"Type":"SoftObjectPath","SoftObjectPathValue":"/Engine/BasicShapes/Cube.Cube"}})"));
    UPCGNode* Sphere = Add(TEXT("PCGCreatePointsSettings"), TEXT(R"({"bCullPointsOutsideVolume":false,"PointsToCreate":[{"Density":0.75,"Seed":29}]})"));
    UPCGNode* SphereAttribute = Add(TEXT("PCGAddAttributeSettings"), TEXT(R"({"OutputTarget":{"Selection":"Attribute","AttributeName":"MCP_Mesh"},"AttributeTypes":{"Type":"SoftObjectPath","SoftObjectPathValue":"/Engine/BasicShapes/Sphere.Sphere"}})"));
    UPCGNode* Spawner = Add(TEXT("PCGStaticMeshSpawnerSettings"), TEXT(R"({"MeshSelectorType":"PCGMeshSelectorWeighted","MeshSelectorParameters.MeshEntries":[{"Descriptor":{"StaticMesh":"/Engine/BasicShapes/Sphere.Sphere","ComponentClass":"/Script/Engine.InstancedStaticMeshComponent"},"Weight":7}]})"));
    if (!TestTrue(TEXT("Native compound point/attribute/spawner fixture initialized"), Cube && CubeAttribute && Sphere && SphereAttribute && Spawner))
    {
        AddError(Error);
        return false;
    }
    Graph->AddLabeledEdge(Cube, TEXT("Out"), CubeAttribute, TEXT("In"));
    Graph->AddLabeledEdge(Sphere, TEXT("Out"), SphereAttribute, TEXT("In"));
    Graph->AddLabeledEdge(CubeAttribute, TEXT("Out"), Spawner, TEXT("In"));
    Graph->AddLabeledEdge(SphereAttribute, TEXT("Out"), Spawner, TEXT("In"));
    Graph->AddLabeledEdge(Spawner, TEXT("Out"), Graph->GetOutputNode(), TEXT("Out"));
    const FString Before = GetPCGGraphRevision(Graph.Get());
    if (!TestFalse(TEXT("Complete nonempty compound fixture revision"), Before.IsEmpty())) return false;
    const auto OriginalObjects = CaptureAuthoredObjects(Graph.Get());
    const auto Fields = ParseFields(TEXT(R"({"MeshSelectorParameters.AttributeName":"MCP_Mesh","InstanceDataPackerParameters.AttributeSelectors":["PCGBegin($Density)PCGEnd","$Density"],"InstanceDataPackerType":"PCGInstanceDataPackerByAttribute","MeshSelectorType":"PCGMeshSelectorByAttribute"})"));
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetObjectField(TEXT("settings"), Fields);
    for (int32 Iteration = 0; Iteration < 3; ++Iteration)
    {
        TestFalse(TEXT("Malformed selector suffix refused on staged compound graph"),
            ValidatePCGNodeSettings(Graph.Get(), Spawner, TEXT("set_pcg_node_settings"), Payload, Error));
        TestEqual(TEXT("Failed preflight preserves compound authored revision"), GetPCGGraphRevision(Graph.Get()), Before);
        CollectGarbage(RF_NoFlags);
        const FString After = GetPCGGraphRevision(Graph.Get());
        if (After != Before)
        {
            const auto CurrentObjects = CaptureAuthoredObjects(Graph.Get());
            for (const auto& Pair : OriginalObjects)
            {
                const FString* Current = CurrentObjects.Find(Pair.Key);
                if (!Current || *Current != Pair.Value)
                    AddInfo(FString::Printf(TEXT("Authored object delta: %s before=%s after=%s"), *Pair.Key, *Pair.Value, Current ? **Current : TEXT("<absent>")));
            }
            for (const auto& Pair : CurrentObjects)
                if (!OriginalObjects.Contains(Pair.Key)) AddInfo(TEXT("Added authored object: ") + Pair.Key + TEXT(" ") + Pair.Value);
        }
        TestEqual(TEXT("GC after failed compound preflight cannot change the authored token"), After, Before);
    }
    return true;
}
#endif
