#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "UObject/UnrealType.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
namespace McpLevelStructure
{

bool HandleConfigureRuntimeHashSetGrid(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TSharedPtr<FJsonObject>& Payload,
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

    void* PartitionsArrayPtr = PartitionsProperty->ContainerPtrToValuePtr<void>(HashSet);
    FScriptArrayHelper ArrayHelper(ArrayProp, PartitionsArrayPtr);

    const FName TargetPartitionName = GridName.IsEmpty() ? FName(TEXT("MainPartition")) : FName(*GridName);

    FStructProperty* StructProp = CastField<FStructProperty>(ArrayProp->Inner);
    if (!StructProp)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("RuntimePartitions array element is not a struct"), nullptr);
        return true;
    }
    UStruct* PartitionStruct = StructProp->Struct;

    FNameProperty* NameProp = CastField<FNameProperty>(PartitionStruct->FindPropertyByName(TEXT("Name")));
    // Without a Name field no row can be matched, and a created row could never be found again.
    if (!NameProp)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("RuntimePartitions rows have no Name field on this engine version"), nullptr, TEXT("INVALID_PARTITION_STRUCTURE"));
        return true;
    }
    int32 Index = INDEX_NONE;
    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        if (NameProp->GetPropertyValue_InContainer(ArrayHelper.GetRawPtr(i)) == TargetPartitionName)
        {
            Index = i;
            break;
        }
    }
    if (Index == INDEX_NONE && !bCreateIfMissing)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Grid '%s' not found in RuntimeHashSet; pass createIfMissing true to add it"), *TargetPartitionName.ToString()),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    HashSet->Modify();
    const bool bCreated = Index == INDEX_NONE;
    if (bCreated)
    {
        Index = ArrayHelper.AddValue();
        NameProp->SetPropertyValue_InContainer(ArrayHelper.GetRawPtr(Index), TargetPartitionName);
    }
    const bool bFound = true;

    // The first of Names that Owner has as a numeric property, set on Container to Value.
    auto SetNumber = [](const UStruct* Owner, void* Container, std::initializer_list<const TCHAR*> Names, double Value)
    {
        for (const TCHAR* Name : Names)
        {
            if (FNumericProperty* Prop = CastField<FNumericProperty>(Owner->FindPropertyByName(Name)))
            {
                void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Container);
                if (Prop->IsFloatingPoint())
                {
                    Prop->SetFloatingPointPropertyValue(ValuePtr, Value);
                }
                else
                {
                    Prop->SetIntPropertyValue(ValuePtr, static_cast<int64>(Value));
                }
                return true;
            }
        }
        return false;
    };
    bool bLoadingRangeApplied = false;
    bool bCellSizeApplied = false;
    if (bFound)
    {
        // The grid settings live on the row's MainLayer partition object (a new row has none yet).
        void* Row = ArrayHelper.GetRawPtr(Index);
        FObjectPropertyBase* LayerProp = CastField<FObjectPropertyBase>(PartitionStruct->FindPropertyByName(TEXT("MainLayer")));
        UObject* MainLayer = LayerProp ? LayerProp->GetObjectPropertyValue_InContainer(Row) : nullptr;
        if (MainLayer)
        {
            MainLayer->Modify();
        }
        auto SetField = [&](std::initializer_list<const TCHAR*> Names, double Value)
        {
            return SetNumber(PartitionStruct, Row, Names, Value) ||
                   (MainLayer && SetNumber(MainLayer->GetClass(), MainLayer, Names, Value));
        };
        bLoadingRangeApplied = SetField({TEXT("LoadingRange")}, LoadingRange);
        bCellSizeApplied = SetField({TEXT("CellSize"), TEXT("GridSize")}, GridCellSize);
    }

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
    ResponseJson->SetBoolField(TEXT("loadingRangeApplied"), bLoadingRangeApplied);
    ResponseJson->SetBoolField(TEXT("cellSizeApplied"), bCellSizeApplied);

    FString Message = bCreated
        ? FString::Printf(TEXT("Created new partition '%s' in RuntimeHashSet"), *TargetPartitionName.ToString())
        : FString::Printf(TEXT("Updated partition '%s' in RuntimeHashSet"), *TargetPartitionName.ToString());

    LevelStructureHelpers::SendLevelEditResult(Subsystem, RequestId, Socket, Payload, World->PersistentLevel, Message, ResponseJson);
    return true;
}

}
#endif
