#include "McpAutomationBridge_WorldPartitionHandlersPrivate.h"

#if WITH_EDITOR
namespace McpWorldPartitionHandlers
{
bool HandleLoadCells(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UWorld* World,
    UWorldPartition* WorldPartition)
{
    FVector Origin = FVector::ZeroVector;
    FVector Extent = FVector(25000.0f, 25000.0f, 25000.0f);

    const TArray<TSharedPtr<FJsonValue>>* OriginArr;
    if (Payload->TryGetArrayField(TEXT("origin"), OriginArr) &&
        OriginArr && OriginArr->Num() >= 3)
    {
        Origin.X = (*OriginArr)[0]->AsNumber();
        Origin.Y = (*OriginArr)[1]->AsNumber();
        Origin.Z = (*OriginArr)[2]->AsNumber();
    }

    const TArray<TSharedPtr<FJsonValue>>* ExtentArr;
    if (Payload->TryGetArrayField(TEXT("extent"), ExtentArr) &&
        ExtentArr && ExtentArr->Num() >= 3)
    {
        Extent.X = (*ExtentArr)[0]->AsNumber();
        Extent.Y = (*ExtentArr)[1]->AsNumber();
        Extent.Z = (*ExtentArr)[2]->AsNumber();
    }

    FBox Bounds(Origin - Extent, Origin + Extent);

#if MCP_HAS_WP_EDITOR_SUBSYSTEM
    UWorldPartitionEditorSubsystem* WPEditorSubsystem =
        GEditor->GetEditorSubsystem<UWorldPartitionEditorSubsystem>();
    if (WPEditorSubsystem)
    {
        WPEditorSubsystem->LoadRegion(Bounds);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("action"), TEXT("manage_world_partition"));
        Result->SetStringField(TEXT("subAction"), TEXT("load_cells"));
        Result->SetStringField(TEXT("method"), TEXT("EditorSubsystem"));
        Result->SetBoolField(TEXT("requested"), true);

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Region load requested."), Result);
        return true;
    }
#endif

#if MCP_HAS_WP_LOADER_ADAPTER
    UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter =
        WorldPartition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(
            World, Bounds, TEXT("MCP Loaded Region"));
    if (EditorLoaderAdapter && EditorLoaderAdapter->GetLoaderAdapter())
    {
        EditorLoaderAdapter->GetLoaderAdapter()->SetUserCreated(true);
        EditorLoaderAdapter->GetLoaderAdapter()->Load();

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("action"), TEXT("manage_world_partition"));
        Result->SetStringField(TEXT("subAction"), TEXT("load_cells"));
        Result->SetStringField(TEXT("method"), TEXT("LoaderAdapter"));
        Result->SetBoolField(TEXT("requested"), true);

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Region load requested via LoaderAdapter."), Result);
        return true;
    }
#endif

    Bridge->SendAutomationError(RequestingSocket, RequestId,
        TEXT("WorldPartition region loading not supported in this engine version."),
        TEXT("NOT_SUPPORTED"));
    return true;
}
}
#endif
