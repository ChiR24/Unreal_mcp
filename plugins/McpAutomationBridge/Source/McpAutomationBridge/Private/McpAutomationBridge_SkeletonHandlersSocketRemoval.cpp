#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleDeleteSocket(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    FString SocketName = GetStringFieldSkel(Payload, TEXT("socketName"));

    if (SocketName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("socketName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Try skeletal mesh first
    if (!SkeletalMeshPath.IsEmpty())
    {
        FString Error;
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
        if (!Mesh)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
            return true;
        }

        USkeleton* Skeleton = Mesh->GetSkeleton();
        if (Skeleton)
        {
            int32 SocketIndex = Skeleton->Sockets.IndexOfByPredicate(
                [&SocketName](const USkeletalMeshSocket* S) { return S && S->SocketName == FName(*SocketName); });

            if (SocketIndex != INDEX_NONE)
            {
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
        }

        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Socket '%s' not found"), *SocketName), TEXT("SOCKET_NOT_FOUND"));
        return true;
    }
    else if (!SkeletonPath.IsEmpty())
    {
        FString Error;
        USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
        if (!Skeleton)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
            return true;
        }

        int32 SocketIndex = Skeleton->Sockets.IndexOfByPredicate(
            [&SocketName](const USkeletalMeshSocket* S) { return S && S->SocketName == FName(*SocketName); });

        if (SocketIndex != INDEX_NONE)
        {
            Skeleton->Modify();
            Skeleton->Sockets.RemoveAt(SocketIndex);
            McpSafeAssetSave(Skeleton);

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("socketName"), SocketName);
            Result->SetStringField(TEXT("skeletonPath"), SkeletonPath);
            Result->SetNumberField(TEXT("remainingSockets"), Skeleton->Sockets.Num());

            SendAutomationResponse(RequestingSocket, RequestId, true,
                FString::Printf(TEXT("Socket '%s' deleted"), *SocketName), Result);
            return true;
        }

        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Socket '%s' not found"), *SocketName), TEXT("SOCKET_NOT_FOUND"));
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId,
        TEXT("skeletalMeshPath or skeletonPath is required"), TEXT("MISSING_PARAM"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("delete_socket requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

#endif // WITH_EDITOR
