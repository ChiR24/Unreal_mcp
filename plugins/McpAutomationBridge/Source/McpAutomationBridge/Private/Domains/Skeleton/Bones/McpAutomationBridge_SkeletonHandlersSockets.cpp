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

bool UMcpAutomationBridgeSubsystem::HandleListSockets(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    USkeleton* Skeleton = LoadPayloadSkeletonOrReply(*this, RequestId, RequestingSocket, Payload);
    if (!Skeleton)
    {
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> SocketArray;
    for (USkeletalMeshSocket* Socket : Skeleton->Sockets)
    {
        if (!Socket) continue;

        TSharedPtr<FJsonObject> SocketObj = McpHandlerUtils::CreateResultObject();
        SocketObj->SetStringField(TEXT("name"), Socket->SocketName.ToString());
        SocketObj->SetStringField(TEXT("boneName"), Socket->BoneName.ToString());

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
    FString SocketName = GetJsonStringField(Payload, TEXT("socketName"));
    FString BoneName = GetJsonStringField(Payload, TEXT("attachBoneName"));
    if (BoneName.IsEmpty())
    {
        BoneName = GetJsonStringField(Payload, TEXT("boneName"));
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

    USkeleton* Skeleton = LoadPayloadSkeletonOrReply(*this, RequestId, RequestingSocket, Payload);
    if (!Skeleton)
    {
        return true;
    }

    if (Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(*BoneName)) == INDEX_NONE)
    {
        // A socket on a bone that does not exist is silently dead; refuse it.
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Bone %s not found on skeleton %s (use list_bones)"), *BoneName, *Skeleton->GetPathName()),
            TEXT("BONE_NOT_FOUND"));
        return true;
    }
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

    USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(Skeleton);
    if (!NewSocket)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create socket object"), TEXT("CREATION_FAILED"));
        return true;
    }
    NewSocket->SocketName = FName(*SocketName);
    NewSocket->RelativeLocation = ExtractVectorField(Payload, TEXT("relativeLocation"), FVector::ZeroVector);
    NewSocket->RelativeRotation = ExtractRotatorField(Payload, TEXT("relativeRotation"), FRotator::ZeroRotator);
    NewSocket->RelativeScale = ExtractVectorField(Payload, TEXT("relativeScale"), FVector::OneVector);
    NewSocket->BoneName = FName(*BoneName);

    // delete_socket already calls Modify() before it mutates Sockets; without
    // it here the add is saved but cannot be undone.
    Skeleton->Modify();
    Skeleton->Sockets.Add(NewSocket);
    McpSafeAssetSave(Skeleton);

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

    USkeletalMeshSocket* Socket = Skeleton->FindSocket(FName(*SocketName));

    if (!Socket)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Socket '%s' not found"), *SocketName),
            TEXT("SOCKET_NOT_FOUND"));
        return true;
    }

    FString NewBoneName = GetJsonStringField(Payload, TEXT("attachBoneName"));
    if (!NewBoneName.IsEmpty() &&
        Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(*NewBoneName)) == INDEX_NONE)
    {
        // create_socket refuses an unknown bone for exactly this reason ("a
        // socket on a bone that does not exist is silently dead"); moving an
        // existing socket onto one was still accepted and reported success.
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Bone %s not found on skeleton %s (use list_bones)"), *NewBoneName, *Skeleton->GetPathName()),
            TEXT("BONE_NOT_FOUND"));
        return true;
    }

    Skeleton->Modify();
    if (!NewBoneName.IsEmpty())
    {
        Socket->BoneName = FName(*NewBoneName);
    }

    if (Payload->HasField(TEXT("relativeLocation")))
    {
        Socket->RelativeLocation = ExtractVectorField(Payload, TEXT("relativeLocation"), FVector::ZeroVector);
    }

    if (Payload->HasField(TEXT("relativeRotation")))
    {
        Socket->RelativeRotation = ExtractRotatorField(Payload, TEXT("relativeRotation"), FRotator::ZeroRotator);
    }

    if (Payload->HasField(TEXT("relativeScale")))
    {
        Socket->RelativeScale = ExtractVectorField(Payload, TEXT("relativeScale"), FVector::OneVector);
    }

    McpSafeAssetSave(Skeleton);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("socketName"), SocketName);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Socket '%s' configured"), *SocketName), Result);
    return true;
}

