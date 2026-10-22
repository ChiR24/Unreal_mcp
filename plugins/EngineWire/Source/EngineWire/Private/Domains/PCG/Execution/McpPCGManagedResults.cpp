#include "Domains/PCG/Execution/McpPCGExecution.h"
#if MCP_HAS_PCG_EXECUTION
#include "Domains/PCG/Execution/McpPCGManagedInventory.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Data/PCGBasePointData.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

namespace McpPCGHandlers
{
TSharedPtr<FJsonObject> ReadPCGManagedResults(UPCGComponent* Component, int32 Offset, int32 Limit, int32 SampleLimit)
{
    Offset = FMath::Max(0, Offset);
    Limit = FMath::Clamp(Limit, 1, 100);
    SampleLimit = FMath::Clamp(SampleLimit, 0, 16);
    FPCGReadBudget Budget;
    FPCGManagedInventory Inventory;
    if (!ReadBoundedPCGManagedInventory(Component, Budget, Inventory)) return nullptr;
    const auto& Components = Inventory.Components;
    const auto& Actors = Inventory.Actors;
    const auto& RootActors = Inventory.RootActors;
    auto Result = McpHandlerUtils::CreateResultObject();
    TArray<UActorComponent*> Sorted = Components.Array();
    Sorted.Sort([](const UActorComponent& A, const UActorComponent& B) { return A.GetPathName().Compare(B.GetPathName(), ESearchCase::CaseSensitive) < 0; });
    TArray<TSharedPtr<FJsonValue>> Rows;
    int64 Instances = 0;
    int32 IsmCount = 0, HismCount = 0, ResourceIndex = 0;
    for (UActorComponent* Value : Sorted)
    {
        if (!Budget.IsWithinTime()) return nullptr;
        const bool bEmitRow = ResourceIndex >= Offset && ResourceIndex - Offset < Limit;
        ++ResourceIndex;
        UInstancedStaticMeshComponent* Ism = Cast<UInstancedStaticMeshComponent>(Value);
        const bool bHism = Ism && Ism->IsA<UHierarchicalInstancedStaticMeshComponent>();
        const int32 Count = Ism ? Ism->GetInstanceCount() : 0;
        if (Ism)
        {
            bHism ? ++HismCount : ++IsmCount;
            Instances += Count;
        }
        if (!bEmitRow) continue;
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("componentPath"), Value->GetPathName());
        Row->SetStringField(TEXT("class"), Value->GetClass()->GetPathName());
        if (Ism)
        {
            Row->SetNumberField(TEXT("instanceCount"), Count);
            Row->SetStringField(TEXT("kind"), bHism ? TEXT("HISM") : TEXT("ISM"));
            if (UStaticMesh* Mesh = Ism->GetStaticMesh()) Row->SetStringField(TEXT("meshPath"), Mesh->GetPathName());
            TArray<TSharedPtr<FJsonValue>> Samples;
            const int32 SamplesToRead = FMath::Min(SampleLimit, Count);
            for (int32 Sample = 0; Sample < SamplesToRead; ++Sample)
            {
                const int32 Index = SamplesToRead > 1 && Sample + 1 == SamplesToRead ? Count - 1 : Sample;
                FTransform Transform;
                if (!Ism->GetInstanceTransform(Index, Transform, true)) continue;
                auto Data = McpHandlerUtils::TransformToJson(Transform);
                Data->SetNumberField(TEXT("index"), Index);
                Samples.Add(MakeShared<FJsonValueObject>(Data));
            }
            Row->SetArrayField(TEXT("sampleTransforms"), Samples);
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TArray<TSharedPtr<FJsonValue>> Page = MoveTemp(Rows);
    TArray<TSharedPtr<FJsonValue>> ActorRows, OutputRows;
    TArray<AActor*> SortedActors = Actors.Array();
    SortedActors.Sort([](const AActor& A, const AActor& B) { return A.GetPathName().Compare(B.GetPathName(), ESearchCase::CaseSensitive) < 0; });
    for (int32 Index = FMath::Max(0, Offset); Index < SortedActors.Num() && ActorRows.Num() < FMath::Clamp(Limit, 1, 100); ++Index)
    {
        if (!Budget.IsWithinTime()) return nullptr;
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actorPath"), SortedActors[Index]->GetPathName());
        Row->SetBoolField(TEXT("rootManagedActor"), RootActors.Contains(SortedActors[Index]));
        Row->SetObjectField(TEXT("transform"), McpHandlerUtils::TransformToJson(SortedActors[Index]->GetActorTransform()));
        ActorRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    int64 OutputPoints = 0;
    const auto& Output = Component->GetGeneratedGraphOutput().TaggedData;
    if (Output.Num() > FPCGReadBudget::MaxObjects) return nullptr;
    for (int32 Index = 0; Index < Output.Num(); ++Index)
    {
        if (!Budget.IsWithinTime()) return nullptr;
        const FPCGTaggedData& Tagged = Output[Index];
        const UPCGBasePointData* Points = Cast<UPCGBasePointData>(Tagged.Data);
        if (Points) OutputPoints += Points->GetNumPoints();
        if (Index < Offset || OutputRows.Num() >= FMath::Clamp(Limit, 1, 100)) continue;
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("pin"), Tagged.Pin.ToString());
        if (Tagged.Data) Row->SetStringField(TEXT("dataClass"), Tagged.Data->GetClass()->GetPathName());
        if (Points) Row->SetNumberField(TEXT("pointCount"), Points->GetNumPoints());
        TArray<TSharedPtr<FJsonValue>> Tags;
        for (const FString& Tag : Tagged.Tags)
        {
            if (Tags.Num() >= 32) break;
            if (Tag.Len() > 1024) return nullptr;
            Tags.Add(MakeShared<FJsonValueString>(Tag));
        }
        Row->SetArrayField(TEXT("tags"), Tags);
        Row->SetBoolField(TEXT("tagsTruncated"), Tagged.Tags.Num() > Tags.Num());
        OutputRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetStringField(TEXT("componentPath"), Component->GetPathName());
    Result->SetBoolField(TEXT("generating"), Component->IsGenerating());
    Result->SetBoolField(TEXT("cleaning"), Component->IsCleaningUp());
    Result->SetNumberField(TEXT("managedResourceCount"), Inventory.ResourceCount);
    Result->SetNumberField(TEXT("managedComponentCount"), Components.Num());
    Result->SetNumberField(TEXT("managedRootActorCount"), RootActors.Num());
    Result->SetNumberField(TEXT("managedActorCount"), Actors.Num());
    Result->SetNumberField(TEXT("instanceCount"), static_cast<double>(Instances));
    Result->SetNumberField(TEXT("ismComponentCount"), IsmCount);
    Result->SetNumberField(TEXT("hismComponentCount"), HismCount);
    Result->SetNumberField(TEXT("unresolvedResourceReferences"), Inventory.Unresolved);
    Result->SetNumberField(TEXT("outputDataCount"), Output.Num());
    Result->SetNumberField(TEXT("outputPointCount"), static_cast<double>(OutputPoints));
    Result->SetNumberField(TEXT("offset"), Offset);
    Result->SetNumberField(TEXT("limit"), Limit);
    Result->SetBoolField(TEXT("componentsTruncated"), static_cast<int64>(Offset) + Limit < Sorted.Num());
    Result->SetBoolField(TEXT("actorsTruncated"), static_cast<int64>(Offset) + Limit < SortedActors.Num());
    Result->SetBoolField(TEXT("outputTruncated"), static_cast<int64>(Offset) + Limit < Output.Num());
    Result->SetArrayField(TEXT("components"), Page);
    Result->SetArrayField(TEXT("actors"), ActorRows);
    Result->SetArrayField(TEXT("outputs"), OutputRows);
    FString Encoded;
    auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded);
    if (!FJsonSerializer::Serialize(Result.ToSharedRef(), Writer) || Encoded.Len() > 80000 || !Budget.IsWithinTime()) return nullptr;
    return Result;
}
}
#endif
