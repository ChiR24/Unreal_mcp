#include "Domains/AI/McpAIContext.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
namespace
{
// AMassSpawner keeps Count, EntityTypes and SpawnDataGenerators protected in every UE 5.x, and from 5.7 neither it nor
// the EQS points generator exports its class, so both are found by class path and filled by reflection.
struct FMassSpawnerFields
{
    UClass* Spawner = nullptr;
    UClass* Generator = nullptr;
    FIntProperty* Count = nullptr;
    FArrayProperty* EntityTypes = nullptr;
    FSoftObjectProperty* EntityConfig = nullptr;
    FArrayProperty* Generators = nullptr;
    FObjectProperty* GeneratorInstance = nullptr;
    FStructProperty* Request = nullptr;
    FObjectProperty* QueryTemplate = nullptr;
};

template <typename TProperty>
TProperty* FindField(const UStruct* Owner, const TCHAR* Name)
{
    return Owner ? CastField<TProperty>(Owner->FindPropertyByName(Name)) : nullptr;
}

const UStruct* ElementStruct(const FArrayProperty* Array)
{
    const FStructProperty* Inner = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
    return Inner ? Inner->Struct : nullptr;
}

bool ResolveMassSpawnerFields(FMassSpawnerFields& F)
{
    if (!FModuleManager::Get().ModuleExists(TEXT("MassSpawner")) || !FModuleManager::Get().LoadModule(TEXT("MassSpawner")))
    {
        return false;
    }
    F.Spawner = FindObject<UClass>(nullptr, TEXT("/Script/MassSpawner.MassSpawner"));
    F.Generator = FindObject<UClass>(nullptr, TEXT("/Script/MassSpawner.MassEntityEQSSpawnPointsGenerator"));
    F.Count = FindField<FIntProperty>(F.Spawner, TEXT("Count"));
    F.EntityTypes = FindField<FArrayProperty>(F.Spawner, TEXT("EntityTypes"));
    F.EntityConfig = FindField<FSoftObjectProperty>(ElementStruct(F.EntityTypes), TEXT("EntityConfig"));
    F.Generators = FindField<FArrayProperty>(F.Spawner, TEXT("SpawnDataGenerators"));
    F.GeneratorInstance = FindField<FObjectProperty>(ElementStruct(F.Generators), TEXT("GeneratorInstance"));
    F.Request = FindField<FStructProperty>(F.Generator, TEXT("EQSRequest"));
    F.QueryTemplate = FindField<FObjectProperty>(F.Request ? F.Request->Struct : nullptr, TEXT("QueryTemplate"));
    return F.Count && F.EntityConfig && F.GeneratorInstance && F.QueryTemplate;
}

// A new element at the end of an array-of-struct property, constructed with the struct's defaults (Proportion 1).
void* AppendElement(FArrayProperty* Array, UObject* Owner)
{
    FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Owner));
    return Helper.GetRawPtr(Helper.AddValue());
}

// The asset at Path when it is a TypeClass (the class a spawner field holds), else null with a refusal in OutError.
UObject* LoadTypedAsset(const FString& Path, UClass* TypeClass, const TCHAR* Param, FString& OutError, FString& OutCode)
{
    UObject* Asset = Path.IsEmpty() ? nullptr : McpLoadAsset(Path);
    if (Asset && Asset->IsA(TypeClass))
    {
        return Asset;
    }
    OutError = Asset ? FString::Printf(TEXT("%s %s is of class %s; it needs %s. Nothing was placed."), Param, *Path, *Asset->GetClass()->GetName(), *TypeClass->GetName())
                     : FString::Printf(TEXT("%s %s does not load (it needs %s). Nothing was placed."), Param, *Path, *TypeClass->GetName());
    OutCode = Asset ? TEXT("TYPE_MISMATCH") : TEXT("NOT_FOUND");
    return nullptr;
}
}

// Implements the "add_mass_spawner" action: places a Mass Spawner that spawns spawnCount entities of configPath when play
// begins, at the items of the Environment Query queryPath run around it. It used to dirty a Blueprint and answer
// "Spawner configured" while placing nothing.
bool HandleAddMassSpawner(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const auto Fail = [&](const FString& Message, const TCHAR* Code)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Message, Code);
        return true;
    };
    const FString ConfigPath = GetJsonStringField(Payload, TEXT("configPath"));
    const FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const double SpawnCount = GetJsonNumberField(Payload, TEXT("spawnCount"), 100);
    if (SpawnCount < 1 || SpawnCount != FMath::FloorToDouble(SpawnCount) || SpawnCount > MAX_int32)
    {
        return Fail(TEXT("spawnCount must be a whole number of at least 1 (a spawner with 0 spawns nothing). Nothing was placed."), TEXT("INVALID_ARGUMENT"));
    }
    FMassSpawnerFields F;
    if (!ResolveMassSpawnerFields(F))
    {
        return Fail(TEXT("The Mass Spawner is not available: enable the MassGameplay plugin. Nothing was placed."), TEXT("MASS_PLUGIN_NOT_ENABLED"));
    }
    FString Error;
    FString Code;
    UObject* Config = LoadTypedAsset(ConfigPath, F.EntityConfig->PropertyClass, TEXT("configPath"), Error, Code);
    UObject* Query = Config ? LoadTypedAsset(QueryPath, F.QueryTemplate->PropertyClass, TEXT("queryPath"), Error, Code) : nullptr;
    if (!Query)
    {
        return Fail(Error, *Code);
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return Fail(TEXT("No editor world available"), TEXT("NO_WORLD"));
    }
    // A second actor under one label left every later call by that name on whichever came first.
    if (!ActorName.IsEmpty() && FindActorByNameInWorldForMcp(World, ActorName, true))
    {
        return Fail(FString::Printf(TEXT("An actor named %s is already in the level; pick another actorName. Nothing was placed."), *ActorName), TEXT("NAME_TAKEN"));
    }

    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add Mass Spawner")), EMcpMutationDurability::EditorStateOnly, TArray<UObject*>());
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.ObjectFlags |= RF_Transactional;
    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    AActor* Spawner = World->SpawnActor(F.Spawner, &Location, nullptr, SpawnParams);
    if (!Spawner)
    {
        return Fail(TEXT("The Mass Spawner could not be spawned."), TEXT("SPAWN_FAILED"));
    }
    Spawner->SetActorLabel(ActorName.IsEmpty() ? Spawner->GetName() : ActorName);
    F.Count->SetPropertyValue_InContainer(Spawner, static_cast<int32>(SpawnCount));
    void* EntityType = AppendElement(F.EntityTypes, Spawner);
    F.EntityConfig->SetObjectPropertyValue(F.EntityConfig->ContainerPtrToValuePtr<void>(EntityType), Config);
    // Without a points generator the spawner spawns nothing; the EQS one runs the query with the spawner as querier.
    UObject* Generator = NewObject<UObject>(Spawner, F.Generator, NAME_None, RF_Transactional);
    void* GeneratorEntry = AppendElement(F.Generators, Spawner);
    F.GeneratorInstance->SetObjectPropertyValue(F.GeneratorInstance->ContainerPtrToValuePtr<void>(GeneratorEntry), Generator);
    F.QueryTemplate->SetObjectPropertyValue(F.QueryTemplate->ContainerPtrToValuePtr<void>(F.Request->ContainerPtrToValuePtr<void>(Generator)), Query);
    // As the details panel does: 5.8 collects the query's named parameters into the request here.
    FPropertyChangedEvent QueryChanged(F.QueryTemplate);
    QueryChanged.SetActiveMemberProperty(F.Request);
    Generator->PostEditChangeProperty(QueryChanged);
    Spawner->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), McpActorRef(Spawner));
    Result->SetStringField(TEXT("actorPath"), Spawner->GetPathName());
    Result->SetStringField(TEXT("configPath"), Config->GetOutermost()->GetName());
    Result->SetStringField(TEXT("queryPath"), Query->GetOutermost()->GetName());
    Result->SetNumberField(TEXT("spawnCount"), static_cast<int32>(SpawnCount));
    Result->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Spawner->GetActorLocation()));
    Result->SetBoolField(TEXT("autoSpawnOnBeginPlay"), true);
    McpHandlerUtils::AddVerification(Result, Spawner);
    Transaction.DescribeInto(Result);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Mass spawner '%s' placed: %d entities of %s at the items of %s when play begins"),
        *McpActorRef(Spawner), static_cast<int32>(SpawnCount), *Config->GetName(), *Query->GetName()), Result);
    return true;
}
}
