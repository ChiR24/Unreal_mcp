#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
namespace
{
// Lower-camel key for a Blueprint variable name: "MaxHealth" -> "maxHealth".
FString ToStatKey(const FString& VarName)
{
    return VarName.IsEmpty() ? VarName : VarName.Left(1).ToLower() + VarName.Mid(1);
}

// Emits the authored variable names and their CDO defaults as exported text,
// and returns the numeric subset as a stats object keyed in lower camel case
// so a weapon readback carries values (damage, fireRate, ...) not just names.
TSharedPtr<FJsonObject> DescribeCombatVariables(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Info)
{
    TArray<TSharedPtr<FJsonValue>> VariableList;
    TSharedPtr<FJsonObject> Defaults = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
    UClass* GeneratedClass = Blueprint->GeneratedClass;
    UObject* CDO = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;

    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        const FString VarName = Var.VarName.ToString();
        VariableList.Add(MakeShared<FJsonValueString>(VarName));

        FProperty* Property = GeneratedClass ? GeneratedClass->FindPropertyByName(Var.VarName) : nullptr;
        if (!Property || !CDO)
        {
            Defaults->SetStringField(VarName, Var.DefaultValue);
            continue;
        }

        FString Text;
        Property->ExportText_InContainer(0, Text, CDO, nullptr, CDO, PPF_None);
        Defaults->SetStringField(VarName, Text);

        if (FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
        {
            const void* ValuePtr = Numeric->ContainerPtrToValuePtr<void>(CDO);
            if (Numeric->IsFloatingPoint())
            {
                Stats->SetNumberField(ToStatKey(VarName), Numeric->GetFloatingPointPropertyValue(ValuePtr));
            }
            else if (!Numeric->IsEnum())
            {
                Stats->SetNumberField(ToStatKey(VarName), static_cast<double>(Numeric->GetSignedIntPropertyValue(ValuePtr)));
            }
        }
    }

    Info->SetArrayField(TEXT("variables"), VariableList);
    Info->SetObjectField(TEXT("variableDefaults"), Defaults);
    return Stats;
}
}

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
        DescribeCombatVariables(Blueprint, Info);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetObjectField(TEXT("combatInfo"), Info);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Combat info retrieved."), Result);
        return true;
    }
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

        TSharedPtr<FJsonObject> Stats = DescribeCombatVariables(Blueprint, Info);
        Info->SetObjectField(TEXT("stats"), Stats);
        Info->SetNumberField(TEXT("statCount"), Stats->Values.Num());

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetObjectField(TEXT("combatInfo"), Info);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Combat stats retrieved."), Result);
        return true;
    }
    return false;
}
#endif
}
