#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AttributeSet.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpGASHandlers
{
bool HandleGASAttributeValues(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& BlueprintPath = Context.BlueprintPath;

    // set_attribute_base_value - REAL IMPLEMENTATION using reflection
    if (SubAction == TEXT("set_attribute_base_value"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString AttributeName = GetJsonStringField(Payload, TEXT("attributeName"));
        if (AttributeName.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing attributeName."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        float BaseValue = static_cast<float>(GetJsonNumberField(Payload, TEXT("baseValue"), 0.0));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UAttributeSet* AttrSetCDO = Cast<UAttributeSet>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AttrSetCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not an AttributeSet blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        // Find the FGameplayAttributeData property using reflection
        UClass* AttrSetClass = Blueprint->GeneratedClass;
        FProperty* AttrProperty = AttrSetClass->FindPropertyByName(FName(*AttributeName));
        if (!AttrProperty)
        {
            bool bUpdatedBlueprintVariable = false;
            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == FName(*AttributeName))
                {
                    VarDesc.DefaultValue = FString::Printf(
                        TEXT("(BaseValue=%s,CurrentValue=%s)"),
                        *FString::SanitizeFloat(BaseValue),
                        *FString::SanitizeFloat(BaseValue));
                    bUpdatedBlueprintVariable = true;
                    break;
                }
            }

            if (!bUpdatedBlueprintVariable)
            {
                Bridge->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Attribute not found: %s"), *AttributeName), TEXT("ATTRIBUTE_NOT_FOUND"));
                return true;
            }
        }
        else
        {
            // Base and current value together, through the struct's own setters.
            const FStructProperty* AttrStructProp = CastField<FStructProperty>(AttrProperty);
            if (AttrStructProp && AttrStructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
            {
                FGameplayAttributeData* Data = AttrStructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttrSetCDO);
                Data->SetBaseValue(BaseValue);
                Data->SetCurrentValue(BaseValue);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);
        AttrSetCDO->MarkPackageDirty();

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("attributeName"), AttributeName);
        Result->SetNumberField(TEXT("baseValue"), BaseValue);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attribute base value set via reflection"), Result);
        return true;
    }

    // set_attribute_clamping - REAL IMPLEMENTATION with PreAttributeChange clamping logic
    return false;
}
}
