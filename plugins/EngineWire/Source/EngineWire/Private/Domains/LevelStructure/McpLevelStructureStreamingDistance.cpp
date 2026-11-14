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
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Reflection/McpEnumName.h"
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
    const FString LevelName = GetJsonStringField(Payload, TEXT("levelName"));

    if (LevelName.IsEmpty())
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("levelName is required for set_streaming_distance"), TEXT("INVALID_ARGUMENT"));
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
        Subsystem->SendAutomationError(Socket, RequestId, StreamingDistance <= 0.0
            ? FString(TEXT("streamingDistance must be greater than 0. Nothing was created."))
            : FString::Printf(TEXT("streamingUsage '%s' is not one: use Loading, LoadingAndVisibility, VisibilityBlockingOnLoad, "
                "BlockingOnLoad or LoadingNotVisible. Nothing was created."), *StreamingUsage), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("No editor world available"), TEXT("NO_EDITOR_WORLD"));
        return true;
    }

    // A read (createVolume false) adds no level: one the world does not stream has no volumes to list.
    ULevelStreaming* FoundLevel = FindOrAddStreamingLevelOrReply(Subsystem, RequestId, Socket, World, LevelName, bCreateVolume);
    if (!FoundLevel)
    {
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
                // The usage's name, as the create branch answers it and streamingUsage takes it, not its number.
                VolumeObj->SetStringField(TEXT("usage"),
                    McpEnumName(Volume->StreamingUsage).RightChop(4));
                VolumesArray.Add(MakeShared<FJsonValueObject>(VolumeObj));
            }
        }

        TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(ResponseJson, World);
        ResponseJson->SetStringField(TEXT("levelName"), LevelName);
        ResponseJson->SetArrayField(TEXT("streamingVolumes"), VolumesArray);
        ResponseJson->SetNumberField(TEXT("volumeCount"), VolumesArray.Num());
        ResponseJson->SetStringField(TEXT("note"), TEXT("Use createVolume=true to create a streaming volume for distance-based loading"));

        // A read: nothing to save, so no save and no note that the level is unsaved.
        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Level '%s' has %d streaming volume(s)"), *LevelName, VolumesArray.Num()), ResponseJson);
        return true;
    }

    // One undo step for the volume and the level's list of volumes, closed before the level is saved.
    TUniquePtr<FMcpScopedEditorTransaction> SpawnStep = McpBeginSpawnTransaction(RequestId);
    // A spawned volume has no brush: scaling it left bounds of zero, a volume no viewer is ever inside, so the level
    // never streamed by distance. Spawned with a box reaching streamingDistance from location on each axis.
    ALevelStreamingVolume* NewVolume = VolumeHelpers::SpawnVolumeActor<ALevelStreamingVolume>(
        World, FString::Printf(TEXT("StreamingVolume_%s"), *LevelName), VolumeLocation, FRotator::ZeroRotator,
        FVector(StreamingDistance));

    if (!NewVolume)
    {
        if (SpawnStep) SpawnStep->Cancel(); // nothing was placed
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("Failed to spawn ALevelStreamingVolume actor"), TEXT("ACTOR_SPAWN_FAILED"));
        return true;
    }

    NewVolume->StreamingUsage = static_cast<EStreamingVolumeUsage>(Usage);

    // As the Details panel does: the change event updates each streaming volume's list of levels, which Play reads.
    // Without it the volume streamed nothing until the level was saved and loaded again. The level is given
    // RF_Transactional (as in configure_level_streaming), so undo takes the volume out of its list too.
    FoundLevel->SetFlags(RF_Transactional);
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
    ResponseJson->SetStringField(TEXT("streamingUsage"), McpEnumName(NewVolume->StreamingUsage).RightChop(4));

    ResponseJson->SetObjectField(TEXT("volumeLocation"), McpHandlerUtils::VectorToJson(VolumeLocation));

    ResponseJson->SetNumberField(TEXT("totalStreamingVolumes"), FoundLevel->EditorStreamingVolumes.Num());

    FString Message = FString::Printf(TEXT("Created streaming volume for level '%s' with distance %.0f at (%f, %f, %f)"),
        *LevelName, StreamingDistance, VolumeLocation.X, VolumeLocation.Y, VolumeLocation.Z);
    SpawnStep.Reset();
    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, World->PersistentLevel, Message, ResponseJson);
    return true;
}

}
