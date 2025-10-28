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
#endif
// Additional editor headers for viewport control
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Blueprint.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Math/UnrealMathUtility.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleControlActorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("control_actor"), ESearchCase::IgnoreCase) && !Lower.StartsWith(TEXT("control_actor"))) return false;

    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("control_actor payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    FString SubAction; Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

    // Execute native handlers for each subaction on GameThread.
#if WITH_EDITOR
    {
        if (!GEditor)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }

        UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        if (!ActorSS)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
            return true;
        }

        auto FindActorByName = [&](const FString& Target) -> AActor*
        {
            if (Target.IsEmpty())
            {
                return nullptr;
            }
            AActor* Found = nullptr;
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            for (AActor* A : AllActors)
            {
                if (!A)
                {
                    continue;
                }
                if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
                    A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
                    A->GetPathName().Equals(Target, ESearchCase::IgnoreCase))
                {
                    Found = A;
                    break;
                }
            }
            if (!Found)
            {
                if (UObject* Obj = UEditorAssetLibrary::LoadAsset(Target))
                {
                    Found = Cast<AActor>(Obj);
                }
            }
            return Found;
        };

        auto ExtractVectorField = [](const TSharedPtr<FJsonObject>& Source, const TCHAR* FieldName, const FVector& DefaultValue) -> FVector
        {
            FVector Parsed = DefaultValue;
            ReadVectorField(Source, FieldName, Parsed, DefaultValue);
            return Parsed;
        };

        auto ExtractRotatorField = [](const TSharedPtr<FJsonObject>& Source, const TCHAR* FieldName, const FRotator& DefaultValue) -> FRotator
        {
            FRotator Parsed = DefaultValue;
            ReadRotatorField(Source, FieldName, Parsed, DefaultValue);
            return Parsed;
        };

        if (LowerSub == TEXT("spawn"))
        {
            FString ClassPath; Payload->TryGetStringField(TEXT("classPath"), ClassPath);
            FString ActorName; Payload->TryGetStringField(TEXT("actorName"), ActorName);

            FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
            FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

            UClass* ResolvedClass = nullptr;
            UStaticMesh* ResolvedStaticMesh = nullptr;
            if (ClassPath.StartsWith(TEXT("/")) || ClassPath.Contains(TEXT("/")))
            {
                if (UObject* Loaded = UEditorAssetLibrary::LoadAsset(ClassPath))
                {
                    if (UBlueprint* BP = Cast<UBlueprint>(Loaded))
                    {
                        ResolvedClass = BP->GeneratedClass;
                    }
                    else if (UClass* C = Cast<UClass>(Loaded))
                    {
                        ResolvedClass = C;
                    }
                    else if (UStaticMesh* Mesh = Cast<UStaticMesh>(Loaded))
                    {
                        ResolvedStaticMesh = Mesh;
                    }
                }
            }
            if (!ResolvedClass)
            {
                ResolvedClass = ResolveClassByName(ClassPath);
            }

            const bool bSpawnStaticMeshActor = (ResolvedClass == nullptr && ResolvedStaticMesh != nullptr);

            if (!ResolvedClass && !bSpawnStaticMeshActor)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Class not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor class not found"), Resp, TEXT("CLASS_NOT_FOUND"));
                return true;
            }

            AActor* Spawned = nullptr;
            if (bSpawnStaticMeshActor)
            {
                Spawned = ActorSS->SpawnActorFromClass(AStaticMeshActor::StaticClass(), Location, Rotation);
                if (!Spawned)
                {
                    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                    Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn actor"));
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to spawn actor"), Resp, TEXT("SPAWN_FAILED"));
                    return true;
                }

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
            else
            {
                Spawned = ActorSS->SpawnActorFromClass(ResolvedClass, Location, Rotation);
            }

            if (!Spawned)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn actor"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to spawn actor"), Resp, TEXT("SPAWN_FAILED"));
                return true;
            }

            if (!ActorName.IsEmpty())
            {
                Spawned->SetActorLabel(ActorName);
            }

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
            Resp->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
            if (bSpawnStaticMeshActor && ResolvedStaticMesh)
            {
                Resp->SetStringField(TEXT("meshPath"), ResolvedStaticMesh->GetPathName());
            }
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor spawned"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("spawn_blueprint"))
        {
            FString BlueprintPath; Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
            if (BlueprintPath.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint path required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            FString ActorName; Payload->TryGetStringField(TEXT("actorName"), ActorName);
            FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
            FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

            UClass* ResolvedClass = nullptr;
            if (BlueprintPath.StartsWith(TEXT("/")) || BlueprintPath.Contains(TEXT("/")))
            {
                if (UObject* Loaded = UEditorAssetLibrary::LoadAsset(BlueprintPath))
                {
                    if (UBlueprint* BP = Cast<UBlueprint>(Loaded))
                    {
                        ResolvedClass = BP->GeneratedClass;
                    }
                    else if (UClass* C = Cast<UClass>(Loaded))
                    {
                        ResolvedClass = C;
                    }
                }
            }
            if (!ResolvedClass)
            {
                ResolvedClass = ResolveClassByName(BlueprintPath);
            }

            if (!ResolvedClass)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Blueprint class not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint class not found"), Resp, TEXT("CLASS_NOT_FOUND"));
                return true;
            }

            AActor* Spawned = ActorSS->SpawnActorFromClass(ResolvedClass, Location, Rotation);
            if (!Spawned)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn blueprint"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to spawn blueprint"), Resp, TEXT("SPAWN_FAILED"));
                return true;
            }

            if (!ActorName.IsEmpty())
            {
                Spawned->SetActorLabel(ActorName);
            }

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
            Resp->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
            Resp->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint spawned"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("delete") || LowerSub == TEXT("remove"))
        {
            TArray<FString> Targets;
            const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("actorNames"), NamesArray) && NamesArray)
            {
                for (const TSharedPtr<FJsonValue>& Entry : *NamesArray)
                {
                    if (Entry.IsValid() && Entry->Type == EJson::String)
                    {
                        const FString Value = Entry->AsString().TrimStartAndEnd();
                        if (!Value.IsEmpty())
                        {
                            Targets.AddUnique(Value);
                        }
                    }
                }
            }

            FString SingleName;
            if (Targets.Num() == 0)
            {
                Payload->TryGetStringField(TEXT("actorName"), SingleName);
                if (!SingleName.IsEmpty())
                {
                    Targets.AddUnique(SingleName);
                }
            }

            if (Targets.Num() == 0)
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName or actorNames required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            TArray<FString> Deleted;
            TArray<FString> Missing;

            for (const FString& Name : Targets)
            {
                AActor* Found = FindActorByName(Name);
                if (!Found)
                {
                    Missing.Add(Name);
                    continue;
                }

                const bool bDeleted = ActorSS->DestroyActor(Found);
                if (bDeleted)
                {
                    Deleted.Add(Name);
                }
                else
                {
                    Missing.Add(Name);
                }
            }

            const bool bAllDeleted = Missing.Num() == 0;
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), bAllDeleted);
            Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

            TArray<TSharedPtr<FJsonValue>> DeletedArray;
            for (const FString& Name : Deleted)
            {
                DeletedArray.Add(MakeShared<FJsonValueString>(Name));
            }
            Resp->SetArrayField(TEXT("deleted"), DeletedArray);

            if (Missing.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> MissingArray;
                for (const FString& Name : Missing)
                {
                    MissingArray.Add(MakeShared<FJsonValueString>(Name));
                }
                Resp->SetArrayField(TEXT("missing"), MissingArray);
            }

            const FString Message = bAllDeleted
                ? TEXT("Actors deleted")
                : TEXT("Some actors could not be deleted");

            SendAutomationResponse(RequestingSocket, RequestId, bAllDeleted, Message, Resp, bAllDeleted ? FString() : TEXT("DELETE_PARTIAL"));
            return true;
        }

        if (LowerSub == TEXT("apply_force") || LowerSub == TEXT("apply_force_to_actor"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            FVector ForceVector = ExtractVectorField(Payload, TEXT("force"), FVector::ZeroVector);

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return true;
            }

            UPrimitiveComponent* Prim = Found->FindComponentByClass<UPrimitiveComponent>();
            if (!Prim)
            {
                if (UStaticMeshComponent* SMC = Found->FindComponentByClass<UStaticMeshComponent>())
                {
                    Prim = SMC;
                }
            }

            if (!Prim)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("No component to apply force"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No component to apply force"), Resp, TEXT("NO_COMPONENT"));
                return true;
            }

            if (Prim->Mobility == EComponentMobility::Static)
            {
                Prim->SetMobility(EComponentMobility::Movable);
            }
            if (!Prim->IsSimulatingPhysics())
            {
                Prim->SetSimulatePhysics(true);
            }

            Prim->AddForce(ForceVector);
            Prim->WakeAllRigidBodies();
            Prim->MarkRenderStateDirty();

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            TArray<TSharedPtr<FJsonValue>> Applied;
            Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.X));
            Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Y));
            Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Z));
            Resp->SetArrayField(TEXT("applied"), Applied);
            Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Force applied"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("set_transform") || LowerSub == TEXT("set_actor_transform"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            if (TargetName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return true;
            }

            FVector Location = ExtractVectorField(Payload, TEXT("location"), Found->GetActorLocation());
            FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), Found->GetActorRotation());
            FVector Scale = ExtractVectorField(Payload, TEXT("scale"), Found->GetActorScale3D());

            Found->Modify();
            Found->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
            Found->SetActorRotation(Rotation, ETeleportType::TeleportPhysics);
            Found->SetActorScale3D(Scale);
            Found->MarkComponentsRenderStateDirty();
            Found->MarkPackageDirty();

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            TArray<TSharedPtr<FJsonValue>> LocArray;
            LocArray.Add(MakeShared<FJsonValueNumber>(Location.X));
            LocArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
            LocArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
            Resp->SetArrayField(TEXT("location"), LocArray);
            TArray<TSharedPtr<FJsonValue>> RotArray;
            RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
            RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
            RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
            Resp->SetArrayField(TEXT("rotation"), RotArray);
            TArray<TSharedPtr<FJsonValue>> ScaleArray;
            ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
            ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
            ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
            Resp->SetArrayField(TEXT("scale"), ScaleArray);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor transform updated"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("add_component"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            if (TargetName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            FString ComponentType; Payload->TryGetStringField(TEXT("componentType"), ComponentType);
            if (ComponentType.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("componentType required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            FString ComponentName; Payload->TryGetStringField(TEXT("componentName"), ComponentName);

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return true;
            }

            UClass* ComponentClass = ResolveClassByName(ComponentType);
            if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), FString::Printf(TEXT("Component class not found: %s"), *ComponentType));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Component class not found"), Resp, TEXT("CLASS_NOT_FOUND"));
                return true;
            }

            if (ComponentName.TrimStartAndEnd().IsEmpty())
            {
                ComponentName = FString::Printf(TEXT("%s_%d"), *ComponentClass->GetName(), FMath::Rand());
            }

            FName DesiredName = FName(*ComponentName);
            UActorComponent* NewComponent = NewObject<UActorComponent>(Found, ComponentClass, DesiredName, RF_Transactional);
            if (!NewComponent)
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create component"), nullptr, TEXT("CREATE_COMPONENT_FAILED"));
                return true;
            }

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
                    if (!Property)
                    {
                        PropertyWarnings.Add(FString::Printf(TEXT("Property not found: %s"), *Pair.Key));
                        continue;
                    }
                    FString ApplyError;
                    if (ApplyJsonValueToProperty(NewComponent, Property, Pair.Value, ApplyError))
                    {
                        AppliedProperties.Add(Pair.Key);
                    }
                    else
                    {
                        PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *Pair.Key, *ApplyError));
                    }
                }
            }

            NewComponent->RegisterComponent();
            if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComponent))
            {
                SceneComp->UpdateComponentToWorld();
            }
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
                for (const FString& PropName : AppliedProperties)
                {
                    PropsArray.Add(MakeShared<FJsonValueString>(PropName));
                }
                Resp->SetArrayField(TEXT("appliedProperties"), PropsArray);
            }
            if (PropertyWarnings.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> WarnArray;
                for (const FString& Warning : PropertyWarnings)
                {
                    WarnArray.Add(MakeShared<FJsonValueString>(Warning));
                }
                Resp->SetArrayField(TEXT("warnings"), WarnArray);
            }
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Component added"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("set_component_properties"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            if (TargetName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            FString ComponentName; Payload->TryGetStringField(TEXT("componentName"), ComponentName);
            if (ComponentName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("componentName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
            if (!(Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) && PropertiesPtr && (*PropertiesPtr).IsValid()))
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("properties object required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return true;
            }

            UActorComponent* TargetComponent = nullptr;
            for (UActorComponent* Comp : Found->GetComponents())
            {
                if (!Comp) continue;
                if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
                {
                    TargetComponent = Comp;
                    break;
                }
            }

            if (!TargetComponent)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Component not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Component not found"), Resp, TEXT("COMPONENT_NOT_FOUND"));
                return true;
            }

            TArray<FString> AppliedProperties;
            TArray<FString> PropertyWarnings;
            UClass* ComponentClass = TargetComponent->GetClass();
            TargetComponent->Modify();

            for (const auto& Pair : (*PropertiesPtr)->Values)
            {
                FProperty* Property = ComponentClass->FindPropertyByName(*Pair.Key);
                if (!Property)
                {
                    PropertyWarnings.Add(FString::Printf(TEXT("Property not found: %s"), *Pair.Key));
                    continue;
                }
                FString ApplyError;
                if (ApplyJsonValueToProperty(TargetComponent, Property, Pair.Value, ApplyError))
                {
                    AppliedProperties.Add(Pair.Key);
                }
                else
                {
                    PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *Pair.Key, *ApplyError));
                }
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
                for (const FString& PropName : AppliedProperties)
                {
                    PropsArray.Add(MakeShared<FJsonValueString>(PropName));
                }
                Resp->SetArrayField(TEXT("applied"), PropsArray);
            }
            if (PropertyWarnings.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> WarnArray;
                for (const FString& Warning : PropertyWarnings)
                {
                    WarnArray.Add(MakeShared<FJsonValueString>(Warning));
                }
                Resp->SetArrayField(TEXT("warnings"), WarnArray);
            }
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Component properties updated"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("get_components"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            if (TargetName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return true;
            }

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
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor components retrieved"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("duplicate"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            if (TargetName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return true;
            }

            FVector Offset = ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
            AActor* Duplicated = ActorSS->DuplicateActor(Found, Found->GetWorld(), Offset);
            if (!Duplicated)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Duplicate failed"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to duplicate actor"), Resp, TEXT("DUPLICATE_FAILED"));
                return true;
            }

            FString NewName; Payload->TryGetStringField(TEXT("newName"), NewName);
            if (!NewName.TrimStartAndEnd().IsEmpty())
            {
                Duplicated->SetActorLabel(NewName);
            }

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
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor duplicated"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("find_by_tag"))
        {
            FString TagValue; Payload->TryGetStringField(TEXT("tag"), TagValue);
            if (TagValue.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("tag required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            FString MatchType; Payload->TryGetStringField(TEXT("matchType"), MatchType);
            MatchType = MatchType.ToLower();

            FName TagName(*TagValue);
            TArray<TSharedPtr<FJsonValue>> Matches;

            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            for (AActor* Actor : AllActors)
            {
                if (!Actor) continue;
                bool bMatches = false;
                if (MatchType == TEXT("contains"))
                {
                    for (const FName& Existing : Actor->Tags)
                    {
                        if (Existing.ToString().Contains(TagValue, ESearchCase::IgnoreCase))
                        {
                            bMatches = true;
                            break;
                        }
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
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actors found"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("set_blueprint_variables"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            if (TargetName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            const TSharedPtr<FJsonObject>* VariablesPtr = nullptr;
            if (!(Payload->TryGetObjectField(TEXT("variables"), VariablesPtr) && VariablesPtr && (*VariablesPtr).IsValid()))
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("variables object required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return true;
            }

            UClass* ActorClass = Found->GetClass();
            Found->Modify();
            TArray<FString> Applied;
            TArray<FString> Warnings;

            for (const auto& Pair : (*VariablesPtr)->Values)
            {
                FProperty* Property = ActorClass->FindPropertyByName(*Pair.Key);
                if (!Property)
                {
                    Warnings.Add(FString::Printf(TEXT("Property not found: %s"), *Pair.Key));
                    continue;
                }

                FString ApplyError;
                if (ApplyJsonValueToProperty(Found, Property, Pair.Value, ApplyError))
                {
                    Applied.Add(Pair.Key);
                }
                else
                {
                    Warnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *Pair.Key, *ApplyError));
                }
            }

            Found->MarkComponentsRenderStateDirty();
            Found->MarkPackageDirty();

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            if (Applied.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> AppliedArray;
                for (const FString& Name : Applied)
                {
                    AppliedArray.Add(MakeShared<FJsonValueString>(Name));
                }
                Resp->SetArrayField(TEXT("updated"), AppliedArray);
            }
            if (Warnings.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> WarnArray;
                for (const FString& Warning : Warnings)
                {
                    WarnArray.Add(MakeShared<FJsonValueString>(Warning));
                }
                Resp->SetArrayField(TEXT("warnings"), WarnArray);
            }
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variables updated"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("create_snapshot"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            if (TargetName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            FString SnapshotName; Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
            if (SnapshotName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("snapshotName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return true;
            }

            const FString SnapshotKey = FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
            CachedActorSnapshots.Add(SnapshotKey, Found->GetActorTransform());

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("snapshotName"), SnapshotName);
            Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Snapshot created"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("list") || LowerSub == TEXT("list_actors"))
        {
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Reserve(AllActors.Num());

            for (AActor* A : AllActors)
            {
                if (!A)
                {
                    continue;
                }
                TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("name"), A->GetName());
                Entry->SetStringField(TEXT("label"), A->GetActorLabel());
                Entry->SetStringField(TEXT("class"), A->GetClass() ? A->GetClass()->GetPathName() : TEXT(""));
                Entry->SetStringField(TEXT("path"), A->GetPathName());
                Arr.Add(MakeShared<FJsonValueObject>(Entry));
            }

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetArrayField(TEXT("actors"), Arr);
            Resp->SetNumberField(TEXT("count"), Arr.Num());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor list retrieved"), Resp, FString());
            return true;
        }

        if (LowerSub == TEXT("get") || LowerSub == TEXT("get_actor") || LowerSub == TEXT("get_actor_by_name"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            if (TargetName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), false);
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return true;
            }

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("name"), Found->GetName());
            Resp->SetStringField(TEXT("label"), Found->GetActorLabel());
            Resp->SetStringField(TEXT("path"), Found->GetPathName());
            Resp->SetStringField(TEXT("class"), Found->GetClass() ? Found->GetClass()->GetPathName() : TEXT(""));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor resolved"), Resp, FString());
            return true;
        }

        SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Unknown actor control action: %s"), *LowerSub), nullptr, TEXT("UNKNOWN_ACTION"));
    }
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor control requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
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
    {
        if (LowerSub == TEXT("play"))
        {
            // PIE start helper varies across engine versions; provide a safe fallback response
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Play in Editor start is not implemented for this engine version"), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
        }

        if (LowerSub == TEXT("stop"))
        {
            // PIE stop helper varies across engine versions; attempt graceful response only
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Play in Editor stop is not implemented for this engine version"), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
        }

        if (LowerSub == TEXT("focus_actor"))
        {
            FString ActorName;
            Payload->TryGetStringField(TEXT("actorName"), ActorName);
            if (ActorName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }

            if (GEditor)
            {
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
                            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Viewport focused on actor"), nullptr, FString());
                            return true;
                        }
                    }
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND"));
                    return true;
                }
            }

            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
        }

        if (LowerSub == TEXT("set_camera"))
        {
            const TSharedPtr<FJsonObject>* Loc = nullptr; FVector Location(0,0,0); FRotator Rotation(0,0,0);
            if (Payload->TryGetObjectField(TEXT("location"), Loc) && Loc && (*Loc).IsValid()) ReadVectorField(*Loc, TEXT(""), Location, Location);
            if (Payload->TryGetObjectField(TEXT("rotation"), Loc) && Loc && (*Loc).IsValid()) ReadRotatorField(*Loc, TEXT(""), Rotation, Rotation);

            if (GEditor)
            {
#if defined(MCP_HAS_UNREALEDITOR_SUBSYSTEM)
                if (UUnrealEditorSubsystem* UES = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
                {
                    UES->SetLevelViewportCameraInfo(Location, Rotation);
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
                    if (ULevelEditorSubsystem* LES = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
                    {
                        LES->EditorInvalidateViewports();
                    }
#endif
                    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetBoolField(TEXT("success"), true);
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera set"), Resp, FString());
                    return true;
                }
#endif
                if (FEditorViewportClient* ViewportClient = GEditor->GetActiveViewport() ? (FEditorViewportClient*)GEditor->GetActiveViewport()->GetClient() : nullptr)
                {
                    ViewportClient->SetViewLocation(Location);
                    ViewportClient->SetViewRotation(Rotation);
                    ViewportClient->Invalidate();
                    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetBoolField(TEXT("success"), true);
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera set"), Resp, FString());
                    return true;
                }
            }

            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
        }

        if (LowerSub == TEXT("set_view_mode"))
        {
            FString Mode; Payload->TryGetStringField(TEXT("viewMode"), Mode);
            if (GEditor)
            {
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
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("View mode set"), Resp, FString());
                    return true;
                }
            }
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("View mode command failed"), nullptr, TEXT("EXEC_FAILED"));
            return true;
        }
        else
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Unknown editor control action: %s"), *LowerSub), nullptr, TEXT("UNKNOWN_ACTION"));
            return true;
        }
    }
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor control requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleLevelAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    const bool bIsLevelAction = (
        Lower == TEXT("save_current_level") ||
        Lower == TEXT("create_new_level") ||
        Lower == TEXT("stream_level") ||
        Lower == TEXT("spawn_light") ||
        Lower == TEXT("build_lighting")
    );
    if (!bIsLevelAction) return false;
#if WITH_EDITOR
    if (Lower == TEXT("save_current_level"))
    {
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("functionName"), TEXT("SAVE_DIRTY_PACKAGES"));
        return HandleExecuteEditorFunction(RequestId, TEXT("execute_editor_function"), P, RequestingSocket);
    }
    if (Lower == TEXT("build_lighting"))
    {
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("functionName"), TEXT("BUILD_LIGHTING"));
        if (Payload.IsValid()) { FString Q; if (Payload->TryGetStringField(TEXT("quality"), Q) && !Q.IsEmpty()) P->SetStringField(TEXT("quality"), Q); }
        return HandleExecuteEditorFunction(RequestId, TEXT("execute_editor_function"), P, RequestingSocket);
    }
    if (Lower == TEXT("create_new_level"))
    {
        FString LevelPath; if (Payload.IsValid()) Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
        if (LevelPath.TrimStartAndEnd().IsEmpty()) LevelPath = TEXT("/Engine/Maps/Entry");
        const FString Cmd = FString::Printf(TEXT("Open %s"), *LevelPath);
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("command"), Cmd);
        return HandleExecuteEditorFunction(RequestId, TEXT("execute_console_command"), P, RequestingSocket);
    }
    if (Lower == TEXT("stream_level"))
    {
        FString LevelName; bool bLoad = true; bool bVis = true;
        if (Payload.IsValid()) { Payload->TryGetStringField(TEXT("levelName"), LevelName); Payload->TryGetBoolField(TEXT("shouldBeLoaded"), bLoad); Payload->TryGetBoolField(TEXT("shouldBeVisible"), bVis); if (LevelName.IsEmpty()) Payload->TryGetStringField(TEXT("levelPath"), LevelName); }
        if (LevelName.TrimStartAndEnd().IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("stream_level requires levelName or levelPath"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
        const FString Cmd = FString::Printf(TEXT("StreamLevel %s %s %s"), *LevelName, bLoad ? TEXT("Load") : TEXT("Unload"), bVis ? TEXT("Show") : TEXT("Hide"));
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("command"), Cmd);
        return HandleExecuteEditorFunction(RequestId, TEXT("execute_console_command"), P, RequestingSocket);
    }
    if (Lower == TEXT("spawn_light"))
    {
        FString LightType = TEXT("Point"); if (Payload.IsValid()) Payload->TryGetStringField(TEXT("lightType"), LightType);
        const FString LT = LightType.ToLower();
        FString ClassName;
        if (LT == TEXT("directional")) ClassName = TEXT("DirectionalLight");
        else if (LT == TEXT("spot")) ClassName = TEXT("SpotLight");
        else if (LT == TEXT("rect")) ClassName = TEXT("RectLight");
        else ClassName = TEXT("PointLight");
        TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
        if (Payload.IsValid()) { const TSharedPtr<FJsonObject>* L = nullptr; if (Payload->TryGetObjectField(TEXT("location"), L) && L && (*L).IsValid()) Params->SetObjectField(TEXT("location"), *L); const TSharedPtr<FJsonObject>* R = nullptr; if (Payload->TryGetObjectField(TEXT("rotation"), R) && R && (*R).IsValid()) Params->SetObjectField(TEXT("rotation"), *R); }
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("functionName"), TEXT("SPAWN_ACTOR_AT_LOCATION")); P->SetStringField(TEXT("class_path"), ClassName); P->SetObjectField(TEXT("params"), Params.ToSharedRef());
        return HandleExecuteEditorFunction(RequestId, TEXT("execute_editor_function"), P, RequestingSocket);
    }
    return false;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Level actions require editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
