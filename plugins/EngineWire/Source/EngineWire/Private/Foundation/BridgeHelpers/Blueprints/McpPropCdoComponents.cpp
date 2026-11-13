#include "Foundation/BridgeHelpers/Blueprints/McpPropCdoComponents.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Foundation/BridgeHelpers/Blueprints/McpBPActorBinding.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

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
        USkeletalMesh* Mesh = MCP_SKELETAL_MESH_ASSET(SkelComp);
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
            McpPropertyReflection::ExportObjectToJsonBounded(Template, false);
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
    ForEachScsNode(Blueprint, [&](USCS_Node* Node, bool bInherited)
    {
        SourceMap.FindOrAdd(Node->GetVariableName().ToString(), bInherited ? TEXT("SCS_Inherited") : TEXT("SCS"));
        return true;
    });
    return SourceMap;
}

UActorComponent* FindCdoComponent(
    UBlueprint* Blueprint,
    UObject* CDO,
    const FString& ComponentName,
    bool bCreateInheritedOverride,
    FCreatedInheritedOverride* OutCreated,
    bool* bOutFoundComponent)
{
    if (OutCreated)
    {
        *OutCreated = FCreatedInheritedOverride();
    }
    if (bOutFoundComponent)
    {
        *bOutFoundComponent = false;
    }

    // A native component goes by its object name or, usually, by the UPROPERTY that holds it: ACharacter's skeletal
    // mesh is declared as `Mesh` but constructed as `CharacterMesh0`, and CapsuleComponent is `CollisionCylinder`.
    const AActor* DefaultActor = Cast<AActor>(CDO);
    if (UActorComponent* Comp = DefaultActor ? McpFindActorComponent(DefaultActor, ComponentName, UActorComponent::StaticClass()) : nullptr)
    {
        if (bOutFoundComponent)
        {
            *bOutFoundComponent = true;
        }
        return Comp;
    }

    UBlueprintGeneratedClass* ActualBPGC = Blueprint
        ? Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass)
        : nullptr;
    if (!ActualBPGC)
    {
        return nullptr;
    }

    UActorComponent* Found = nullptr;
    ForEachScsNode(Blueprint, [&](USCS_Node* Node, bool)
    {
        if (!Node->ComponentTemplate ||
            !Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
        {
            return true;
        }
        if (bOutFoundComponent)
        {
            *bOutFoundComponent = true;
        }
        const bool bInheritedNode = Node->GetSCS() != ActualBPGC->SimpleConstructionScript;
        if (!bCreateInheritedOverride || !bInheritedNode)
        {
            Found = Node->GetActualComponentTemplate(ActualBPGC);
            return false;
        }
        // An inherited node is edited through this Blueprint's override of it.
        FComponentKey Key(Node);
        const bool bBlueprintCanOverrideComponentFromKey = Key.IsValid()
            && Blueprint->ParentClass
            && Blueprint->ParentClass->IsChildOf(Key.GetComponentOwner());
        UInheritableComponentHandler* InheritableComponentHandler =
            bBlueprintCanOverrideComponentFromKey ? Blueprint->GetInheritableComponentHandler(true) : nullptr;
        if (!InheritableComponentHandler)
        {
            return false;
        }
        Found = InheritableComponentHandler->GetOverridenComponentTemplate(Key);
        if (!Found)
        {
            Blueprint->Modify();
            InheritableComponentHandler->Modify();
            Found = InheritableComponentHandler->CreateOverridenComponentTemplate(Key);
            if (Found && OutCreated)
            {
                OutCreated->Handler = InheritableComponentHandler;
                OutCreated->Key = Key;
            }
        }
        return false;
    });
    return Found;
}
}
