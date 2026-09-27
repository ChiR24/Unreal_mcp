#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleDeleteSocket(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SocketName = GetJsonStringField(Payload, TEXT("socketName"));
    if (SocketName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("socketName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    USkeleton* Skeleton = LoadPayloadSkeletonOrReply(*this, RequestId, RequestingSocket, Payload);
    if (!Skeleton)
    {
        return true;
    }
    int32 SocketIndex = INDEX_NONE;
    if (!Skeleton->FindSocketAndIndex(FName(*SocketName), SocketIndex))
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Socket '%s' not found"), *SocketName), TEXT("SOCKET_NOT_FOUND"));
        return true;
    }

    Skeleton->Modify();
    Skeleton->Sockets.RemoveAt(SocketIndex);
    McpSafeAssetSave(Skeleton);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("socketName"), SocketName);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());
    Result->SetNumberField(TEXT("remainingSockets"), Skeleton->Sockets.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Socket '%s' deleted"), *SocketName), Result);
    return true;
}

