#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "UObject/UnrealType.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#endif
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"

namespace McpLevelStructure
{

bool HandleConfigureGridSize(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString GridName = GetJsonStringField(Payload, TEXT("gridName"), TEXT(""));
    // The editor's own bounds (its change handler holds them): a cell under 1600 units, or 0, broke streaming generation.
    int32 GridCellSize = FMath::Max(GetJsonIntField(Payload, TEXT("gridCellSize"), 12800), 1600);
    float LoadingRange = FMath::Max(static_cast<float>(GetJsonNumberField(Payload, TEXT("loadingRange"), 25600.0)), 0.0f);
    bool bBlockOnSlowStreaming = GetJsonBoolField(Payload, TEXT("bBlockOnSlowStreaming"), false);
    int32 Priority = GetJsonIntField(Payload, TEXT("priority"), 0);
    bool bCreateIfMissing = GetJsonBoolField(Payload, TEXT("createIfMissing"), true);
    const bool bHasValue = Payload->HasField(TEXT("gridCellSize")) || Payload->HasField(TEXT("loadingRange")) ||
                           Payload->HasField(TEXT("bBlockOnSlowStreaming")) || Payload->HasField(TEXT("priority"));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("No editor world available"), TEXT("NO_WORLD"));
        return true;
    }

    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("World Partition is not enabled for this level"), TEXT("WORLD_PARTITION_NOT_ENABLED"));
        return true;
    }

    // Get the runtime hash - World Partition uses UWorldPartitionRuntimeSpatialHash for grid-based streaming
    UWorldPartitionRuntimeHash* RuntimeHash = WorldPartition->RuntimeHash;
    if (!RuntimeHash)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("World Partition RuntimeHash not available"), TEXT("RUNTIME_HASH_UNAVAILABLE"));
        return true;
    }

    UWorldPartitionRuntimeSpatialHash* SpatialHash = Cast<UWorldPartitionRuntimeSpatialHash>(RuntimeHash);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    UWorldPartitionRuntimeHashSet* HashSet = Cast<UWorldPartitionRuntimeHashSet>(RuntimeHash);

    if (!SpatialHash && !HashSet)
#else
    if (!SpatialHash)
#endif
    {
        TSharedPtr<FJsonObject> ErrorJson = McpHandlerUtils::CreateResultObject();
        ErrorJson->SetStringField(TEXT("currentHashType"), RuntimeHash->GetClass()->GetName());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        ErrorJson->SetStringField(TEXT("supportedHashTypes"), TEXT("WorldPartitionRuntimeSpatialHash, WorldPartitionRuntimeHashSet"));
#else
        ErrorJson->SetStringField(TEXT("supportedHashTypes"), TEXT("WorldPartitionRuntimeSpatialHash"));
#endif
        ErrorJson->SetStringField(TEXT("hint"), TEXT("World Partition must use RuntimeSpatialHash for grid configuration."));
        ErrorJson->SetStringField(TEXT("solution"), TEXT("Create a new level with World Partition enabled, or check World Partition settings in the editor."));

        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("World Partition is using unsupported hash type: %s. Grid configuration not applicable."),
                *RuntimeHash->GetClass()->GetName()),
            ErrorJson, TEXT("INVALID_PARTITION_TYPE"));
        return true;
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    // Handle RuntimeHashSet (UE 5.3+ only)
    if (HashSet)
    {
        return HandleConfigureRuntimeHashSetGrid(
            Subsystem, RequestId, Socket, Payload, World, HashSet, GridName,
            GridCellSize, LoadingRange, bCreateIfMissing);
    }
#endif // ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3

    // Access the editor-only Grids array via reflection since it's protected
    // The Grids property is TArray<FSpatialHashRuntimeGrid> which holds the editable grid configuration
    FProperty* GridsProperty = SpatialHash->GetClass()->FindPropertyByName(TEXT("Grids"));
    if (!GridsProperty)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("Could not find Grids property on RuntimeSpatialHash"), TEXT("REFLECTION_FAILED"));
        return true;
    }

    FArrayProperty* ArrayProp = CastField<FArrayProperty>(GridsProperty);
    if (!ArrayProp)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("Grids property is not an array"), TEXT("REFLECTION_FAILED"));
        return true;
    }

    void* GridsArrayPtr = GridsProperty->ContainerPtrToValuePtr<void>(SpatialHash);
    FScriptArrayHelper ArrayHelper(ArrayProp, GridsArrayPtr);

    bool bFound = false;
    bool bCreated = false;
    int32 ModifiedIndex = -1;
    FName TargetGridName = GridName.IsEmpty() ? FName(NAME_None) : FName(*GridName);
    // One undo step for the grid edit, opened at the change: a refusal before it (no value to change, no such grid)
    // leaves no step and the level as it was.
    TUniquePtr<FMcpScopedEditorTransaction> GridStep;
    const auto BeginGridEdit = [&GridStep, SpatialHash]()
    {
        GridStep = MakeUnique<FMcpScopedEditorTransaction>(FText::FromString(TEXT("Configure World Partition Grid")),
                                                           EMcpMutationDurability::EditorStateOnly, TArray<UObject*>{SpatialHash});
        SpatialHash->Modify();
    };

    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        FSpatialHashRuntimeGrid* Grid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(i));
        if (GridName.IsEmpty() || Grid->GridName == TargetGridName)
        {
            if (!bHasValue)
            {
                Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Grid '%s' found; pass "
                    "gridCellSize, loadingRange, bBlockOnSlowStreaming or priority to change it. Nothing was changed."),
                    *Grid->GridName.ToString()), TEXT("INVALID_ARGUMENT"));
                return true;
            }
            // Before the change, so undo returns to it (the snapshot was taken after, so undo restored the new
            // values); only what was sent changes: the defaults of the fields left out overwrote the grid's own.
            BeginGridEdit();
            if (Payload->HasField(TEXT("gridCellSize"))) { Grid->CellSize = GridCellSize; }
            if (Payload->HasField(TEXT("loadingRange"))) { Grid->LoadingRange = LoadingRange; }
            if (Payload->HasField(TEXT("bBlockOnSlowStreaming"))) { Grid->bBlockOnSlowStreaming = bBlockOnSlowStreaming; }
            if (Payload->HasField(TEXT("priority"))) { Grid->Priority = Priority; }

            bFound = true;
            ModifiedIndex = i;
            break;
        }
    }

    if (!bFound && bCreateIfMissing && !GridName.IsEmpty())
    {
        BeginGridEdit();
        int32 NewIndex = ArrayHelper.AddValue();
        FSpatialHashRuntimeGrid* NewGrid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(NewIndex));
        NewGrid->GridName = FName(*GridName);
        NewGrid->CellSize = GridCellSize;
        NewGrid->LoadingRange = LoadingRange;
        NewGrid->bBlockOnSlowStreaming = bBlockOnSlowStreaming;
        NewGrid->Priority = Priority;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        NewGrid->Origin = FVector2D::ZeroVector;
#endif
        NewGrid->DebugColor = FLinearColor::MakeRandomColor();
        NewGrid->bClientOnlyVisible = false;
        NewGrid->HLODLayer = nullptr;

        bCreated = true;
        ModifiedIndex = NewIndex;
    }

    if (!bFound && !bCreated)
    {
        TArray<FString> AvailableGrids;
        for (int32 i = 0; i < ArrayHelper.Num(); ++i)
        {
            FSpatialHashRuntimeGrid* Grid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(i));
            AvailableGrids.Add(Grid->GridName.ToString());
        }

        FString AvailableStr = AvailableGrids.Num() > 0
            ? FString::Join(AvailableGrids, TEXT(", "))
            : TEXT("(none - use createIfMissing=true to create a new grid)");

        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Grid '%s' not found. Available grids: %s"), *GridName, *AvailableStr), TEXT("GRID_NOT_FOUND"));
        return true;
    }

    SpatialHash->MarkPackageDirty();
    World->MarkPackageDirty();

    TArray<TSharedPtr<FJsonValue>> GridsArray;
    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        FSpatialHashRuntimeGrid* Grid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(i));
        TSharedPtr<FJsonObject> GridObj = McpHandlerUtils::CreateResultObject();
        GridObj->SetStringField(TEXT("gridName"), Grid->GridName.ToString());
        GridObj->SetNumberField(TEXT("cellSize"), Grid->CellSize);
        GridObj->SetNumberField(TEXT("loadingRange"), Grid->LoadingRange);
        GridObj->SetBoolField(TEXT("blockOnSlowStreaming"), Grid->bBlockOnSlowStreaming);
        GridObj->SetNumberField(TEXT("priority"), Grid->Priority);
        GridObj->SetBoolField(TEXT("modified"), i == ModifiedIndex);
        GridsArray.Add(MakeShared<FJsonValueObject>(GridObj));
    }

    // The grid as it now stands: a field left out keeps the grid's own value, which the reply printed as the default.
    const FSpatialHashRuntimeGrid* Edited = reinterpret_cast<const FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(ModifiedIndex));
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, World);
    ResponseJson->SetStringField(TEXT("gridName"), Edited->GridName.ToString());
    ResponseJson->SetNumberField(TEXT("cellSize"), Edited->CellSize);
    ResponseJson->SetNumberField(TEXT("loadingRange"), Edited->LoadingRange);
    ResponseJson->SetBoolField(TEXT("blockOnSlowStreaming"), Edited->bBlockOnSlowStreaming);
    ResponseJson->SetNumberField(TEXT("priority"), Edited->Priority);
    ResponseJson->SetBoolField(TEXT("created"), bCreated);
    ResponseJson->SetBoolField(TEXT("modified"), bFound);
    ResponseJson->SetArrayField(TEXT("allGrids"), GridsArray);
    ResponseJson->SetStringField(TEXT("note"), TEXT("Grid configuration updated. Regenerate streaming data to apply changes (World Partition > Generate Streaming)."));

    FString Action = bCreated ? TEXT("Created") : TEXT("Configured");
    FString Message = FString::Printf(TEXT("%s grid '%s': CellSize=%d, LoadingRange=%.0f"),
        *Action, *Edited->GridName.ToString(), Edited->CellSize, Edited->LoadingRange);
    GridStep.Reset(); // closed before the level is saved
    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, World->PersistentLevel, Message, ResponseJson);
    return true;

}

}
