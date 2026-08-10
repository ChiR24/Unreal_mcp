#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructurePayload.h"

#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/LevelStreamingVolume.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleSetStreamingDistance(
    UMcpAutomationBridgeSubsystem* Subsystem,
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
    TSharedPtr<FJsonObject> VolumeLocationJson = GetObjectField(Payload, TEXT("volumeLocation"));
    FVector VolumeLocation = VolumeLocationJson.IsValid() ? LevelStructureHelpers::GetVectorFromJson(VolumeLocationJson) : FVector::ZeroVector;
    bool bCreateVolume = GetJsonBoolField(Payload, TEXT("createVolume"), true);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_EDITOR_WORLD"));
        return true;
    }

    ULevelStreaming* FoundLevel = nullptr;
    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (StreamingLevel && StreamingLevel->GetWorldAssetPackageFName().ToString().Contains(LevelName))
        {
            FoundLevel = StreamingLevel;
            break;
        }
    }

    // This handles cases where the sublevel was created but the streaming reference wasn't loaded
    if (!FoundLevel)
    {
        TArray<FString> PotentialPaths;

        if (LevelName.StartsWith(TEXT("/Game/")))
        {
            PotentialPaths.Add(LevelName);
        }
        FString WorldPath = FPaths::GetPath(World->GetOutermost()->GetName());
        PotentialPaths.Add(WorldPath / LevelName);
        PotentialPaths.Add(FString(TEXT("/Game/")) / LevelName);
        PotentialPaths.Add(FString(TEXT("/Game/")) + LevelName);

        for (const FString& TestPath : PotentialPaths)
        {
            FString TestFullPath = TestPath;
            if (!TestFullPath.EndsWith(TEXT(".umap")))
            {
                // Already a package path, check if package exists
                if (FPackageName::DoesPackageExist(TestFullPath))
                {
                    ULevelStreamingDynamic* NewStreamingLevel = NewObject<ULevelStreamingDynamic>(World, ULevelStreamingDynamic::StaticClass());
                    if (NewStreamingLevel)
                    {
                        NewStreamingLevel->SetWorldAssetByPackageName(FName(*TestFullPath));
                        NewStreamingLevel->LevelTransform = FTransform::Identity;
                        NewStreamingLevel->SetShouldBeVisible(true);
                        NewStreamingLevel->SetShouldBeLoaded(true);

                        World->AddStreamingLevel(NewStreamingLevel);
                        FoundLevel = NewStreamingLevel;

                        UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("Created streaming reference for existing level: %s"), *TestFullPath);
                        break;
                    }
                }
            }
        }
    }

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
                VolumeObj->SetStringField(TEXT("name"), Volume->GetActorLabel());
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

        Subsystem->SendAutomationResponse(Socket, RequestId, true,
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

    if (StreamingUsage == TEXT("Loading"))
    {
        NewVolume->StreamingUsage = EStreamingVolumeUsage::SVB_Loading;
    }
    else if (StreamingUsage == TEXT("VisibilityBlockingOnLoad"))
    {
        NewVolume->StreamingUsage = EStreamingVolumeUsage::SVB_VisibilityBlockingOnLoad;
    }
    else if (StreamingUsage == TEXT("BlockingOnLoad"))
    {
        NewVolume->StreamingUsage = EStreamingVolumeUsage::SVB_BlockingOnLoad;
    }
    else if (StreamingUsage == TEXT("LoadingNotVisible"))
    {
        NewVolume->StreamingUsage = EStreamingVolumeUsage::SVB_LoadingNotVisible;
    }
    else // Default: LoadingAndVisibility
    {
        NewVolume->StreamingUsage = EStreamingVolumeUsage::SVB_LoadingAndVisibility;
    }

    // Scale the volume to match the streaming distance (brush default is ~200 units cube)
    // We scale to create a sphere-like volume with radius = StreamingDistance
    FVector DesiredScale = FVector(StreamingDistance / 100.0); // Brush is ~200 units, half = 100
    NewVolume->SetActorScale3D(DesiredScale);

    FoundLevel->EditorStreamingVolumes.AddUnique(NewVolume);

    // Note: UpdateStreamingLevelsRefs() is not exported/available in all UE versions
    // The association via EditorStreamingVolumes is sufficient - refs update on save
    UE_LOG(LogMcpLevelStructureHandlers, Verbose, TEXT("Streaming volume created - refs will update on save"));

    FoundLevel->MarkPackageDirty();
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, NewVolume);
    ResponseJson->SetStringField(TEXT("levelName"), LevelName);
    ResponseJson->SetStringField(TEXT("volumeName"), NewVolume->GetActorLabel());
    ResponseJson->SetNumberField(TEXT("streamingDistance"), StreamingDistance);
    ResponseJson->SetStringField(TEXT("streamingUsage"), StreamingUsage);

    TSharedPtr<FJsonObject> LocationJson = McpHandlerUtils::CreateResultObject();
    LocationJson->SetNumberField(TEXT("x"), VolumeLocation.X);
    LocationJson->SetNumberField(TEXT("y"), VolumeLocation.Y);
    LocationJson->SetNumberField(TEXT("z"), VolumeLocation.Z);
    ResponseJson->SetObjectField(TEXT("volumeLocation"), LocationJson);

    ResponseJson->SetNumberField(TEXT("totalStreamingVolumes"), FoundLevel->EditorStreamingVolumes.Num());

    FString Message = FString::Printf(TEXT("Created streaming volume for level '%s' with distance %.0f at (%f, %f, %f)"),
        *LevelName, StreamingDistance, VolumeLocation.X, VolumeLocation.Y, VolumeLocation.Z);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
#endif
