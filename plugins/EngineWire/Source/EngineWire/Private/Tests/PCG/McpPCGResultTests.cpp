#include "Misc/AutomationTest.h"
#include "Domains/PCG/Execution/McpPCGExecution.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "PCGManagedResource.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/PlatformMemory.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGResultPageTest, "EngineWire.PCG.Execution.ManagedResultPagesBoundTransformExports",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGResultPageTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    constexpr int32 ComponentCount = 256, InstancesPerComponent = 256;
    const uint64 MemoryBefore = FPlatformMemory::GetStats().UsedPhysical;
    TStrongObjectPtr<UPCGComponent> Source(NewObject<UPCGComponent>(GetTransientPackage()));
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Engine fixture mesh"), Mesh)) return false;
    UPCGManagedComponentList* Resource = NewObject<UPCGManagedComponentList>(Source.Get());
    TArray<TStrongObjectPtr<UInstancedStaticMeshComponent>> Components;
    for (int32 Index = 0; Index < ComponentCount; ++Index)
    {
        TStrongObjectPtr<UInstancedStaticMeshComponent> Component(NewObject<UInstancedStaticMeshComponent>(
            Source.Get(), FName(*FString::Printf(TEXT("McpPCGStress_%03d"), Index)), RF_Transient));
        Component->SetStaticMesh(Mesh);
        TArray<FTransform> Transforms;
        Transforms.Reserve(InstancesPerComponent);
        for (int32 Instance = 0; Instance < InstancesPerComponent; ++Instance)
            Transforms.Emplace(FVector(Index, Instance, 0));
        Component->AddInstances(Transforms, false, false, false);
        if (!TestEqual(TEXT("Unregistered native fixture stores every instance"), Component->GetInstanceCount(), InstancesPerComponent)) return false;
        Resource->GeneratedComponents.Add(Component.Get());
        Components.Add(MoveTemp(Component));
    }
    Resource->GeneratedComponents.Add(Components[0].Get());
    Source->AddToManagedResources(Resource);
    const double Started = FPlatformTime::Seconds();
    auto Page = ReadPCGManagedResults(Source.Get(), 128, 3, 2);
    const double Milliseconds = (FPlatformTime::Seconds() - Started) * 1000.0;
    TestEqual(TEXT("Repeated managed references are deduplicated"), Page->GetIntegerField(TEXT("managedComponentCount")), ComponentCount);
    TestEqual(TEXT("The shared managed list is counted once"), Page->GetIntegerField(TEXT("managedResourceCount")), 1);
    TestEqual(TEXT("Inventory counts all native instances, not only page samples"), Page->GetNumberField(TEXT("instanceCount")), static_cast<double>(ComponentCount * InstancesPerComponent));
    TestEqual(TEXT("All fixture components are ISMs"), Page->GetIntegerField(TEXT("ismComponentCount")), ComponentCount);
    TestTrue(TEXT("There are components after this page"), Page->GetBoolField(TEXT("componentsTruncated")));
    const auto& Rows = Page->GetArrayField(TEXT("components"));
    TestEqual(TEXT("Only requested component rows are constructed"), Rows.Num(), 3);
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const auto Row = Rows[Index]->AsObject();
        TestTrue(TEXT("Page starts at the requested deterministic component"), Row->GetStringField(TEXT("componentPath")).Contains(
            FString::Printf(TEXT("McpPCGStress_%03d"), 128 + Index)));
        const auto& Samples = Row->GetArrayField(TEXT("sampleTransforms"));
        if (!TestEqual(TEXT("Only two transforms are exported per on-page component"), Samples.Num(), 2)) return false;
        TestEqual(TEXT("First instance is sampled"), Samples[0]->AsObject()->GetIntegerField(TEXT("index")), 0);
        TestEqual(TEXT("Last instance is sampled"), Samples[1]->AsObject()->GetIntegerField(TEXT("index")), InstancesPerComponent - 1);
        const auto LastLocation = Samples[1]->AsObject()->GetObjectField(TEXT("location"));
        TestEqual(TEXT("Sample belongs to the requested component"), LastLocation->GetNumberField(TEXT("x")), static_cast<double>(128 + Index));
        TestEqual(TEXT("Sample is the last real native instance"), LastLocation->GetNumberField(TEXT("y")), static_cast<double>(InstancesPerComponent - 1));
    }
    auto NoSamples = ReadPCGManagedResults(Source.Get(), 0, 1, 0);
    TestEqual(TEXT("A zero-sample query does not export instance transforms"),
        NoSamples->GetArrayField(TEXT("components"))[0]->AsObject()->GetArrayField(TEXT("sampleTransforms")).Num(), 0);
    auto PastEnd = ReadPCGManagedResults(Source.Get(), ComponentCount, 1, 16);
    TestEqual(TEXT("An off-end page is empty"), PastEnd->GetArrayField(TEXT("components")).Num(), 0);
    TestFalse(TEXT("An off-end page is not marked truncated"), PastEnd->GetBoolField(TEXT("componentsTruncated")));
    const int64 MemoryDelta = static_cast<int64>(FPlatformMemory::GetStats().UsedPhysical) - static_cast<int64>(MemoryBefore);
    AddInfo(FString::Printf(TEXT("Synthetic unregistered resource fixture: %d components, %d instances; 3-row/2-sample inventory %.3f ms; process physical-memory delta %.2f MiB (includes mesh load)."),
        ComponentCount, ComponentCount * InstancesPerComponent, Milliseconds, static_cast<double>(MemoryDelta) / (1024.0 * 1024.0)));
    return true;
}
#endif
