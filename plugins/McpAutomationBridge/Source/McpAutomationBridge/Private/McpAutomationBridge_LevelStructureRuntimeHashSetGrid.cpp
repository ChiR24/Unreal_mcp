#include "McpAutomationBridge_LevelStructureActions.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "UObject/UnrealType.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#endif

#if WITH_EDITOR && WITH_EDITORONLY_DATA && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
namespace McpLevelStructure
{

bool HandleConfigureRuntimeHashSetGrid(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UWorld* World,
    UWorldPartitionRuntimeHashSet* HashSet,
    const FString& GridName,
    int32 GridCellSize,
    float LoadingRange,
    bool bCreateIfMissing)
{
    // For HashSet, we use the RuntimePartitions API instead of Grids
    // RuntimePartitions is an array of FWorldPartitionRuntimePartition
    FProperty* PartitionsProperty = HashSet->GetClass()->FindPropertyByName(TEXT("RuntimePartitions"));
    if (!PartitionsProperty)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Could not find RuntimePartitions property on RuntimeHashSet"), nullptr);
        return true;
    }

    FArrayProperty* ArrayProp = CastField<FArrayProperty>(PartitionsProperty);
    if (!ArrayProp)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("RuntimePartitions property is not an array"), nullptr);
        return true;
    }

    // Get the array helper
    void* PartitionsArrayPtr = PartitionsProperty->ContainerPtrToValuePtr<void>(HashSet);
    FScriptArrayHelper ArrayHelper(ArrayProp, PartitionsArrayPtr);

    // Find or create the partition
    bool bFound = false;
    bool bCreated = false;
    int32 ModifiedIndex = -1;
    FName TargetPartitionName = GridName.IsEmpty() ? FName(TEXT("MainPartition")) : FName(*GridName);

    // Get the struct type from the array property
    FStructProperty* StructProp = CastField<FStructProperty>(ArrayProp->Inner);
    if (!StructProp)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("RuntimePartitions array element is not a struct"), nullptr);
        return true;
    }

    UStruct* PartitionStruct = StructProp->Struct;

    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        void* PartitionPtr = ArrayHelper.GetRawPtr(i);
        if (!PartitionPtr) continue;

        // Get the Name property from the partition struct
        FProperty* NameProp = PartitionStruct->FindPropertyByName(TEXT("Name"));
        if (NameProp && NameProp->IsA<FNameProperty>())
        {
            FNameProperty* NameProperty = CastField<FNameProperty>(NameProp);
            FName PartitionName = NameProperty->GetPropertyValue(PartitionPtr);

            if (PartitionName == TargetPartitionName)
            {
                // Found the partition - update its settings via reflection
                // LoadingRange equivalent
                FProperty* LoadingRangeProp = PartitionStruct->FindPropertyByName(TEXT("LoadingRange"));
                if (LoadingRangeProp && LoadingRangeProp->IsA<FFloatProperty>())
                {
                    CastField<FFloatProperty>(LoadingRangeProp)->SetPropertyValue(PartitionPtr, LoadingRange);
                }

                // GridCellSize equivalent (may be called GridSize or CellSize)
                FProperty* GridSizeProp = PartitionStruct->FindPropertyByName(TEXT("GridSize"));
                if (!GridSizeProp)
                {
                    GridSizeProp = PartitionStruct->FindPropertyByName(TEXT("CellSize"));
                }
                if (GridSizeProp && GridSizeProp->IsA<FIntProperty>())
                {
                    CastField<FIntProperty>(GridSizeProp)->SetPropertyValue(PartitionPtr, GridCellSize);
                }

                bFound = true;
                ModifiedIndex = i;
                break;
            }
        }
    }

    // If not found and createIfMissing is true, add a new partition
    if (!bFound && bCreateIfMissing)
    {
        int32 NewIndex = ArrayHelper.AddValue();
        void* NewPartition = ArrayHelper.GetRawPtr(NewIndex);
        if (NewPartition)
        {
            // Initialize the new partition
            FProperty* NameProp = PartitionStruct->FindPropertyByName(TEXT("Name"));
            if (NameProp && NameProp->IsA<FNameProperty>())
            {
                CastField<FNameProperty>(NameProp)->SetPropertyValue(NewPartition, TargetPartitionName);
            }

            FProperty* LoadingRangeProp = PartitionStruct->FindPropertyByName(TEXT("LoadingRange"));
            if (LoadingRangeProp && LoadingRangeProp->IsA<FFloatProperty>())
            {
                CastField<FFloatProperty>(LoadingRangeProp)->SetPropertyValue(NewPartition, LoadingRange);
            }

            FProperty* GridSizeProp = PartitionStruct->FindPropertyByName(TEXT("GridSize"));
            if (!GridSizeProp)
            {
                GridSizeProp = PartitionStruct->FindPropertyByName(TEXT("CellSize"));
            }
            if (GridSizeProp && GridSizeProp->IsA<FIntProperty>())
            {
                CastField<FIntProperty>(GridSizeProp)->SetPropertyValue(NewPartition, GridCellSize);
            }

            bCreated = true;
            bFound = true;
        }
    }

    // Mark package dirty
    HashSet->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, World);
    ResponseJson->SetBoolField(TEXT("success"), true);
    ResponseJson->SetStringField(TEXT("hashType"), TEXT("RuntimeHashSet"));
    ResponseJson->SetStringField(TEXT("partitionName"), TargetPartitionName.ToString());
    ResponseJson->SetNumberField(TEXT("loadingRange"), LoadingRange);
    ResponseJson->SetNumberField(TEXT("cellSize"), GridCellSize);
    ResponseJson->SetBoolField(TEXT("created"), bCreated);
    ResponseJson->SetBoolField(TEXT("modified"), bFound);

    FString Message = bCreated
        ? FString::Printf(TEXT("Created new partition '%s' in RuntimeHashSet"), *TargetPartitionName.ToString())
        : FString::Printf(TEXT("Updated partition '%s' in RuntimeHashSet"), *TargetPartitionName.ToString());

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
#endif
