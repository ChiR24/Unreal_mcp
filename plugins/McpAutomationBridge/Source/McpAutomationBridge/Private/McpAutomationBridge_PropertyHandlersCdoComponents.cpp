#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PropertyHandlersCdoComponents.h"

#if WITH_EDITOR
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

#include "Animation/AnimInstance.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"

namespace McpPropertyCdoComponents
{
TSharedPtr<FJsonObject> BuildComponentSummary(
    UActorComponent* Template,
    const FString& DisplayName,
    const FString& Source,
    bool bDetailed,
    const TArray<FName>& PropertyFilter)
{
    TSharedPtr<FJsonObject> CompObj = McpHandlerUtils::CreateResultObject();
    CompObj->SetStringField(TEXT("name"), DisplayName);
    CompObj->SetStringField(TEXT("class"), Template->GetClass()->GetName());
    CompObj->SetStringField(TEXT("source"), Source);

    if (USceneComponent* SceneComp = Cast<USceneComponent>(Template))
    {
        TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();
        TransformObj->SetObjectField(TEXT("location"),
            McpHandlerUtils::VectorToJson(SceneComp->GetRelativeLocation()));
        TransformObj->SetObjectField(TEXT("rotation"),
            McpHandlerUtils::RotatorToJson(SceneComp->GetRelativeRotation()));
        TransformObj->SetObjectField(TEXT("scale"),
            McpHandlerUtils::VectorToJson(SceneComp->GetRelativeScale3D()));
        CompObj->SetObjectField(TEXT("transform"), TransformObj);
    }

    if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Template))
    {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        USkeletalMesh* Mesh = SkelComp->GetSkeletalMeshAsset();
#else
        USkeletalMesh* Mesh = SkelComp->SkeletalMesh;
#endif
        CompObj->SetStringField(TEXT("skeletalMesh"),
            Mesh ? Mesh->GetPathName() : TEXT("None"));
        CompObj->SetStringField(TEXT("animClass"),
            SkelComp->AnimClass ? SkelComp->AnimClass->GetPathName() : TEXT("None"));
    }

    if (UStaticMeshComponent* StaticComp = Cast<UStaticMeshComponent>(Template))
    {
        CompObj->SetStringField(TEXT("staticMesh"),
            StaticComp->GetStaticMesh()
                ? StaticComp->GetStaticMesh()->GetPathName()
                : TEXT("None"));
    }

    if (PropertyFilter.Num() > 0)
    {
        TSharedPtr<FJsonObject> Props =
            McpPropertyReflection::ExportPropertiesToJson(Template, PropertyFilter);
        if (Props.IsValid())
        {
            CompObj->SetObjectField(TEXT("properties"), Props);
        }
    }
    else if (bDetailed)
    {
        TSharedPtr<FJsonObject> Props =
            McpPropertyReflection::ExportObjectToJson(Template, false);
        if (Props.IsValid())
        {
            CompObj->SetObjectField(TEXT("properties"), Props);
        }
    }

    return CompObj;
}

TMap<FString, FString> BuildScsSourceMap(UBlueprint* Blueprint)
{
    TMap<FString, FString> SourceMap;
    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (!Node) continue;
                const FString VarName = Node->GetVariableName().ToString();
                if (!SourceMap.Contains(VarName))
                {
                    SourceMap.Add(VarName,
                        (Bp == Blueprint) ? TEXT("SCS") : TEXT("SCS_Inherited"));
                }
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }
    return SourceMap;
}

UActorComponent* FindCdoComponent(
    UBlueprint* Blueprint,
    UObject* CDO,
    const FString& ComponentName,
    bool bCreateInheritedOverride,
    UInheritableComponentHandler** OutCreatedInheritedOverrideHandler,
    FComponentKey* OutCreatedInheritedOverrideKey,
    bool* bOutFoundComponent)
{
    if (OutCreatedInheritedOverrideHandler)
    {
        *OutCreatedInheritedOverrideHandler = nullptr;
    }
    if (OutCreatedInheritedOverrideKey)
    {
        *OutCreatedInheritedOverrideKey = FComponentKey();
    }
    if (bOutFoundComponent)
    {
        *bOutFoundComponent = false;
    }

    if (AActor* DefaultActor = Cast<AActor>(CDO))
    {
        TInlineComponentArray<UActorComponent*> Components;
        DefaultActor->GetComponents(Components);
        for (UActorComponent* Comp : Components)
        {
            if (Comp && Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
            {
                if (bOutFoundComponent)
                {
                    *bOutFoundComponent = true;
                }
                return Comp;
            }
        }
    }

    UBlueprintGeneratedClass* ActualBPGC = Blueprint
        ? Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass)
        : nullptr;
    if (!ActualBPGC)
    {
        return nullptr;
    }

    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate &&
                    Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
                {
                    if (bOutFoundComponent)
                    {
                        *bOutFoundComponent = true;
                    }
                    const bool bInheritedNode = Node->GetSCS() != ActualBPGC->SimpleConstructionScript;
                    if (bCreateInheritedOverride && bInheritedNode)
                    {
                        FComponentKey Key(Node);
                        const bool bBlueprintCanOverrideComponentFromKey = Key.IsValid()
                            && Blueprint->ParentClass
                            && Blueprint->ParentClass->IsChildOf(Key.GetComponentOwner());
                        if (bBlueprintCanOverrideComponentFromKey)
                        {
                            if (UInheritableComponentHandler* InheritableComponentHandler = Blueprint->GetInheritableComponentHandler(true))
                            {
                                if (UActorComponent* OverrideTemplate = InheritableComponentHandler->GetOverridenComponentTemplate(Key))
                                {
                                    return OverrideTemplate;
                                }
                                Blueprint->Modify();
                                InheritableComponentHandler->Modify();
                                if (UActorComponent* OverrideTemplate = InheritableComponentHandler->CreateOverridenComponentTemplate(Key))
                                {
                                    if (OutCreatedInheritedOverrideHandler)
                                    {
                                        *OutCreatedInheritedOverrideHandler = InheritableComponentHandler;
                                    }
                                    if (OutCreatedInheritedOverrideKey)
                                    {
                                        *OutCreatedInheritedOverrideKey = Key;
                                    }
                                    return OverrideTemplate;
                                }
                            }
                        }

                        return nullptr;
                    }

                    return Node->GetActualComponentTemplate(ActualBPGC);
                }
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }
    return nullptr;
}
}
#endif
