# Unreal Engine 5.6 API Reference for MCP Implementation

This document provides key engine API information to guide implementation of the MCP roadmap phases.

---

## Phase 6: Geometry & Mesh Creation

### Plugin Location
`X:/Unreal_Engine/UE_5.6/Engine/Plugins/Runtime/GeometryScripting/`

### Key Headers
| File | Purpose |
|------|---------|
| `GeometryScript/MeshPrimitiveFunctions.h` | Primitives (Box, Sphere, Cylinder, etc.) |
| `GeometryScript/MeshBooleanFunctions.h` | Boolean operations (Union, Difference, Intersection) |
| `GeometryScript/MeshModelingFunctions.h` | Modeling ops (Extrude, Bevel, etc.) |
| `GeometryScript/MeshDeformFunctions.h` | Deformers (Bend, Twist, etc.) |
| `GeometryScript/MeshRemeshFunctions.h` | Remeshing, simplification |
| `GeometryScript/MeshUVFunctions.h` | UV operations |
| `GeometryScript/CollisionFunctions.h` | Collision generation |
| `GeometryScript/MeshSubdivideFunctions.h` | Subdivision |
| `GeometryScript/MeshNormalsFunctions.h` | Normal operations |

### Core Types
```cpp
// Dynamic mesh container
UDynamicMesh* TargetMesh;

// Primitive options
FGeometryScriptPrimitiveOptions Options;
Options.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerFace;
Options.UVMode = EGeometryScriptPrimitiveUVMode::Uniform;
```

### Usage Pattern
All functions are BlueprintCallable static methods in `UGeometryScript*` classes:
```cpp
UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
    TargetMesh, 
    Options,
    FTransform::Identity,
    Width, Height, Depth,
    WidthSegments, HeightSegments, DepthSegments
);
```

---

## Phase 7: Skeletal Mesh & Rigging

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/Engine/SkeletalMesh.h` | Skeletal mesh asset |
| `Runtime/Engine/Classes/Animation/Skeleton.h` | USkeleton definition |
| `Runtime/Engine/Classes/PhysicsEngine/PhysicsAsset.h` | Physics asset |
| `Runtime/Engine/Classes/Animation/MorphTarget.h` | Morph targets |
| `Runtime/Engine/Classes/Engine/SkeletalMeshSocket.h` | Sockets |

### Skeleton & Bone Operations
```cpp
USkeleton* Skeleton;

// Add virtual bone (runtime reference bone)
Skeleton->AddNewVirtualBone(SourceBoneName, TargetBoneName, VirtualBoneName);

// Get bone info
int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
FTransform BoneTransform = Skeleton->GetReferenceSkeleton().GetRefBonePose()[BoneIndex];
```

### Physics Asset
```cpp
UPhysicsAsset* PhysAsset;

// Add body (capsule, sphere, box, convex)
int32 BodyIndex = PhysAsset->FindBodyIndex(BoneName);
FKAggregateGeom& AggGeom = PhysAsset->SkeletalBodySetups[BodyIndex]->AggGeom;
AggGeom.SphereElems.Add(FKSphereElem(Radius));
AggGeom.BoxElems.Add(FKBoxElem(X, Y, Z));
AggGeom.SphylElems.Add(FKSphylElem(Radius, Length));  // Capsule

// Add constraint
int32 ConstraintIndex = PhysAsset->FindConstraintIndex(BoneName);
FConstraintInstance& Constraint = PhysAsset->ConstraintSetup[ConstraintIndex]->DefaultInstance;
Constraint.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, Angle);
```

### Morph Targets
```cpp
UMorphTarget* MorphTarget;
FMorphTargetDelta Delta;
Delta.PositionDelta = FVector3f(X, Y, Z);
Delta.TangentZDelta = FVector3f(0, 0, 1);
Delta.SourceIdx = VertexIndex;
MorphTarget->PopulateDeltas(Deltas, LODIndex, BaseMesh);
```

---

## Phase 8: Material Authoring

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Public/Materials/Material.h` | UMaterial class |
| `Runtime/Engine/Public/Materials/MaterialExpression.h` | Base expression class |
| `Runtime/Engine/Public/Materials/MaterialInstance.h` | Material instances |
| `Editor/MaterialEditor/Public/MaterialEditingLibrary.h` | High-level editor API |

### Core API (UMaterialEditingLibrary)
```cpp
// Create expression node
UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
    Material, 
    UMaterialExpressionTextureSample::StaticClass(),
    NodePosX, NodePosY
);

// Connect nodes
UMaterialEditingLibrary::ConnectMaterialExpressions(
    FromExpr, "RGB",
    ToExpr, "A"
);

// Connect to material property
UMaterialEditingLibrary::ConnectMaterialProperty(
    Expr, "RGB", 
    EMaterialProperty::MP_BaseColor
);

// Recompile
UMaterialEditingLibrary::RecompileMaterial(Material);
```

### Expression Types
- `UMaterialExpressionTextureSample`
- `UMaterialExpressionScalarParameter`
- `UMaterialExpressionVectorParameter`
- `UMaterialExpressionAdd`, `Multiply`, `Lerp`, etc.
- `UMaterialExpressionStaticSwitchParameter`

---

## Phase 9: Texture Generation & Processing

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/Engine/Texture2D.h` | 2D textures |
| `Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h` | Render targets |
| `Runtime/Engine/Classes/Engine/CanvasRenderTarget2D.h` | Canvas rendering |
| `Runtime/Engine/Public/ImageUtils.h` | Image utilities |

### Texture Creation
```cpp
// Create transient texture
UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
Texture->UpdateResource();

// Lock and write pixel data
FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
FMemory::Memcpy(Data, PixelData, DataSize);
Mip.BulkData.Unlock();
Texture->UpdateResource();
```

### Render Target Operations
```cpp
UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
RenderTarget->InitAutoFormat(Width, Height);
RenderTarget->UpdateResourceImmediate();

// Read pixels
TArray<FColor> Pixels;
FRenderTarget* RT = RenderTarget->GameThread_GetRenderTargetResource();
RT->ReadPixels(Pixels);
```

### Texture Settings
```cpp
Texture->CompressionSettings = TC_Default;  // TC_Normalmap, TC_Masks, etc.
Texture->LODGroup = TEXTUREGROUP_World;
Texture->SRGB = true;
Texture->Filter = TF_Bilinear;
Texture->AddressX = TA_Wrap;
Texture->AddressY = TA_Wrap;
```

---

## Phase 10: Complete Animation System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/Animation/AnimSequence.h` | Animation sequences |
| `Runtime/Engine/Classes/Animation/AnimMontage.h` | Montages |
| `Runtime/Engine/Classes/Animation/BlendSpace.h` | Blend spaces |
| `Runtime/Engine/Classes/Animation/AnimBlueprint.h` | Anim blueprints |
| `Plugins/Animation/ControlRig/Source/ControlRig/Public/ControlRig.h` | Control Rig |
| `Plugins/Animation/IKRig/Source/IKRig/Public/IKRig.h` | IK Rig |
| `Plugins/Animation/IKRig/Source/IKRig/Public/Retargeter/IKRetargeter.h` | Retargeting |

### Animation Sequence Keyframes
```cpp
UAnimSequence* AnimSeq;
IAnimationDataController& Controller = AnimSeq->GetController();

// Add bone track
Controller.AddBoneCurve(BoneName);

// Set keyframe
FTransform Transform(Rotation, Location, Scale);
Controller.SetBoneTrackKeys(BoneName, {Location}, {Rotation}, {Scale});

// Add notify
FAnimNotifyEvent& Notify = AnimSeq->Notifies.AddDefaulted_GetRef();
Notify.NotifyName = NotifyName;
Notify.SetTime(TimeInSeconds);
```

### Blend Space
```cpp
UBlendSpace* BlendSpace;

// Add sample
FBlendSample Sample;
Sample.Animation = AnimSequence;
Sample.SampleValue = FVector(X, Y, 0);
BlendSpace->AddSample(Sample);

// Configure axes
BlendSpace->GetBlendParameter(0).DisplayName = "Speed";
BlendSpace->GetBlendParameter(0).Min = 0.f;
BlendSpace->GetBlendParameter(0).Max = 600.f;
```

### Control Rig
```cpp
UControlRig* ControlRig;

// Add control
FRigControlElement* Control = ControlRig->FindControl(ControlName);
Control->Settings.ControlType = ERigControlType::Transform;

// Set control value
ControlRig->SetControlValue(ControlName, FRigControlValue::Make<FTransform>(Transform));
```

### IK Retargeting
```cpp
UIKRetargeter* Retargeter;

// Set source/target IK rigs
Retargeter->SetSourceIKRig(SourceIKRig);
Retargeter->SetTargetIKRig(TargetIKRig);

// Map chains
Retargeter->SetChainMapping(SourceChainName, TargetChainName);
```

---

## Phase 11: Complete Audio System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/Sound/SoundCue.h` | Sound cue graph |
| `Runtime/Engine/Classes/Sound/SoundNode*.h` | Sound nodes (Mixer, Random, etc.) |
| `Runtime/Engine/Classes/Sound/SoundAttenuation.h` | Attenuation settings |
| `Runtime/Engine/Classes/Sound/SoundConcurrency.h` | Concurrency settings |
| `Runtime/Engine/Classes/Sound/SoundMix.h` | Sound mix presets |
| `Runtime/Engine/Classes/Sound/SoundSubmix.h` | Submix routing |
| `Runtime/Engine/Classes/Sound/ReverbEffect.h` | Reverb effects |
| `Runtime/Engine/Classes/Sound/AudioVolume.h` | Audio volumes |
| `Plugins/Runtime/Metasound/Source/MetasoundEngine/Public/*.h` | MetaSounds |

### Sound Cue Creation
```cpp
USoundCue* SoundCue = NewObject<USoundCue>();

// Add nodes
USoundNodeWavePlayer* WavePlayer = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();
WavePlayer->SetSoundWave(SoundWave);

USoundNodeRandom* RandomNode = SoundCue->ConstructSoundNode<USoundNodeRandom>();
RandomNode->ChildNodes.Add(WavePlayer);

USoundNodeMixer* MixerNode = SoundCue->ConstructSoundNode<USoundNodeMixer>();
MixerNode->InputVolume.Add(1.0f);

// Set first node (output)
SoundCue->FirstNode = MixerNode;
SoundCue->CompileSoundNodesFromGraphNodes();
```

### Sound Node Types
- `USoundNodeWavePlayer` - Play sound wave
- `USoundNodeRandom` - Random selection
- `USoundNodeMixer` - Mix multiple sources
- `USoundNodeAttenuation` - Distance attenuation
- `USoundNodeLooping` - Loop control
- `USoundNodeDelay` - Time delay
- `USoundNodeModulator` - Pitch/volume modulation
- `USoundNodeConcatenator` - Sequential playback

### Attenuation & Concurrency
```cpp
USoundAttenuation* Attenuation = NewObject<USoundAttenuation>();
Attenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
Attenuation->Attenuation.AttenuationShapeExtents = FVector(400.f);
Attenuation->Attenuation.FalloffDistance = 3600.f;

USoundConcurrency* Concurrency = NewObject<USoundConcurrency>();
Concurrency->Concurrency.MaxCount = 8;
Concurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopLowestPriority;
```

---

## Phase 12: Complete Niagara VFX System

### Plugin Location
`X:/Unreal_Engine/UE_5.6/Engine/Plugins/FX/Niagara/`

### Key Headers
| File | Purpose |
|------|---------|
| `Niagara/Public/NiagaraSystem.h` | System asset |
| `Niagara/Public/NiagaraEmitter.h` | Emitter definition |
| `Niagara/Public/NiagaraScript.h` | Module scripts |
| `Niagara/Public/NiagaraDataInterface*.h` | Data interfaces |
| `Niagara/Public/NiagaraComponent.h` | Component |
| `Niagara/Public/NiagaraParameterStore.h` | Parameters |

### System & Emitter Creation
```cpp
UNiagaraSystem* System = NewObject<UNiagaraSystem>();

// Add emitter
UNiagaraEmitter* Emitter = NewObject<UNiagaraEmitter>();
System->AddEmitterHandle(*Emitter, EmitterName);

// Configure emitter
Emitter->SimTarget = ENiagaraSimTarget::CPUSim;  // or GPUComputeSim
Emitter->bLocalSpace = false;
```

### Parameter Access
```cpp
UNiagaraComponent* NiagaraComp;

// Set parameters
NiagaraComp->SetVariableFloat(FName("SpawnRate"), 100.f);
NiagaraComp->SetVariableVec3(FName("Velocity"), FVector(0, 0, 100));
NiagaraComp->SetVariableLinearColor(FName("Color"), FLinearColor::Red);
NiagaraComp->SetVariableObject(FName("Mesh"), StaticMesh);
```

### Data Interfaces
- `UNiagaraDataInterfaceMesh` - Static mesh sampling
- `UNiagaraDataInterfaceSkeletalMesh` - Skeletal mesh sampling
- `UNiagaraDataInterfaceTexture` - Texture sampling
- `UNiagaraDataInterfaceAudioSpectrum` - Audio reactive
- `UNiagaraDataInterfaceVolumeTexture` - 3D volumes
- `UNiagaraDataInterfaceRenderTarget2D` - Render target output

---

## Phase 13: Gameplay Ability System (GAS)

### Plugin Location
`X:/Unreal_Engine/UE_5.6/Engine/Plugins/Runtime/GameplayAbilities/`

### Key Headers
| File | Purpose |
|------|---------|
| `AbilitySystemComponent.h` | Main interface for GAS |
| `Abilities/GameplayAbility.h` | Ability logic definition |
| `GameplayEffect.h` | Effect spec and modifiers |
| `AttributeSet.h` | Attribute management |
| `GameplayAbilitySpec.h` | Ability granting structs |

### Core API Patterns

**Granting Abilities:**
```cpp
FGameplayAbilitySpec Spec(AbilityClass, Level, InputID, SourceObject);
FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
```

**Applying Effects:**
```cpp
FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, Level, Context);
FActiveGameplayEffectHandle ActiveHandle = ASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
```

**Attribute Access:**
```cpp
float Value = ASC->GetNumericAttribute(Attribute);
ASC->SetNumericAttributeBase(Attribute, NewValue);
```

### Replication Modes
- `Minimal` - Tags/attributes only (AI)
- `Mixed` - Full for owners, minimal for others (players)
- `Full` - Full for all

---

## Phase 14: Character & Movement System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/GameFramework/Character.h` | Base character |
| `Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h` | Movement component |
| `Runtime/Engine/Classes/GameFramework/CharacterMovementReplication.h` | Network movement |
| `Runtime/Engine/Classes/GameFramework/RootMotionSource.h` | Root motion |

### Character Movement Configuration
```cpp
UCharacterMovementComponent* CMC = Character->GetCharacterMovement();

// Basic movement settings
CMC->MaxWalkSpeed = 600.f;
CMC->MaxWalkSpeedCrouched = 300.f;
CMC->JumpZVelocity = 420.f;
CMC->AirControl = 0.2f;
CMC->GravityScale = 1.0f;
CMC->BrakingDecelerationWalking = 2048.f;

// Ground settings
CMC->GroundFriction = 8.f;
CMC->MaxStepHeight = 45.f;
CMC->SetWalkableFloorAngle(45.f);

// Flying/Swimming
CMC->MaxFlySpeed = 600.f;
CMC->MaxSwimSpeed = 300.f;
CMC->BuoyancyCoefficient = 1.f;
```

### Custom Movement Modes
```cpp
// Add custom mode
CMC->SetMovementMode(MOVE_Custom, CustomModeIndex);

// In tick
void UMyMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
    // Custom physics logic
}
```

### Root Motion
```cpp
// Apply root motion source
FRootMotionSource_ConstantForce* RMS = new FRootMotionSource_ConstantForce();
RMS->InstanceName = "Jump";
RMS->AccumulateMode = ERootMotionAccumulateMode::Override;
RMS->Priority = 5;
RMS->Force = FVector(0, 0, 1000);
RMS->Duration = 0.5f;
CMC->ApplyRootMotionSource(RMS);
```

---

## Phase 15: Combat & Weapons System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/GameFramework/DamageType.h` | Damage type |
| `Runtime/Engine/Classes/GameFramework/ProjectileMovementComponent.h` | Projectiles |
| `Runtime/Engine/Classes/Engine/HitResult.h` | Hit detection |
| `Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h` | Trace functions |

### Damage System
```cpp
// Apply damage
UGameplayStatics::ApplyDamage(
    TargetActor,
    DamageAmount,
    InstigatorController,
    DamageCauser,
    UDamageType::StaticClass()
);

// Point damage (with hit info)
UGameplayStatics::ApplyPointDamage(
    TargetActor,
    DamageAmount,
    ShotDirection,
    HitResult,
    InstigatorController,
    DamageCauser,
    UDamageType::StaticClass()
);

// Radial damage
UGameplayStatics::ApplyRadialDamage(
    World,
    BaseDamage,
    Origin,
    DamageRadius,
    UDamageType::StaticClass(),
    IgnoreActors,
    DamageCauser,
    InstigatorController,
    bDoFullDamage
);
```

### Projectile Movement
```cpp
UProjectileMovementComponent* Projectile;

Projectile->InitialSpeed = 3000.f;
Projectile->MaxSpeed = 3000.f;
Projectile->bRotationFollowsVelocity = true;
Projectile->bShouldBounce = false;
Projectile->ProjectileGravityScale = 0.f;
Projectile->HomingTargetComponent = Target;
Projectile->HomingAccelerationMagnitude = 10000.f;
```

### Hit Detection (Traces)
```cpp
// Line trace
FHitResult Hit;
bool bHit = UKismetSystemLibrary::LineTraceSingle(
    World,
    Start,
    End,
    ETraceTypeQuery::TraceTypeQuery1,
    false,  // bTraceComplex
    IgnoreActors,
    EDrawDebugTrace::ForDuration,
    Hit,
    true   // bIgnoreSelf
);

// Sphere trace
UKismetSystemLibrary::SphereTraceSingle(...);

// Capsule trace
UKismetSystemLibrary::CapsuleTraceSingle(...);
```

---

## Phase 16: AI System

### Key Headers
| File | Purpose |
|------|---------|
| `BehaviorTree/BehaviorTree.h` | BT asset definition |
| `BehaviorTree/BTTaskNode.h` | Task base class |
| `BehaviorTree/BTDecorator.h` | Decorator base class |
| `BehaviorTree/BTService.h` | Service base class |
| `BehaviorTree/BlackboardComponent.h` | Blackboard runtime |
| `EnvironmentQuery/EnvQuery.h` | EQS query asset |
| `Perception/AIPerceptionComponent.h` | Perception component |

### BT Node Implementation Pattern
```cpp
// Task execution
virtual EBTNodeResult::Type ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, 
    uint8* NodeMemory
) override;

// Decorator condition
virtual bool CalculateRawConditionValue(
    UBehaviorTreeComponent& OwnerComp, 
    uint8* NodeMemory
) const override;
```

### Perception Config
```cpp
UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>();
SightConfig->SightRadius = 3000.f;
SightConfig->LoseSightRadius = 3500.f;
SightConfig->PeripheralVisionAngleDegrees = 90.f;
PerceptionComponent->ConfigureSense(*SightConfig);
```

### State Tree (UE5.3+)
Located at: `Plugins/Runtime/StateTree/`
- Uses struct-based tasks (`FStateTreeTaskBase`) for performance
- `FStateTreeExecutionContext` handles runtime

---

## Phase 17: Inventory & Items System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/Engine/DataTable.h` | Item data tables |
| `Runtime/Engine/Classes/Engine/DataAsset.h` | Item data assets |
| `Runtime/Engine/Classes/GameFramework/Actor.h` | Pickup actors |
| `Runtime/Engine/Classes/Components/ActorComponent.h` | Inventory component base |

### Item Definition Pattern (Data Asset)
```cpp
UCLASS()
class UItemDataAsset : public UPrimaryDataAsset
{
    UPROPERTY(EditDefaultsOnly)
    FName ItemID;
    
    UPROPERTY(EditDefaultsOnly)
    FText DisplayName;
    
    UPROPERTY(EditDefaultsOnly)
    UTexture2D* Icon;
    
    UPROPERTY(EditDefaultsOnly)
    int32 MaxStackSize = 1;
    
    // For Asset Manager
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("Item", GetFName());
    }
};
```

### Inventory Component Pattern
```cpp
UCLASS()
class UInventoryComponent : public UActorComponent
{
    UPROPERTY(Replicated)
    TArray<FInventorySlot> Slots;
    
    UFUNCTION(BlueprintCallable)
    bool AddItem(UItemDataAsset* Item, int32 Count);
    
    UFUNCTION(BlueprintCallable)
    bool RemoveItem(UItemDataAsset* Item, int32 Count);
    
    UFUNCTION(BlueprintCallable)
    int32 GetItemCount(UItemDataAsset* Item) const;
};
```

### Asset Manager (Loading Items)
```cpp
UAssetManager& AM = UAssetManager::Get();

// Get all items
TArray<FPrimaryAssetId> ItemIds;
AM.GetPrimaryAssetIdList(FPrimaryAssetType("Item"), ItemIds);

// Async load
AM.LoadPrimaryAssets(ItemIds, TArray<FName>(), 
    FStreamableDelegate::CreateLambda([](){ /* Loaded */ }));
```

---

## Phase 18: Interaction System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/Components/PrimitiveComponent.h` | Collision for interaction |
| `Runtime/Engine/Classes/GameFramework/Actor.h` | Interactable actors |
| `Runtime/Engine/Classes/Kismet/GameplayStatics.h` | Trace utilities |

### Interactable Interface Pattern
```cpp
UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableInterface : public UInterface
{
    GENERATED_BODY()
};

class IInteractableInterface
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool CanInteract(AActor* Interactor) const;
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void OnInteract(AActor* Interactor);
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FText GetInteractionPrompt() const;
};
```

### Interaction Component Pattern
```cpp
UCLASS()
class UInteractionComponent : public UActorComponent
{
    UPROPERTY(EditDefaultsOnly)
    float InteractionRange = 300.f;
    
    UPROPERTY(EditDefaultsOnly)
    TEnumAsByte<ECollisionChannel> TraceChannel;
    
    UFUNCTION(BlueprintCallable)
    AActor* GetFocusedInteractable();
    
    UFUNCTION(BlueprintCallable)
    bool TryInteract();
    
private:
    void PerformInteractionTrace();
};
```

### Trace-Based Detection
```cpp
FHitResult Hit;
FVector Start = Camera->GetComponentLocation();
FVector End = Start + Camera->GetForwardVector() * InteractionRange;

bool bHit = GetWorld()->LineTraceSingleByChannel(
    Hit, Start, End, ECC_Visibility
);

if (bHit && Hit.GetActor()->Implements<UInteractableInterface>())
{
    IInteractableInterface::Execute_OnInteract(Hit.GetActor(), this);
}
```

---

## Phase 19: UMG Widget System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/UMG/Public/Blueprint/UserWidget.h` | Base widget class |
| `Runtime/UMG/Public/Blueprint/WidgetTree.h` | Widget hierarchy |
| `Runtime/UMG/Public/Components/CanvasPanel.h` | Absolute layout |
| `Runtime/UMG/Public/Components/Button.h` | Button widget |
| `Runtime/UMG/Public/Components/TextBlock.h` | Text display |
| `Editor/UMGEditor/Public/WidgetBlueprint.h` | Widget BP asset |

### Widget Creation Pattern
```cpp
// Create widget from class
UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), WidgetClass);
Widget->AddToViewport();

// Create primitive widget programmatically
UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
UCanvasPanelSlot* Slot = MyCanvas->AddChildToCanvas(Button);
Slot->SetPosition(FVector2D(100.f, 100.f));
Slot->SetAnchors(FAnchors(0.5f, 0.5f));
```

### Animation API
```cpp
Widget->PlayAnimation(Animation);
Widget->PlayAnimationForward(Animation);
Widget->StopAnimation(Animation);
Widget->IsAnimationPlaying(Animation);
```

---

## Phase 20: Networking & Replication

### Key Headers
| File | Purpose |
|------|---------|
| `GameFramework/Actor.h` | Actor replication |
| `Engine/ActorChannel.h` | Actor channel |
| `Net/UnrealNetwork.h` | Replication macros |
| `ReplicationGraph/ReplicationGraph.h` | Scalable replication |

### Property Replication
```cpp
// Header
UPROPERTY(ReplicatedUsing = OnRep_Health)
float Health;

UFUNCTION()
void OnRep_Health();

// Implementation
void AMyActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMyActor, Health);
    DOREPLIFETIME_CONDITION(AMyActor, PrivateData, COND_OwnerOnly);
}
```

### RPC Declaration
```cpp
UFUNCTION(Server, Reliable, WithValidation)
void ServerDoAction();

UFUNCTION(Client, Unreliable)
void ClientNotify();

UFUNCTION(NetMulticast, Reliable)
void MulticastEffect();
```

---

## Phase 21: Game Framework

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/GameFramework/GameModeBase.h` | Game mode |
| `Runtime/Engine/Classes/GameFramework/GameStateBase.h` | Game state |
| `Runtime/Engine/Classes/GameFramework/PlayerController.h` | Player controller |
| `Runtime/Engine/Classes/GameFramework/PlayerState.h` | Player state |
| `Runtime/Engine/Classes/Engine/GameInstance.h` | Game instance |
| `Runtime/Engine/Classes/GameFramework/GameSession.h` | Session management |

### Game Mode Configuration
```cpp
AGameModeBase* GameMode;

// Class defaults
GameMode->DefaultPawnClass = AMyCharacter::StaticClass();
GameMode->PlayerControllerClass = AMyPlayerController::StaticClass();
GameMode->PlayerStateClass = AMyPlayerState::StaticClass();
GameMode->GameStateClass = AMyGameState::StaticClass();
GameMode->HUDClass = AMyHUD::StaticClass();

// Spawn control
GameMode->bStartPlayersAsSpectators = false;
GameMode->bPauseable = true;
```

### Game Instance Subsystems
```cpp
UGameInstance* GI = GetGameInstance();

// Get subsystem
UMySubsystem* Subsystem = GI->GetSubsystem<UMySubsystem>();

// Travel to level
GI->GetWorld()->ServerTravel("/Game/Maps/NewLevel");
```

---

## Phase 22: Sessions & Local Multiplayer

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/GameFramework/GameSession.h` | Session base |
| `Runtime/Engine/Classes/Engine/LocalPlayer.h` | Local player |
| `Runtime/Engine/Classes/GameFramework/PlayerController.h` | Player input |
| `Runtime/Engine/Classes/Engine/GameViewportClient.h` | Split screen |

### Local Multiplayer (Split Screen)
```cpp
UGameViewportClient* Viewport = GetWorld()->GetGameViewport();

// Enable split screen
Viewport->SetForceDisableSplitscreen(false);
Viewport->MaxSplitscreenPlayers = 4;

// Create local player
ULocalPlayer* NewPlayer = GEngine->CreateLocalPlayer(
    GetWorld(),
    ControllerId,
    ErrorMessage,
    true  // bSpawnPlayerController
);

// Remove local player
GEngine->RemoveLocalPlayer(GetWorld(), LocalPlayer);
```

### Input Mode Per Player
```cpp
APlayerController* PC = GetPlayerController(0);

// Game only
FInputModeGameOnly InputMode;
PC->SetInputMode(InputMode);

// UI only
FInputModeUIOnly UIMode;
UIMode.SetWidgetToFocus(Widget->TakeWidget());
PC->SetInputMode(UIMode);

// Game and UI
FInputModeGameAndUI MixedMode;
MixedMode.SetHideCursorDuringCapture(false);
PC->SetInputMode(MixedMode);
```

### Session Handling (LAN)
```cpp
AGameSession* Session = GetWorld()->GetAuthGameMode()->GameSession;

// Register player
Session->RegisterPlayer(NewPlayer, UniqueId, bWasFromInvite);

// Unregister player
Session->UnregisterPlayer(ExitingPlayer);

// Kick player
Session->KickPlayer(PlayerToKick, KickReason);
```

---

## Phase 23-24: World Structure & Volumes

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Public/WorldPartition/*.h` | World Partition |
| `Runtime/Engine/Classes/Engine/LevelStreaming.h` | Level streaming |
| `Runtime/Engine/Classes/Engine/BlockingVolume.h` | Blocking volume |
| `Runtime/Engine/Classes/Engine/TriggerVolume.h` | Trigger volume |
| `Runtime/Engine/Classes/Engine/PostProcessVolume.h` | Post process |
| `Runtime/Engine/Classes/Sound/AudioVolume.h` | Audio volume |
| `Runtime/Engine/Classes/GameFramework/PhysicsVolume.h` | Physics volume |

### World Partition
```cpp
UWorldPartition* WP = World->GetWorldPartition();

// Data layers
UDataLayerAsset* DataLayer;
WP->SetDataLayerRuntimeState(DataLayer, EDataLayerRuntimeState::Activated);

// Streaming sources
WP->AddStreamingSource(StreamingSource);
```

### Level Streaming
```cpp
// Load level
ULevelStreamingDynamic* StreamingLevel = ULevelStreamingDynamic::LoadLevelInstance(
    World,
    LevelName,
    Location,
    Rotation,
    bSucceeded
);

// Unload
StreamingLevel->SetShouldBeLoaded(false);
StreamingLevel->SetShouldBeVisible(false);
```

### Volume Types
```cpp
// Post Process Volume
APostProcessVolume* PPV;
PPV->Settings.bOverride_BloomIntensity = true;
PPV->Settings.BloomIntensity = 0.5f;
PPV->bUnbound = true;  // Affects entire world
PPV->BlendWeight = 1.0f;

// Audio Volume
AAudioVolume* AudioVol;
AudioVol->Settings.bApplyReverb = true;
AudioVol->Settings.ReverbEffect = ReverbAsset;
AudioVol->Settings.InteriorSettings.bIsWorldSettings = true;

// Physics Volume
APhysicsVolume* PhysVol;
PhysVol->FluidFriction = 0.3f;
PhysVol->bWaterVolume = true;
PhysVol->TerminalVelocity = 4000.f;
```

---

## Phase 25: Navigation System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/NavigationSystem/Public/NavigationSystem.h` | Nav system |
| `Runtime/NavigationSystem/Public/NavigationData.h` | Nav data base |
| `Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h` | Recast nav mesh |
| `Runtime/NavigationSystem/Public/NavAreas/NavArea.h` | Nav areas |
| `Runtime/AIModule/Classes/Navigation/NavLinkProxy.h` | Nav links |

### Navigation Mesh
```cpp
UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

// Rebuild navigation
NavSys->Build();

// Query path
FPathFindingQuery Query;
Query.StartLocation = Start;
Query.EndLocation = End;
FPathFindingResult Result = NavSys->FindPathSync(Query);
```

### Nav Modifiers
```cpp
// Nav area costs
ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
NavMesh->SetAreaCost(NavAreaClass, Cost);

// Nav link
ANavLinkProxy* Link;
Link->GetSmartLinkComp()->SetLinkData(Start, End, ENavLinkDirection::BothWays);
```

---

## Phase 26: Spline System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/Components/SplineComponent.h` | Spline component |
| `Runtime/Engine/Classes/Components/SplineMeshComponent.h` | Spline mesh |
| `Runtime/Landscape/Classes/LandscapeSplineActor.h` | Landscape splines |

### Spline Component
```cpp
USplineComponent* Spline;

// Add/modify points
Spline->AddSplinePoint(Location, ESplineCoordinateSpace::World);
Spline->SetSplinePointType(Index, ESplinePointType::Curve);
Spline->SetTangentAtSplinePoint(Index, Tangent, ESplineCoordinateSpace::World);

// Query spline
FVector Location = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
FRotator Rotation = Spline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
float Length = Spline->GetSplineLength();

// Spline settings
Spline->SetClosedLoop(true);
Spline->Duration = 1.0f;  // For timeline-based movement
```

### Spline Mesh Component
```cpp
USplineMeshComponent* SplineMesh;

// Set mesh and spline segment
SplineMesh->SetStaticMesh(Mesh);
SplineMesh->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent);
SplineMesh->SetForwardAxis(ESplineMeshAxis::X);

// Deformation
SplineMesh->SetStartScale(FVector2D(1.0f, 1.0f));
SplineMesh->SetEndScale(FVector2D(1.5f, 1.5f));
```

---

## Phase 27: PCG Framework

### Plugin Location
`X:/Unreal_Engine/UE_5.6/Engine/Plugins/PCG/`

### Key Headers
| File | Purpose |
|------|---------|
| `PCGGraph.h` | Graph definition |
| `PCGNode.h` | Node instance |
| `PCGSettings.h` | Node configuration |
| `PCGPoint.h` | FPCGPoint struct |
| `PCGSubsystem.h` | Execution scheduling |
| `Helpers/PCGPointHelpers.h` | Point operations |

### Core Data Flow
```
UPCGGraph → UPCGNode → UPCGSettings
                ↓
         FPCGPoint data
                ↓
    UPCGStaticMeshSpawner (output)
```

### Built-in Node Types
- **Samplers**: `UPCGSurfaceSamplerSettings`, `UPCGVolumeSamplerSettings`
- **Filters**: `UPCGDensityFilterSettings`, `UPCGFilterByAttributeSettings`
- **Spawners**: `UPCGStaticMeshSpawnerSettings`, `UPCGSpawnActorSettings`

---

## Phase 28: Landscape & Foliage

### Key Headers
| File | Purpose |
|------|---------|
| `Landscape/Classes/Landscape.h` | Main landscape actor |
| `Landscape/Classes/LandscapeProxy.h` | Base/streaming proxy |
| `Landscape/Classes/LandscapeComponent.h` | Grid component |
| `Landscape/Public/LandscapeEdit.h` | Editing APIs |
| `Foliage/Public/InstancedFoliageActor.h` | Foliage manager |
| `Foliage/Public/FoliageType.h` | Foliage configuration |

### Heightmap Import/Export
```cpp
ALandscapeProxy::Import(...);
ALandscapeProxy::LandscapeExportHeightmapToRenderTarget(...);
ALandscapeProxy::LandscapeImportHeightmapFromRenderTarget(...);
```

### Sculpting/Painting
```cpp
// Via FLandscapeEditDataInterface
FLandscapeEditDataInterface Interface(...);
Interface.SetHeightData(...);  // Sculpting
Interface.SetAlphaData(...);   // Layer painting
```

### Foliage Spawning
```cpp
AInstancedFoliageActor* FoliageActor = ...;
FoliageActor->AddInstances(FoliageType, Instances);
```

---

## Phase 29: Advanced Lighting & Rendering

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/Components/LightComponent.h` | Light base |
| `Runtime/Engine/Classes/Components/SkyLightComponent.h` | Sky light |
| `Runtime/Engine/Classes/Components/SkyAtmosphereComponent.h` | Sky atmosphere |
| `Runtime/Engine/Classes/Components/VolumetricCloudComponent.h` | Volumetric clouds |
| `Runtime/Engine/Classes/Components/ReflectionCaptureComponent.h` | Reflections |
| `Runtime/Engine/Classes/Engine/ExponentialHeightFog.h` | Height fog |
| `Runtime/Renderer/Public/PostProcess/*.h` | Post processing |

### Lumen Configuration
```cpp
// Project settings for Lumen GI
URendererSettings* Settings = GetMutableDefault<URendererSettings>();
Settings->DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
Settings->ReflectionMethod = EReflectionMethod::Lumen;

// Per-component settings
UPrimitiveComponent* Comp;
Comp->bVisibleInRayTracing = true;
Comp->bAffectDynamicIndirectLighting = true;
```

### Sky Atmosphere
```cpp
USkyAtmosphereComponent* SkyAtmo;
SkyAtmo->SetRayleighScattering(FLinearColor(0.175287f, 0.409607f, 1.0f));
SkyAtmo->SetMieScattering(FLinearColor(0.003996f, 0.003996f, 0.003996f));
SkyAtmo->SetAtmosphereHeight(60.f);  // km
SkyAtmo->SetSunDiskColorScale(FLinearColor::White);
```

### Volumetric Clouds
```cpp
UVolumetricCloudComponent* Clouds;
Clouds->SetLayerBottomAltitude(5.f);  // km
Clouds->SetLayerHeight(10.f);
Clouds->SetTracingStartMaxDistance(350.f);
Clouds->SetPlanetRadius(6360.f);
```

### Reflection Capture
```cpp
// Sphere reflection
USphereReflectionCaptureComponent* Sphere;
Sphere->SetInfluenceRadius(3000.f);
Sphere->Brightness = 1.0f;

// Box reflection (for interiors)
UBoxReflectionCaptureComponent* Box;
Box->SetBoxTransitionDistance(100.f);
```

---

## Phase 30: Sequencer & Cinematics

### Key Headers
| File | Purpose |
|------|---------|
| `LevelSequence/Public/LevelSequence.h` | Sequence asset |
| `MovieScene/Public/MovieScene.h` | Data container |
| `MovieScene/Public/MovieSceneTrack.h` | Track base |
| `MovieScene/Public/MovieSceneSection.h` | Section base |
| `MovieScene/Public/Channels/MovieSceneFloatChannel.h` | Keyframes |
| `MovieRenderPipelineCore/Public/MoviePipeline.h` | MRQ |
| `TakeRecorder/Public/Recorder/TakeRecorderSubsystem.h` | Recording |

### Track/Section Creation
```cpp
// Add track
UMovieSceneTrack* Track = MovieScene->AddTrack(UMovieSceneAudioTrack::StaticClass());

// Add section
UMovieSceneSection* Section = Track->CreateNewSection();
Track->AddSection(*Section);
Section->SetRange(TRange<FFrameNumber>::Inclusive(Start, End));
```

### Keyframe Manipulation
```cpp
FMovieSceneChannelProxy& Proxy = Section->GetChannelProxy();
FMovieSceneFloatChannel* Channel = Proxy.GetChannel<FMovieSceneFloatChannel>(0);
Channel->AddCubicKey(FrameNumber, Value, RCTM_Auto);
```

### Movie Render Queue
```cpp
UMoviePipelineQueueEngineSubsystem* Subsystem = ...;
UMoviePipelineExecutorJob* Job = Subsystem->AllocateJob(Sequence);
// Configure settings
Subsystem->RenderJob(Job);
```

---

## Phase 31: Data & Persistence

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/GameFramework/SaveGame.h` | Save game base |
| `Runtime/Engine/Classes/Engine/DataTable.h` | Data tables |
| `Runtime/Engine/Classes/Engine/CompositeDataTable.h` | Composite tables |
| `Runtime/Engine/Classes/Curves/CurveFloat.h` | Float curves |
| `Runtime/Engine/Classes/Curves/CurveVector.h` | Vector curves |
| `Runtime/Engine/Classes/Engine/DataAsset.h` | Data assets |

### Save Game System
```cpp
// Create and save
UMySaveGame* SaveGame = Cast<UMySaveGame>(
    UGameplayStatics::CreateSaveGameObject(UMySaveGame::StaticClass())
);
SaveGame->PlayerLocation = Player->GetActorLocation();
SaveGame->Health = HealthComponent->GetHealth();
UGameplayStatics::SaveGameToSlot(SaveGame, "Slot1", 0);

// Load
UMySaveGame* LoadedGame = Cast<UMySaveGame>(
    UGameplayStatics::LoadGameFromSlot("Slot1", 0)
);

// Async save/load
UGameplayStatics::AsyncSaveGameToSlot(SaveGame, "Slot1", 0, OnComplete);
UGameplayStatics::AsyncLoadGameFromSlot("Slot1", 0, OnComplete);
```

### Data Tables
```cpp
// Access row
UDataTable* Table;
FMyRowStruct* Row = Table->FindRow<FMyRowStruct>(RowName, ContextString);

// Get all rows
TArray<FMyRowStruct*> AllRows;
Table->GetAllRows(ContextString, AllRows);

// Add row at runtime
FMyRowStruct NewRow;
NewRow.Value = 100;
Table->AddRow(RowName, NewRow);
```

### Curves
```cpp
UCurveFloat* Curve;

// Add key
FKeyHandle Handle = Curve->FloatCurve.AddKey(Time, Value);

// Set interpolation
Curve->FloatCurve.SetKeyInterpMode(Handle, RCIM_Cubic);
Curve->FloatCurve.SetKeyTangentMode(Handle, RCTM_Auto);

// Evaluate
float Result = Curve->GetFloatValue(Time);
```

---

## Phase 32: Build & Deployment

### Key Headers
| File | Purpose |
|------|---------|
| `Programs/UnrealBuildTool/` | UBT source |
| `Programs/AutomationTool/` | UAT source |
| `Developer/DesktopPlatform/Public/DesktopPlatformModule.h` | Platform utilities |

### Build Automation (via Commandlet)
```cpp
// Run from command line
UnrealEditor-Cmd.exe MyProject.uproject -run=Cook -TargetPlatform=Windows

// Package project
RunUAT.bat BuildCookRun -project=MyProject.uproject -platform=Win64 -clientconfig=Shipping -cook -stage -pak -archive
```

### Project Settings Access
```cpp
// Platform settings
UAndroidRuntimeSettings* AndroidSettings = GetMutableDefault<UAndroidRuntimeSettings>();
UWindowsTargetSettings* WindowsSettings = GetMutableDefault<UWindowsTargetSettings>();

// Packaging settings
UProjectPackagingSettings* PackSettings = GetMutableDefault<UProjectPackagingSettings>();
PackSettings->BuildConfiguration = EProjectPackagingBuildConfigurations::PPBC_Shipping;
PackSettings->bCompressed = true;
```

### Build Configuration
```cpp
// Via command line or DefaultGame.ini
[/Script/UnrealEd.ProjectPackagingSettings]
BuildConfiguration=PPBC_Shipping
bCompressed=true
BlueprintNativizationMethod=Disabled
```

---

## Phase 33: Testing & Quality

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Core/Public/Misc/AutomationTest.h` | Test framework base |
| `Developer/AutomationController/Public/IAutomationControllerManager.h` | Test controller |
| `Developer/FunctionalTesting/Classes/FunctionalTest.h` | Functional tests |

### Automation Test Definition
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMyTest, "Project.Category.TestName", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMyTest::RunTest(const FString& Parameters)
{
    // Arrange
    int32 A = 5;
    int32 B = 3;
    
    // Act
    int32 Result = A + B;
    
    // Assert
    TestEqual(TEXT("Addition works"), Result, 8);
    
    return true;
}
```

### Complex/Latent Tests
```cpp
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMyComplexTest, "Project.Complex.Test",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

void FMyComplexTest::GetTests(TArray<FString>& OutBeautifiedNames, 
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add("TestCase1");
    OutTestCommands.Add("Param1");
    OutBeautifiedNames.Add("TestCase2");
    OutTestCommands.Add("Param2");
}

bool FMyComplexTest::RunTest(const FString& Parameters)
{
    // Parameters contains the test command
    return true;
}
```

### Functional Tests (In-World)
```cpp
UCLASS()
class AFunctionalTest_MyFeature : public AFunctionalTest
{
    virtual void PrepareTest() override;
    virtual void StartTest() override;
    
    // Call when done
    void OnTestComplete()
    {
        FinishTest(EFunctionalTestResult::Succeeded, "Test passed");
    }
};
```

### Running Tests
```bash
# Command line
UnrealEditor-Cmd.exe MyProject.uproject -ExecCmds="Automation RunTests Project"

# Gauntlet (CI)
RunUAT.bat RunUnreal -project=MyProject -test=Project.Category -platform=Win64
```

---

## Phase 34: Editor Utilities

### Key Headers
| File | Purpose |
|------|---------|
| `Editor/Blutility/Classes/EditorUtilityWidget.h` | Editor utility widget |
| `Editor/Blutility/Classes/EditorUtilityBlueprint.h` | Editor utility BP |
| `Editor/Blutility/Classes/AssetActionUtility.h` | Asset actions |
| `Editor/Blutility/Classes/ActorActionUtility.h` | Actor actions |
| `Editor/Blutility/Classes/EditorUtilityTask.h` | Background tasks |

### Editor Utility Widget
```cpp
UEditorUtilityWidget* Widget;

// Run from C++
UEditorUtilitySubsystem* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
Subsystem->SpawnAndRegisterTab(WidgetBlueprint);
```

### Asset/Actor Actions
```cpp
// Asset action (right-click menu)
UCLASS()
class UMyAssetAction : public UAssetActionUtility
{
    UFUNCTION(CallInEditor, Category = "MyTools")
    void ProcessSelectedAssets();
};

// Actor action (right-click on actors)
UCLASS()
class UMyActorAction : public UActorActionUtility
{
    UFUNCTION(CallInEditor, Category = "MyTools")
    void ProcessSelectedActors();
};
```

### Editor Utility Task (Background)
```cpp
UCLASS()
class UMyEditorTask : public UEditorUtilityTask
{
    virtual void BeginExecution() override;
    virtual void EndExecution(EEditorScriptExecutionResult Result) override;
    
    // Use FinishExecutingTask() when done
};
```

---

## Phase 35: Additional Gameplay Systems

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Classes/Components/TimelineComponent.h` | Timeline component |
| `Runtime/Engine/Classes/Engine/DirectionalLightComponent.h` | Sun for time of day |
| `Runtime/Engine/Public/SubtitleManager.h` | Subtitle display |
| `Runtime/Engine/Classes/Kismet/GameplayStatics.h` | General gameplay |

### Quest System Pattern (Custom)
```cpp
USTRUCT(BlueprintType)
struct FQuestObjective
{
    UPROPERTY(EditDefaultsOnly)
    FName ObjectiveID;
    
    UPROPERTY(EditDefaultsOnly)
    FText Description;
    
    UPROPERTY(BlueprintReadWrite)
    int32 CurrentProgress = 0;
    
    UPROPERTY(EditDefaultsOnly)
    int32 RequiredProgress = 1;
    
    bool IsComplete() const { return CurrentProgress >= RequiredProgress; }
};

UCLASS()
class UQuestDataAsset : public UPrimaryDataAsset
{
    UPROPERTY(EditDefaultsOnly)
    FName QuestID;
    
    UPROPERTY(EditDefaultsOnly)
    TArray<FQuestObjective> Objectives;
    
    UPROPERTY(EditDefaultsOnly)
    TArray<UQuestDataAsset*> Prerequisites;
};
```

### Time of Day System
```cpp
// Rotate directional light for sun
ADirectionalLight* Sun;
FRotator SunRotation = FRotator(-45.f + (TimeOfDay * 180.f / 24.f), 0.f, 0.f);
Sun->SetActorRotation(SunRotation);

// Update sky atmosphere
USkyAtmosphereComponent* SkyAtmo;
SkyAtmo->SetAtmosphereSunLight(Sun);
SkyAtmo->MarkRenderStateDirty();

// Update sky light (recapture)
USkyLightComponent* SkyLight;
SkyLight->RecaptureSky();
```

### Dialogue System Pattern
```cpp
USTRUCT(BlueprintType)
struct FDialogueNode
{
    UPROPERTY(EditDefaultsOnly)
    FText SpeakerName;
    
    UPROPERTY(EditDefaultsOnly)
    FText DialogueText;
    
    UPROPERTY(EditDefaultsOnly)
    USoundWave* VoiceOver;
    
    UPROPERTY(EditDefaultsOnly)
    TArray<FDialogueChoice> Choices;
    
    UPROPERTY(EditDefaultsOnly)
    FName NextNodeID;  // If no choices
};
```

### Subtitle Manager
```cpp
FSubtitleManager* SubMgr = FSubtitleManager::GetSubtitleManager();

// Display subtitle
FSubtitle Sub;
Sub.Priority = 0;
Sub.Text = LOCTEXT("Subtitle", "Hello, world!");
Sub.StartTime = 0.f;
Sub.EndTime = 3.f;
SubMgr->DisplaySubtitle(this, {Sub}, Duration);
```

---

## Phase 36-39: Third-Party Plugin Integration

### Quixel Bridge (Megascans)
**Location**: `Plugins/Bridge/`
```cpp
// Assets imported via Bridge app automatically
// Access imported asset data:
UMSAssetImportData* ImportData = Cast<UMSAssetImportData>(
    Asset->GetAssetImportData()
);
```

### MetaHuman
**Location**: `Plugins/Runtime/MetaHumans/`
```cpp
// MetaHuman Blueprint components
UMetaHumanComponent* MetaHuman;
MetaHuman->SetLODLevel(0);  // 0 = highest quality

// DNA integration for procedural heads
// Requires RigLogic plugin
```

### Audio Middleware (Wwise/FMOD Integration Pattern)
```cpp
// Wwise (separate plugin)
#include "AkGameplayStatics.h"
UAkGameplayStatics::PostEvent(SoundEvent, Actor);
UAkGameplayStatics::SetRTPCValue(RTPCName, Value, Actor);
UAkGameplayStatics::SetState(StateGroup, State);
UAkGameplayStatics::SetSwitch(SwitchGroup, SwitchState, Actor);

// FMOD (separate plugin)
#include "FMODBlueprintStatics.h"
UFMODBlueprintStatics::PlayEvent2D(Event, bAutoPlay);
UFMODBlueprintStatics::SetGlobalParameterByName(Name, Value);
```

### Live Link (MoCap/Animation)
**Location**: `Plugins/Animation/LiveLink/`
```cpp
ILiveLinkClient* Client = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(
    ILiveLinkClient::ModularFeatureName
);

// Available sources
TArray<FGuid> SourceGuids = Client->GetSources();

// Evaluate frame
FLiveLinkSubjectFrameData FrameData;
Client->EvaluateFrame_AnyThread(SubjectName, ULiveLinkAnimationRole::StaticClass(), FrameData);

// Get skeleton data
const FLiveLinkSkeletonStaticData* StaticData = FrameData.StaticData.Cast<FLiveLinkSkeletonStaticData>();
const FLiveLinkAnimationFrameData* FrameAnimData = FrameData.FrameData.Cast<FLiveLinkAnimationFrameData>();
```

---

## Phase 40: Virtual Production

### Key Locations
- nDisplay: `Plugins/Runtime/nDisplay/`
- Live Link: `Plugins/Animation/LiveLink/`
- DMX: `Plugins/VirtualProduction/DMX/`
- Stage Monitoring: `Plugins/VirtualProduction/StageMonitoring/`

### Key Headers
| File | Purpose |
|------|---------|
| `nDisplay/Source/DisplayCluster/Public/DisplayClusterRootActor.h` | nDisplay root |
| `nDisplay/Source/DisplayClusterConfiguration/Public/DisplayClusterConfigurationTypes_ICVFX.h` | ICVFX config |
| `LiveLink/Source/LiveLink/Public/LiveLinkClient.h` | Live Link client |
| `DMXRuntime/Public/Library/DMXLibrary.h` | DMX library |
| `StageMonitor/Public/IStageMonitor.h` | Stage monitoring |

### nDisplay
```cpp
ADisplayClusterRootActor* RootActor;

// Get managers via module
IDisplayCluster& Cluster = IDisplayCluster::Get();
IDisplayClusterRenderManager* RenderMgr = Cluster.GetRenderMgr();
IDisplayClusterClusterManager* ClusterMgr = Cluster.GetClusterMgr();

// Configuration data
UDisplayClusterConfigurationData* Config = RootActor->GetConfigData();
```

### ICVFX (In-Camera VFX)
```cpp
// Access ICVFX camera settings
FDisplayClusterConfigurationICVFX_CameraSettings& CamSettings = 
    Config->StageSettings.DefaultCamera;

CamSettings.bEnable = true;
CamSettings.RenderSettings.GenerateMips.bAutoGenerateMips = true;

// Light cards
Config->StageSettings.Lightcard.bEnable = true;
```

### Live Link
```cpp
ILiveLinkClient* Client = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(
    ILiveLinkClient::ModularFeatureName
);

// Get subject data
FLiveLinkSubjectFrameData FrameData;
Client->EvaluateFrame_AnyThread(SubjectName, Role, FrameData);

// Push data from custom source
Client->PushSubjectFrameData_AnyThread(SubjectKey, MoveTemp(FrameData));
```

### DMX
```cpp
UDMXLibrary* Library;

// Get fixture patches
TArray<UDMXEntityFixturePatch*> Patches = Library->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

// Send DMX
for (UDMXEntityFixturePatch* Patch : Patches)
{
    TMap<int32, uint8> ChannelValues;
    ChannelValues.Add(1, 255);  // Channel 1 = 255
    Patch->SendDMX(ChannelValues);
}
```

---

## Phase 41: XR (VR/AR/MR)

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/HeadMountedDisplay/Public/IXRTrackingSystem.h` | XR tracking |
| `Runtime/HeadMountedDisplay/Public/MotionControllerComponent.h` | Motion controllers |
| `Plugins/Runtime/OpenXR/Source/OpenXRHMD/Public/*.h` | OpenXR |
| `Runtime/HeadMountedDisplay/Public/XRMotionControllerBase.h` | Controller base |

### XR Tracking
```cpp
IXRTrackingSystem* XRSystem = GEngine->XRSystem.Get();

// Get HMD position/rotation
FQuat Orientation;
FVector Position;
XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, Orientation, Position);

// Check XR enabled
bool bIsXR = XRSystem && XRSystem->IsHeadTrackingAllowed();
```

### Motion Controllers
```cpp
UMotionControllerComponent* LeftHand;
LeftHand->MotionSource = FXRMotionControllerBase::LeftHandSourceId;
LeftHand->bDisplayDeviceModel = true;

// Get grip/aim pose
FVector GripLocation = LeftHand->GetComponentLocation();
FRotator GripRotation = LeftHand->GetComponentRotation();
```

### OpenXR Actions
```cpp
// Via Enhanced Input (recommended)
UInputAction* GrabAction;
// Bind to XR controller input in Input Mapping Context
```

---

## Phase 43: Online Services

### Plugin Location
`Plugins/Online/OnlineSubsystem*/`

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Online/OnlineSubsystem/Public/OnlineSubsystem.h` | OSS base |
| `Runtime/Online/OnlineSubsystem/Public/Interfaces/*.h` | All interfaces |
| `Plugins/Online/OnlineSubsystemEOS/Public/*.h` | Epic Online Services |

### Online Subsystem Access
```cpp
IOnlineSubsystem* OSS = IOnlineSubsystem::Get();

// Session interface
IOnlineSessionPtr Sessions = OSS->GetSessionInterface();

// Friends interface
IOnlineFriendsPtr Friends = OSS->GetFriendsInterface();

// Leaderboards
IOnlineLeaderboardsPtr Leaderboards = OSS->GetLeaderboardsInterface();

// Achievements
IOnlineAchievementsPtr Achievements = OSS->GetAchievementsInterface();
```

### Session Management
```cpp
IOnlineSessionPtr Sessions = OSS->GetSessionInterface();

// Create session
FOnlineSessionSettings Settings;
Settings.bIsLANMatch = false;
Settings.NumPublicConnections = 4;
Settings.bShouldAdvertise = true;
Sessions->CreateSession(LocalPlayerNum, SessionName, Settings);

// Find sessions
TSharedRef<FOnlineSessionSearch> Search = MakeShared<FOnlineSessionSearch>();
Search->MaxSearchResults = 20;
Sessions->FindSessions(LocalPlayerNum, Search);

// Join session
Sessions->JoinSession(LocalPlayerNum, SessionName, SearchResult);
```

---

## Phase 42: AI & NPC Plugins

### Mass Entity/AI (Crowd Simulation)
**Location**: `Plugins/Runtime/MassGameplay/`

### Key Concepts
| Concept | Description |
|---------|-------------|
| `FMassFragment` | Data component (struct) |
| `UMassProcessor` | System that operates on fragments |
| `FMassEntityHandle` | Lightweight entity reference |
| `UMassEntityConfigAsset` | Entity template |

### Mass Entity Creation
```cpp
UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();

// Create archetype
FMassArchetypeHandle Archetype = EntitySubsystem->CreateArchetype(
    {FTransformFragment::StaticStruct(), FAgentFragment::StaticStruct()},
    FMassArchetypeSharedFragmentValues()
);

// Spawn entities
TArray<FMassEntityHandle> Entities;
EntitySubsystem->BatchCreateEntities(Archetype, 1000, Entities);

// Set fragment data
for (FMassEntityHandle& Entity : Entities)
{
    FTransformFragment& Transform = EntitySubsystem->GetFragmentDataChecked<FTransformFragment>(Entity);
    Transform.SetTransform(FTransform::Identity);
}
```

### Mass Processor Pattern
```cpp
UCLASS()
class UMyMassProcessor : public UMassProcessor
{
protected:
    virtual void ConfigureQueries() override
    {
        EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
        EntityQuery.AddRequirement<FVelocityFragment>(EMassFragmentAccess::ReadOnly);
    }
    
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override
    {
        EntityQuery.ForEachEntityChunk(EntityManager, Context, 
            [](FMassExecutionContext& Context)
        {
            // Process entities in chunk
        });
    }
};
```

---

## Phase 44: Streaming & Distribution

### Pixel Streaming
**Location**: `Plugins/Media/PixelStreaming/`
```cpp
// Enable via project settings or command line
// -PixelStreamingIP=0.0.0.0 -PixelStreamingPort=8888

// Blueprint functions
UPixelStreamingBlueprints::SendPixelStreamingResponse(Response);
UPixelStreamingBlueprints::GetPlayerCount();

// Input component
UPixelStreamingInputComponent* PSInput;
PSInput->SetMouseModeEnabled(true);
PSInput->SetTouchEventsEnabled(true);
```

### Remote Control API
**Location**: `Plugins/VirtualProduction/RemoteControl/`
```cpp
// Expose property
URemoteControlPreset* Preset;
Preset->ExposeProperty(Object, PropertyName, DisplayName);

// Expose function
Preset->ExposeFunction(Object, FunctionName, DisplayName);

// Web server runs on port 30010 by default
// REST API: /remote/object/{ObjectPath}/property/{PropertyName}
```

---

## Phase 45: Utility Plugins

### Python Scripting
**Location**: `Plugins/Experimental/PythonScriptPlugin/`
```cpp
// Execute Python from C++
IPythonScriptPlugin& Python = FModuleManager::LoadModuleChecked<IPythonScriptPlugin>("PythonScriptPlugin");
Python.ExecPythonCommand(TEXT("print('Hello from Python')"));

// Execute script file
Python.ExecPythonScript(TEXT("/Game/Scripts/my_script.py"));
```

### Python in Editor (unreal module)
```python
import unreal

# Get selected actors
selected = unreal.EditorLevelLibrary.get_selected_level_actors()

# Spawn actor
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.StaticMeshActor, 
    unreal.Vector(0, 0, 0)
)

# Modify properties
actor.set_actor_label("MyActor")
actor.static_mesh_component.set_static_mesh(mesh)

# Asset operations
unreal.EditorAssetLibrary.duplicate_asset("/Game/Source", "/Game/Dest")
```

---

## Phase 47: Accessibility System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Engine/Public/SubtitleManager.h` | Subtitle display |
| `Runtime/Slate/Public/Widgets/Accessibility/*.h` | Accessible widgets |
| `Runtime/ApplicationCore/Public/GenericPlatform/GenericPlatformApplicationMisc.h` | Screen reader |

### Colorblind Filters
```cpp
// Project settings or console variables
r.Color.Colorblind.Mode = 1  // 0=Off, 1=Deuteranope, 2=Protanope, 3=Tritanope
r.Color.Colorblind.Severity = 1.0  // 0.0 to 1.0
```

### Subtitle System
```cpp
FSubtitleManager* SubMgr = FSubtitleManager::GetSubtitleManager();

// Configure subtitle options
SubMgr->SetSubtitleBackgroundBrush(BackgroundBrush);
SubMgr->SetSubtitleTextSize(24.f);

// Display subtitles
TArray<FSubtitle> Subtitles;
FSubtitle Sub;
Sub.Text = LOCTEXT("Line1", "Hello, player!");
Sub.StartTime = 0.f;
Sub.EndTime = 3.f;
Subtitles.Add(Sub);

SubMgr->DisplaySubtitle(OwningActor, Subtitles, Duration);
```

### Accessibility Settings Pattern
```cpp
UCLASS(Config=Game)
class UAccessibilitySettings : public UDeveloperSettings
{
    UPROPERTY(Config, EditAnywhere)
    bool bEnableSubtitles = true;
    
    UPROPERTY(Config, EditAnywhere)
    float SubtitleScale = 1.0f;
    
    UPROPERTY(Config, EditAnywhere)
    bool bHighContrastMode = false;
    
    UPROPERTY(Config, EditAnywhere)
    EColorBlindMode ColorBlindMode = EColorBlindMode::Off;
    
    UPROPERTY(Config, EditAnywhere)
    bool bReducedMotion = false;
};
```

### Input Remapping
```cpp
// Enhanced Input supports runtime rebinding
UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC);

// Get all mappings
TArray<FEnhancedActionKeyMapping> Mappings;
Subsystem->GetAllPlayerMappableActionKeyMappings(Mappings);

// Remap action
Subsystem->AddPlayerMappedKey(ActionName, NewKey);
```

---

## Phase 48: Modding & UGC System

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/PakFile/Public/IPlatformFilePak.h` | Pak file interface |
| `Runtime/Core/Public/Misc/Paths.h` | Content paths |
| `Runtime/AssetRegistry/Public/AssetRegistry/IAssetRegistry.h` | Asset discovery |

### Pak File Loading
```cpp
// Mount additional pak file
FPakPlatformFile* PakPlatform = static_cast<FPakPlatformFile*>(
    FPlatformFileManager::Get().GetPlatformFile()
);

bool bMounted = PakPlatform->Mount(
    *PakFilePath,
    0,  // Priority
    *MountPoint,
    false  // bLoadIndex
);

// After mounting, assets are accessible via /Game/Mods/...
```

### Dynamic Asset Loading
```cpp
// Scan for new assets
IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
AssetRegistry.ScanPathsSynchronous({"/Game/Mods/"}, true);

// Find assets in mod folder
TArray<FAssetData> ModAssets;
AssetRegistry.GetAssetsByPath(FName("/Game/Mods"), ModAssets, true);

// Load asset
UObject* Asset = ModAssets[0].GetAsset();
```

### Mod Manifest Pattern
```cpp
USTRUCT(BlueprintType)
struct FModManifest
{
    UPROPERTY()
    FString ModName;
    
    UPROPERTY()
    FString Version;
    
    UPROPERTY()
    FString Author;
    
    UPROPERTY()
    TArray<FString> Dependencies;
    
    UPROPERTY()
    FString EntryPointClass;  // Blueprint class to spawn
};

// Load manifest from JSON in pak
FString ManifestPath = FPaths::Combine(ModMountPoint, TEXT("manifest.json"));
FString JsonString;
FFileHelper::LoadFileToString(JsonString, *ManifestPath);
FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &Manifest);
```

### Steam Workshop Integration Pattern
```cpp
// Via Online Subsystem Steam
IOnlineSubsystem* OSS = IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
if (OSS)
{
    // Workshop item operations via SteamAPI
    // Requires Steamworks SDK integration
}
```

---

## Phase 46: Chaos Physics (Destruction, Vehicles, Cloth)

### Key Headers
| File | Purpose |
|------|---------|
| `Runtime/Experimental/Chaos/Public/GeometryCollection/GeometryCollection.h` | Core destruction data structure |
| `Runtime/Experimental/GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h` | Actor wrapper for destruction |
| `Runtime/Experimental/GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h` | Simulation and damage handling |
| `Plugins/Experimental/Fracture/Source/FractureEngine/Public/FractureEngineFracturing.h` | Fracturing operations API |
| `Plugins/Experimental/ChaosVehiclesPlugin/Source/ChaosVehicles/Public/ChaosWheeledVehicleMovementComponent.h` | Vehicle movement |
| `Runtime/ClothingSystemRuntimeCommon/Public/ClothingAsset.h` | Cloth asset definition |
| `Runtime/Experimental/FieldSystem/Source/FieldSystemEngine/Public/Field/FieldSystemComponent.h` | Physics fields/forces |

### Geometry Collection & Destruction
```cpp
// Core components
UGeometryCollection* Collection;      // Rest state and hierarchy
UGeometryCollectionComponent* Comp;   // Manages simulation

// Triggering destruction via Strain
Comp->ApplyExternalStrain(ItemIndex, Location, Radius, PropagationDepth, PropagationFactor, Strain);

// Triggering via Physics Fields
Comp->ApplyPhysicsField(bEnabled, TargetType, MetaData, FieldNode);

// Anchoring pieces (static foundation)
Comp->SetAnchoredByIndex(Index, bAnchored);
Comp->SetAnchoredByBox(Box, bAnchored);
```

### Fracturing API (FFractureEngineFracturing)
```cpp
// Fracturing methods - all static functions
FFractureEngineFracturing::VoronoiFracture(...);  // Point-based fracturing
FFractureEngineFracturing::PlaneCutter(...);      // Slicing by planes
FFractureEngineFracturing::SliceCutter(...);      // Uniform slicing
FFractureEngineFracturing::BrickCutter(...);      // Brick-like patterns
FFractureEngineFracturing::MeshCutter(...);       // Custom mesh "cookie cutter"
```

### Chaos Vehicles
```cpp
UChaosWheeledVehicleMovementComponent* VehicleComp;

// Wheel setup
TArray<FChaosWheelSetup> WheelSetups;

// Mechanical configuration structs
FVehicleEngineConfig Engine;        // Torque curves, max RPM, MOI
FVehicleDifferentialConfig Diff;    // AWD/FWD/RWD splits
FVehicleTransmissionConfig Trans;   // Automatic/Manual, gear ratios
FVehicleSteeringConfig Steering;    // Ackermann, Angle Ratio modes

// Initialize physics vehicle
VehicleComp->CreatePhysicsVehicle();  // Creates UChaosWheeledVehicleSimulation
```

### Chaos Cloth
```cpp
// Cloth asset binding
UClothingAssetCommon* ClothAsset;
ClothAsset->BindToSkeletalMesh(SkeletalMesh, LODIndex, SectionIndex);

// Configuration via ClothConfigs map
TMap<FName, TObjectPtr<UClothConfigBase>> ClothConfigs;

// Chaos-specific config (from ChaosCloth plugin)
UChaosClothConfig* Config;
Config->Damping;
Config->Stiffness;
Config->CollisionThickness;
Config->GravityScale;
```

### Field System (Forces & Impulses)
```cpp
UFieldSystemComponent* FieldComp;

// Persistent fields (long-running forces like gravity wells)
FieldComp->AddPersistentField(bEnabled, TargetType, MetaData, FieldNode);

// Transient fields (one-shot impulses like explosions)
FieldComp->ApplyPhysicsField(bEnabled, TargetType, MetaData, FieldNode);

// Specific helpers
FieldComp->ApplyRadialForce(Location, Radius, Strength);
FieldComp->ApplyRadialVectorFalloffForce(Location, Radius, Strength);
FieldComp->ApplyLinearForce(Direction, Strength);
FieldComp->ApplyStayDynamicField(Location, Radius);  // Force kinematic → dynamic
```

---

## Implementation Notes

### Editor vs Runtime
- Most creation APIs require Editor module (`WITH_EDITOR`)
- Runtime modification uses different patterns (e.g., `UMaterialInstanceDynamic`)

### Thread Safety
- Most UObject operations must be on Game Thread
- Use `AsyncTask(ENamedThreads::GameThread, ...)` for thread-safe operations

### Garbage Collection
- Use `UPROPERTY()` for UObject references
- Use `TWeakObjectPtr<>` for non-owning references
- Use `AddToRoot()` for temporary prevention of GC

### Error Handling Pattern
```cpp
if (!IsValid(Object))
{
    UE_LOG(LogMCP, Error, TEXT("Invalid object"));
    return FGeometryScriptResult::Error("Invalid object");
}
```

---

## Quick Reference: Module Dependencies

| Phase | Required Modules |
|-------|------------------|
| 6 (Geometry) | GeometryScriptingCore |
| 7 (Skeletal) | Engine, SkeletalMeshUtilitiesCommon |
| 8 (Materials) | Engine, MaterialEditor (Editor) |
| 9 (Textures) | Engine, ImageCore |
| 10 (Animation) | Engine, ControlRig, IKRig |
| 11 (Audio) | Engine, AudioMixer, MetasoundEngine |
| 12 (Niagara) | Niagara, NiagaraCore |
| 13 (GAS) | GameplayAbilities, GameplayTags |
| 14 (Character) | Engine |
| 15 (Combat) | Engine |
| 16 (AI) | AIModule, StateTreeModule |
| 17 (Inventory) | Engine, AssetRegistry |
| 18 (Interaction) | Engine |
| 19 (UMG) | UMG, UMGEditor (Editor) |
| 20 (Networking) | Engine (NetDriver) |
| 21 (Game Framework) | Engine |
| 22 (Sessions) | Engine |
| 23-24 (World/Volumes) | Engine, WorldPartitionEditor (Editor) |
| 25 (Navigation) | NavigationSystem, AIModule |
| 26 (Splines) | Engine |
| 27 (PCG) | PCG |
| 28 (Landscape) | Landscape, Foliage |
| 29 (Lighting) | Engine, Renderer |
| 30 (Sequencer) | LevelSequence, MovieScene, MovieRenderPipelineCore |
| 31 (Data) | Engine |
| 32 (Build) | DesktopPlatform, TargetPlatform |
| 33 (Testing) | AutomationController, FunctionalTesting |
| 34 (Editor Utilities) | Blutility (Editor) |
| 35 (Gameplay) | Engine |
| 36-39 (Plugins) | Bridge, LiveLink, (External: Wwise, FMOD) |
| 40 (Virtual Production) | DisplayCluster, LiveLink, DMXRuntime |
| 41 (XR) | HeadMountedDisplay, OpenXRHMD |
| 42 (Mass AI) | MassEntity, MassGameplay |
| 43 (Online) | OnlineSubsystem, OnlineSubsystemUtils |
| 44 (Streaming) | PixelStreaming, RemoteControl |
| 45 (Utility) | PythonScriptPlugin |
| 46 (Chaos Physics) | Chaos, GeometryCollectionEngine, ChaosVehicles, ChaosCloth, FieldSystemEngine |
| 47 (Accessibility) | Engine, Slate |
| 48 (Modding) | PakFile, AssetRegistry |

---

*Document generated from UE 5.6 source analysis for MCP implementation guidance.*
