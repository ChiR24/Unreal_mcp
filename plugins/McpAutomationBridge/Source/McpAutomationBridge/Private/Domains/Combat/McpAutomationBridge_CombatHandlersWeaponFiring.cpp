#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
bool FCombatActionContext::HandleWeaponFiring() const
{
    if (SubAction == TEXT("configure_projectile"))
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

        FString ProjectileClass = GetJsonStringField(Payload, TEXT("projectileClass"));
        double ProjectileSpeed = GetJsonNumberField(Payload, TEXT("projectileSpeed"), 5000.0);

        AddBlueprintVariableCombat(Blueprint, TEXT("ProjectileClassPath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ProjectileSpeed"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* ClassProp = FindFProperty<FStrProperty>(BPGC, TEXT("ProjectileClassPath")))
                {
                    ClassProp->SetPropertyValue_InContainer(CDO, ProjectileClass);
                }
                if (FDoubleProperty* SpeedProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ProjectileSpeed")))
                {
                    SpeedProp->SetPropertyValue_InContainer(CDO, ProjectileSpeed);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("projectileClass"), ProjectileClass);
        Result->SetNumberField(TEXT("projectileSpeed"), ProjectileSpeed);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile firing configured."), Result);
        return true;
    }
    return false;
}
}
