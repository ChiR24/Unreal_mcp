#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"
#include "Domains/Volume/McpVolumeGeometry.h"

#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/LevelStreamingVolume.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace McpLevelStructure
{

bool HandleSetStreamingDistance(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    // CRITICAL: levelName is required - no default fallback
    FString LevelName;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("levelName"), LevelName);
    }

    if (LevelName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("levelName is required for set_streaming_distance"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double StreamingDistance = GetJsonNumberField(Payload, TEXT("streamingDistance"), 10000.0);
    FString StreamingUsage = GetJsonStringField(Payload, TEXT("streamingUsage"), TEXT("LoadingAndVisibility"));
    FVector VolumeLocation = ExtractVectorField(Payload, TEXT("volumeLocation"), FVector::ZeroVector);
    bool bCreateVolume = GetJsonBoolField(Payload, TEXT("createVolume"), true);

    // An unknown usage became LoadingAndVisibility without a word; checked before anything is spawned.
    const int64 Usage = StaticEnum<EStreamingVolumeUsage>()->GetValueByNameString(TEXT("SVB_") + StreamingUsage);
    if (bCreateVolume && (Usage < 0 || Usage >= SVB_MAX || StreamingDistance <= 0.0))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, StreamingDistance <= 0.0
            ? FString(TEXT("streamingDistance must be greater than 0. Nothing was created."))
            : FString::Printf(TEXT("streamingUsage '%s' is not one: use Loading, LoadingAndVisibility, VisibilityBlockingOnLoad, "
                "BlockingOnLoad or LoadingNotVisible. Nothing was created."), *StreamingUsage), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_EDITOR_WORLD"));
        return true;
    }

    ULevelStreaming* FoundLevel = FindOrAddStreamingLevel(World, LevelName);
    if (!FoundLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Streaming level not found: %s"), *LevelName), nullptr, TEXT("LEVEL_NOT_FOUND"));
        return true;
    }

    // ULevelStreaming doesn't have a streaming distance property directly
    // Instead, we create/configure an ALevelStreamingVolume and associate it

    if (!bCreateVolume)
    {
        // Just report current streaming volumes
        TArray<TSharedPtr<FJsonValue>> VolumesArray;
        for (ALevelStreamingVolume* Volume : FoundLevel->EditorStreamingVolumes)
        {
            if (Volume)
            {
                TSharedPtr<FJsonObject> VolumeObj = McpHandlerUtils::CreateResultObject();
                VolumeObj->SetStringField(TEXT("name"), McpActorRef(Volume));
                VolumeObj->SetNumberField(TEXT("usage"), static_cast<int32>(Volume->StreamingUsage));
                VolumesArray.Add(MakeShared<FJsonValueObject>(VolumeObj));
            }
        }

        TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(ResponseJson, World);
        ResponseJson->SetStringField(TEXT("levelName"), LevelName);
        ResponseJson->SetArrayField(TEXT("streamingVolumes"), VolumesArray);
        ResponseJson->SetNumberField(TEXT("volumeCount"), VolumesArray.Num());
        ResponseJson->SetStringField(TEXT("note"), TEXT("Use createVolume=true to create a streaming volume for distance-based loading"));

        SendLevelEditResult(Subsystem, RequestId, Socket, Payload, World->PersistentLevel,
            FString::Printf(TEXT("Level '%s' has %d streaming volume(s)"), *LevelName, VolumesArray.Num()), ResponseJson);
        return true;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = MakeUniqueObjectName(World, ALevelStreamingVolume::StaticClass(),
        FName(*FString::Printf(TEXT("StreamingVolume_%s"), *LevelName)));
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ALevelStreamingVolume* NewVolume = World->SpawnActor<ALevelStreamingVolume>(
        ALevelStreamingVolume::StaticClass(),
        VolumeLocation,
        FRotator::ZeroRotator,
        SpawnParams
    );

    if (!NewVolume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn ALevelStreamingVolume actor"), nullptr);
        return true;
    }

    NewVolume->SetActorLabel(FString::Printf(TEXT("StreamingVolume_%s"), *LevelName));

    NewVolume->StreamingUsage = static_cast<EStreamingVolumeUsage>(Usage);

    // A spawned volume has no brush: scaling it left bounds of zero, a volume no viewer is ever inside, so the level
    // never streamed by distance. A box reaching streamingDistance from location on each axis.
    VolumeHelpers::CreateBoxBrushForVolume(NewVolume, FVector(StreamingDistance));

    // As the Details panel does: the change event updates each streaming volume's list of levels, which Play reads.
    // Without it the volume streamed nothing until the level was saved and loaded again.
    FoundLevel->Modify();
    FoundLevel->EditorStreamingVolumes.AddUnique(NewVolume);
    FPropertyChangedEvent VolumesChanged(FindFProperty<FProperty>(ULevelStreaming::StaticClass(),
        GET_MEMBER_NAME_CHECKED(ULevelStreaming, EditorStreamingVolumes)), EPropertyChangeType::ArrayAdd);
    FoundLevel->PostEditChangeProperty(VolumesChanged);

    FoundLevel->MarkPackageDirty();
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, NewVolume);
    ResponseJson->SetStringField(TEXT("levelName"), LevelName);
    ResponseJson->SetStringField(TEXT("volumeName"), NewVolume->GetActorLabel());
    ResponseJson->SetNumberField(TEXT("streamingDistance"), StreamingDistance);
    ResponseJson->SetStringField(TEXT("streamingUsage"), StaticEnum<EStreamingVolumeUsage>()->GetNameStringByValue(Usage).RightChop(4));

    ResponseJson->SetObjectField(TEXT("volumeLocation"), McpHandlerUtils::VectorToJson(VolumeLocation));

    ResponseJson->SetNumberField(TEXT("totalStreamingVolumes"), FoundLevel->EditorStreamingVolumes.Num());

    FString Message = FString::Printf(TEXT("Created streaming volume for level '%s' with distance %.0f at (%f, %f, %f)"),
        *LevelName, StreamingDistance, VolumeLocation.X, VolumeLocation.Y, VolumeLocation.Z);
    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, World->PersistentLevel, Message, ResponseJson);
    return true;
}

}
