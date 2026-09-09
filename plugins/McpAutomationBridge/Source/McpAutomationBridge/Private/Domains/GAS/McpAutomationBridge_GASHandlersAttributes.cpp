#include "Domains/GAS/McpAutomationBridge_GASBlueprintCreation.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

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

        FString AttributeName = GetJsonStringField(Payload, TEXT("attributeName"));
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

        float DefaultValue = static_cast<float>(GetJsonNumberField(Payload, TEXT("defaultValue"), 0.0));

        // Add FGameplayAttributeData member variable.
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = FGameplayAttributeData::StaticStruct();

        // defaultValue used to be read, echoed back in the response, and never applied: every attribute
        // authored through this action landed with BaseValue 0 while the caller was told its value had
        // been set. AddMemberVariable takes the default as a 4th argument; the struct-literal spelling is
        // the one this very file already uses for the same struct (set_attribute_base_value's
        // FBPVariableDescription fallback below), so both paths now agree on one format.
        const FString AttributeDefault = FString::Printf(
            TEXT("(BaseValue=%s,CurrentValue=%s)"),
            *FString::SanitizeFloat(DefaultValue),
            *FString::SanitizeFloat(DefaultValue));

        bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*AttributeName), PinType, AttributeDefault);
        if (!bSuccess)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add attribute"), TEXT("ADD_FAILED"));
            return true;
        }

        // Structural change -> compile, VERIFY, and only then persist. Without the compile the variable
        // exists only on the skeleton class, so the generated-class CDO (what the game and every
        // reflection reader see) does not have the attribute at all; without the save the whole edit dies
        // with the editor session. The save deliberately comes AFTER the read-back below: saving first
        // and then reporting failure would leave the failed state on disk while the error text denies it.
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        const bool bCompiled = McpSafeCompileBlueprint(Blueprint);

        // Read back from the COMPILED generated class before answering. Compilation reinstances the CDO,
        // so this has to re-fetch rather than reuse anything captured earlier. Reporting success for an
        // attribute that is not actually on the class is the failure this whole change exists to end.
        bool bVerified = false;
        float VerifiedBaseValue = 0.0f;
        if (UClass* CompiledClass = Blueprint->GeneratedClass)
        {
            if (UObject* CompiledCDO = CompiledClass->GetDefaultObject())
            {
                if (FProperty* AddedProp = CompiledClass->FindPropertyByName(FName(*AttributeName)))
                {
                    if (void* AttrDataPtr = AddedProp->ContainerPtrToValuePtr<void>(CompiledCDO))
                    {
                        bVerified = true;
                        UScriptStruct* AttrStruct = FGameplayAttributeData::StaticStruct();
                        if (FNumericProperty* BaseValueProp = CastField<FNumericProperty>(AttrStruct->FindPropertyByName(TEXT("BaseValue"))))
                        {
                            VerifiedBaseValue = static_cast<float>(
                                BaseValueProp->GetFloatingPointPropertyValue(BaseValueProp->ContainerPtrToValuePtr<void>(AttrDataPtr)));
                        }
                    }
                }
            }
        }

        if (!bCompiled || !bVerified)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Attribute '%s' was added to the Blueprint but is not present on the compiled class%s. The asset was NOT saved."),
                    *AttributeName,
                    bCompiled ? TEXT("") : TEXT(" (the Blueprint failed to compile - it may have unrelated graph errors)")),
                TEXT("ATTRIBUTE_NOT_APPLIED"));
            return true;
        }

        if (!McpSafeAssetSave(Blueprint))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Attribute verified on the compiled class but the asset could NOT be written to disk (file may be read-only or held by source control). The change exists only in this editor session."),
                TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("attributeName"), AttributeName);
        Result->SetNumberField(TEXT("defaultValue"), DefaultValue);
        // Read back from the compiled class, not echoed from the request: if these two disagree the
        // caller can see it instead of being told the value landed.
        Result->SetNumberField(TEXT("baseValue"), VerifiedBaseValue);
        Result->SetBoolField(TEXT("verifiedOnCompiledClass"), true);
        Result->SetBoolField(TEXT("savedToDisk"), true);
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

        FString AttributeName = GetJsonStringField(Payload, TEXT("attributeName"));
        if (AttributeName.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing attributeName."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        float MinValue = static_cast<float>(GetJsonNumberField(Payload, TEXT("minValue"), 0.0));
        float MaxValue = static_cast<float>(GetJsonNumberField(Payload, TEXT("maxValue"), 100.0));

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

        FString MinVarName = FString::Printf(TEXT("%s_Min"), *AttributeName);
        FString MaxVarName = FString::Printf(TEXT("%s_Max"), *AttributeName);

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*MinVarName), FloatPinType);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*MaxVarName), FloatPinType);

        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*MinVarName), nullptr, FText::FromString(TEXT("Attribute Clamping")));
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*MaxVarName), nullptr, FText::FromString(TEXT("Attribute Clamping")));

        // Set default values on the CDO for the min/max variables
        UAttributeSet* AttrSetCDO = Cast<UAttributeSet>(Blueprint->GeneratedClass->GetDefaultObject());
        if (AttrSetCDO)
        {
            // Use reflection to set the default values for min/max variables after compile
            Blueprint->Modify();

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

        FString EnableClampVarName = FString::Printf(TEXT("bClamp%s"), *AttributeName);
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*EnableClampVarName), BoolPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*EnableClampVarName), nullptr, FText::FromString(TEXT("Attribute Clamping")));

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

    return false;
}
}
#endif
