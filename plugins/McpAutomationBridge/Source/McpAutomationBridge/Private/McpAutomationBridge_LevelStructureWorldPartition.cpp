#include "McpAutomationBridge_LevelStructureActions.h"
#include "McpAutomationBridge_LevelStructureEditorWorld.h"

#include "Engine/World.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "UObject/UnrealType.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#endif
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleConfigureGridSize(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString GridName = GetJsonStringField(Payload, TEXT("gridName"), TEXT(""));
    int32 GridCellSize = GetJsonIntField(Payload, TEXT("gridCellSize"), 12800);
    float LoadingRange = static_cast<float>(GetJsonNumberField(Payload, TEXT("loadingRange"), 25600.0));
    bool bBlockOnSlowStreaming = GetJsonBoolField(Payload, TEXT("bBlockOnSlowStreaming"), false);
    int32 Priority = GetJsonIntField(Payload, TEXT("priority"), 0);
    bool bCreateIfMissing = GetJsonBoolField(Payload, TEXT("createIfMissing"), true);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition is not enabled for this level"), nullptr);
        return true;
    }

    // Get the runtime hash - World Partition uses UWorldPartitionRuntimeSpatialHash for grid-based streaming
    UWorldPartitionRuntimeHash* RuntimeHash = WorldPartition->RuntimeHash;
    if (!RuntimeHash)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition RuntimeHash not available"), nullptr);
        return true;
    }

    // Check if we're dealing with RuntimeSpatialHash or RuntimeHashSet
    UWorldPartitionRuntimeSpatialHash* SpatialHash = Cast<UWorldPartitionRuntimeSpatialHash>(RuntimeHash);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    UWorldPartitionRuntimeHashSet* HashSet = Cast<UWorldPartitionRuntimeHashSet>(RuntimeHash);

    if (!SpatialHash && !HashSet)
#else
    if (!SpatialHash)
#endif
    {
        // Neither supported hash type
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

#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    // Handle RuntimeHashSet (UE 5.3+ only)
    if (HashSet)
    {
        return HandleConfigureRuntimeHashSetGrid(
            Subsystem, RequestId, Socket, World, HashSet, GridName,
            GridCellSize, LoadingRange, bCreateIfMissing);
    }
#endif // ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1

    // Handle RuntimeSpatialHash (existing code)
    // Access the editor-only Grids array via reflection since it's protected
    // The Grids property is TArray<FSpatialHashRuntimeGrid> which holds the editable grid configuration
    FProperty* GridsProperty = SpatialHash->GetClass()->FindPropertyByName(TEXT("Grids"));
    if (!GridsProperty)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Could not find Grids property on RuntimeSpatialHash"), nullptr);
        return true;
    }

    FArrayProperty* ArrayProp = CastField<FArrayProperty>(GridsProperty);
    if (!ArrayProp)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Grids property is not an array"), nullptr);
        return true;
    }

    // Get the array helper
    void* GridsArrayPtr = GridsProperty->ContainerPtrToValuePtr<void>(SpatialHash);
    FScriptArrayHelper ArrayHelper(ArrayProp, GridsArrayPtr);

    // Find the grid by name, or use the first one if no name specified
    bool bFound = false;
    bool bCreated = false;
    int32 ModifiedIndex = -1;
    FName TargetGridName = GridName.IsEmpty() ? FName(NAME_None) : FName(*GridName);

    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        FSpatialHashRuntimeGrid* Grid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(i));
        if (Grid)
        {
            // Match by name, or use first grid if no name specified
            if (GridName.IsEmpty() || Grid->GridName == TargetGridName)
            {
                // Modify the grid settings
                Grid->CellSize = GridCellSize;
                Grid->LoadingRange = LoadingRange;
                Grid->bBlockOnSlowStreaming = bBlockOnSlowStreaming;
                Grid->Priority = Priority;

                bFound = true;
                ModifiedIndex = i;
                break;
            }
        }
    }

    // If not found and createIfMissing is true, add a new grid
    if (!bFound && bCreateIfMissing && !GridName.IsEmpty())
    {
        int32 NewIndex = ArrayHelper.AddValue();
        FSpatialHashRuntimeGrid* NewGrid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(NewIndex));
        if (NewGrid)
        {
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
    }

    if (!bFound && !bCreated)
    {
        // List available grids
        TArray<FString> AvailableGrids;
        for (int32 i = 0; i < ArrayHelper.Num(); ++i)
        {
            FSpatialHashRuntimeGrid* Grid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(i));
            if (Grid)
            {
                AvailableGrids.Add(Grid->GridName.ToString());
            }
        }

        FString AvailableStr = AvailableGrids.Num() > 0
            ? FString::Join(AvailableGrids, TEXT(", "))
            : TEXT("(none - use createIfMissing=true to create a new grid)");

        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Grid '%s' not found. Available grids: %s"), *GridName, *AvailableStr), nullptr);
        return true;
    }

    // Mark the object as modified
    SpatialHash->Modify();
    SpatialHash->MarkPackageDirty();
    World->MarkPackageDirty();

    // Build response with current grid configuration
    TArray<TSharedPtr<FJsonValue>> GridsArray;
    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        FSpatialHashRuntimeGrid* Grid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(i));
        if (Grid)
        {
            TSharedPtr<FJsonObject> GridObj = McpHandlerUtils::CreateResultObject();
            GridObj->SetStringField(TEXT("gridName"), Grid->GridName.ToString());
            GridObj->SetNumberField(TEXT("cellSize"), Grid->CellSize);
            GridObj->SetNumberField(TEXT("loadingRange"), Grid->LoadingRange);
            GridObj->SetBoolField(TEXT("blockOnSlowStreaming"), Grid->bBlockOnSlowStreaming);
            GridObj->SetNumberField(TEXT("priority"), Grid->Priority);
            GridObj->SetBoolField(TEXT("modified"), i == ModifiedIndex);
            GridsArray.Add(MakeShared<FJsonValueObject>(GridObj));
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, World);
    ResponseJson->SetStringField(TEXT("gridName"), GridName.IsEmpty() ? TEXT("(default)") : GridName);
    ResponseJson->SetNumberField(TEXT("cellSize"), GridCellSize);
    ResponseJson->SetNumberField(TEXT("loadingRange"), LoadingRange);
    ResponseJson->SetBoolField(TEXT("blockOnSlowStreaming"), bBlockOnSlowStreaming);
    ResponseJson->SetNumberField(TEXT("priority"), Priority);
    ResponseJson->SetBoolField(TEXT("created"), bCreated);
    ResponseJson->SetBoolField(TEXT("modified"), bFound);
    ResponseJson->SetArrayField(TEXT("allGrids"), GridsArray);
    ResponseJson->SetStringField(TEXT("note"), TEXT("Grid configuration updated. Regenerate streaming data to apply changes (World Partition > Generate Streaming)."));

    FString Action = bCreated ? TEXT("Created") : TEXT("Configured");
    FString Message = FString::Printf(TEXT("%s grid '%s' with CellSize=%d, LoadingRange=%.0f"),
        *Action, GridName.IsEmpty() ? TEXT("(default)") : *GridName, GridCellSize, LoadingRange);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;

#else
    // Non-editor build: report current state only
    TArray<TSharedPtr<FJsonValue>> GridsArray;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
    // UE 5.7+: ForEachStreamingGrid is available as public API
    SpatialHash->ForEachStreamingGrid([&GridsArray](const FSpatialHashStreamingGrid& Grid)
    {
        TSharedPtr<FJsonObject> GridObj = McpHandlerUtils::CreateResultObject();
        GridObj->SetStringField(TEXT("gridName"), Grid.GridName.ToString());
        GridObj->SetNumberField(TEXT("cellSize"), Grid.CellSize);
        GridObj->SetNumberField(TEXT("loadingRange"), Grid.LoadingRange);
        GridsArray.Add(MakeShared<FJsonValueObject>(GridObj));
    });
#else
    // UE 5.0-5.6: ForEachStreamingGrid not available - return empty grid info
    UE_LOG(LogMcpLevelStructureHandlers, Warning, TEXT("ForEachStreamingGrid not available in UE versions < 5.7"));
#endif

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetArrayField(TEXT("currentGrids"), GridsArray);
    ResponseJson->SetStringField(TEXT("note"), TEXT("Grid configuration requires editor build to modify."));

    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("Grid configuration requires editor build"), ResponseJson);
    return true;
#endif
}

}
#endif
