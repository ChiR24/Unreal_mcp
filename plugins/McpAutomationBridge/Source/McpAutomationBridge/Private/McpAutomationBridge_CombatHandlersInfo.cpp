#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleInfoActions() const
{
    if (SubAction == TEXT("get_combat_info"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Info = McpHandlerUtils::CreateResultObject();
        Info->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Info->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("Unknown"));

        // Check for components
        bool bHasWeaponMesh = false;
        bool bHasProjectileMovement = false;
        bool bHasCollision = false;
        TArray<TSharedPtr<FJsonValue>> ComponentList;

        if (Blueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate)
                {
                    ComponentList.Add(MakeShared<FJsonValueString>(Node->GetVariableName().ToString()));

                    if (Node->ComponentTemplate->IsA<UStaticMeshComponent>() ||
                        Node->ComponentTemplate->IsA<USkeletalMeshComponent>())
                    {
                        bHasWeaponMesh = true;
                    }
                    if (Node->ComponentTemplate->IsA<UProjectileMovementComponent>())
                    {
                        bHasProjectileMovement = true;
                    }
                    if (Node->ComponentTemplate->IsA<USphereComponent>() ||
                        Node->ComponentTemplate->IsA<UCapsuleComponent>() ||
                        Node->ComponentTemplate->IsA<UBoxComponent>())
                    {
                        bHasCollision = true;
                    }
                }
            }
        }

        Info->SetBoolField(TEXT("hasWeaponMesh"), bHasWeaponMesh);
        Info->SetBoolField(TEXT("hasProjectileMovement"), bHasProjectileMovement);
        Info->SetBoolField(TEXT("hasCollision"), bHasCollision);
        Info->SetArrayField(TEXT("components"), ComponentList);

        // List Blueprint variables
        TArray<TSharedPtr<FJsonValue>> VariableList;
        for (const FBPVariableDescription& Var : Blueprint->NewVariables)
        {
            VariableList.Add(MakeShared<FJsonValueString>(Var.VarName.ToString()));
        }
        Info->SetArrayField(TEXT("variables"), VariableList);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetObjectField(TEXT("combatInfo"), Info);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Combat info retrieved."), Result);
        return true;
    }

    // ============================================================
    // ALIASES
    // ============================================================

    // setup_damage_type -> alias for create_damage_type

    if (SubAction == TEXT("get_combat_stats"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Info = McpHandlerUtils::CreateResultObject();
        Info->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Info->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("Unknown"));

        TArray<TSharedPtr<FJsonValue>> VariableList;
        for (const FBPVariableDescription& Var : Blueprint->NewVariables)
        {
            VariableList.Add(MakeShared<FJsonValueString>(Var.VarName.ToString()));
        }
        Info->SetArrayField(TEXT("variables"), VariableList);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetObjectField(TEXT("combatInfo"), Info);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Combat stats retrieved."), Result);
        return true;
    }

    // ============================================================
    // NEW SUB-ACTIONS
    // ============================================================

    // create_damage_effect - creates a blueprint with damage effect variables

    return false;
}
#endif
}
