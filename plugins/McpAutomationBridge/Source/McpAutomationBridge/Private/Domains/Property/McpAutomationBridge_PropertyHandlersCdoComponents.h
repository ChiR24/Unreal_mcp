#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "Engine/Blueprint.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

class UActorComponent;
class UBlueprint;

namespace McpPropertyCdoComponents
{
// Visits Blueprint's SCS nodes, then each parent Blueprint's (bInherited true
// there); Visit returns false to stop the walk.
template <typename TVisitor>
void ForEachScsNode(UBlueprint* Blueprint, TVisitor&& Visit)
{
    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && !Visit(Node, Bp != Blueprint))
                {
                    return;
                }
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }
}

TSharedPtr<FJsonObject> BuildComponentSummary(
    UActorComponent* Template,
    const FString& DisplayName,
    const FString& Source,
    bool bDetailed,
    const TArray<FName>& PropertyFilter);

TMap<FString, FString> BuildScsSourceMap(UBlueprint* Blueprint);

/**
 * Every name FindCdoComponent() would accept, in a stable order: native object
 * names, the UPROPERTY aliases that point at them (ACharacter's `Mesh`), and
 * SCS variable names from this Blueprint and its parents. Used to turn a bare
 * COMPONENT_NOT_FOUND into a one-round-trip fix.
 */
TArray<FString> CollectResolvableComponentNames(UBlueprint* Blueprint, UObject* CDO);

// The inherited-component override FindCdoComponent created for this call. A call that ends up not using it must
// Rollback(), or the child stops inheriting its parent's later edits to that component.
struct FCreatedInheritedOverride
{
    UInheritableComponentHandler* Handler = nullptr;
    FComponentKey Key;

    void Rollback()
    {
        if (Handler && Key.IsValid())
        {
            Handler->RemoveOverridenComponentTemplate(Key);
        }
        Handler = nullptr;
        Key = FComponentKey();
    }
};

UActorComponent* FindCdoComponent(
    UBlueprint* Blueprint,
    UObject* CDO,
    const FString& ComponentName,
    bool bCreateInheritedOverride,
    UInheritableComponentHandler** OutCreatedInheritedOverrideHandler = nullptr,
    FComponentKey* OutCreatedInheritedOverrideKey = nullptr,
    bool* bOutFoundComponent = nullptr);
}
