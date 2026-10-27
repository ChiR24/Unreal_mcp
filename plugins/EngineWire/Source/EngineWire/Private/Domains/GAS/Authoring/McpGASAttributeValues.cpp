#include "Domains/GAS/McpGASPayloadFields.h"
#include "Domains/GAS/McpGASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Safety/McpSafeOperations.h"
#include "EngineWireSubsystem.h"
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
    UEngineWireSubsystem* Bridge = Context.Subsystem;
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

        // An omitted baseValue used to write 0 over the attribute.
        if (!Payload->HasField(TEXT("baseValue")))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing baseValue; nothing was changed."), TEXT("INVALID_ARGUMENT"));
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
            // Any other property of that name was left untouched while the reply said its value was set.
            const FStructProperty* AttrStructProp = CastField<FStructProperty>(AttrProperty);
            if (!AttrStructProp || !AttrStructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
            {
                Bridge->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("%s is not a Gameplay Attribute (FGameplayAttributeData); nothing was changed."), *AttributeName),
                    TEXT("INVALID_TYPE"));
                return true;
            }
            AttrSetCDO->Modify();
            FGameplayAttributeData* Data = AttrStructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttrSetCDO);
            Data->SetBaseValue(BaseValue);
            Data->SetCurrentValue(BaseValue);
        }

        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("Attribute base value")))
        {
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("attributeName"), AttributeName);
        Result->SetNumberField(TEXT("baseValue"), BaseValue);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attribute base value set"), Result);
        return true;
    }

    // set_attribute_clamping - REAL IMPLEMENTATION with PreAttributeChange clamping logic
    return false;
}
}
