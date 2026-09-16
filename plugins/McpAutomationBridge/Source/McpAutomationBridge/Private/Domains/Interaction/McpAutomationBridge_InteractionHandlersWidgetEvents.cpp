#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersMutationEvidence.h"

namespace McpInteractionHandlers
{
bool HandleInteractionWidgetEventAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("configure_interaction_widget"))
    {
        const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
        const FString WidgetClass = GetJsonStringField(Payload, TEXT("widgetClass"));
        const bool ShowOnHover = GetJsonBoolField(Payload, TEXT("showOnHover"), true);
        const bool ShowPromptText = GetJsonBoolField(Payload, TEXT("showPromptText"), true);
        const FString PromptTextFormat = GetJsonStringField(Payload, TEXT("promptTextFormat"), TEXT("Press {Key} to Interact"));
#if WITH_EDITOR
        if (BlueprintPath.IsEmpty())
        {
            // An empty path reached LoadBlueprintAsset, which answered "BLUEPRINT_NOT_FOUND: Empty request".
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter 'blueprintPath'"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString ResolvedPath;
        FString LoadError;
        UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
        if (!Blueprint)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
            return true;
        }

        FEdGraphPinType BoolType;
        BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FEdGraphPinType StringType;
        StringType.PinCategory = UEdGraphSchema_K2::PC_String;
        FEdGraphPinType SoftClassType;
        SoftClassType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        AddBlueprintVariableIfMissing(Blueprint, TEXT("bShowOnHover"), BoolType);
        AddBlueprintVariableIfMissing(Blueprint, TEXT("bShowPromptText"), BoolType);
        AddBlueprintVariableIfMissing(Blueprint, TEXT("PromptTextFormat"), StringType);
        AddBlueprintVariableIfMissing(Blueprint, TEXT("InteractionWidgetClass"), SoftClassType);

        // Every value below used to be echoed in the response under "configured": true while nothing was
        // written anywhere -- widgetClass, showOnHover, showPromptText and promptTextFormat were all
        // accepted and silently dropped. The members must exist on GeneratedClass before they can be set,
        // so compile first, then apply, then report whether the apply actually took.
        McpSafeCompileBlueprint(Blueprint);

        int32 PropertiesNotApplied = 0;
        if (Blueprint->GeneratedClass)
        {
            if (UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject())
            {
                auto ApplyToCdo = [CDO, &PropertiesNotApplied](const TCHAR* PropertyName, const TSharedPtr<FJsonValue>& Value)
                {
                    FProperty* Prop = CDO->GetClass()->FindPropertyByName(PropertyName);
                    FString ApplyError;
                    if (!Prop || !ApplyJsonValueToProperty(CDO, Prop, Value, ApplyError))
                    {
                        ++PropertiesNotApplied;
                    }
                };
                ApplyToCdo(TEXT("bShowOnHover"), MakeShared<FJsonValueBoolean>(ShowOnHover));
                ApplyToCdo(TEXT("bShowPromptText"), MakeShared<FJsonValueBoolean>(ShowPromptText));
                ApplyToCdo(TEXT("PromptTextFormat"), MakeShared<FJsonValueString>(PromptTextFormat));
                if (!WidgetClass.IsEmpty())
                {
                    ApplyToCdo(TEXT("InteractionWidgetClass"), MakeShared<FJsonValueString>(WidgetClass));
                }
            }
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        if (!WidgetClass.IsEmpty())
        {
            Result->SetStringField(TEXT("widgetClass"), WidgetClass);
        }
        Result->SetBoolField(TEXT("showOnHover"), ShowOnHover);
        Result->SetBoolField(TEXT("showPromptText"), ShowPromptText);
        Result->SetStringField(TEXT("promptTextFormat"), PromptTextFormat);
        Result->SetBoolField(TEXT("configured"), true);
        Result->SetBoolField(TEXT("propertiesApplied"), PropertiesNotApplied == 0);
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        const bool bWidgetSaved = McpSafeAssetSave(Blueprint);
        TArray<FString> WidgetChanges;
        WidgetChanges.Add(TEXT("configured interaction widget"));
        if (bWidgetSaved) { WidgetChanges.Add(TEXT("saved")); }
        AddMutationEvidence(Result, Blueprint, WidgetChanges);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction widget configured"), Result);
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("configure_interaction_widget is editor-only"), TEXT("EDITOR_ONLY"));
#endif
        return true;
    }

    if (SubAction == TEXT("add_interaction_events"))
    {
        const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
#if WITH_EDITOR
        if (BlueprintPath.IsEmpty())
        {
            // An empty path reached LoadBlueprintAsset, which answered "BLUEPRINT_NOT_FOUND: Empty request".
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter 'blueprintPath'"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString ResolvedPath;
        FString LoadError;
        UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
        if (!Blueprint)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
            return true;
        }

        const TArray<FString> EventNames = {TEXT("OnInteractionStart"), TEXT("OnInteractionEnd"), TEXT("OnInteractableFound"), TEXT("OnInteractableLost")};
        FEdGraphPinType DelegateType;
        DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
        TArray<TSharedPtr<FJsonValue>> AddedEvents;
        for (const FString& EventName : EventNames)
        {
            bool bExists = false;
            for (const FBPVariableDescription& Var : Blueprint->NewVariables)
            {
                if (Var.VarName.ToString() == EventName)
                {
                    bExists = true;
                    break;
                }
            }
            if (!bExists)
            {
                FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*EventName), DelegateType);
                AddedEvents.Add(MakeShared<FJsonValueString>(EventName));
            }
            else
            {
                AddedEvents.Add(MakeShared<FJsonValueString>(EventName + TEXT(" (exists)")));
            }
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetArrayField(TEXT("eventsAdded"), AddedEvents);
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("eventCount"), EventNames.Num());
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        const bool bEventsSaved = McpSafeAssetSave(Blueprint);
        TArray<FString> EventChanges;
        EventChanges.Add(TEXT("added interaction events"));
        if (bEventsSaved) { EventChanges.Add(TEXT("saved")); }
        AddMutationEvidence(Result, Blueprint, EventChanges);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction events added"), Result);
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("add_interaction_events is editor-only"), TEXT("EDITOR_ONLY"));
#endif
        return true;
    }

    return false;
}
}
