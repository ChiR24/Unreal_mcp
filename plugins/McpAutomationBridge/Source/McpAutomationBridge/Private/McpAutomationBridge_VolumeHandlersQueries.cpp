#include "McpAutomationBridge_VolumeActionDeclarations.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/TriggerBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Volume.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_VolumeRequestParsing.h"
#include "McpAutomationBridge_VolumeResponses.h"
#include "McpAutomationBridge_VolumeWorldResolution.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

namespace McpVolumeHandlers
{
#if WITH_EDITOR
template<typename TActorClass>
static void AppendVolumeInfo(UWorld* World, const FString& Filter, const FString& VolumeType, bool bTriggerAlias, TArray<TSharedPtr<FJsonValue>>& VolumesArray, int32& TotalCount)
{
    for (TActorIterator<TActorClass> It(World); It; ++It)
    {
        TActorClass* Actor = *It;
        if (!Actor)
        {
            continue;
        }
        const FString ClassName = Actor->GetClass()->GetName();
        if (!VolumeType.IsEmpty() && !ClassName.Contains(VolumeType) &&
            !(bTriggerAlias && VolumeType.Equals(TEXT("Trigger"), ESearchCase::IgnoreCase)))
        {
            continue;
        }
        const FString ActorLabel = Actor->GetActorLabel();
        if (!Filter.IsEmpty() && !ActorLabel.Contains(Filter))
        {
            continue;
        }
        TSharedPtr<FJsonObject> VolumeInfo = McpHandlerUtils::CreateResultObject();
        VolumeInfo->SetStringField(TEXT("name"), ActorLabel);
        VolumeInfo->SetStringField(TEXT("class"), ClassName);
        VolumeInfo->SetObjectField(TEXT("location"), VolumeHelpers::CreateVectorObject(Actor->GetActorLocation()));
        FVector Origin;
        FVector BoxExtent;
        Actor->GetActorBounds(false, Origin, BoxExtent);
        VolumeInfo->SetObjectField(TEXT("extent"), VolumeHelpers::CreateVectorObject(BoxExtent));
        VolumesArray.Add(MakeShared<FJsonValueObject>(VolumeInfo));
        TotalCount++;
    }
}

bool HandleGetVolumesInfo(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString PathParam = GetJsonStringField(Payload, TEXT("path"), TEXT(""));
    if (!PathParam.IsEmpty() && (PathParam.Contains(TEXT("..")) || PathParam.Contains(TEXT("\\"))))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("get_volumes_info does not accept path parameter with traversal characters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }
    UWorld* World = nullptr;
    if (!VolumeHelpers::ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return true;
    }
    const FString Filter = GetJsonStringField(Payload, TEXT("filter"), TEXT(""));
    const FString VolumeType = GetJsonStringField(Payload, TEXT("volumeType"), TEXT(""));
    TArray<TSharedPtr<FJsonValue>> VolumesArray;
    int32 TotalCount = 0;
    AppendVolumeInfo<AVolume>(World, Filter, VolumeType, false, VolumesArray, TotalCount);
    AppendVolumeInfo<ATriggerBase>(World, Filter, VolumeType, true, VolumesArray, TotalCount);
    TSharedPtr<FJsonObject> VolumesInfo = McpHandlerUtils::CreateResultObject();
    VolumesInfo->SetNumberField(TEXT("totalCount"), TotalCount);
    VolumesInfo->SetArrayField(TEXT("volumes"), VolumesArray);
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetObjectField(TEXT("volumesInfo"), VolumesInfo);
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d volumes/triggers"), TotalCount), ResponseJson);
    return true;
}

bool HandleRemoveVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT(""));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    UWorld* World = nullptr;
    if (!ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return true;
    }
    AActor* VolumeActor = FindVolumeByName(World, VolumeName);
    if (!VolumeActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Volume not found: %s"), *VolumeName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    const FString VolumeClass = VolumeActor->GetClass()->GetName();
    const FString VolumeLabel = VolumeActor->GetActorLabel();
    World->DestroyActor(VolumeActor, true);
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeLabel);
    ResponseJson->SetStringField(TEXT("volumeClass"), VolumeClass);
    ResponseJson->SetBoolField(TEXT("existsAfter"), false);
    ResponseJson->SetStringField(TEXT("action"), TEXT("manage_volumes:deleted"));
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed volume: %s"), *VolumeName), ResponseJson);
    return true;
}
#endif
}
