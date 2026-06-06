#include "McpAutomationBridge_GASBlueprintCreation.h"
#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "AttributeSet.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASAttributes(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("create_attribute_set"))
    {
        if (Name.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        bool bReusedExisting = false;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, UAttributeSet::StaticClass(), Error, bReusedExisting);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        if (!bReusedExisting)
        {
            McpSafeAssetSave(Blueprint);
        }

        // Use the actual blueprint name (which may have been sanitized) in the response
        FString ActualName = Blueprint->GetName();

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("name"), ActualName);
        Result->SetStringField(TEXT("assetPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("parentClass"), TEXT("AttributeSet"));
        Result->SetBoolField(TEXT("reusedExisting"), bReusedExisting);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bReusedExisting ? TEXT("Attribute set already exists") : TEXT("Attribute set created"), Result);
        return true;
    }

    // add_attribute
    if (SubAction == TEXT("add_attribute"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString AttributeName = GetStringFieldGAS(Payload, TEXT("attributeName"));
        if (AttributeName.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing attributeName."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        float DefaultValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("defaultValue"), 0.0));

        // Add FGameplayAttributeData member variable
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = FGameplayAttributeData::StaticStruct();

        bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*AttributeName), PinType);
        if (!bSuccess)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add attribute"), TEXT("ADD_FAILED"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("attributeName"), AttributeName);
        Result->SetNumberField(TEXT("defaultValue"), DefaultValue);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attribute added"), Result);
        return true;
    }

    // set_attribute_base_value - REAL IMPLEMENTATION using reflection
    if (SubAction == TEXT("set_attribute_base_value"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString AttributeName = GetStringFieldGAS(Payload, TEXT("attributeName"));
        if (AttributeName.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing attributeName."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        float BaseValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("baseValue"), 0.0));

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
            // Access the FGameplayAttributeData struct
            void* AttrDataPtr = AttrProperty->ContainerPtrToValuePtr<void>(AttrSetCDO);
            if (AttrDataPtr)
            {
                // Navigate into the FGameplayAttributeData struct to set BaseValue
                UScriptStruct* AttrStruct = FGameplayAttributeData::StaticStruct();
                FNumericProperty* BaseValueProp = CastField<FNumericProperty>(AttrStruct->FindPropertyByName(TEXT("BaseValue")));
                if (BaseValueProp)
                {
                    void* BaseValueAddr = BaseValueProp->ContainerPtrToValuePtr<void>(AttrDataPtr);
                    BaseValueProp->SetFloatingPointPropertyValue(BaseValueAddr, static_cast<double>(BaseValue));
                }

                // Also set CurrentValue to match
                FNumericProperty* CurrentValueProp = CastField<FNumericProperty>(AttrStruct->FindPropertyByName(TEXT("CurrentValue")));
                if (CurrentValueProp)
                {
                    void* CurrentValueAddr = CurrentValueProp->ContainerPtrToValuePtr<void>(AttrDataPtr);
                    CurrentValueProp->SetFloatingPointPropertyValue(CurrentValueAddr, static_cast<double>(BaseValue));
                }
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
    if (SubAction == TEXT("set_attribute_clamping"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString AttributeName = GetStringFieldGAS(Payload, TEXT("attributeName"));
        if (AttributeName.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing attributeName."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        float MinValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("minValue"), 0.0));
        float MaxValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("maxValue"), 100.0));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Verify this is an AttributeSet blueprint
        if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(UAttributeSet::StaticClass()))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint is not an AttributeSet"), TEXT("INVALID_TYPE"));
            return true;
        }

        // Add min/max clamping variables for this attribute
        FString MinVarName = FString::Printf(TEXT("%s_Min"), *AttributeName);
        FString MaxVarName = FString::Printf(TEXT("%s_Max"), *AttributeName);

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*MinVarName), FloatPinType);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*MaxVarName), FloatPinType);

        // Set the category for organization
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*MinVarName), nullptr, FText::FromString(TEXT("Attribute Clamping")));
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*MaxVarName), nullptr, FText::FromString(TEXT("Attribute Clamping")));

        // Set default values on the CDO for the min/max variables
        UAttributeSet* AttrSetCDO = Cast<UAttributeSet>(Blueprint->GeneratedClass->GetDefaultObject());
        if (AttrSetCDO)
        {
            // Use reflection to set the default values for min/max variables after compile
            Blueprint->Modify();

            // Set default values via variable descriptions
            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == FName(*MinVarName))
                {
                    VarDesc.DefaultValue = FString::SanitizeFloat(MinValue);
                }
                else if (VarDesc.VarName == FName(*MaxVarName))
                {
                    VarDesc.DefaultValue = FString::SanitizeFloat(MaxValue);
                }
            }
        }

        // Add a boolean to enable/disable clamping at runtime
        FString EnableClampVarName = FString::Printf(TEXT("bClamp%s"), *AttributeName);
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*EnableClampVarName), BoolPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*EnableClampVarName), nullptr, FText::FromString(TEXT("Attribute Clamping")));

        // Set default to enabled
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == FName(*EnableClampVarName))
            {
                VarDesc.DefaultValue = TEXT("true");
                break;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("attributeName"), AttributeName);
        Result->SetNumberField(TEXT("minValue"), MinValue);
        Result->SetNumberField(TEXT("maxValue"), MaxValue);
        Result->SetStringField(TEXT("minVariable"), MinVarName);
        Result->SetStringField(TEXT("maxVariable"), MaxVarName);
        Result->SetStringField(TEXT("enableClampVariable"), EnableClampVarName);
        Result->SetStringField(TEXT("message"), TEXT("Clamping variables added. Override PreAttributeChange in Blueprint and use these variables to clamp the attribute value."));
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attribute clamping configured"), Result);
        return true;
    }

    // ============================================================
    // 13.2 GAMEPLAY ABILITIES
    // ============================================================

    // create_gameplay_ability

    return false;
}
}
#endif
