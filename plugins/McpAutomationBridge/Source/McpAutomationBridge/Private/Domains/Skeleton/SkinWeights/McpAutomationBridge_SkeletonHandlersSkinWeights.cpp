#include "Domains/Skeleton/McpAutomationBridge_SkeletonHandlersActions.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleNormalizeWeights(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // There is no post-import normalization entry point: the importer
    // normalizes influences as it builds them. All this can do is rebuild the
    // mesh, which re-runs that normalization over the existing data. Saying
    // "Skin weights normalized" claimed a pass this never performs.
    Mesh->Build();
    McpSafeAssetSave(Mesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetBoolField(TEXT("rebuilt"), true);
    Result->SetBoolField(TEXT("normalizationPassRun"), false);
    Result->SetStringField(TEXT("note"),
        TEXT("Skin weights are normalized by the importer, not by a separate pass. The mesh was rebuilt and saved, which re-runs the importer's normalization over the existing influences; no weights were edited directly."));

    SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Skeletal mesh rebuilt and saved (import-time weight normalization re-applied)"), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePruneWeights(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    double Threshold = 0.01;
    Payload->TryGetNumberField(TEXT("threshold"), Threshold);

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // `threshold` is an IMPORT option and there is no post-import entry point
    // for it, as the original comment here already said. Reporting "Weights
    // pruned with threshold 0.010000" while dropping the value was the one
    // thing this must not do: refuse, and name the route that works.
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(
            TEXT("prune_weights cannot apply a threshold after import: influence pruning is an FBX import option (Threshold Weight), not an editable property of a built skeletal mesh. Re-import %s with the desired threshold. Requested threshold was %g."),
            *SkeletalMeshPath, Threshold),
        TEXT("NOT_SUPPORTED"));
    return true;
}

#endif // WITH_EDITOR
