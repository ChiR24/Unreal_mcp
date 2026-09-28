#pragma once

#include "CoreMinimal.h"

#include "Engine/Brush.h"
#include "Engine/World.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"
#include "Domains/Volume/McpAutomationBridge_VolumeRequestParsing.h"
#include "Domains/Volume/McpAutomationBridge_VolumeResponses.h"
#include "Domains/Volume/McpAutomationBridge_VolumeWorldResolution.h"

namespace VolumeHelpers
{
bool CreateBoxBrushForVolume(ABrush* Volume, const FVector& Extent);
void SetVolumeExtentGeometry(AActor* VolumeActor, const FVector& Extent);

template<typename TVolumeClass>
TVolumeClass* SpawnVolumeActor(
    UWorld* World,
    const FString& VolumeName,
    const FVector& Location,
    const FRotator& Rotation,
    const FVector& Extent)
{
    if (!World)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TVolumeClass* Volume = World->SpawnActor<TVolumeClass>(Location, Rotation, SpawnParams);
    if (Volume)
    {
        if (!VolumeName.IsEmpty())
        {
            Volume->SetActorLabel(VolumeName);
        }
        if (Extent != FVector::ZeroVector)
        {
            if constexpr (TIsDerivedFrom<TVolumeClass, ABrush>::IsDerived)
            {
                CreateBoxBrushForVolume(Volume, Extent);
            }
        }
    }
    return Volume;
}

// The whole create_<box volume> request: name/location/rotation/extent from
// the payload, spawn in the editor world, reply with the volume's identity.
template<typename TVolumeClass>
bool CreateBoxVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FVector& DefaultExtent)
{
    const FString ClassName = TVolumeClass::StaticClass()->GetName();
    FVolumeCreateArgs Args;
    FVector Extent;
    UWorld* World = nullptr;
    if (!ReadNamedTransform(Subsystem, RequestId, Payload, Socket, TEXT("TriggerVolume"), Args) ||
        !ReadExtent(Subsystem, RequestId, Payload, Socket, TEXT("extent"), DefaultExtent, Extent) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return true;
    }
    TVolumeClass* Volume = SpawnVolumeActor<TVolumeClass>(World, Args.VolumeName, Args.Location, Args.Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn ") + ClassName, nullptr);
        return true;
    }
    LevelStructureHelpers::SendLevelEditResult(Subsystem, RequestId, Socket, Payload, Volume->GetLevel(),
        FString::Printf(TEXT("Created %s: %s"), *ClassName, *Args.VolumeName), CreateVolumeResponse(Volume, TEXT("A") + ClassName));
    return true;
}
}
