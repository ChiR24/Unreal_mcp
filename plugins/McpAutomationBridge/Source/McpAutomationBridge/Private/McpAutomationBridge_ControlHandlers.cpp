#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h"
#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#endif
// Additional editor headers for viewport control
#include "Editor.h"
#include "Modules/ModuleManager.h"
#if __has_include("LevelEditor.h")
#include "LevelEditor.h"
#define MCP_HAS_LEVEL_EDITOR_MODULE 1
#else
#define MCP_HAS_LEVEL_EDITOR_MODULE 0
#endif
#if __has_include("Settings/LevelEditorPlaySettings.h")
#include "Settings/LevelEditorPlaySettings.h"
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 1
#else
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 0
#endif
#include "EditorViewportClient.h"
#include "Engine/Blueprint.h"
#include "Components/PrimitiveComponent.h"
#if __has_include("FileHelpers.h")
#include "FileHelpers.h"
#endif
#include "Components/StaticMeshComponent.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Exporters/Exporter.h"
#include "Misc/OutputDevice.h"
#endif

// Helper class for capturing export output
/* UE5.6: Use built-in FStringOutputDevice from UnrealString.h */

// Helper functions
static FVector ExtractVectorField(const TSharedPtr<FJsonObject>& Source, const TCHAR* FieldName, const FVector& DefaultValue)
{
    FVector Parsed = DefaultValue;
    ReadVectorField(Source, FieldName, Parsed, DefaultValue);
    return Parsed;
}

static FRotator ExtractRotatorField(const TSharedPtr<FJsonObject>& Source, const TCHAR* FieldName, const FRotator& DefaultValue)
{
    FRotator Parsed = DefaultValue;
    ReadRotatorField(Source, FieldName, Parsed, DefaultValue);
    return Parsed;
}

AActor* UMcpAutomationBridgeSubsystem::FindActorByName(const FString& Target)
{
#if WITH_EDITOR
    if (Target.IsEmpty() || !GEditor) return nullptr;

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) return nullptr;

    TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
    for (AActor* A : AllActors)
    {
        if (!A) continue;
        if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
            A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
            A->GetPathName().Equals(Target, ESearchCase::IgnoreCase))
        {
            return A;
        }
    }
    
    // Fallback: try to load as asset if it looks like a path
    if (Target.StartsWith(TEXT("/")))
    {
         if (UObject* Obj = UEditorAssetLibrary::LoadAsset(Target))
         {
             return Cast<AActor>(Obj);
         }
    }
#endif
    return nullptr;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawn(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString ClassPath; Payload->TryGetStringField(TEXT("classPath"), ClassPath);
    FString ActorName; Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

    UClass* ResolvedClass = nullptr;
    UStaticMesh* ResolvedStaticMesh = nullptr;
    // Skip LoadAsset for script classes (e.g. /Script/Engine.CameraActor) to avoid LogEditorAssetSubsystem errors
    if ((ClassPath.StartsWith(TEXT("/")) || ClassPath.Contains(TEXT("/"))) && !ClassPath.StartsWith(TEXT("/Script/")))
    {
        if (UObject* Loaded = UEditorAssetLibrary::LoadAsset(ClassPath))
        {
            if (UBlueprint* BP = Cast<UBlueprint>(Loaded)) ResolvedClass = BP->GeneratedClass;
            else if (UClass* C = Cast<UClass>(Loaded)) ResolvedClass = C;
            else if (UStaticMesh* Mesh = Cast<UStaticMesh>(Loaded)) ResolvedStaticMesh = Mesh;
        }
    }
    if (!ResolvedClass) ResolvedClass = ResolveClassByName(ClassPath);

    const bool bSpawnStaticMeshActor = (ResolvedClass == nullptr && ResolvedStaticMesh != nullptr);

    if (!ResolvedClass && !bSpawnStaticMeshActor)
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("error"), TEXT("Class not found"));
        SendAutomationResponse(Socket, RequestId, false, TEXT("Actor class not found"), Resp, TEXT("CLASS_NOT_FOUND"));
        return true;
    }

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    AActor* Spawned = nullptr;
    if (bSpawnStaticMeshActor)
    {
        Spawned = ActorSS->SpawnActorFromClass(AStaticMeshActor::StaticClass(), Location, Rotation);
        if (Spawned)
        {
            if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Spawned))
            {
                if (UStaticMeshComponent* MeshComponent = StaticMeshActor->GetStaticMeshComponent())
                {
                    MeshComponent->SetStaticMesh(ResolvedStaticMesh);
                    MeshComponent->SetMobility(EComponentMobility::Movable);
                    MeshComponent->MarkRenderStateDirty();
                }
            }
        }
    }
    else
    {
        Spawned = ActorSS->SpawnActorFromClass(ResolvedClass, Location, Rotation);
    }

    if (!Spawned)
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn actor"));
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn actor"), Resp, TEXT("SPAWN_FAILED"));
        return true;
    }

    if (!ActorName.IsEmpty()) Spawned->SetActorLabel(ActorName);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
    if (bSpawnStaticMeshActor && ResolvedStaticMesh) Resp->SetStringField(TEXT("meshPath"), ResolvedStaticMesh->GetPathName());
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Spawned actor '%s'"), *Spawned->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor spawned"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawnBlueprint(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString BlueprintPath; Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (BlueprintPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("Blueprint path required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    FString ActorName; Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

    UClass* ResolvedClass = nullptr;

    // Prefer the same blueprint resolution heuristics used by manage_blueprint
    // so that short names and package paths behave consistently.
    FString NormalizedPath;
    FString LoadError;
    if (!BlueprintPath.IsEmpty())
    {
        UBlueprint* BlueprintAsset = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
        if (BlueprintAsset && BlueprintAsset->GeneratedClass)
        {
            ResolvedClass = BlueprintAsset->GeneratedClass;
        }
    }

    if (!ResolvedClass && (BlueprintPath.StartsWith(TEXT("/")) || BlueprintPath.Contains(TEXT("/"))))
    {
        if (UObject* Loaded = UEditorAssetLibrary::LoadAsset(BlueprintPath))
        {
            if (UBlueprint* BP = Cast<UBlueprint>(Loaded)) ResolvedClass = BP->GeneratedClass;
            else if (UClass* C = Cast<UClass>(Loaded)) ResolvedClass = C;
        }
    }
    if (!ResolvedClass) ResolvedClass = ResolveClassByName(BlueprintPath);

    if (!ResolvedClass)
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("error"), TEXT("Blueprint class not found"));
        SendAutomationResponse(Socket, RequestId, false, TEXT("Blueprint class not found"), Resp, TEXT("CLASS_NOT_FOUND"));
        return true;
    }

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    AActor* Spawned = ActorSS->SpawnActorFromClass(ResolvedClass, Location, Rotation);
    if (!Spawned)
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn blueprint"));
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn blueprint"), Resp, TEXT("SPAWN_FAILED"));
        return true;
    }

    if (!ActorName.IsEmpty()) Spawned->SetActorLabel(ActorName);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
    Resp->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Spawned blueprint '%s'"), *Spawned->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Blueprint spawned"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDelete(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    TArray<FString> Targets;
    const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("actorNames"), NamesArray) && NamesArray)
    {
        for (const TSharedPtr<FJsonValue>& Entry : *NamesArray)
        {
            if (Entry.IsValid() && Entry->Type == EJson::String)
            {
                const FString Value = Entry->AsString().TrimStartAndEnd();
                if (!Value.IsEmpty()) Targets.AddUnique(Value);
            }
        }
    }

    FString SingleName;
    if (Targets.Num() == 0)
    {
        Payload->TryGetStringField(TEXT("actorName"), SingleName);
        if (!SingleName.IsEmpty()) Targets.AddUnique(SingleName);
    }

    if (Targets.Num() == 0) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName or actorNames required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    TArray<FString> Deleted;
    TArray<FString> Missing;

    for (const FString& Name : Targets)
    {
        AActor* Found = FindActorByName(Name);
        if (!Found) { Missing.Add(Name); continue; }
        if (ActorSS->DestroyActor(Found))
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Deleted actor '%s'"), *Name);
            Deleted.Add(Name);
        }
        else Missing.Add(Name);
    }

    const bool bAllDeleted = Missing.Num() == 0;
    const bool bAnyDeleted = Deleted.Num() > 0;
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), bAllDeleted);
    Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

    TArray<TSharedPtr<FJsonValue>> DeletedArray;
    for (const FString& Name : Deleted) DeletedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("deleted"), DeletedArray);

    if (Missing.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> MissingArray;
        for (const FString& Name : Missing) MissingArray.Add(MakeShared<FJsonValueString>(Name));
        Resp->SetArrayField(TEXT("missing"), MissingArray);
    }

    FString Message;
    FString ErrorCode;
    if (!bAnyDeleted && Missing.Num() > 0)
    {
        Message = TEXT("Actors not found");
        ErrorCode = TEXT("NOT_FOUND");
    }
    else
    {
        Message = bAllDeleted ? TEXT("Actors deleted") : TEXT("Some actors could not be deleted");
        ErrorCode = bAllDeleted ? FString() : TEXT("DELETE_PARTIAL");
    }
    SendAutomationResponse(Socket, RequestId, bAllDeleted, Message, Resp, ErrorCode);
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorApplyForce(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    FVector ForceVector = ExtractVectorField(Payload, TEXT("force"), FVector::ZeroVector);

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    UPrimitiveComponent* Prim = Found->FindComponentByClass<UPrimitiveComponent>();
    if (!Prim)
    {
        if (UStaticMeshComponent* SMC = Found->FindComponentByClass<UStaticMeshComponent>()) Prim = SMC;
    }

    if (!Prim) { SendAutomationResponse(Socket, RequestId, false, TEXT("No component to apply force"), nullptr, TEXT("NO_COMPONENT")); return true; }

    if (Prim->Mobility == EComponentMobility::Static) Prim->SetMobility(EComponentMobility::Movable);
    if (!Prim->IsSimulatingPhysics()) Prim->SetSimulatePhysics(true);

    Prim->AddForce(ForceVector);
    Prim->WakeAllRigidBodies();
    Prim->MarkRenderStateDirty();

    // Verify physics state
    const bool bIsSimulating = Prim->IsSimulatingPhysics();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), bIsSimulating);
    TArray<TSharedPtr<FJsonValue>> Applied;
    Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.X));
    Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Y));
    Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Z));
    Resp->SetArrayField(TEXT("applied"), Applied);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    
    if (!bIsSimulating)
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to enable physics simulation"), Resp, TEXT("PHYSICS_FAILED"));
        return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Applied force to '%s'"), *Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Force applied"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetTransform(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    FVector Location = ExtractVectorField(Payload, TEXT("location"), Found->GetActorLocation());
    FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), Found->GetActorRotation());
    FVector Scale = ExtractVectorField(Payload, TEXT("scale"), Found->GetActorScale3D());

    Found->Modify();
    Found->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    Found->SetActorRotation(Rotation, ETeleportType::TeleportPhysics);
    Found->SetActorScale3D(Scale);
    Found->MarkComponentsRenderStateDirty();
    Found->MarkPackageDirty();

    // Verify transform
    const FVector NewLoc = Found->GetActorLocation();
    const FRotator NewRot = Found->GetActorRotation();
    const FVector NewScale = Found->GetActorScale3D();
    
    const bool bLocMatch = NewLoc.Equals(Location, 1.0f); // 1 unit tolerance
    // Rotation comparison is tricky due to normalization, skipping strict check for now but logging if very different
    const bool bScaleMatch = NewScale.Equals(Scale, 0.01f);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), bLocMatch && bScaleMatch);
    
    if (!bLocMatch || !bScaleMatch)
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to set transform exactly"), Resp, TEXT("TRANSFORM_MISMATCH"));
        return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Set transform for '%s'"), *Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor transform updated"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetTransform(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    const FTransform Current = Found->GetActorTransform();
    const FVector Location = Current.GetLocation();
    const FRotator Rotation = Current.GetRotation().Rotator();
    const FVector Scale = Current.GetScale3D();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    
    auto MakeArray = [](const FVector& Vec)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
        return Arr;
    };

    Resp->SetArrayField(TEXT("location"), MakeArray(Location));
    TArray<TSharedPtr<FJsonValue>> RotArray;
    RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    Resp->SetArrayField(TEXT("rotation"), RotArray);
    Resp->SetArrayField(TEXT("scale"), MakeArray(Scale));

    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor transform retrieved"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetVisibility(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    bool bVisible = true;
    if (Payload->HasField(TEXT("visible"))) Payload->TryGetBoolField(TEXT("visible"), bVisible);

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    Found->Modify();
    Found->SetActorHiddenInGame(!bVisible);
    Found->SetActorEnableCollision(bVisible);

    for (UActorComponent* Comp : Found->GetComponents())
    {
        if (!Comp) continue;
        if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
        {
            Prim->SetVisibility(bVisible, true);
            Prim->SetHiddenInGame(!bVisible);
        }
    }

    Found->MarkComponentsRenderStateDirty();
    Found->MarkPackageDirty();

    // Verify visibility state
    const bool bIsHidden = Found->IsHidden();
    const bool bStateMatches = (bIsHidden == !bVisible);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), bStateMatches);
    Resp->SetBoolField(TEXT("visible"), !bIsHidden);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    
    if (!bStateMatches)
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to set actor visibility"), Resp, TEXT("VISIBILITY_MISMATCH"));
        return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Set visibility to %s for '%s'"), bVisible ? TEXT("True") : TEXT("False"), *Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor visibility updated"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddComponent(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    FString ComponentType; Payload->TryGetStringField(TEXT("componentType"), ComponentType);
    if (ComponentType.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("componentType required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    FString ComponentName; Payload->TryGetStringField(TEXT("componentName"), ComponentName);

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    UClass* ComponentClass = ResolveClassByName(ComponentType);
    if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass())) { SendAutomationResponse(Socket, RequestId, false, TEXT("Component class not found"), nullptr, TEXT("CLASS_NOT_FOUND")); return true; }

    if (ComponentName.TrimStartAndEnd().IsEmpty()) ComponentName = FString::Printf(TEXT("%s_%d"), *ComponentClass->GetName(), FMath::Rand());

    FName DesiredName = FName(*ComponentName);
    UActorComponent* NewComponent = NewObject<UActorComponent>(Found, ComponentClass, DesiredName, RF_Transactional);
    if (!NewComponent) { SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create component"), nullptr, TEXT("CREATE_COMPONENT_FAILED")); return true; }

    Found->Modify();
    NewComponent->SetFlags(RF_Transactional);
    Found->AddInstanceComponent(NewComponent);
    NewComponent->OnComponentCreated();

    if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComponent))
    {
        if (Found->GetRootComponent() && !SceneComp->GetAttachParent())
        {
            SceneComp->SetupAttachment(Found->GetRootComponent());
        }
    }

    TArray<FString> AppliedProperties;
    TArray<FString> PropertyWarnings;
    const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
    if (Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) && PropertiesPtr && (*PropertiesPtr).IsValid())
    {
        for (const auto& Pair : (*PropertiesPtr)->Values)
        {
            FProperty* Property = ComponentClass->FindPropertyByName(*Pair.Key);
            if (!Property) { PropertyWarnings.Add(FString::Printf(TEXT("Property not found: %s"), *Pair.Key)); continue; }
            FString ApplyError;
            if (ApplyJsonValueToProperty(NewComponent, Property, Pair.Value, ApplyError)) AppliedProperties.Add(Pair.Key);
            else PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *Pair.Key, *ApplyError));
        }
    }

    NewComponent->RegisterComponent();
    if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComponent)) SceneComp->UpdateComponentToWorld();
    NewComponent->MarkPackageDirty();
    Found->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("componentName"), NewComponent->GetName());
    Resp->SetStringField(TEXT("componentPath"), NewComponent->GetPathName());
    Resp->SetStringField(TEXT("componentClass"), ComponentClass->GetPathName());
    if (AppliedProperties.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> PropsArray;
        for (const FString& PropName : AppliedProperties) PropsArray.Add(MakeShared<FJsonValueString>(PropName));
        Resp->SetArrayField(TEXT("appliedProperties"), PropsArray);
    }
    if (PropertyWarnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> WarnArray;
        for (const FString& Warning : PropertyWarnings) WarnArray.Add(MakeShared<FJsonValueString>(Warning));
        Resp->SetArrayField(TEXT("warnings"), WarnArray);
    }
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Added component '%s' to '%s'"), *NewComponent->GetName(), *Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Component added"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetComponentProperties(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    FString ComponentName; Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    if (ComponentName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("componentName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
    if (!(Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) && PropertiesPtr && PropertiesPtr->IsValid()))
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("properties object required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    UActorComponent* TargetComponent = nullptr;
    for (UActorComponent* Comp : Found->GetComponents())
    {
        if (!Comp) continue;
        if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)) { TargetComponent = Comp; break; }
    }

    if (!TargetComponent) { SendAutomationResponse(Socket, RequestId, false, TEXT("Component not found"), nullptr, TEXT("COMPONENT_NOT_FOUND")); return true; }

    TArray<FString> AppliedProperties;
    TArray<FString> PropertyWarnings;
    UClass* ComponentClass = TargetComponent->GetClass();
    TargetComponent->Modify();

    for (const auto& Pair : (*PropertiesPtr)->Values)
    {
        FProperty* Property = ComponentClass->FindPropertyByName(*Pair.Key);
        if (!Property) { PropertyWarnings.Add(FString::Printf(TEXT("Property not found: %s"), *Pair.Key)); continue; }
        FString ApplyError;
        if (ApplyJsonValueToProperty(TargetComponent, Property, Pair.Value, ApplyError)) AppliedProperties.Add(Pair.Key);
        else PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *Pair.Key, *ApplyError));
    }

    if (USceneComponent* SceneComponent = Cast<USceneComponent>(TargetComponent))
    {
        SceneComponent->MarkRenderStateDirty();
        SceneComponent->UpdateComponentToWorld();
    }
    TargetComponent->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    if (AppliedProperties.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> PropsArray;
        for (const FString& PropName : AppliedProperties) PropsArray.Add(MakeShared<FJsonValueString>(PropName));
        Resp->SetArrayField(TEXT("applied"), PropsArray);
    }
    if (PropertyWarnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> WarnArray;
        for (const FString& Warning : PropertyWarnings) WarnArray.Add(MakeShared<FJsonValueString>(Warning));
        Resp->SetArrayField(TEXT("warnings"), WarnArray);
    }
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Updated properties for component '%s' on '%s'"), *TargetComponent->GetName(), *Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Component properties updated"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponents(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    for (UActorComponent* Comp : Found->GetComponents())
    {
        if (!Comp) continue;
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Comp->GetName());
        Entry->SetStringField(TEXT("class"), Comp->GetClass() ? Comp->GetClass()->GetPathName() : TEXT(""));
        Entry->SetStringField(TEXT("path"), Comp->GetPathName());
        ComponentsArray.Add(MakeShared<FJsonValueObject>(Entry));
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("components"), ComponentsArray);
    Resp->SetNumberField(TEXT("count"), ComponentsArray.Num());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor components retrieved"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDuplicate(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    FVector Offset = ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    AActor* Duplicated = ActorSS->DuplicateActor(Found, Found->GetWorld(), Offset);
    if (!Duplicated) { SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to duplicate actor"), nullptr, TEXT("DUPLICATE_FAILED")); return true; }

    FString NewName; Payload->TryGetStringField(TEXT("newName"), NewName);
    if (!NewName.TrimStartAndEnd().IsEmpty()) Duplicated->SetActorLabel(NewName);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("source"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("actorName"), Duplicated->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Duplicated->GetPathName());
    TArray<TSharedPtr<FJsonValue>> OffsetArray;
    OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.X));
    OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Y));
    OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Z));
    Resp->SetArrayField(TEXT("offset"), OffsetArray);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Duplicated '%s' to '%s'"), *Found->GetActorLabel(), *Duplicated->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor duplicated"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAttach(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString ChildName; Payload->TryGetStringField(TEXT("childActor"), ChildName);
    FString ParentName; Payload->TryGetStringField(TEXT("parentActor"), ParentName);
    if (ChildName.IsEmpty() || ParentName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("childActor and parentActor required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Child = FindActorByName(ChildName);
    AActor* Parent = FindActorByName(ParentName);
    if (!Child || !Parent) { SendAutomationResponse(Socket, RequestId, false, TEXT("Child or parent actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    if (Child == Parent)
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Cannot attach actor to itself"), nullptr, TEXT("CYCLE_DETECTED"));
        return true;
    }

    USceneComponent* ChildRoot = Child->GetRootComponent();
    USceneComponent* ParentRoot = Parent->GetRootComponent();
    if (!ChildRoot || !ParentRoot) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor missing root component"), nullptr, TEXT("ROOT_MISSING")); return true; }

    Child->Modify();
    ChildRoot->Modify();
    ChildRoot->AttachToComponent(ParentRoot, FAttachmentTransformRules::KeepWorldTransform);
    Child->SetOwner(Parent);
    Child->MarkPackageDirty();
    Parent->MarkPackageDirty();

    // Verify attachment
    bool bAttached = false;
    if (Child->GetRootComponent() && Child->GetRootComponent()->GetAttachParent() == ParentRoot)
    {
        bAttached = true;
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), bAttached);
    Resp->SetStringField(TEXT("child"), Child->GetActorLabel());
    Resp->SetStringField(TEXT("parent"), Parent->GetActorLabel());
    
    if (!bAttached)
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to attach actor"), Resp, TEXT("ATTACH_FAILED"));
        return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Attached '%s' to '%s'"), *Child->GetActorLabel(), *Parent->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor attached"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDetach(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    USceneComponent* RootComp = Found->GetRootComponent();
    if (!RootComp || !RootComp->GetAttachParent())
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
        Resp->SetStringField(TEXT("note"), TEXT("Actor was not attached"));
        SendAutomationResponse(Socket, RequestId, true, TEXT("Actor already detached"), Resp, FString());
        return true;
    }

    Found->Modify();
    RootComp->Modify();
    RootComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    Found->SetOwner(nullptr);
    Found->MarkPackageDirty();

    // Verify detachment
    const bool bDetached = (RootComp->GetAttachParent() == nullptr);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), bDetached);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    
    if (!bDetached)
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to detach actor"), Resp, TEXT("DETACH_FAILED"));
        return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Detached '%s'"), *Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor detached"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByTag(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TagValue; Payload->TryGetStringField(TEXT("tag"), TagValue);
    if (TagValue.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("tag required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    FString MatchType; Payload->TryGetStringField(TEXT("matchType"), MatchType);
    MatchType = MatchType.ToLower();
    FName TagName(*TagValue);
    TArray<TSharedPtr<FJsonValue>> Matches;

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;
        bool bMatches = false;
        if (MatchType == TEXT("contains"))
        {
            for (const FName& Existing : Actor->Tags)
            {
                if (Existing.ToString().Contains(TagValue, ESearchCase::IgnoreCase)) { bMatches = true; break; }
            }
        }
        else
        {
            bMatches = Actor->ActorHasTag(TagName);
        }

        if (bMatches)
        {
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
            Entry->SetStringField(TEXT("path"), Actor->GetPathName());
            Entry->SetStringField(TEXT("class"), Actor->GetClass() ? Actor->GetClass()->GetPathName() : TEXT(""));
            Matches.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("actors"), Matches);
    Resp->SetNumberField(TEXT("count"), Matches.Num());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actors found"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddTag(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    FString TagValue; Payload->TryGetStringField(TEXT("tag"), TagValue);
    if (TargetName.IsEmpty() || TagValue.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName and tag required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    const FName TagName(*TagValue);
    const bool bAlreadyHad = Found->Tags.Contains(TagName);

    Found->Modify();
    Found->Tags.AddUnique(TagName);
    Found->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("wasPresent"), bAlreadyHad);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("tag"), TagName.ToString());
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Added tag '%s' to '%s'"), *TagName.ToString(), *Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Tag applied to actor"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByName(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString Query; Payload->TryGetStringField(TEXT("name"), Query);
    if (Query.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("name required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    const TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
    TArray<TSharedPtr<FJsonValue>> Matches;
    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;
        const FString Label = Actor->GetActorLabel();
        const FString Name = Actor->GetName();
        const FString Path = Actor->GetPathName();
        const bool bMatches = Label.Contains(Query, ESearchCase::IgnoreCase) ||
            Name.Contains(Query, ESearchCase::IgnoreCase) ||
            Path.Contains(Query, ESearchCase::IgnoreCase);
        if (bMatches)
        {
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("label"), Label);
            Entry->SetStringField(TEXT("name"), Name);
            Entry->SetStringField(TEXT("path"), Path);
            Entry->SetStringField(TEXT("class"), Actor->GetClass() ? Actor->GetClass()->GetPathName() : TEXT(""));
            Matches.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("count"), Matches.Num());
    Resp->SetArrayField(TEXT("actors"), Matches);
    Resp->SetStringField(TEXT("query"), Query);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor query executed"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDeleteByTag(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TagValue; Payload->TryGetStringField(TEXT("tag"), TagValue);
    if (TagValue.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("tag required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    const FName TagName(*TagValue);
    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    const TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
    TArray<FString> Deleted;

    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;
        if (Actor->ActorHasTag(TagName))
        {
            const FString Label = Actor->GetActorLabel();
            if (ActorSS->DestroyActor(Actor)) Deleted.Add(Label);
        }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("tag"), TagName.ToString());
    Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());
    TArray<TSharedPtr<FJsonValue>> DeletedArray;
    for (const FString& Name : Deleted) DeletedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("deleted"), DeletedArray);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actors deleted by tag"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetBlueprintVariables(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    const TSharedPtr<FJsonObject>* VariablesPtr = nullptr;
    if (!(Payload->TryGetObjectField(TEXT("variables"), VariablesPtr) && VariablesPtr && VariablesPtr->IsValid()))
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("variables object required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    UClass* ActorClass = Found->GetClass();
    Found->Modify();
    TArray<FString> Applied;
    TArray<FString> Warnings;

    for (const auto& Pair : (*VariablesPtr)->Values)
    {
        FProperty* Property = ActorClass->FindPropertyByName(*Pair.Key);
        if (!Property) { Warnings.Add(FString::Printf(TEXT("Property not found: %s"), *Pair.Key)); continue; }

        FString ApplyError;
        if (ApplyJsonValueToProperty(Found, Property, Pair.Value, ApplyError)) Applied.Add(Pair.Key);
        else Warnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *Pair.Key, *ApplyError));
    }

    Found->MarkComponentsRenderStateDirty();
    Found->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    if (Applied.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> AppliedArray;
        for (const FString& Name : Applied) AppliedArray.Add(MakeShared<FJsonValueString>(Name));
        Resp->SetArrayField(TEXT("updated"), AppliedArray);
    }
    if (Warnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> WarnArray;
        for (const FString& Warning : Warnings) WarnArray.Add(MakeShared<FJsonValueString>(Warning));
        Resp->SetArrayField(TEXT("warnings"), WarnArray);
    }
    SendAutomationResponse(Socket, RequestId, true, TEXT("Variables updated"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCreateSnapshot(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    FString SnapshotName; Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
    if (SnapshotName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("snapshotName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    const FString SnapshotKey = FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
    CachedActorSnapshots.Add(SnapshotKey, Found->GetActorTransform());

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("snapshotName"), SnapshotName);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Snapshot created"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRestoreSnapshot(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    FString SnapshotName; Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
    if (SnapshotName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("snapshotName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true;
    }

    const FString SnapshotKey = FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
    if (!CachedActorSnapshots.Contains(SnapshotKey))
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Snapshot not found"), nullptr, TEXT("SNAPSHOT_NOT_FOUND"));
        return true;
    }

    const FTransform& SavedTransform = CachedActorSnapshots[SnapshotKey];
    Found->Modify();
    Found->SetActorTransform(SavedTransform);
    Found->MarkComponentsRenderStateDirty();
    Found->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("snapshotName"), SnapshotName);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Snapshot restored"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorExport(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    FMcpOutputCapture OutputCapture;
    UExporter::ExportToOutputDevice(nullptr, Found, nullptr, OutputCapture, TEXT("T3D"), 0, 0, false);
    FString OutputString = FString::Join(OutputCapture.Consume(), TEXT("\n"));

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("t3d"), OutputString);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor exported"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetBoundingBox(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    FVector Origin, BoxExtent;
    Found->GetActorBounds(false, Origin, BoxExtent);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    
    auto MakeArray = [](const FVector& Vec)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
        return Arr;
    };

    Resp->SetArrayField(TEXT("origin"), MakeArray(Origin));
    Resp->SetArrayField(TEXT("extent"), MakeArray(BoxExtent));
    SendAutomationResponse(Socket, RequestId, true, TEXT("Bounding box retrieved"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetMetadata(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("name"), Found->GetName());
    Resp->SetStringField(TEXT("label"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("path"), Found->GetPathName());
    Resp->SetStringField(TEXT("class"), Found->GetClass() ? Found->GetClass()->GetPathName() : TEXT(""));
    
    TArray<TSharedPtr<FJsonValue>> TagsArray;
    for (const FName& Tag : Found->Tags)
    {
        TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
    }
    Resp->SetArrayField(TEXT("tags"), TagsArray);

    const FTransform Current = Found->GetActorTransform();
    auto MakeArray = [](const FVector& Vec)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
        return Arr;
    };
    Resp->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));
    
    SendAutomationResponse(Socket, RequestId, true, TEXT("Metadata retrieved"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRemoveTag(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
    FString TagValue; Payload->TryGetStringField(TEXT("tag"), TagValue);
    if (TargetName.IsEmpty() || TagValue.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName and tag required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }

    const FName TagName(*TagValue);
    if (!Found->Tags.Contains(TagName))
    {
        // Idempotent success
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetBoolField(TEXT("wasPresent"), false);
        Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
        Resp->SetStringField(TEXT("tag"), TagValue);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Tag not present (idempotent)"), Resp, FString());
        return true;
    }

    Found->Modify();
    Found->Tags.Remove(TagName);
    Found->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("wasPresent"), true);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("tag"), TagValue);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("ControlActor: Removed tag '%s' from '%s'"), *TagValue, *Found->GetActorLabel());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Tag removed from actor"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("control_actor"), ESearchCase::IgnoreCase) && !Lower.StartsWith(TEXT("control_actor"))) return false;
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("control_actor payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    FString SubAction; Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

    UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("HandleControlActorAction: %s RequestId=%s"), *LowerSub, *RequestId);

#if WITH_EDITOR
    if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return true; }
    if (!GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return true; }

    if (LowerSub == TEXT("spawn")) return HandleControlActorSpawn(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("spawn_blueprint")) return HandleControlActorSpawnBlueprint(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("delete") || LowerSub == TEXT("remove")) return HandleControlActorDelete(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("apply_force") || LowerSub == TEXT("apply_force_to_actor")) return HandleControlActorApplyForce(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("set_transform") || LowerSub == TEXT("set_actor_transform")) return HandleControlActorSetTransform(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("get_transform") || LowerSub == TEXT("get_actor_transform")) return HandleControlActorGetTransform(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("set_visibility") || LowerSub == TEXT("set_actor_visibility")) return HandleControlActorSetVisibility(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("add_component")) return HandleControlActorAddComponent(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("set_component_properties")) return HandleControlActorSetComponentProperties(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("get_components")) return HandleControlActorGetComponents(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("duplicate")) return HandleControlActorDuplicate(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("attach")) return HandleControlActorAttach(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("detach")) return HandleControlActorDetach(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("find_by_tag")) return HandleControlActorFindByTag(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("add_tag")) return HandleControlActorAddTag(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("remove_tag")) return HandleControlActorRemoveTag(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("find_by_name")) return HandleControlActorFindByName(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("delete_by_tag")) return HandleControlActorDeleteByTag(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("set_blueprint_variables")) return HandleControlActorSetBlueprintVariables(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("create_snapshot")) return HandleControlActorCreateSnapshot(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("restore_snapshot")) return HandleControlActorRestoreSnapshot(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("export")) return HandleControlActorExport(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("get_bounding_box")) return HandleControlActorGetBoundingBox(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("get_metadata")) return HandleControlActorGetMetadata(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("list") || LowerSub == TEXT("list_actors")) return HandleControlActorList(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("get") || LowerSub == TEXT("get_actor") || LowerSub == TEXT("get_actor_by_name")) return HandleControlActorGet(RequestId, Payload, RequestingSocket);

    SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Unknown actor control action: %s"), *LowerSub), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor control requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPlay(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (GEditor->PlayWorld)
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetBoolField(TEXT("alreadyPlaying"), true);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Play session already active"), Resp, FString());
        return true;
    }

    FRequestPlaySessionParams PlayParams;
    PlayParams.WorldType = EPlaySessionWorldType::PlayInEditor;
#if MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS
    PlayParams.EditorPlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
#endif
#if MCP_HAS_LEVEL_EDITOR_MODULE
    if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
    {
        TSharedPtr<IAssetViewport> DestinationViewport = LevelEditorModule->GetFirstActiveViewport();
        if (DestinationViewport.IsValid()) PlayParams.DestinationSlateViewport = DestinationViewport;
    }
#endif

    GEditor->RequestPlaySession(PlayParams);
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Play in Editor started"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStop(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!GEditor->PlayWorld)
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetBoolField(TEXT("alreadyStopped"), true);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Play session not active"), Resp, FString());
        return true;
    }

    GEditor->RequestEndPlayMap();
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Play in Editor stopped"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorFocusActor(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString ActorName; Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    if (UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
    {
        TArray<AActor*> Actors = ActorSS->GetAllLevelActors();
        for (AActor* Actor : Actors)
        {
            if (!Actor) continue;
            if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
            {
                GEditor->SelectNone(true, true, false);
                GEditor->SelectActor(Actor, true, true, true);
                GEditor->Exec(nullptr, TEXT("EDITORTEMPVIEWPORT"));
                GEditor->MoveViewportCamerasToActor(*Actor, false);
                SendAutomationResponse(Socket, RequestId, true, TEXT("Viewport focused on actor"), nullptr, FString());
                return true;
            }
        }
        SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetCamera(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    const TSharedPtr<FJsonObject>* Loc = nullptr; FVector Location(0,0,0); FRotator Rotation(0,0,0);
    if (Payload->TryGetObjectField(TEXT("location"), Loc) && Loc && (*Loc).IsValid()) ReadVectorField(*Loc, TEXT(""), Location, Location);
    if (Payload->TryGetObjectField(TEXT("rotation"), Loc) && Loc && (*Loc).IsValid()) ReadRotatorField(*Loc, TEXT(""), Rotation, Rotation);

#if defined(MCP_HAS_UNREALEDITOR_SUBSYSTEM)
    if (UUnrealEditorSubsystem* UES = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
    {
        UES->SetLevelViewportCameraInfo(Location, Rotation);
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
        if (ULevelEditorSubsystem* LES = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) LES->EditorInvalidateViewports();
#endif
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp, FString());
        return true;
    }
#endif
    if (FEditorViewportClient* ViewportClient = GEditor->GetActiveViewport() ? (FEditorViewportClient*)GEditor->GetActiveViewport()->GetClient() : nullptr)
    {
        ViewportClient->SetViewLocation(Location);
        ViewportClient->SetViewRotation(Rotation);
        ViewportClient->Invalidate();
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp, FString());
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewMode(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString Mode; Payload->TryGetStringField(TEXT("viewMode"), Mode);
    FString LowerMode = Mode.ToLower();
    FString Chosen;
    if (LowerMode == TEXT("lit")) Chosen = TEXT("Lit");
    else if (LowerMode == TEXT("unlit")) Chosen = TEXT("Unlit");
    else if (LowerMode == TEXT("wireframe")) Chosen = TEXT("Wireframe");
    else if (LowerMode == TEXT("detaillighting")) Chosen = TEXT("DetailLighting");
    else if (LowerMode == TEXT("lightingonly")) Chosen = TEXT("LightingOnly");
    else if (LowerMode == TEXT("lightcomplexity")) Chosen = TEXT("LightComplexity");
    else if (LowerMode == TEXT("shadercomplexity")) Chosen = TEXT("ShaderComplexity");
    else if (LowerMode == TEXT("lightmapdensity")) Chosen = TEXT("LightmapDensity");
    else if (LowerMode == TEXT("stationarylightoverlap")) Chosen = TEXT("StationaryLightOverlap");
    else if (LowerMode == TEXT("reflectionoverride")) Chosen = TEXT("ReflectionOverride");
    else Chosen = Mode;

    const FString Cmd = FString::Printf(TEXT("viewmode %s"), *Chosen);
    if (GEditor->Exec(nullptr, *Cmd))
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetBoolField(TEXT("success"), true); Resp->SetStringField(TEXT("viewMode"), Chosen);
        SendAutomationResponse(Socket, RequestId, true, TEXT("View mode set"), Resp, FString());
        return true;
    }
    SendAutomationResponse(Socket, RequestId, false, TEXT("View mode command failed"), nullptr, TEXT("EXEC_FAILED"));
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("control_editor"), ESearchCase::IgnoreCase) && !Lower.StartsWith(TEXT("control_editor"))) return false;
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("control_editor payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    FString SubAction; Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
    if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return true; }

    if (LowerSub == TEXT("play")) return HandleControlEditorPlay(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("stop")) return HandleControlEditorStop(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("focus_actor")) return HandleControlEditorFocusActor(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("set_camera")) return HandleControlEditorSetCamera(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("set_view_mode")) return HandleControlEditorSetViewMode(RequestId, Payload, RequestingSocket);
    if (LowerSub == TEXT("open_asset")) return HandleControlEditorOpenAsset(RequestId, Payload, RequestingSocket);

    SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Unknown editor control action: %s"), *LowerSub), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor control requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorOpenAsset(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString AssetPath; Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    if (AssetPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

    if (!GEditor) { SendAutomationResponse(Socket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return true; }

    UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSS) { SendAutomationResponse(Socket, RequestId, false, TEXT("AssetEditorSubsystem not available"), nullptr, TEXT("SUBSYSTEM_MISSING")); return true; }

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"), nullptr, TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to load asset"), nullptr, TEXT("LOAD_FAILED"));
        return true;
    }

    const bool bOpened = AssetEditorSS->OpenEditorForAsset(Asset);
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), bOpened);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    
    if (bOpened)
    {
        SendAutomationResponse(Socket, RequestId, true, TEXT("Asset opened"), Resp, FString());
    }
    else
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to open asset editor"), Resp, TEXT("OPEN_FAILED"));
    }
    return true;
#else
    return false;
#endif
}



bool UMcpAutomationBridgeSubsystem::HandleControlActorList(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
        SendAutomationResponse(Socket, RequestId, false, TEXT("EditorActorSubsystem unavailable"), nullptr, TEXT("SUBSYSTEM_MISSING"));
        return true;
    }

    const TArray<AActor*>& AllActors = ActorSS->GetAllLevelActors();
    TArray<TSharedPtr<FJsonValue>> ActorsArray;

    for (AActor* Actor : AllActors) {
        if (!Actor) continue;
        const FString Label = Actor->GetActorLabel();
        const FString Name = Actor->GetName();
        if (!Filter.IsEmpty() && !Label.Contains(Filter) && !Name.Contains(Filter)) continue;

        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("label"), Label);
        Entry->SetStringField(TEXT("name"), Name);
        Entry->SetStringField(TEXT("path"), Actor->GetPathName());
        Entry->SetStringField(TEXT("class"), Actor->GetClass() ? Actor->GetClass()->GetPathName() : TEXT(""));
        ActorsArray.Add(MakeShared<FJsonValueObject>(Entry));
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("actors"), ActorsArray);
    Resp->SetNumberField(TEXT("count"), ActorsArray.Num());
    if (!Filter.IsEmpty()) Resp->SetStringField(TEXT("filter"), Filter);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Actors listed"), Resp, FString());
    return true;
#else
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGet(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString TargetName;
    Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) {
        SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    AActor* Found = FindActorByName(TargetName);
    if (!Found) {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    const FTransform Current = Found->GetActorTransform();
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("name"), Found->GetName());
    Resp->SetStringField(TEXT("label"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("path"), Found->GetPathName());
    Resp->SetStringField(TEXT("class"), Found->GetClass() ? Found->GetClass()->GetPathName() : TEXT(""));

    TArray<TSharedPtr<FJsonValue>> TagsArray;
    for (const FName& Tag : Found->Tags) {
        TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
    }
    Resp->SetArrayField(TEXT("tags"), TagsArray);

    auto MakeArray = [](const FVector& Vec) -> TArray<TSharedPtr<FJsonValue>> {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
        Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
        return Arr;
    };
    Resp->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));
    Resp->SetArrayField(TEXT("scale"), MakeArray(Current.GetScale3D()));

    SendAutomationResponse(Socket, RequestId, true, TEXT("Actor retrieved"), Resp, FString());
    return true;
#else
    return false;
#endif
}
