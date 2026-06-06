#include "McpAutomationBridge_InteractionHandlersPrivate.h"

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

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("widgetClass"), WidgetClass);
        Result->SetBoolField(TEXT("showOnHover"), ShowOnHover);
        Result->SetBoolField(TEXT("showPromptText"), ShowPromptText);
        Result->SetStringField(TEXT("promptTextFormat"), PromptTextFormat);
        Result->SetBoolField(TEXT("configured"), true);
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeAssetSave(Blueprint);
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
        McpSafeAssetSave(Blueprint);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction events added"), Result);
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("add_interaction_events is editor-only"), TEXT("EDITOR_ONLY"));
#endif
        return true;
    }

    return false;
}
}
