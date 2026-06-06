#include "McpAutomationBridge_LevelStructureActions.h"
#include "McpAutomationBridge_LevelStructureEditorWorld.h"
#include "McpAutomationBridge_LevelStructurePayload.h"

#include "Engine/World.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Misc/PackageName.h"
#include "PackedLevelActor/PackedLevelActor.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleCreateLevelInstance(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString LevelInstanceName = GetJsonStringField(Payload, TEXT("levelInstanceName"), TEXT("LevelInstance"));
    FString LevelAssetPath = GetJsonStringField(Payload, TEXT("levelAssetPath"), TEXT(""));
    FVector InstanceLocation = LevelStructureHelpers::GetVectorFromJson(GetObjectField(Payload, TEXT("instanceLocation")));
    FRotator InstanceRotation = LevelStructureHelpers::GetRotatorFromJson(GetObjectField(Payload, TEXT("instanceRotation")));
    FVector InstanceScale = LevelStructureHelpers::GetVectorFromJson(GetObjectField(Payload, TEXT("instanceScale")), FVector(1.0));

    if (LevelAssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("levelAssetPath is required"), nullptr);
        return true;
    }

    // Validate that the level asset exists
    FString NormalizedLevelPath = LevelAssetPath;
    if (!NormalizedLevelPath.StartsWith(TEXT("/Game/")))
    {
        NormalizedLevelPath = TEXT("/Game/") + NormalizedLevelPath;
    }
    // Remove .umap extension if present
    NormalizedLevelPath.RemoveFromEnd(TEXT(".umap"));

    if (!FPackageName::DoesPackageExist(NormalizedLevelPath))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Level asset not found: %s"), *LevelAssetPath), nullptr, TEXT("LEVEL_NOT_FOUND"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Get Level Instance Subsystem
    ULevelInstanceSubsystem* LevelInstanceSubsystem = World->GetSubsystem<ULevelInstanceSubsystem>();
    if (!LevelInstanceSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Level Instance Subsystem not available"), nullptr);
        return true;
    }

    // Spawn Level Instance Actor
    FActorSpawnParameters SpawnParams;
    // CRITICAL FIX: Use MakeUniqueObjectName to prevent "Cannot generate unique name" crash
    SpawnParams.Name = MakeUniqueObjectName(World, ALevelInstance::StaticClass(), FName(*LevelInstanceName));
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ALevelInstance* LevelInstanceActor = World->SpawnActor<ALevelInstance>(
        ALevelInstance::StaticClass(),
        InstanceLocation,
        InstanceRotation,
        SpawnParams
    );

    if (!LevelInstanceActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn Level Instance actor"), nullptr);
        return true;
    }

    LevelInstanceActor->SetActorScale3D(InstanceScale);
    // Set actor label to the requested name (may differ from internal name if collision occurred)
    LevelInstanceActor->SetActorLabel(*LevelInstanceName);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, LevelInstanceActor);
    ResponseJson->SetStringField(TEXT("levelInstanceName"), LevelInstanceName);
    ResponseJson->SetStringField(TEXT("levelAssetPath"), LevelAssetPath);

    TSharedPtr<FJsonObject> LocationJson = McpHandlerUtils::CreateResultObject();
    LocationJson->SetNumberField(TEXT("x"), InstanceLocation.X);
    LocationJson->SetNumberField(TEXT("y"), InstanceLocation.Y);
    LocationJson->SetNumberField(TEXT("z"), InstanceLocation.Z);
    ResponseJson->SetObjectField(TEXT("location"), LocationJson);

    FString Message = FString::Printf(TEXT("Created Level Instance: %s"), *LevelInstanceName);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

bool HandleCreatePackedLevelActor(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString PackedLevelName = GetJsonStringField(Payload, TEXT("packedLevelName"), TEXT("PackedLevel"));
    FString LevelAssetPath = GetJsonStringField(Payload, TEXT("levelAssetPath"), TEXT(""));
    FVector InstanceLocation = LevelStructureHelpers::GetVectorFromJson(GetObjectField(Payload, TEXT("instanceLocation")));
    FRotator InstanceRotation = LevelStructureHelpers::GetRotatorFromJson(GetObjectField(Payload, TEXT("instanceRotation")));
    bool bPackBlueprints = GetJsonBoolField(Payload, TEXT("bPackBlueprints"), true);
    bool bPackStaticMeshes = GetJsonBoolField(Payload, TEXT("bPackStaticMeshes"), true);

    // Validate levelAssetPath if provided
    if (!LevelAssetPath.IsEmpty())
    {
        FString NormalizedLevelPath = LevelAssetPath;
        if (!NormalizedLevelPath.StartsWith(TEXT("/Game/")))
        {
            NormalizedLevelPath = TEXT("/Game/") + NormalizedLevelPath;
        }
        NormalizedLevelPath.RemoveFromEnd(TEXT(".umap"));

        if (!FPackageName::DoesPackageExist(NormalizedLevelPath))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Level asset not found: %s"), *LevelAssetPath), nullptr, TEXT("LEVEL_NOT_FOUND"));
            return true;
        }
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Spawn Packed Level Actor
    FActorSpawnParameters SpawnParams;
    // CRITICAL FIX: Use MakeUniqueObjectName to prevent "Cannot generate unique name" crash
    // This prevents fatal error when multiple actors with same name are created
    SpawnParams.Name = MakeUniqueObjectName(World, APackedLevelActor::StaticClass(), FName(*PackedLevelName));
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;  // Auto-generate unique name if still taken
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APackedLevelActor* PackedActor = World->SpawnActor<APackedLevelActor>(
        APackedLevelActor::StaticClass(),
        InstanceLocation,
        InstanceRotation,
        SpawnParams
    );

    if (!PackedActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn Packed Level Actor"), nullptr);
        return true;
    }

    // Set actor label to the requested name (may differ from internal name if collision occurred)
    PackedActor->SetActorLabel(*PackedLevelName);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, PackedActor);
    ResponseJson->SetStringField(TEXT("packedLevelName"), PackedLevelName);
    ResponseJson->SetStringField(TEXT("levelAssetPath"), LevelAssetPath);
    ResponseJson->SetBoolField(TEXT("packBlueprints"), bPackBlueprints);
    ResponseJson->SetBoolField(TEXT("packStaticMeshes"), bPackStaticMeshes);

    FString Message = FString::Printf(TEXT("Created Packed Level Actor: %s"), *PackedLevelName);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

// ============================================================================
// Utility Handlers (1 action)
// ============================================================================

}
#endif
