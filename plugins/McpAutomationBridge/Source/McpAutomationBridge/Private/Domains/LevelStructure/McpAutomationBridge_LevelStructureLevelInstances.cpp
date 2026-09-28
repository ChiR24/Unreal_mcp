#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/PackageName.h"
#include "Safety/McpSafeOperations.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#if ENGINE_MINOR_VERSION >= 3 && __has_include("PackedLevelActor/PackedLevelActorBuilder.h")
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "PackedLevelActor/PackedLevelActorBuilder.h"
#define MCP_HAS_PACKED_LEVEL_BUILDER 1
#else
#define MCP_HAS_PACKED_LEVEL_BUILDER 0
#endif

// create_level_instance places an ALevelInstance that really references and loads a world
// asset; create_packed_level_actor runs the editor's "Create Packed Level Blueprint" bake.
namespace McpLevelStructure
{
namespace
{
// The package of levelAssetPath when it names an existing level other than the open one; replies and returns
// false otherwise. OutWorld is the soft reference the engine APIs take.
bool ResolveInstancedLevel(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                           const TSharedPtr<FJsonObject>& Payload, UWorld* EditorWorld, FString& OutPackage, TSoftObjectPtr<UWorld>& OutWorld)
{
    const FString Requested = GetJsonStringField(Payload, TEXT("levelAssetPath"));
    const FString Sanitized = Requested.IsEmpty() ? FString() : SanitizeProjectRelativePath(Requested);
    OutPackage = Sanitized.IsEmpty() ? FString() : FPackageName::ObjectPathToPackageName(Sanitized);
    OutPackage.RemoveFromEnd(TEXT(".umap"));
    if (OutPackage.IsEmpty() || !FPackageName::DoesPackageExist(OutPackage))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(
            TEXT("No level asset at '%s'. Pass the /Game path of a saved level (manage_level_structure get_level_structure_info lists them)."),
            *Requested), nullptr, TEXT("LEVEL_NOT_FOUND"));
        return false;
    }
    if (EditorWorld && EditorWorld->GetOutermost()->GetName().Equals(OutPackage, ESearchCase::IgnoreCase))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(
            TEXT("'%s' is the level that is open; a level cannot contain an instance of itself. Open another level first."), *OutPackage),
            nullptr, TEXT("LEVEL_INSTANCE_LOOP"));
        return false;
    }
    OutWorld = TSoftObjectPtr<UWorld>(FSoftObjectPath(OutPackage + TEXT(".") + FPackageName::GetShortName(OutPackage)));
    return true;
}

// A saved level saves by default; an unsaved /Temp/ level cannot, so it defaults to not saving.
void DefaultSaveToLevelState(const TSharedPtr<FJsonObject>& Payload, UWorld* World)
{
    if (Payload.IsValid() && !Payload->HasField(TEXT("save")))
    {
        Payload->SetBoolField(TEXT("save"), World && !World->GetOutermost()->GetName().StartsWith(TEXT("/Temp/")));
    }
}

#if MCP_HAS_PACKED_LEVEL_BUILDER
// Instances the packer wrote into the Blueprint's ISM component templates.
int32 CountPackedBlueprintInstances(const UBlueprint* BP, int32& OutIsmComponents)
{
    int32 Instances = 0;
    OutIsmComponents = 0;
    if (BP && BP->SimpleConstructionScript)
    {
        for (const USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
        {
            if (const UInstancedStaticMeshComponent* Ism = Node ? Cast<UInstancedStaticMeshComponent>(Node->ComponentTemplate) : nullptr)
            {
                ++OutIsmComponents;
                Instances += Ism->GetInstanceCount();
            }
        }
    }
    return Instances;
}

// Packs the level into a throwaway /Temp Blueprint and counts what it holds, so an existing Blueprint is only
// rewritten when the pack will really fill it (the builder empties and saves the target before it knows).
int32 ProbePackedInstanceCount(FPackedLevelActorBuilder& Builder, const TSoftObjectPtr<UWorld>& WorldAsset)
{
    const FString ProbeName = TEXT("McpPackProbe_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString ProbePath = TEXT("/Temp/McpPackProbe/") + ProbeName + TEXT(".") + ProbeName;
    const TSoftObjectPtr<UBlueprint> Probe{FSoftObjectPath(ProbePath)};
    UBlueprint* ProbeBP = FPackedLevelActorBuilder::CreatePackedLevelActorBlueprint(Probe, WorldAsset, false);
    int32 IsmComponents = 0;
    const int32 Instances = ProbeBP && Builder.CreateOrUpdateBlueprint(WorldAsset, Probe, false, false)
        ? CountPackedBlueprintInstances(ProbeBP, IsmComponents) : 0;
    if (ProbeBP)
    {
        McpSafeOperations::McpDeleteAssetAndFile(ProbePath);
    }
    return Instances;
}
#endif
}

bool HandleCreateLevelInstance(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UWorld* World = LevelStructureHelpers::GetEditorWorld();
    FString Package;
    TSoftObjectPtr<UWorld> WorldAsset;
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }
    if (!ResolveInstancedLevel(Subsystem, RequestId, Socket, Payload, World, Package, WorldAsset))
    {
        return true;
    }
    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    const FString Label = GetJsonStringField(Payload, TEXT("label"), TEXT("LI_") + FPackageName::GetShortName(Package));

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ALevelInstance* Instance = World->SpawnActor<ALevelInstance>(ALevelInstance::StaticClass(), Location, Rotation, SpawnParams);
    if (!Instance)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("The Level Instance actor could not be spawned."), nullptr, TEXT("ACTOR_SPAWN_FAILED"));
        return true;
    }
    // SetWorldAsset refuses a level that would contain itself somewhere down its instances.
    if (!Instance->SetWorldAsset(WorldAsset))
    {
        World->DestroyActor(Instance);
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(
            TEXT("The engine refused '%s' as the instance's level (it would instance the open level through its own level instances). Nothing was placed."),
            *Package), nullptr, TEXT("LEVEL_INSTANCE_LOOP"));
        return true;
    }
    Instance->SetActorLabel(Label);
    // LoadLevelInstance only queues the load for a later tick; the block-load finishes it now so the reply
    // reads the real level (5.0 takes ALevelInstance*, 5.1+ the ILevelInstanceInterface it implements).
    if (ULevelInstanceSubsystem* LevelInstances = World->GetSubsystem<ULevelInstanceSubsystem>())
    {
        LevelInstances->BlockLoadLevelInstance(Instance);
    }
    if (!Instance->IsLoaded() || !Instance->GetLoadedLevel())
    {
        World->DestroyActor(Instance);
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(
            TEXT("'%s' did not load as a Level Instance, so nothing was placed. Open the level on its own to check it loads, then retry."),
            *Package), nullptr, TEXT("LEVEL_INSTANCE_NOT_LOADED"));
        return true;
    }

    int32 ChildActors = 0;
    if (const ULevel* Loaded = Instance->GetLoadedLevel())
    {
        for (const AActor* Actor : Loaded->Actors)
        {
            ChildActors += Actor ? 1 : 0;
        }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), Instance->GetName());
    Result->SetStringField(TEXT("actorLabel"), Instance->GetActorLabel());
    Result->SetStringField(TEXT("worldAsset"), Instance->GetWorldAsset().ToString());
    Result->SetBoolField(TEXT("loaded"), Instance->IsLoaded());
    Result->SetNumberField(TEXT("childActorCount"), ChildActors);
    McpHandlerUtils::AddVerification(Result, Instance);
    DefaultSaveToLevelState(Payload, World);
    LevelStructureHelpers::SendLevelEditResult(Subsystem, RequestId, Socket, Payload, Instance->GetLevel(), FString::Printf(
        TEXT("Placed Level Instance '%s' of %s (%s, %d actors)"), *Label, *Package,
        Instance->IsLoaded() ? TEXT("loaded") : TEXT("not loaded"), ChildActors), Result);
    return true;
}

bool HandleCreatePackedLevelActor(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if MCP_HAS_PACKED_LEVEL_BUILDER
    UWorld* World = LevelStructureHelpers::GetEditorWorld();
    FString Package;
    TSoftObjectPtr<UWorld> WorldAsset;
    if (!World || !ResolveInstancedLevel(Subsystem, RequestId, Socket, Payload, World, Package, WorldAsset))
    {
        if (!World)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        }
        return true;
    }
    const FString RequestedBP = GetJsonStringField(Payload, TEXT("blueprintPath"),
        FPackageName::GetLongPackagePath(Package) / (FPackedLevelActorBuilder::GetPackedBPPrefix() + FPackageName::GetShortName(Package)));
    FString BPPackage = SanitizeProjectRelativePath(RequestedBP);
    BPPackage = BPPackage.IsEmpty() ? FString() : FPackageName::ObjectPathToPackageName(BPPackage);
    if (BPPackage.IsEmpty() || !FPackageName::IsValidLongPackageName(BPPackage))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(
            TEXT("blueprintPath '%s' is not a valid asset path; pass a /Game path such as /Game/Maps/BPP_Room."), *RequestedBP), nullptr, TEXT("INVALID_ASSET_PATH"));
        return true;
    }
    const FString BPObjectPath = BPPackage + TEXT(".") + FPackageName::GetShortName(BPPackage);
    TSoftObjectPtr<UBlueprint> BPAsset{FSoftObjectPath(BPObjectPath)};
    // An unsaved Blueprint counts too: creating over it would open the editor's modal overwrite dialog.
    const bool bExisted = StaticFindObject(UObject::StaticClass(), nullptr, *BPObjectPath) != nullptr || FPackageName::DoesPackageExist(BPPackage);
    UBlueprint* BP = bExisted ? BPAsset.LoadSynchronous() : nullptr;
    if (bExisted && (!BP || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(APackedLevelActor::StaticClass())))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(
            TEXT("'%s' exists and is not a Packed Level Blueprint; pass another blueprintPath."), *BPPackage), nullptr, TEXT("ASSET_EXISTS"));
        return true;
    }
    const TSharedPtr<FPackedLevelActorBuilder> Builder = FPackedLevelActorBuilder::CreateDefaultBuilder();
    if (bExisted && ProbePackedInstanceCount(*Builder, WorldAsset) == 0)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(
            TEXT("%s has nothing to pack (no static meshes would reach the Blueprint), so the existing Blueprint '%s' was left unchanged."),
            *Package, *BPPackage), nullptr, TEXT("NOTHING_TO_PACK"));
        return true;
    }
    // Created here so the builder never opens its save-as dialog for a missing Blueprint.
    if (!BP && !FPackedLevelActorBuilder::CreatePackedLevelActorBlueprint(BPAsset, WorldAsset, false))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("Could not create the Blueprint '%s'."), *BPPackage), nullptr, TEXT("CREATE_FAILED"));
        return true;
    }
    const bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);
    const bool bPacked = Builder->CreateOrUpdateBlueprint(WorldAsset, BPAsset, bSave, false);
    BP = BPAsset.LoadSynchronous();
    int32 IsmComponents = 0;
    const int32 Instances = CountPackedBlueprintInstances(BP, IsmComponents);
    if (!bPacked || Instances == 0)
    {
        const bool bRemoved = !bExisted && McpSafeOperations::McpDeleteAssetAndFile(BPObjectPath);
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(
            TEXT("%s had nothing to pack (no static meshes reached the Blueprint)%s."), *Package,
            bExisted ? TEXT("; the existing Blueprint now holds no instances") : (bRemoved ? TEXT("; the new Blueprint was removed") : TEXT("; the new Blueprint could not be removed"))),
            nullptr, bPacked ? TEXT("NOTHING_TO_PACK") : TEXT("PACK_FAILED"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BPObjectPath);
    Result->SetBoolField(TEXT("blueprintCreated"), !bExisted);
    Result->SetNumberField(TEXT("ismComponentCount"), IsmComponents);
    Result->SetNumberField(TEXT("instanceCount"), Instances);
    Result->SetBoolField(TEXT("blueprintSaved"), bSave && !BP->GetOutermost()->IsDirty());
    const FString Message = FString::Printf(TEXT("Packed %s into %s (%d ISM components, %d instances)"), *Package, *BPObjectPath, IsmComponents, Instances);
    if (!GetJsonBoolField(Payload, TEXT("spawn"), true))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, Result);
        return true;
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Spawned = World->SpawnActor<AActor>(BP->GeneratedClass, ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector), FRotator::ZeroRotator, SpawnParams);
    if (!Spawned)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, Message + TEXT(", but the actor could not be placed."), Result, TEXT("ACTOR_SPAWN_FAILED"));
        return true;
    }
    Spawned->SetActorLabel(FPackageName::GetShortName(BPPackage));
    Result->SetStringField(TEXT("actorName"), Spawned->GetName());
    McpHandlerUtils::AddVerification(Result, Spawned);
    DefaultSaveToLevelState(Payload, World);
    LevelStructureHelpers::SendLevelEditResult(Subsystem, RequestId, Socket, Payload, Spawned->GetLevel(), Message + TEXT(" and placed it"), Result);
#else
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("Packed Level Blueprints can be built from code on Unreal Engine 5.3 or later; on this version use the editor's Create Packed Level Blueprint command, or create_level_structure kind level_instance."),
        nullptr, TEXT("ENGINE_VERSION_UNSUPPORTED"));
#endif
    return true;
}
}
