#include "Dom/JsonObject.h"
// McpAutomationBridge_VolumeHandlers.cpp
// Phase 24: Volumes & Zones Handlers
//
// Complete volume and trigger system including:
// - Trigger Volumes (trigger_volume, trigger_box, trigger_sphere, trigger_capsule)
// - Gameplay Volumes (blocking, kill_z, pain_causing, physics)
// - Audio Volumes (audio, reverb)
// - Rendering Volumes (cull_distance, precomputed_visibility, lightmass_importance)
// - Navigation Volumes (nav_mesh_bounds, nav_modifier, camera_blocking)
// - Volume Configuration (set_volume_extent, set_volume_properties)

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/TriggerVolume.h"
#include "Engine/TriggerBox.h"
#include "Engine/TriggerSphere.h"
#include "Engine/TriggerCapsule.h"
#include "Engine/BlockingVolume.h"
#include "GameFramework/KillZVolume.h"
#include "GameFramework/PainCausingVolume.h"
#include "GameFramework/PhysicsVolume.h"
#include "Sound/AudioVolume.h"
#include "Sound/ReverbEffect.h"
#include "Engine/CullDistanceVolume.h"
#include "Lightmass/PrecomputedVisibilityVolume.h"
#include "Lightmass/LightmassImportanceVolume.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavModifierVolume.h"
#include "GameFramework/CameraBlockingVolume.h"
#include "Components/BrushComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Brush.h"
#include "Engine/Polys.h"
#include "Builders/CubeBuilder.h"
// PostProcessVolume only exists in UE 5.1-5.6 (removed in 5.7+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1 && ENGINE_MINOR_VERSION <= 6
#include "Engine/PostProcessVolume.h"
#define MCP_HAS_POSTPROCESS_VOLUME 1
#else
#define MCP_HAS_POSTPROCESS_VOLUME 0
#endif
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpVolumeHandlers, Log, All);

// ============================================================================
// Helper Functions
// ============================================================================
// NOTE: Uses consolidated JSON helpers from McpAutomationBridgeHelpers.h:
//   - GetJsonStringField(Obj, Field, Default)
//   - GetJsonNumberField(Obj, Field, Default)
//   - GetJsonBoolField(Obj, Field, Default)
//   - GetJsonIntField(Obj, Field, Default)
//   - ExtractVectorField(Source, FieldName, Default)
//   - ExtractRotatorField(Source, FieldName, Default)
// ============================================================================

namespace VolumeHelpers
{
#if WITH_EDITOR
    // Get current editor world
    UWorld* GetEditorWorld()
    {
        if (GEditor)
        {
            return GEditor->GetEditorWorldContext().World();
        }
        return nullptr;
    }

    // Get FVector from JSON object field
    FVector GetVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, FVector Default = FVector::ZeroVector)
    {
        return ExtractVectorField(Payload, *FieldName, Default);
    }

    // Get FRotator from JSON object field
    FRotator GetRotatorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, FRotator Default = FRotator::ZeroRotator)
    {
        return ExtractRotatorField(Payload, *FieldName, Default);
    }

    // Create a box brush for a volume
    // Note: UCubeBuilder is allocated with GetTransientPackage() as outer to prevent GC accumulation
    bool CreateBoxBrushForVolume(ABrush* Volume, const FVector& Extent)
    {
        if (!Volume)
        {
            return false;
        }

        // Use UCubeBuilder to create the brush shape
        // Allocate with transient package as outer to ensure proper GC cleanup
        UCubeBuilder* CubeBuilder = NewObject<UCubeBuilder>(GetTransientPackage());
        CubeBuilder->X = Extent.X * 2.0f;
        CubeBuilder->Y = Extent.Y * 2.0f;
        CubeBuilder->Z = Extent.Z * 2.0f;

        // Build the brush
        CubeBuilder->Build(Volume->GetWorld(), Volume);

        return true;
    }

    // Create a sphere brush for a volume (for TriggerSphere)
    // Uses UCubeBuilder for a bounding box but sets the collision shape via the component
    bool CreateSphereBrushForVolume(ABrush* Volume, float Radius)
    {
        if (!Volume)
        {
            return false;
        }

        // For sphere volumes, we create a bounding cube but the actual collision uses USphereComponent
        // The brush is only for editor visualization
        UCubeBuilder* CubeBuilder = NewObject<UCubeBuilder>(GetTransientPackage());
        CubeBuilder->X = Radius * 2.0f;
        CubeBuilder->Y = Radius * 2.0f;
        CubeBuilder->Z = Radius * 2.0f;

        CubeBuilder->Build(Volume->GetWorld(), Volume);

        return true;
    }

    // Create a capsule brush for a volume (for TriggerCapsule)
    // Uses UCubeBuilder for a bounding box but sets the collision shape via the component
    bool CreateCapsuleBrushForVolume(ABrush* Volume, float Radius, float HalfHeight)
    {
        if (!Volume)
        {
            return false;
        }

        // For capsule volumes, we create a bounding box but the actual collision uses UCapsuleComponent
        // The brush is only for editor visualization
        UCubeBuilder* CubeBuilder = NewObject<UCubeBuilder>(GetTransientPackage());
        CubeBuilder->X = Radius * 2.0f;
        CubeBuilder->Y = Radius * 2.0f;
        CubeBuilder->Z = HalfHeight * 2.0f;

        CubeBuilder->Build(Volume->GetWorld(), Volume);

        return true;
    }

    // Find volume by name in the world
    AActor* FindVolumeByName(UWorld* World, const FString& VolumeName)
    {
        if (!World || VolumeName.IsEmpty())
        {
            return nullptr;
        }

        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor && Actor->GetActorLabel().Equals(VolumeName, ESearchCase::IgnoreCase))
            {
                // Check if it's a volume type
                if (Actor->IsA<AVolume>() || Actor->IsA<ATriggerBase>())
                {
                    return Actor;
                }
            }
        }

        return nullptr;
    }

    // Generic volume spawning template for brush-based volumes (AVolume subclasses)
    // This version is used when TVolumeClass inherits from ABrush
    template<typename TVolumeClass>
    typename TEnableIf<TIsDerivedFrom<TVolumeClass, ABrush>::Value, TVolumeClass*>::Type
    SpawnVolumeActor(
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
            // Set label/name
            if (!VolumeName.IsEmpty())
            {
                Volume->SetActorLabel(VolumeName);
            }

            // For brush-based volumes, set up the brush geometry
            if (Extent != FVector::ZeroVector)
            {
                CreateBoxBrushForVolume(Volume, Extent);
            }
        }

        return Volume;
    }

    // Overload for non-brush trigger actors (ATriggerBox, ATriggerSphere, ATriggerCapsule)
    // These inherit from ATriggerBase which does NOT inherit from ABrush
    template<typename TVolumeClass>
    typename TEnableIf<!TIsDerivedFrom<TVolumeClass, ABrush>::Value, TVolumeClass*>::Type
    SpawnVolumeActor(
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
            // Set label/name
            if (!VolumeName.IsEmpty())
            {
                Volume->SetActorLabel(VolumeName);
            }

            // For non-brush triggers, the extent is set via their shape component
            // This is handled by the specific handler (e.g., HandleCreateTriggerBox)
            // No brush geometry to create here
        }

        return Volume;
    }
#endif
}

// ============================================================================
// Trigger Volume Handlers (4 actions)
// ============================================================================

#if WITH_EDITOR

static bool HandleCreateTriggerVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    ATriggerVolume* Volume = SpawnVolumeActor<ATriggerVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn TriggerVolume"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ATriggerVolume"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created TriggerVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreateTriggerBox(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerBox"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("boxExtent"), FVector(100.0f, 100.0f, 100.0f));
    if (Extent == FVector::ZeroVector)
    {
        Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    ATriggerBox* Volume = SpawnVolumeActor<ATriggerBox>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn TriggerBox"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ATriggerBox"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created TriggerBox: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreateTriggerSphere(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerSphere"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    float Radius = GetJsonNumberField(Payload, TEXT("sphereRadius"), 100.0f);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    // TriggerSphere is NOT brush-based - it uses USphereComponent for collision
    // Pass zero extent to skip brush creation (Cast<ABrush> will fail anyway)
    ATriggerSphere* Volume = SpawnVolumeActor<ATriggerSphere>(World, VolumeName, Location, Rotation, FVector::ZeroVector);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn TriggerSphere"), nullptr);
        return true;
    }

    // Configure the sphere component radius
    if (USphereComponent* SphereComp = Volume->GetCollisionComponent() ? Cast<USphereComponent>(Volume->GetCollisionComponent()) : nullptr)
    {
        SphereComp->SetSphereRadius(Radius);
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ATriggerSphere"));
    ResponseJson->SetNumberField(TEXT("radius"), Radius);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created TriggerSphere: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreateTriggerCapsule(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerCapsule"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    float Radius = GetJsonNumberField(Payload, TEXT("capsuleRadius"), 50.0f);
    float HalfHeight = GetJsonNumberField(Payload, TEXT("capsuleHalfHeight"), 100.0f);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    // TriggerCapsule is NOT brush-based - it uses UCapsuleComponent for collision
    // Pass zero extent to skip brush creation (Cast<ABrush> will fail anyway)
    ATriggerCapsule* Volume = SpawnVolumeActor<ATriggerCapsule>(World, VolumeName, Location, Rotation, FVector::ZeroVector);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn TriggerCapsule"), nullptr);
        return true;
    }

    // Configure the capsule component dimensions
    if (UCapsuleComponent* CapsuleComp = Volume->GetCollisionComponent() ? Cast<UCapsuleComponent>(Volume->GetCollisionComponent()) : nullptr)
    {
        CapsuleComp->SetCapsuleSize(Radius, HalfHeight);
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ATriggerCapsule"));
    ResponseJson->SetNumberField(TEXT("radius"), Radius);
    ResponseJson->SetNumberField(TEXT("halfHeight"), HalfHeight);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created TriggerCapsule: %s"), *VolumeName), ResponseJson);
    return true;
}

// ============================================================================
// Gameplay Volume Handlers (11 actions)
// ============================================================================

static bool HandleCreateBlockingVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("BlockingVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    ABlockingVolume* Volume = SpawnVolumeActor<ABlockingVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn BlockingVolume"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ABlockingVolume"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created BlockingVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreateKillZVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("KillZVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(10000.0f, 10000.0f, 100.0f));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AKillZVolume* Volume = SpawnVolumeActor<AKillZVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn KillZVolume"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("AKillZVolume"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created KillZVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreatePainCausingVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("PainCausingVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    bool bPainCausing = GetJsonBoolField(Payload, TEXT("bPainCausing"), true);
    float DamagePerSec = GetJsonNumberField(Payload, TEXT("damagePerSec"), 10.0f);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    APainCausingVolume* Volume = SpawnVolumeActor<APainCausingVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn PainCausingVolume"), nullptr);
        return true;
    }

    // Configure pain properties
    Volume->bPainCausing = bPainCausing;
    Volume->DamagePerSec = DamagePerSec;

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("APainCausingVolume"));
    ResponseJson->SetBoolField(TEXT("bPainCausing"), bPainCausing);
    ResponseJson->SetNumberField(TEXT("damagePerSec"), DamagePerSec);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created PainCausingVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreatePhysicsVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("PhysicsVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    bool bWaterVolume = GetJsonBoolField(Payload, TEXT("bWaterVolume"), false);
    float FluidFriction = GetJsonNumberField(Payload, TEXT("fluidFriction"), 0.3f);
    float TerminalVelocity = GetJsonNumberField(Payload, TEXT("terminalVelocity"), 4000.0f);
    int32 Priority = GetJsonIntField(Payload, TEXT("priority"), 0);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    APhysicsVolume* Volume = SpawnVolumeActor<APhysicsVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn PhysicsVolume"), nullptr);
        return true;
    }

    // Configure physics volume properties
    Volume->bWaterVolume = bWaterVolume;
    Volume->FluidFriction = FluidFriction;
    Volume->TerminalVelocity = TerminalVelocity;
    Volume->Priority = Priority;

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("APhysicsVolume"));
    ResponseJson->SetBoolField(TEXT("bWaterVolume"), bWaterVolume);
    ResponseJson->SetNumberField(TEXT("fluidFriction"), FluidFriction);
    ResponseJson->SetNumberField(TEXT("terminalVelocity"), TerminalVelocity);
    ResponseJson->SetNumberField(TEXT("priority"), Priority);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created PhysicsVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreateAudioVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("AudioVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(500.0f, 500.0f, 200.0f));
    bool bEnabled = GetJsonBoolField(Payload, TEXT("bEnabled"), true);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AAudioVolume* Volume = SpawnVolumeActor<AAudioVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn AudioVolume"), nullptr);
        return true;
    }

    // Configure audio volume properties
    Volume->SetEnabled(bEnabled);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("AAudioVolume"));
    ResponseJson->SetBoolField(TEXT("bEnabled"), bEnabled);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created AudioVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreateReverbVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("ReverbVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(500.0f, 500.0f, 200.0f));
    bool bEnabled = GetJsonBoolField(Payload, TEXT("bEnabled"), true);
    float ReverbVolumeLevel = GetJsonNumberField(Payload, TEXT("reverbVolume"), 0.5f);
    float FadeTime = GetJsonNumberField(Payload, TEXT("fadeTime"), 0.5f);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    // AudioVolume can act as a reverb volume through its reverb settings
    AAudioVolume* Volume = SpawnVolumeActor<AAudioVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn ReverbVolume (AudioVolume)"), nullptr);
        return true;
    }

    // Configure reverb settings
    Volume->SetEnabled(bEnabled);
    
    // Get the reverb settings and modify them
    FReverbSettings ReverbSettings = Volume->GetReverbSettings();
    ReverbSettings.bApplyReverb = true;
    ReverbSettings.Volume = ReverbVolumeLevel;
    ReverbSettings.FadeTime = FadeTime;
    Volume->SetReverbSettings(ReverbSettings);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("AAudioVolume (Reverb)"));
    ResponseJson->SetBoolField(TEXT("bEnabled"), bEnabled);
    ResponseJson->SetNumberField(TEXT("reverbVolume"), ReverbVolumeLevel);
    ResponseJson->SetNumberField(TEXT("fadeTime"), FadeTime);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created ReverbVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

#if MCP_HAS_POSTPROCESS_VOLUME
static bool HandleCreatePostProcessVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("PostProcessVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(500.0f, 500.0f, 500.0f));
    float Priority = GetJsonNumberField(Payload, TEXT("priority"), 0.0f);
    float BlendRadius = GetJsonNumberField(Payload, TEXT("blendRadius"), 100.0f);
    float BlendWeight = GetJsonNumberField(Payload, TEXT("blendWeight"), 1.0f);
    bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);
    bool bUnbound = GetJsonBoolField(Payload, TEXT("unbound"), false);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    APostProcessVolume* Volume = SpawnVolumeActor<APostProcessVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn PostProcessVolume"), nullptr);
        return true;
    }

    // Configure post process settings
    Volume->Priority = Priority;
    Volume->BlendRadius = BlendRadius;
    Volume->BlendWeight = BlendWeight;
    Volume->bEnabled = bEnabled;
    Volume->bUnbound = bUnbound;

    // Parse post process settings if provided
    if (Payload->HasTypedField<EJson::Object>(TEXT("postProcessSettings")))
    {
        TSharedPtr<FJsonObject> SettingsJson = Payload->GetObjectField(TEXT("postProcessSettings"));
        
        // Bloom
        if (SettingsJson->HasTypedField<EJson::Boolean>(TEXT("bloomEnabled")))
        {
            Volume->Settings.bOverride_BloomIntensity = true;
            Volume->Settings.BloomIntensity = SettingsJson->GetBoolField(TEXT("bloomEnabled")) ? 1.0f : 0.0f;
        }
        
        // Exposure
        if (SettingsJson->HasTypedField<EJson::Number>(TEXT("exposureBias")))
        {
            Volume->Settings.bOverride_AutoExposureBias = true;
            Volume->Settings.AutoExposureBias = SettingsJson->GetNumberField(TEXT("exposureBias"));
        }
        
        // Vignette
        if (SettingsJson->HasTypedField<EJson::Number>(TEXT("vignetteIntensity")))
        {
            Volume->Settings.bOverride_VignetteIntensity = true;
            Volume->Settings.VignetteIntensity = SettingsJson->GetNumberField(TEXT("vignetteIntensity"));
        }
        
        // Saturation
        if (SettingsJson->HasTypedField<EJson::Number>(TEXT("saturation")))
        {
            Volume->Settings.bOverride_ColorSaturation = true;
            FVector4 Saturation = Volume->Settings.ColorSaturation;
            Saturation.X = SettingsJson->GetNumberField(TEXT("saturation"));
            Saturation.Y = SettingsJson->GetNumberField(TEXT("saturation"));
            Saturation.Z = SettingsJson->GetNumberField(TEXT("saturation"));
            Volume->Settings.ColorSaturation = Saturation;
        }
        
        // Contrast
        if (SettingsJson->HasTypedField<EJson::Number>(TEXT("contrast")))
        {
            Volume->Settings.bOverride_ColorContrast = true;
            FVector4 Contrast = Volume->Settings.ColorContrast;
            Contrast.X = SettingsJson->GetNumberField(TEXT("contrast"));
            Contrast.Y = SettingsJson->GetNumberField(TEXT("contrast"));
            Contrast.Z = SettingsJson->GetNumberField(TEXT("contrast"));
            Volume->Settings.ColorContrast = Contrast;
        }
        
        // Gamma
        if (SettingsJson->HasTypedField<EJson::Number>(TEXT("gamma")))
        {
            Volume->Settings.bOverride_ColorGamma = true;
            FVector4 Gamma = Volume->Settings.ColorGamma;
            Gamma.X = SettingsJson->GetNumberField(TEXT("gamma"));
            Gamma.Y = SettingsJson->GetNumberField(TEXT("gamma"));
            Gamma.Z = SettingsJson->GetNumberField(TEXT("gamma"));
            Volume->Settings.ColorGamma = Gamma;
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("APostProcessVolume"));
    ResponseJson->SetNumberField(TEXT("priority"), Priority);
    ResponseJson->SetNumberField(TEXT("blendRadius"), BlendRadius);
    ResponseJson->SetNumberField(TEXT("blendWeight"), BlendWeight);
    ResponseJson->SetBoolField(TEXT("enabled"), bEnabled);
    ResponseJson->SetBoolField(TEXT("unbound"), bUnbound);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created PostProcessVolume: %s"), *VolumeName), ResponseJson);
    return true;
}
#endif // MCP_HAS_POSTPROCESS_VOLUME

static bool HandleCreateCullDistanceVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("CullDistanceVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(1000.0f, 1000.0f, 500.0f));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    ACullDistanceVolume* Volume = SpawnVolumeActor<ACullDistanceVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn CullDistanceVolume"), nullptr);
        return true;
    }

    // Parse and set cull distances if provided
    if (Payload->HasTypedField<EJson::Array>(TEXT("cullDistances")))
    {
        TArray<TSharedPtr<FJsonValue>> CullDistancesJson = Payload->GetArrayField(TEXT("cullDistances"));
        TArray<FCullDistanceSizePair> CullDistances;
        
        for (const TSharedPtr<FJsonValue>& Entry : CullDistancesJson)
        {
            if (Entry->Type == EJson::Object)
            {
                TSharedPtr<FJsonObject> EntryObj = Entry->AsObject();
                FCullDistanceSizePair Pair;
                Pair.Size = GetJsonNumberField(EntryObj, TEXT("size"), 100.0f);
                Pair.CullDistance = GetJsonNumberField(EntryObj, TEXT("cullDistance"), 5000.0f);
                CullDistances.Add(Pair);
            }
        }
        
        if (CullDistances.Num() > 0)
        {
            Volume->CullDistances = CullDistances;
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ACullDistanceVolume"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created CullDistanceVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreatePrecomputedVisibilityVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("PrecomputedVisibilityVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(1000.0f, 1000.0f, 500.0f));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    APrecomputedVisibilityVolume* Volume = SpawnVolumeActor<APrecomputedVisibilityVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn PrecomputedVisibilityVolume"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("APrecomputedVisibilityVolume"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created PrecomputedVisibilityVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreateLightmassImportanceVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("LightmassImportanceVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(5000.0f, 5000.0f, 2000.0f));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    ALightmassImportanceVolume* Volume = SpawnVolumeActor<ALightmassImportanceVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn LightmassImportanceVolume"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ALightmassImportanceVolume"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created LightmassImportanceVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreateNavMeshBoundsVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("NavMeshBoundsVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(2000.0f, 2000.0f, 500.0f));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    ANavMeshBoundsVolume* Volume = SpawnVolumeActor<ANavMeshBoundsVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn NavMeshBoundsVolume"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ANavMeshBoundsVolume"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created NavMeshBoundsVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreateNavModifierVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("NavModifierVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(500.0f, 500.0f, 200.0f));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    ANavModifierVolume* Volume = SpawnVolumeActor<ANavModifierVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn NavModifierVolume"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ANavModifierVolume"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created NavModifierVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleCreateCameraBlockingVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("CameraBlockingVolume"));
    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(200.0f, 200.0f, 200.0f));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    ACameraBlockingVolume* Volume = SpawnVolumeActor<ACameraBlockingVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn CameraBlockingVolume"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ACameraBlockingVolume"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created CameraBlockingVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

// ============================================================================
// Volume Configuration Handlers (2 actions)
// ============================================================================

static bool HandleSetVolumeExtent(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT(""));
    FVector NewExtent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));

    if (VolumeName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("volumeName is required"), nullptr);
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AActor* VolumeActor = FindVolumeByName(World, VolumeName);
    if (!VolumeActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Volume not found: %s"), *VolumeName), nullptr);
        return true;
    }

    ABrush* BrushVolume = Cast<ABrush>(VolumeActor);
    if (BrushVolume)
    {
        CreateBoxBrushForVolume(BrushVolume, NewExtent);
    }
    else
    {
        // For non-brush volumes, try to set actor scale
        VolumeActor->SetActorScale3D(FVector(NewExtent.X / 100.0f, NewExtent.Y / 100.0f, NewExtent.Z / 100.0f));
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeName);

    TSharedPtr<FJsonObject> ExtentJson = MakeShareable(new FJsonObject());
    ExtentJson->SetNumberField(TEXT("x"), NewExtent.X);
    ExtentJson->SetNumberField(TEXT("y"), NewExtent.Y);
    ExtentJson->SetNumberField(TEXT("z"), NewExtent.Z);
    ResponseJson->SetObjectField(TEXT("newExtent"), ExtentJson);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set extent for volume: %s"), *VolumeName), ResponseJson);
    return true;
}

static bool HandleSetVolumeProperties(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT(""));

    if (VolumeName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("volumeName is required"), nullptr);
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AActor* VolumeActor = FindVolumeByName(World, VolumeName);
    if (!VolumeActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Volume not found: %s"), *VolumeName), nullptr);
        return true;
    }

    TArray<FString> PropertiesSet;

    // Physics Volume properties
    if (APhysicsVolume* PhysicsVol = Cast<APhysicsVolume>(VolumeActor))
    {
        if (Payload->HasField(TEXT("bWaterVolume")))
        {
            PhysicsVol->bWaterVolume = GetJsonBoolField(Payload, TEXT("bWaterVolume"), false);
            PropertiesSet.Add(TEXT("bWaterVolume"));
        }
        if (Payload->HasField(TEXT("fluidFriction")))
        {
            PhysicsVol->FluidFriction = GetJsonNumberField(Payload, TEXT("fluidFriction"), 0.3f);
            PropertiesSet.Add(TEXT("fluidFriction"));
        }
        if (Payload->HasField(TEXT("terminalVelocity")))
        {
            PhysicsVol->TerminalVelocity = GetJsonNumberField(Payload, TEXT("terminalVelocity"), 4000.0f);
            PropertiesSet.Add(TEXT("terminalVelocity"));
        }
        if (Payload->HasField(TEXT("priority")))
        {
            PhysicsVol->Priority = GetJsonIntField(Payload, TEXT("priority"), 0);
            PropertiesSet.Add(TEXT("priority"));
        }
    }

    // Pain Causing Volume properties
    if (APainCausingVolume* PainVol = Cast<APainCausingVolume>(VolumeActor))
    {
        if (Payload->HasField(TEXT("bPainCausing")))
        {
            PainVol->bPainCausing = GetJsonBoolField(Payload, TEXT("bPainCausing"), true);
            PropertiesSet.Add(TEXT("bPainCausing"));
        }
        if (Payload->HasField(TEXT("damagePerSec")))
        {
            PainVol->DamagePerSec = GetJsonNumberField(Payload, TEXT("damagePerSec"), 10.0f);
            PropertiesSet.Add(TEXT("damagePerSec"));
        }
    }

    // Audio Volume properties
    if (AAudioVolume* AudioVol = Cast<AAudioVolume>(VolumeActor))
    {
        if (Payload->HasField(TEXT("bEnabled")))
        {
            AudioVol->SetEnabled(GetJsonBoolField(Payload, TEXT("bEnabled"), true));
            PropertiesSet.Add(TEXT("bEnabled"));
        }
        
        // Batch reverb settings changes to avoid multiple SetReverbSettings calls
        bool bModifiedReverb = false;
        FReverbSettings ReverbSettings = AudioVol->GetReverbSettings();
        
        if (Payload->HasField(TEXT("reverbVolume")))
        {
            ReverbSettings.Volume = GetJsonNumberField(Payload, TEXT("reverbVolume"), 0.5f);
            PropertiesSet.Add(TEXT("reverbVolume"));
            bModifiedReverb = true;
        }
        if (Payload->HasField(TEXT("fadeTime")))
        {
            ReverbSettings.FadeTime = GetJsonNumberField(Payload, TEXT("fadeTime"), 0.5f);
            PropertiesSet.Add(TEXT("fadeTime"));
            bModifiedReverb = true;
        }
        
        if (bModifiedReverb)
        {
            AudioVol->SetReverbSettings(ReverbSettings);
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeName);

    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString& Prop : PropertiesSet)
    {
        PropsArray.Add(MakeShareable(new FJsonValueString(Prop)));
    }
    ResponseJson->SetArrayField(TEXT("propertiesSet"), PropsArray);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set %d properties for volume: %s"), PropertiesSet.Num(), *VolumeName), ResponseJson);
    return true;
}

// ============================================================================
// Utility Handlers (1 action)
// ============================================================================

static bool HandleGetVolumesInfo(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString Filter = GetJsonStringField(Payload, TEXT("filter"), TEXT(""));
    FString VolumeType = GetJsonStringField(Payload, TEXT("volumeType"), TEXT(""));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> VolumesArray;
    int32 TotalCount = 0;

    for (TActorIterator<AVolume> It(World); It; ++It)
    {
        AVolume* Volume = *It;
        if (!Volume) continue;

        // Apply type filter if specified
        if (!VolumeType.IsEmpty())
        {
            FString ClassName = Volume->GetClass()->GetName();
            if (!ClassName.Contains(VolumeType))
            {
                continue;
            }
        }

        // Apply name filter if specified
        if (!Filter.IsEmpty())
        {
            FString ActorLabel = Volume->GetActorLabel();
            if (!ActorLabel.Contains(Filter))
            {
                continue;
            }
        }

        TSharedPtr<FJsonObject> VolumeInfo = MakeShareable(new FJsonObject());
        VolumeInfo->SetStringField(TEXT("name"), Volume->GetActorLabel());
        VolumeInfo->SetStringField(TEXT("class"), Volume->GetClass()->GetName());

        FVector Location = Volume->GetActorLocation();
        TSharedPtr<FJsonObject> LocationJson = MakeShareable(new FJsonObject());
        LocationJson->SetNumberField(TEXT("x"), Location.X);
        LocationJson->SetNumberField(TEXT("y"), Location.Y);
        LocationJson->SetNumberField(TEXT("z"), Location.Z);
        VolumeInfo->SetObjectField(TEXT("location"), LocationJson);

        // Try to get bounds
        FVector Origin, BoxExtent;
        Volume->GetActorBounds(false, Origin, BoxExtent);
        TSharedPtr<FJsonObject> ExtentJson = MakeShareable(new FJsonObject());
        ExtentJson->SetNumberField(TEXT("x"), BoxExtent.X);
        ExtentJson->SetNumberField(TEXT("y"), BoxExtent.Y);
        ExtentJson->SetNumberField(TEXT("z"), BoxExtent.Z);
        VolumeInfo->SetObjectField(TEXT("extent"), ExtentJson);

        VolumesArray.Add(MakeShareable(new FJsonValueObject(VolumeInfo)));
        TotalCount++;
    }

    // Also iterate over trigger actors (ATriggerBase doesn't inherit from AVolume)
    for (TActorIterator<ATriggerBase> It(World); It; ++It)
    {
        ATriggerBase* Trigger = *It;
        if (!Trigger) continue;

        // Apply type filter if specified
        if (!VolumeType.IsEmpty())
        {
            FString ClassName = Trigger->GetClass()->GetName();
            if (!ClassName.Contains(VolumeType) && !VolumeType.Equals(TEXT("Trigger"), ESearchCase::IgnoreCase))
            {
                continue;
            }
        }

        // Apply name filter if specified
        if (!Filter.IsEmpty())
        {
            FString ActorLabel = Trigger->GetActorLabel();
            if (!ActorLabel.Contains(Filter))
            {
                continue;
            }
        }

        TSharedPtr<FJsonObject> VolumeInfo = MakeShareable(new FJsonObject());
        VolumeInfo->SetStringField(TEXT("name"), Trigger->GetActorLabel());
        VolumeInfo->SetStringField(TEXT("class"), Trigger->GetClass()->GetName());

        FVector Location = Trigger->GetActorLocation();
        TSharedPtr<FJsonObject> LocationJson = MakeShareable(new FJsonObject());
        LocationJson->SetNumberField(TEXT("x"), Location.X);
        LocationJson->SetNumberField(TEXT("y"), Location.Y);
        LocationJson->SetNumberField(TEXT("z"), Location.Z);
        VolumeInfo->SetObjectField(TEXT("location"), LocationJson);

        // Get bounds
        FVector Origin, BoxExtent;
        Trigger->GetActorBounds(false, Origin, BoxExtent);
        TSharedPtr<FJsonObject> ExtentJson = MakeShareable(new FJsonObject());
        ExtentJson->SetNumberField(TEXT("x"), BoxExtent.X);
        ExtentJson->SetNumberField(TEXT("y"), BoxExtent.Y);
        ExtentJson->SetNumberField(TEXT("z"), BoxExtent.Z);
        VolumeInfo->SetObjectField(TEXT("extent"), ExtentJson);

        VolumesArray.Add(MakeShareable(new FJsonValueObject(VolumeInfo)));
        TotalCount++;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    
    TSharedPtr<FJsonObject> VolumesInfo = MakeShareable(new FJsonObject());
    VolumesInfo->SetNumberField(TEXT("totalCount"), TotalCount);
    VolumesInfo->SetArrayField(TEXT("volumes"), VolumesArray);
    
    ResponseJson->SetObjectField(TEXT("volumesInfo"), VolumesInfo);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d volumes/triggers"), TotalCount), ResponseJson);
    return true;
}

// ============================================================================
// Volume Removal Handler (1 action)
// ============================================================================

static bool HandleRemoveVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT(""));

    if (VolumeName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("volumeName is required for remove_volume"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    // Find the volume by name
    AActor* VolumeActor = FindVolumeByName(World, VolumeName);
    if (!VolumeActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Volume not found: %s"), *VolumeName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Store info before destroying
    FString VolumeClass = VolumeActor->GetClass()->GetName();
    FString VolumeLabel = VolumeActor->GetActorLabel();

    // Destroy the volume actor
    World->DestroyActor(VolumeActor, true);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeLabel);
    ResponseJson->SetStringField(TEXT("volumeClass"), VolumeClass);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed volume: %s"), *VolumeName), ResponseJson);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageVolumesAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpVolumeHandlers, Verbose, TEXT("HandleManageVolumesAction: SubAction=%s"), *SubAction);

    // Trigger Volumes
    if (SubAction == TEXT("create_trigger_volume"))
    {
        return HandleCreateTriggerVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_trigger_box"))
    {
        return HandleCreateTriggerBox(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_trigger_sphere"))
    {
        return HandleCreateTriggerSphere(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_trigger_capsule"))
    {
        return HandleCreateTriggerCapsule(this, RequestId, Payload, Socket);
    }

    // Gameplay Volumes
    if (SubAction == TEXT("create_blocking_volume"))
    {
        return HandleCreateBlockingVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_kill_z_volume"))
    {
        return HandleCreateKillZVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_pain_causing_volume"))
    {
        return HandleCreatePainCausingVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_physics_volume"))
    {
        return HandleCreatePhysicsVolume(this, RequestId, Payload, Socket);
    }

    // Audio Volumes
    if (SubAction == TEXT("create_audio_volume"))
    {
        return HandleCreateAudioVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_reverb_volume"))
    {
        return HandleCreateReverbVolume(this, RequestId, Payload, Socket);
    }

    // Rendering Volumes
#if MCP_HAS_POSTPROCESS_VOLUME
    if (SubAction == TEXT("create_post_process_volume"))
    {
        return HandleCreatePostProcessVolume(this, RequestId, Payload, Socket);
    }
#else
    // PostProcessVolume only exists in UE 5.1-5.6 (removed in 5.0 and 5.7+)
    if (SubAction == TEXT("create_post_process_volume"))
    {
        SendAutomationResponse(Socket, RequestId, false,
            TEXT("PostProcessVolume is only available in UE 5.1-5.6"), nullptr, TEXT("UNSUPPORTED_VERSION"));
        return true;
    }
#endif
    if (SubAction == TEXT("create_cull_distance_volume"))
    {
        return HandleCreateCullDistanceVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_precomputed_visibility_volume"))
    {
        return HandleCreatePrecomputedVisibilityVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_lightmass_importance_volume"))
    {
        return HandleCreateLightmassImportanceVolume(this, RequestId, Payload, Socket);
    }

    // Navigation Volumes
    if (SubAction == TEXT("create_nav_mesh_bounds_volume"))
    {
        return HandleCreateNavMeshBoundsVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_nav_modifier_volume"))
    {
        return HandleCreateNavModifierVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_camera_blocking_volume"))
    {
        return HandleCreateCameraBlockingVolume(this, RequestId, Payload, Socket);
    }

    // Volume Configuration
    if (SubAction == TEXT("set_volume_extent"))
    {
        return HandleSetVolumeExtent(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_volume_properties"))
    {
        return HandleSetVolumeProperties(this, RequestId, Payload, Socket);
    }

    // Volume Removal
    if (SubAction == TEXT("remove_volume"))
    {
        return HandleRemoveVolume(this, RequestId, Payload, Socket);
    }

    // Utility
    if (SubAction == TEXT("get_volumes_info"))
    {
        return HandleGetVolumesInfo(this, RequestId, Payload, Socket);
    }

    // Unknown action
    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown volume subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;

#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Volume operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
