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
// PostProcessVolume exists in UE 5.1+ (verified in 5.3, 5.6, 5.7)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
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

    // ============================================================================
    // Validation Helpers
    // ============================================================================

    // Validate volume name - reject empty, path traversal, and invalid characters
    // Returns true if valid, false if invalid (sets OutError on failure)
    bool ValidateVolumeName(const FString& VolumeName, FString& OutError)
    {
        if (VolumeName.IsEmpty())
        {
            OutError = TEXT("volumeName is required");
            return false;
        }

        // Reject path traversal
        if (VolumeName.Contains(TEXT("..")) || VolumeName.Contains(TEXT("/")) || VolumeName.Contains(TEXT("\\")))
        {
            OutError = TEXT("volumeName must not contain path separators or traversal sequences");
            return false;
        }

        // Reject Windows drive letters
        if (VolumeName.Contains(TEXT(":")))
        {
            OutError = TEXT("volumeName must not contain drive letters");
            return false;
        }

        return true;
    }

    // Validate extent vector - reject negative, NaN, or Infinity values
    // Returns true if valid, false if invalid (sets OutError on failure)
    bool ValidateExtent(const FVector& Extent, FString& OutError)
    {
        if (!FMath::IsFinite(Extent.X) || !FMath::IsFinite(Extent.Y) || !FMath::IsFinite(Extent.Z))
        {
            OutError = TEXT("extent contains NaN or Infinity values");
            return false;
        }

        if (Extent.X <= 0.0f || Extent.Y <= 0.0f || Extent.Z <= 0.0f)
        {
            OutError = TEXT("extent values must be positive");
            return false;
        }

        return true;
    }

    // Validate radius - reject negative, NaN, or Infinity values
    // Returns true if valid, false if invalid (sets OutError on failure)
    bool ValidateRadius(float Radius, FString& OutError)
    {
        if (!FMath::IsFinite(Radius))
        {
            OutError = TEXT("radius contains NaN or Infinity value");
            return false;
        }

        if (Radius <= 0.0f)
        {
            OutError = TEXT("radius must be positive");
            return false;
        }

        return true;
    }

    // Validate capsule dimensions - reject negative, NaN, or Infinity values
    // Returns true if valid, false if invalid (sets OutError on failure)
    bool ValidateCapsuleDimensions(float Radius, float HalfHeight, FString& OutError)
    {
        if (!FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight))
        {
            OutError = TEXT("capsule dimensions contain NaN or Infinity values");
            return false;
        }

        if (Radius <= 0.0f)
        {
            OutError = TEXT("capsule radius must be positive");
            return false;
        }

        if (HalfHeight <= 0.0f)
        {
            OutError = TEXT("capsule half height must be positive");
            return false;
        }

        return true;
    }

    // Validate location vector - reject NaN or Infinity values (zero is valid)
    // Returns true if valid, false if invalid (sets OutError on failure)
    bool ValidateLocation(const FVector& Location, FString& OutError)
    {
        if (!FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y) || !FMath::IsFinite(Location.Z))
        {
            OutError = TEXT("location contains NaN or Infinity values");
            return false;
        }

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);
    
    // Add location for verification
    TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
    LocationObj->SetNumberField(TEXT("x"), Volume->GetActorLocation().X);
    LocationObj->SetNumberField(TEXT("y"), Volume->GetActorLocation().Y);
    LocationObj->SetNumberField(TEXT("z"), Volume->GetActorLocation().Z);
    ResponseJson->SetObjectField(TEXT("location"), LocationObj);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("boxExtent"), FVector(100.0f, 100.0f, 100.0f));
    if (Extent == FVector::ZeroVector)
    {
        Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    }
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);
    
    // Add box extent for verification
    TSharedPtr<FJsonObject> ExtentObj = MakeShared<FJsonObject>();
    ExtentObj->SetNumberField(TEXT("x"), Extent.X);
    ExtentObj->SetNumberField(TEXT("y"), Extent.Y);
    ExtentObj->SetNumberField(TEXT("z"), Extent.Z);
    ResponseJson->SetObjectField(TEXT("boxExtent"), ExtentObj);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    float Radius = GetJsonNumberField(Payload, TEXT("sphereRadius"), 100.0f);
    if (!ValidateRadius(Radius, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    float Radius = GetJsonNumberField(Payload, TEXT("capsuleRadius"), 50.0f);
    float HalfHeight = GetJsonNumberField(Payload, TEXT("capsuleHalfHeight"), 100.0f);
    if (!ValidateCapsuleDimensions(Radius, HalfHeight, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(10000.0f, 10000.0f, 100.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(500.0f, 500.0f, 200.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(500.0f, 500.0f, 200.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(500.0f, 500.0f, 500.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(1000.0f, 1000.0f, 500.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(1000.0f, 1000.0f, 500.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(5000.0f, 5000.0f, 2000.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(2000.0f, 2000.0f, 500.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(500.0f, 500.0f, 200.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // Validate volumeName parameter with backward-compatible default
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("TriggerVolume"));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FRotator Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(200.0f, 200.0f, 200.0f));
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

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

    // MODIFY operation - volumeName is required (no default)
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT(""));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FVector NewExtent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    if (!ValidateExtent(NewExtent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
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
            FString::Printf(TEXT("Volume not found: %s"), *VolumeName), nullptr, TEXT("NOT_FOUND"));
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
    
    // Add verification data
    AddActorVerification(ResponseJson, VolumeActor);

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

    // MODIFY operation - volumeName is required (no default)
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT(""));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
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
            FString::Printf(TEXT("Volume not found: %s"), *VolumeName), nullptr, TEXT("NOT_FOUND"));
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
    
    // Add verification data
    AddActorVerification(ResponseJson, VolumeActor);

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

    // Security check: reject path parameter if present (this tool doesn't use paths)
    FString PathParam = GetJsonStringField(Payload, TEXT("path"), TEXT(""));
    if (!PathParam.IsEmpty())
    {
        if (PathParam.Contains(TEXT("..")) || PathParam.Contains(TEXT("\\")))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                TEXT("get_volumes_info does not accept path parameter with traversal characters"),
                nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }
    }

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

    // DELETE operation - volumeName is required (no default)
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT(""));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
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
    ResponseJson->SetBoolField(TEXT("existsAfter"), false);
    ResponseJson->SetStringField(TEXT("action"), TEXT("manage_volumes:deleted"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed volume: %s"), *VolumeName), ResponseJson);
    return true;
}

// ============================================================================
// Add Volume To Actor Handlers (6 actions)
// These create volumes positioned at an existing actor's location
// ============================================================================

// Helper to find actor by path or name
static AActor* FindActorByPathOrName(UWorld* World, const FString& ActorPath)
{
    if (!World || ActorPath.IsEmpty())
    {
        return nullptr;
    }

    // Try to interpret as actor name first
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor)
        {
            // Check actor label
            if (Actor->GetActorLabel().Equals(ActorPath, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
            // Check actor name
            if (Actor->GetName().Equals(ActorPath, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
            // Check path-like format (e.g., /Game/MCPTest/TestActor)
            FString ActorPathName = Actor->GetPathName();
            if (ActorPathName.Equals(ActorPath, ESearchCase::IgnoreCase) ||
                ActorPathName.EndsWith(ActorPath, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
        }
    }

    return nullptr;
}

static bool HandleAddTriggerVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"), TEXT(""));
    if (ActorPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    // Validate actorPath for security (no path traversal)
    if (ActorPath.Contains(TEXT("..")) || ActorPath.Contains(TEXT("\\")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath contains invalid characters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    FString ValidationError;
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    // Find the target actor
    AActor* TargetActor = FindActorByPathOrName(World, ActorPath);
    if (!TargetActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Create trigger volume at actor's location
    FVector Location = TargetActor->GetActorLocation();
    FRotator Rotation = TargetActor->GetActorRotation();
    
    FString VolumeName = TargetActor->GetActorLabel() + TEXT("_TriggerVolume");
    ATriggerVolume* Volume = SpawnVolumeActor<ATriggerVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn TriggerVolume"), nullptr);
        return true;
    }

    // Attach to the target actor - check return value
    // Note: AttachToActor returns void in UE 5.0, bool in UE 5.1+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    bool bAttachmentSucceeded = Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
#else
    Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
    bool bAttachmentSucceeded = true;  // Assume success in UE 5.0
#endif

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ATriggerVolume"));
    ResponseJson->SetStringField(TEXT("attachedTo"), TargetActor->GetActorLabel());
    ResponseJson->SetBoolField(TEXT("attachmentSucceeded"), bAttachmentSucceeded);
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

    // If attachment failed, return success=false since user's intent (attach to actor) failed
    // Volume was still created, so include it in response for debugging
    FString ResponseMessage = bAttachmentSucceeded 
        ? FString::Printf(TEXT("Added TriggerVolume to actor: %s"), *TargetActor->GetActorLabel())
        : FString::Printf(TEXT("TriggerVolume created but attachment to '%s' failed (volume is static, target may be movable)"), *TargetActor->GetActorLabel());
    
    Subsystem->SendAutomationResponse(Socket, RequestId, bAttachmentSucceeded, ResponseMessage, ResponseJson,
        bAttachmentSucceeded ? TEXT("") : TEXT("ATTACHMENT_FAILED"));
    return true;
}

static bool HandleAddBlockingVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"), TEXT(""));
    if (ActorPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    if (ActorPath.Contains(TEXT("..")) || ActorPath.Contains(TEXT("\\")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath contains invalid characters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(200.0f, 200.0f, 200.0f));
    FString ValidationError;
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AActor* TargetActor = FindActorByPathOrName(World, ActorPath);
    if (!TargetActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FVector Location = TargetActor->GetActorLocation();
    FRotator Rotation = TargetActor->GetActorRotation();
    
    FString VolumeName = TargetActor->GetActorLabel() + TEXT("_BlockingVolume");
    ABlockingVolume* Volume = SpawnVolumeActor<ABlockingVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn BlockingVolume"), nullptr);
        return true;
    }

    // Attach to the target actor - check return value
    // Note: AttachToActor returns void in UE 5.0, bool in UE 5.1+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    bool bAttachmentSucceeded = Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
#else
    Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
    bool bAttachmentSucceeded = true;  // Assume success in UE 5.0
#endif

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ABlockingVolume"));
    ResponseJson->SetStringField(TEXT("attachedTo"), TargetActor->GetActorLabel());
    ResponseJson->SetBoolField(TEXT("attachmentSucceeded"), bAttachmentSucceeded);
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

    // If attachment failed, return success=false since user's intent (attach to actor) failed
    // Volume was still created, so include it in response for debugging
    FString ResponseMessage = bAttachmentSucceeded 
        ? FString::Printf(TEXT("Added BlockingVolume to actor: %s"), *TargetActor->GetActorLabel())
        : FString::Printf(TEXT("BlockingVolume created but attachment to '%s' failed (volume is static, target may be movable)"), *TargetActor->GetActorLabel());
    
    Subsystem->SendAutomationResponse(Socket, RequestId, bAttachmentSucceeded, ResponseMessage, ResponseJson,
        bAttachmentSucceeded ? TEXT("") : TEXT("ATTACHMENT_FAILED"));
    return true;
}

static bool HandleAddKillZVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"), TEXT(""));
    if (ActorPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    if (ActorPath.Contains(TEXT("..")) || ActorPath.Contains(TEXT("\\")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath contains invalid characters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(1000.0f, 1000.0f, 100.0f));
    float KillZHeight = GetJsonNumberField(Payload, TEXT("killZHeight"), 0.0f);
    
    FString ValidationError;
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AActor* TargetActor = FindActorByPathOrName(World, ActorPath);
    if (!TargetActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FVector Location = TargetActor->GetActorLocation();
    // Use KillZHeight for Z position if specified
    if (KillZHeight != 0.0f)
    {
        Location.Z = KillZHeight;
    }
    FRotator Rotation = TargetActor->GetActorRotation();
    
    FString VolumeName = TargetActor->GetActorLabel() + TEXT("_KillZVolume");
    AKillZVolume* Volume = SpawnVolumeActor<AKillZVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn KillZVolume"), nullptr);
        return true;
    }

    // Attach to the target actor - check return value
    // Note: AttachToActor returns void in UE 5.0, bool in UE 5.1+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    bool bAttachmentSucceeded = Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
#else
    Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
    bool bAttachmentSucceeded = true;  // Assume success in UE 5.0
#endif

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("AKillZVolume"));
    ResponseJson->SetStringField(TEXT("attachedTo"), TargetActor->GetActorLabel());
    ResponseJson->SetNumberField(TEXT("killZHeight"), Location.Z);
    ResponseJson->SetBoolField(TEXT("attachmentSucceeded"), bAttachmentSucceeded);
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

    // If attachment failed, return success=false since user's intent (attach to actor) failed
    // Volume was still created, so include it in response for debugging
    FString ResponseMessage = bAttachmentSucceeded 
        ? FString::Printf(TEXT("Added KillZVolume to actor: %s"), *TargetActor->GetActorLabel())
        : FString::Printf(TEXT("KillZVolume created but attachment to '%s' failed (volume is static, target may be movable)"), *TargetActor->GetActorLabel());
    
    Subsystem->SendAutomationResponse(Socket, RequestId, bAttachmentSucceeded, ResponseMessage, ResponseJson,
        bAttachmentSucceeded ? TEXT("") : TEXT("ATTACHMENT_FAILED"));
    return true;
}

static bool HandleAddPhysicsVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"), TEXT(""));
    if (ActorPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    if (ActorPath.Contains(TEXT("..")) || ActorPath.Contains(TEXT("\\")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath contains invalid characters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(300.0f, 300.0f, 300.0f));
    FString ValidationError;
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Optional physics properties
    bool bWaterVolume = GetJsonBoolField(Payload, TEXT("bWaterVolume"), false);
    float FluidFriction = GetJsonNumberField(Payload, TEXT("fluidFriction"), 0.3f);
    float TerminalVelocity = GetJsonNumberField(Payload, TEXT("terminalVelocity"), 4000.0f);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AActor* TargetActor = FindActorByPathOrName(World, ActorPath);
    if (!TargetActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FVector Location = TargetActor->GetActorLocation();
    FRotator Rotation = TargetActor->GetActorRotation();
    
    FString VolumeName = TargetActor->GetActorLabel() + TEXT("_PhysicsVolume");
    APhysicsVolume* Volume = SpawnVolumeActor<APhysicsVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn PhysicsVolume"), nullptr);
        return true;
    }

    // Configure physics properties
    Volume->bWaterVolume = bWaterVolume;
    Volume->FluidFriction = FluidFriction;
    Volume->TerminalVelocity = TerminalVelocity;

    // Attach to the target actor - check return value
    // Note: AttachToActor returns void in UE 5.0, bool in UE 5.1+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    bool bAttachmentSucceeded = Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
#else
    Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
    bool bAttachmentSucceeded = true;  // Assume success in UE 5.0
#endif

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("APhysicsVolume"));
    ResponseJson->SetStringField(TEXT("attachedTo"), TargetActor->GetActorLabel());
    ResponseJson->SetBoolField(TEXT("bWaterVolume"), bWaterVolume);
    ResponseJson->SetBoolField(TEXT("attachmentSucceeded"), bAttachmentSucceeded);
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

    // If attachment failed, return success=false since user's intent (attach to actor) failed
    // Volume was still created, so include it in response for debugging
    FString ResponseMessage = bAttachmentSucceeded 
        ? FString::Printf(TEXT("Added PhysicsVolume to actor: %s"), *TargetActor->GetActorLabel())
        : FString::Printf(TEXT("PhysicsVolume created but attachment to '%s' failed (volume is static, target may be movable)"), *TargetActor->GetActorLabel());
    
    Subsystem->SendAutomationResponse(Socket, RequestId, bAttachmentSucceeded, ResponseMessage, ResponseJson,
        bAttachmentSucceeded ? TEXT("") : TEXT("ATTACHMENT_FAILED"));
    return true;
}

static bool HandleAddCullDistanceVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"), TEXT(""));
    if (ActorPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    if (ActorPath.Contains(TEXT("..")) || ActorPath.Contains(TEXT("\\")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath contains invalid characters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(1000.0f, 1000.0f, 500.0f));
    FString ValidationError;
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AActor* TargetActor = FindActorByPathOrName(World, ActorPath);
    if (!TargetActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FVector Location = TargetActor->GetActorLocation();
    FRotator Rotation = TargetActor->GetActorRotation();
    
    FString VolumeName = TargetActor->GetActorLabel() + TEXT("_CullDistanceVolume");
    ACullDistanceVolume* Volume = SpawnVolumeActor<ACullDistanceVolume>(World, VolumeName, Location, Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn CullDistanceVolume"), nullptr);
        return true;
    }

    // Parse cull distances if provided
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

    // Attach to the target actor - check return value
    // Note: AttachToActor returns void in UE 5.0, bool in UE 5.1+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    bool bAttachmentSucceeded = Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
#else
    Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
    bool bAttachmentSucceeded = true;  // Assume success in UE 5.0
#endif

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("ACullDistanceVolume"));
    ResponseJson->SetStringField(TEXT("attachedTo"), TargetActor->GetActorLabel());
    ResponseJson->SetBoolField(TEXT("attachmentSucceeded"), bAttachmentSucceeded);
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

    // If attachment failed, return success=false since user's intent (attach to actor) failed
    // Volume was still created, so include it in response for debugging
    FString ResponseMessage = bAttachmentSucceeded 
        ? FString::Printf(TEXT("Added CullDistanceVolume to actor: %s"), *TargetActor->GetActorLabel())
        : FString::Printf(TEXT("CullDistanceVolume created but attachment to '%s' failed (volume is static, target may be movable)"), *TargetActor->GetActorLabel());
    
    Subsystem->SendAutomationResponse(Socket, RequestId, bAttachmentSucceeded, ResponseMessage, ResponseJson,
        bAttachmentSucceeded ? TEXT("") : TEXT("ATTACHMENT_FAILED"));
    return true;
}

#if MCP_HAS_POSTPROCESS_VOLUME
static bool HandleAddPostProcessVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"), TEXT(""));
    if (ActorPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    if (ActorPath.Contains(TEXT("..")) || ActorPath.Contains(TEXT("\\")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorPath contains invalid characters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    FVector Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(500.0f, 500.0f, 500.0f));
    FString ValidationError;
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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

    AActor* TargetActor = FindActorByPathOrName(World, ActorPath);
    if (!TargetActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FVector Location = TargetActor->GetActorLocation();
    FRotator Rotation = TargetActor->GetActorRotation();
    
    FString VolumeName = TargetActor->GetActorLabel() + TEXT("_PostProcessVolume");
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

    // Attach to the target actor - check return value
    // Note: AttachToActor returns void in UE 5.0, bool in UE 5.1+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    bool bAttachmentSucceeded = Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
#else
    Volume->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
    bool bAttachmentSucceeded = true;  // Assume success in UE 5.0
#endif

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("APostProcessVolume"));
    ResponseJson->SetStringField(TEXT("attachedTo"), TargetActor->GetActorLabel());
    ResponseJson->SetNumberField(TEXT("priority"), Priority);
    ResponseJson->SetBoolField(TEXT("attachmentSucceeded"), bAttachmentSucceeded);
    
    // Add verification data
    AddActorVerification(ResponseJson, Volume);

    // If attachment failed, return success=false since user's intent (attach to actor) failed
    // Volume was still created, so include it in response for debugging
    FString ResponseMessage = bAttachmentSucceeded 
        ? FString::Printf(TEXT("Added PostProcessVolume to actor: %s"), *TargetActor->GetActorLabel())
        : FString::Printf(TEXT("PostProcessVolume created but attachment to '%s' failed (volume is static, target may be movable)"), *TargetActor->GetActorLabel());
    
    Subsystem->SendAutomationResponse(Socket, RequestId, bAttachmentSucceeded, ResponseMessage, ResponseJson,
        bAttachmentSucceeded ? TEXT("") : TEXT("ATTACHMENT_FAILED"));
    return true;
}
#endif

// ============================================================================
// Volume Bounds Handler (1 action)
// Set volume bounds using min/max corners instead of extent
// ============================================================================

static bool HandleSetVolumeBounds(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;

    // MODIFY operation - volumeName is required (no default)
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT(""));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    // Parse bounds array [minX, minY, minZ, maxX, maxY, maxZ]
    TArray<float> BoundsValues;
    if (Payload->HasTypedField<EJson::Array>(TEXT("bounds")))
    {
        TArray<TSharedPtr<FJsonValue>> BoundsArray = Payload->GetArrayField(TEXT("bounds"));
        for (const TSharedPtr<FJsonValue>& Value : BoundsArray)
        {
            BoundsValues.Add(static_cast<float>(Value->AsNumber()));
        }
    }

    if (BoundsValues.Num() != 6)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("bounds must be an array of 6 values [minX, minY, minZ, maxX, maxY, maxZ]"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Validate bounds values
    for (float Val : BoundsValues)
    {
        if (!FMath::IsFinite(Val))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                TEXT("bounds contains NaN or Infinity values"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }

    FVector MinBound(BoundsValues[0], BoundsValues[1], BoundsValues[2]);
    FVector MaxBound(BoundsValues[3], BoundsValues[4], BoundsValues[5]);

    // Calculate center and extent from bounds
    FVector Center = (MinBound + MaxBound) * 0.5f;
    FVector Extent = (MaxBound - MinBound) * 0.5f;

    if (Extent.X <= 0.0f || Extent.Y <= 0.0f || Extent.Z <= 0.0f)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("bounds must define a valid volume (max > min for all axes)"), nullptr, TEXT("INVALID_ARGUMENT"));
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
            FString::Printf(TEXT("Volume not found: %s"), *VolumeName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Set the volume location to center
    VolumeActor->SetActorLocation(Center);

    // Update the brush geometry for brush-based volumes
    ABrush* BrushVolume = Cast<ABrush>(VolumeActor);
    if (BrushVolume)
    {
        CreateBoxBrushForVolume(BrushVolume, Extent);
    }
    else
    {
        // For non-brush volumes, use scale
        VolumeActor->SetActorScale3D(FVector(Extent.X / 100.0f, Extent.Y / 100.0f, Extent.Z / 100.0f));
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeName);
    
    // Add verification data
    AddActorVerification(ResponseJson, VolumeActor);

    TSharedPtr<FJsonObject> BoundsJson = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> MinArray, MaxArray;
    MinArray.Add(MakeShareable(new FJsonValueNumber(MinBound.X)));
    MinArray.Add(MakeShareable(new FJsonValueNumber(MinBound.Y)));
    MinArray.Add(MakeShareable(new FJsonValueNumber(MinBound.Z)));
    MaxArray.Add(MakeShareable(new FJsonValueNumber(MaxBound.X)));
    MaxArray.Add(MakeShareable(new FJsonValueNumber(MaxBound.Y)));
    MaxArray.Add(MakeShareable(new FJsonValueNumber(MaxBound.Z)));
    BoundsJson->SetArrayField(TEXT("min"), MinArray);
    BoundsJson->SetArrayField(TEXT("max"), MaxArray);
    ResponseJson->SetObjectField(TEXT("bounds"), BoundsJson);

    TSharedPtr<FJsonObject> CenterJson = MakeShareable(new FJsonObject());
    CenterJson->SetNumberField(TEXT("x"), Center.X);
    CenterJson->SetNumberField(TEXT("y"), Center.Y);
    CenterJson->SetNumberField(TEXT("z"), Center.Z);
    ResponseJson->SetObjectField(TEXT("center"), CenterJson);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set bounds for volume: %s"), *VolumeName), ResponseJson);
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
    // PostProcessVolume requires UE 5.1+
    if (SubAction == TEXT("create_post_process_volume"))
    {
        SendAutomationResponse(Socket, RequestId, false,
            TEXT("PostProcessVolume requires UE 5.1 or later"), nullptr, TEXT("UNSUPPORTED_VERSION"));
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
    if (SubAction == TEXT("set_volume_bounds"))
    {
        return HandleSetVolumeBounds(this, RequestId, Payload, Socket);
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

    // Add Volume To Actor handlers
    if (SubAction == TEXT("add_trigger_volume"))
    {
        return HandleAddTriggerVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("add_blocking_volume"))
    {
        return HandleAddBlockingVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("add_kill_z_volume"))
    {
        return HandleAddKillZVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("add_physics_volume"))
    {
        return HandleAddPhysicsVolume(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("add_cull_distance_volume"))
    {
        return HandleAddCullDistanceVolume(this, RequestId, Payload, Socket);
    }
#if MCP_HAS_POSTPROCESS_VOLUME
    if (SubAction == TEXT("add_post_process_volume"))
    {
        return HandleAddPostProcessVolume(this, RequestId, Payload, Socket);
    }
#else
    if (SubAction == TEXT("add_post_process_volume"))
    {
        SendAutomationResponse(Socket, RequestId, false,
            TEXT("PostProcessVolume requires UE 5.1 or later"), nullptr, TEXT("UNSUPPORTED_VERSION"));
        return true;
    }
#endif

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
