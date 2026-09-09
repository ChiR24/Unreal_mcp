#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoComponents.h"

#if WITH_EDITOR
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace McpPropertyCdoComponents
{
TArray<FString> CollectResolvableComponentNames(UBlueprint* Blueprint, UObject* CDO)
{
    TArray<FString> Names;
    TSet<FString> Seen;
    auto AddName = [&Names, &Seen](const FString& Name)
    {
        if (!Name.IsEmpty() && !Seen.Contains(Name))
        {
            Seen.Add(Name);
            Names.Add(Name);
        }
    };

    if (AActor* DefaultActor = Cast<AActor>(CDO))
    {
        TInlineComponentArray<UActorComponent*> Components;
        DefaultActor->GetComponents(Components);
        for (UActorComponent* Comp : Components)
        {
            if (Comp)
            {
                AddName(Comp->GetName());
            }
        }

        // The UPROPERTY aliases FindCdoComponent() also accepts (ACharacter's
        // `Mesh` for CharacterMesh0). Listing both spellings is the point:
        // callers reach for the alias first.
        for (TFieldIterator<FObjectProperty> It(DefaultActor->GetClass()); It; ++It)
        {
            FObjectProperty* ObjProp = *It;
            if (ObjProp && ObjProp->PropertyClass &&
                ObjProp->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
            {
                AddName(ObjProp->GetName());
            }
        }
    }

    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate)
                {
                    AddName(Node->GetVariableName().ToString());
                }
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }

    return Names;
}
}
#endif
