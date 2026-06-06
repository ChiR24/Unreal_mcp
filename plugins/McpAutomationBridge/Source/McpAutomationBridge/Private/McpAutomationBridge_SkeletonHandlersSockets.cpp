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

bool UMcpAutomationBridgeSubsystem::HandleListSockets(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);

    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> SocketArray;
    for (USkeletalMeshSocket* Socket : Skeleton->Sockets)
    {
        if (!Socket) continue;

        TSharedPtr<FJsonObject> SocketObj = McpHandlerUtils::CreateResultObject();
        SocketObj->SetStringField(TEXT("name"), Socket->SocketName.ToString());
        SocketObj->SetStringField(TEXT("boneName"), Socket->BoneName.ToString());

        // Use McpHandlerUtils helpers for transform JSON
        SocketObj->SetObjectField(TEXT("relativeLocation"), McpHandlerUtils::VectorToJson(Socket->RelativeLocation));
        SocketObj->SetObjectField(TEXT("relativeRotation"), McpHandlerUtils::RotatorToJson(Socket->RelativeRotation));
        SocketObj->SetObjectField(TEXT("relativeScale"), McpHandlerUtils::VectorToJson(Socket->RelativeScale));

        SocketArray.Add(MakeShared<FJsonValueObject>(SocketObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("sockets"), SocketArray);
    Result->SetNumberField(TEXT("count"), SocketArray.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sockets listed"), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleCreateSocket(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    }

    FString SocketName = GetStringFieldSkel(Payload, TEXT("socketName"));
    FString BoneName = GetStringFieldSkel(Payload, TEXT("attachBoneName"));
    if (BoneName.IsEmpty())
    {
        BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
    }

    if (SocketName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("socketName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    if (BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("attachBoneName or boneName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);

    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    // Check if socket already exists
    for (USkeletalMeshSocket* ExistingSocket : Skeleton->Sockets)
    {
        if (ExistingSocket && ExistingSocket->SocketName == FName(*SocketName))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Socket '%s' already exists"), *SocketName),
                TEXT("SOCKET_EXISTS"));
            return true;
        }
    }

    // Create the socket
    USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(Skeleton);
    if (!NewSocket)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create socket object"), TEXT("CREATION_FAILED"));
        return true;
    }
    // Parse socket transform using ParseVectorFromJson helpers
    NewSocket->SocketName = FName(*SocketName);
    NewSocket->RelativeLocation = ParseVectorFromJson(Payload, TEXT("relativeLocation"));
    NewSocket->RelativeRotation = ParseRotatorFromJson(Payload, TEXT("relativeRotation"));
    NewSocket->RelativeScale = ParseVectorFromJson(Payload, TEXT("relativeScale"), FVector::OneVector);
    NewSocket->BoneName = FName(*BoneName);

    Skeleton->Sockets.Add(NewSocket);
    McpSafeAssetSave(Skeleton);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("socketName"), SocketName);
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Socket '%s' created on bone '%s'"), *SocketName, *BoneName), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleConfigureSocket(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    }

    FString SocketName = GetStringFieldSkel(Payload, TEXT("socketName"));
    if (SocketName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("socketName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);

    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    // Find the socket
    USkeletalMeshSocket* Socket = nullptr;
    for (USkeletalMeshSocket* S : Skeleton->Sockets)
    {
        if (S && S->SocketName == FName(*SocketName))
        {
            Socket = S;
            break;
        }
    }

    if (!Socket)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Socket '%s' not found"), *SocketName),
            TEXT("SOCKET_NOT_FOUND"));
        return true;
    }

    // Update properties
    FString NewBoneName = GetStringFieldSkel(Payload, TEXT("attachBoneName"));
    if (!NewBoneName.IsEmpty())
    {
        Socket->BoneName = FName(*NewBoneName);
    }

    if (Payload->HasField(TEXT("relativeLocation")))
    {
        Socket->RelativeLocation = ParseVectorFromJson(Payload, TEXT("relativeLocation"));
    }

    if (Payload->HasField(TEXT("relativeRotation")))
    {
        Socket->RelativeRotation = ParseRotatorFromJson(Payload, TEXT("relativeRotation"));
    }

    if (Payload->HasField(TEXT("relativeScale")))
    {
        Socket->RelativeScale = ParseVectorFromJson(Payload, TEXT("relativeScale"), FVector::OneVector);
    }

    McpSafeAssetSave(Skeleton);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("socketName"), SocketName);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Socket '%s' configured"), *SocketName), Result);
    return true;
}

#endif // WITH_EDITOR
