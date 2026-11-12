#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "UObject/UnrealType.h"

static inline USCS_Node *FindScsNodeByName(USimpleConstructionScript *SCS,
                                           const FString &Name) {
  // This used to walk AllNodes by reflection and read a "VariableName"
  // property. UE renamed that field to InternalVariableName years ago, so the
  // lookup silently fell through to comparing the NODE OBJECT's name
  // ("SCS_Node_3") and matched nothing: modify_component in a batch always
  // answered "Component not found", and the add path's duplicate check never
  // fired. GetAllNodes()/GetVariableName() are stable across UE 5.0-5.8.
  if (!SCS || Name.IsEmpty())
    return nullptr;
  if (USCS_Node *Node = SCS->FindSCSNode(FName(*Name))) // FName compare: case-insensitive
    return Node;
  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetName().Equals(Name, ESearchCase::IgnoreCase))
      return Node;
  }
  return nullptr;
}

// Why Class cannot be added to a Blueprint as a component, or empty when it can. USimpleConstructionScript::CreateNode
// is a check() for a class that is no component (the editor is gone), and an abstract one (PrimitiveComponent,
// MeshComponent) makes a template the editor warns about and nulls out on save.
static inline FString McpScsComponentClassRefusal(const UClass *Class, const FString &Named) {
  if (!Class->IsChildOf(UActorComponent::StaticClass()))
    return FString::Printf(TEXT("Class is not a component: %s"), *Named);
  if (Class->HasAnyClassFlags(CLASS_Abstract))
    return FString::Printf(TEXT("Class is abstract and cannot be added as a component: %s"), *Named);
  return FString();
}
