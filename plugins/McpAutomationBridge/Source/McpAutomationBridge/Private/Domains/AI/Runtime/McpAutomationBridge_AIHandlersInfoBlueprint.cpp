#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/EngineTypes.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Character.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
namespace
{
// Component templates the class would instantiate: native default subobjects
// on the CDO plus the SCS templates authored along the Blueprint class chain.
void CollectComponentTemplates(UClass* GeneratedClass, UObject* CDO, TArray<UActorComponent*>& OutComponents)
{
    if (AActor* ActorCDO = Cast<AActor>(CDO))
    {
        TArray<UActorComponent*> NativeComponents;
        ActorCDO->GetComponents(NativeComponents);
        OutComponents.Append(NativeComponents);
    }
    for (UClass* Class = GeneratedClass; Class; Class = Class->GetSuperClass())
    {
        UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Class);
        if (!BPGC || !BPGC->SimpleConstructionScript)
        {
            continue;
        }
        for (USCS_Node* Node : BPGC->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                OutComponents.Add(Node->ComponentTemplate);
            }
        }
    }
}

// Perception presence, the component carrying it and the sense classes its
// SensesConfig array holds (read by reflection: the array is protected).
void DescribePerception(const TArray<UActorComponent*>& Components, const TSharedPtr<FJsonObject>& Result)
{
    UAIPerceptionComponent* PerceptionComponent = nullptr;
    for (UActorComponent* Component : Components)
    {
        PerceptionComponent = Cast<UAIPerceptionComponent>(Component);
        if (PerceptionComponent)
        {
            break;
        }
    }

    TSharedPtr<FJsonObject> Perception = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Senses;
    Perception->SetBoolField(TEXT("present"), PerceptionComponent != nullptr);
    if (PerceptionComponent)
    {
        Perception->SetStringField(TEXT("componentName"), PerceptionComponent->GetName());
        FArrayProperty* SensesProp = FindFProperty<FArrayProperty>(UAIPerceptionComponent::StaticClass(), TEXT("SensesConfig"));
        FObjectPropertyBase* Inner = SensesProp ? CastField<FObjectPropertyBase>(SensesProp->Inner) : nullptr;
        if (SensesProp && Inner)
        {
            FScriptArrayHelper Helper(SensesProp, SensesProp->ContainerPtrToValuePtr<void>(PerceptionComponent));
            for (int32 Index = 0; Index < Helper.Num(); ++Index)
            {
                UAISenseConfig* Config = Cast<UAISenseConfig>(Inner->GetObjectPropertyValue(Helper.GetRawPtr(Index)));
                if (!Config)
                {
                    continue;
                }
                TSharedPtr<FJsonObject> Sense = MakeShared<FJsonObject>();
                Sense->SetStringField(TEXT("configClass"), Config->GetClass()->GetName());
                UClass* SenseClass = Config->GetSenseImplementation();
                Sense->SetStringField(TEXT("senseClass"), SenseClass ? SenseClass->GetName() : TEXT("None"));
                Senses.Add(MakeShared<FJsonValueObject>(Sense));
            }
        }
    }
    Perception->SetArrayField(TEXT("senses"), Senses);
    Result->SetObjectField(TEXT("perception"), Perception);
}

// teamId: an AIController answers through IGenericTeamAgentInterface; a pawn
// reports the first FGenericTeamId member authored on its class chain.
void DescribeTeam(UClass* GeneratedClass, UObject* CDO, const TSharedPtr<FJsonObject>& Result)
{
    if (const AAIController* Controller = Cast<AAIController>(CDO))
    {
        Result->SetNumberField(TEXT("teamId"), Controller->GetGenericTeamId().GetId());
        return;
    }
    for (TFieldIterator<FStructProperty> It(GeneratedClass); It; ++It)
    {
        if (It->Struct != FGenericTeamId::StaticStruct())
        {
            continue;
        }
        const FGenericTeamId* Team = It->ContainerPtrToValuePtr<FGenericTeamId>(CDO);
        Result->SetNumberField(TEXT("teamId"), Team ? Team->GetId() : FGenericTeamId().GetId());
        Result->SetStringField(TEXT("teamProperty"), It->GetName());
        return;
    }
}

// movement: the first movement component template, its GetMaxSpeed() and the
// speed-like float properties it exposes (a CDO's CharacterMovement reports
// GetMaxSpeed() as 0 until a movement mode is active, so MaxWalkSpeed matters).
void DescribeMovement(const TArray<UActorComponent*>& Components, const TSharedPtr<FJsonObject>& Result)
{
    UMovementComponent* Movement = nullptr;
    for (UActorComponent* Component : Components)
    {
        Movement = Cast<UMovementComponent>(Component);
        if (Movement)
        {
            break;
        }
    }
    if (!Movement)
    {
        return;
    }

    TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
    Info->SetStringField(TEXT("componentClass"), Movement->GetClass()->GetName());
    Info->SetStringField(TEXT("componentName"), Movement->GetName());
    Info->SetNumberField(TEXT("maxSpeed"), Movement->GetMaxSpeed());
    const TCHAR* SpeedProps[] = { TEXT("MaxWalkSpeed"), TEXT("MaxFlySpeed"), TEXT("MaxSwimSpeed"), TEXT("MaxSpeed"), TEXT("MaxAcceleration") };
    for (const TCHAR* PropName : SpeedProps)
    {
        if (FFloatProperty* Prop = FindFProperty<FFloatProperty>(Movement->GetClass(), PropName))
        {
            FString Key = PropName;
            Key[0] = FChar::ToLower(Key[0]);
            Info->SetNumberField(Key, Prop->GetPropertyValue_InContainer(Movement));
        }
    }
    Result->SetObjectField(TEXT("movement"), Info);
}
}

// Pawn/Character Blueprint: AI controller class, auto-possess policy,
// perception, team and movement. AIController Blueprint: perception, team and
// the default Behavior Tree / Blackboard when a class property carries one.
// aiInfo only receives the fields its contract declares; everything else sits
// at the top level so the gateway folds it into details.
void DescribeAIBlueprint(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& AIInfo, const TSharedPtr<FJsonObject>& Result)
{
    UClass* GeneratedClass = Blueprint ? Blueprint->GeneratedClass : nullptr;
    UObject* CDO = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
    Result->SetStringField(TEXT("blueprintPath"), Blueprint ? Blueprint->GetPathName() : FString());
    Result->SetStringField(TEXT("parentClass"), Blueprint && Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("Unknown"));
    if (!GeneratedClass || !CDO)
    {
        Result->SetStringField(TEXT("blueprintKind"), TEXT("Uncompiled"));
        return;
    }

    TArray<UActorComponent*> Components;
    CollectComponentTemplates(GeneratedClass, CDO, Components);

    if (APawn* PawnCDO = Cast<APawn>(CDO))
    {
        Result->SetStringField(TEXT("blueprintKind"), CDO->IsA<ACharacter>() ? TEXT("Character") : TEXT("Pawn"));
        if (PawnCDO->AIControllerClass)
        {
            AIInfo->SetStringField(TEXT("controllerClass"), PawnCDO->AIControllerClass->GetName());
        }
        Result->SetStringField(TEXT("aiControllerClass"), PawnCDO->AIControllerClass ? PawnCDO->AIControllerClass->GetPathName() : TEXT("None"));
        Result->SetStringField(TEXT("autoPossessAI"), StaticEnum<EAutoPossessAI>()->GetNameStringByValue(static_cast<int64>(PawnCDO->AutoPossessAI)));
        DescribePerception(Components, Result);
        DescribeTeam(GeneratedClass, CDO, Result);
        DescribeMovement(Components, Result);
        return;
    }

    if (Cast<AAIController>(CDO))
    {
        Result->SetStringField(TEXT("blueprintKind"), TEXT("AIController"));
        AIInfo->SetStringField(TEXT("controllerClass"), GeneratedClass->GetName());
        Result->SetStringField(TEXT("controllerClassPath"), GeneratedClass->GetPathName());
        UBehaviorTree* DefaultTree = nullptr;
        for (TFieldIterator<FObjectProperty> It(GeneratedClass); It; ++It)
        {
            UObject* Value = It->GetObjectPropertyValue_InContainer(CDO);
            if (UBehaviorTree* Tree = Cast<UBehaviorTree>(Value))
            {
                if (!DefaultTree)
                {
                    DefaultTree = Tree;
                    AIInfo->SetStringField(TEXT("assignedBehaviorTree"), Tree->GetName());
                    Result->SetStringField(TEXT("defaultBehaviorTree"), Tree->GetPathName());
                    Result->SetStringField(TEXT("behaviorTreeProperty"), It->GetName());
                }
            }
            else if (UBlackboardData* Blackboard = Cast<UBlackboardData>(Value))
            {
                if (!Result->HasField(TEXT("defaultBlackboard")))
                {
                    AIInfo->SetStringField(TEXT("assignedBlackboard"), Blackboard->GetName());
                    Result->SetStringField(TEXT("defaultBlackboard"), Blackboard->GetPathName());
                }
            }
        }
        if (DefaultTree && DefaultTree->BlackboardAsset && !Result->HasField(TEXT("defaultBlackboard")))
        {
            AIInfo->SetStringField(TEXT("assignedBlackboard"), DefaultTree->BlackboardAsset->GetName());
            Result->SetStringField(TEXT("defaultBlackboard"), DefaultTree->BlackboardAsset->GetPathName());
        }
        DescribePerception(Components, Result);
        DescribeTeam(GeneratedClass, CDO, Result);
        return;
    }

    Result->SetStringField(TEXT("blueprintKind"), TEXT("Other"));
}
}
#endif
