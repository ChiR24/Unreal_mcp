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

    return false;
}
}
#endif
