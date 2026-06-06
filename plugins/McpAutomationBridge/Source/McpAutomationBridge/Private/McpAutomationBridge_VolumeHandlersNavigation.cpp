#include "McpAutomationBridge_VolumeActionDeclarations.h"

#include "Dom/JsonObject.h"
#include "GameFramework/CameraBlockingVolume.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_VolumeGeometry.h"
#include "McpAutomationBridge_VolumeRequestParsing.h"
#include "McpAutomationBridge_VolumeResponses.h"
#include "McpAutomationBridge_VolumeWorldResolution.h"
#include "McpBridgeWebSocket.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavModifierVolume.h"

namespace McpVolumeHandlers
{
#if WITH_EDITOR
template<typename TVolumeClass>
static bool CreateNavigationVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FVector& DefaultExtent,
    const FString& ClassText,
    const FString& FailureText,
    const FString& MessagePrefix)
{
    using namespace VolumeHelpers;
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
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FailureText, nullptr);
        return true;
    }
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        MessagePrefix + Args.VolumeName, CreateVolumeResponse(Volume, ClassText));
    return true;
}

bool HandleCreateNavMeshBoundsVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return CreateNavigationVolume<ANavMeshBoundsVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(2000.0f, 2000.0f, 500.0f), TEXT("ANavMeshBoundsVolume"), TEXT("Failed to spawn NavMeshBoundsVolume"), TEXT("Created NavMeshBoundsVolume: "));
}

bool HandleCreateNavModifierVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return CreateNavigationVolume<ANavModifierVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(500.0f, 500.0f, 200.0f), TEXT("ANavModifierVolume"), TEXT("Failed to spawn NavModifierVolume"), TEXT("Created NavModifierVolume: "));
}

bool HandleCreateCameraBlockingVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return CreateNavigationVolume<ACameraBlockingVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(200.0f, 200.0f, 200.0f), TEXT("ACameraBlockingVolume"), TEXT("Failed to spawn CameraBlockingVolume"), TEXT("Created CameraBlockingVolume: "));
}
#endif
}
