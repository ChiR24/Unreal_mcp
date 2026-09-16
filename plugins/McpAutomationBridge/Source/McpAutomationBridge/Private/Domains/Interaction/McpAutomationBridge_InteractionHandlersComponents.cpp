#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersMutationEvidence.h"

namespace McpInteractionHandlers
{
bool HandleInteractionComponentAuthoringAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("create_interaction_component"))
    {
        const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
        const FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("InteractionComponent"));
#if WITH_EDITOR
        FString ResolvedPath;
        FString LoadError;
        UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
        if (!Blueprint)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
            return true;
        }
        if (!Blueprint->SimpleConstructionScript)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_BP"));
            return true;
        }

        USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(USphereComponent::StaticClass(), *ComponentName);
        if (!Node)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create interaction component"), TEXT("COMPONENT_CREATE_FAILED"));
            return true;
        }

        if (USphereComponent* Template = Cast<USphereComponent>(Node->ComponentTemplate))
        {
            const float TraceDistance = static_cast<float>(GetJsonNumberField(Payload, TEXT("traceDistance"), 200.0));
            Template->SetSphereRadius(TraceDistance);
            Template->SetCollisionProfileName(TEXT("OverlapAll"));
            Template->SetGenerateOverlapEvents(true);
        }
        Blueprint->SimpleConstructionScript->AddNode(Node);
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        const bool bComponentSaved = McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("componentAdded"), true);
        Result->SetStringField(TEXT("componentName"), ComponentName);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        TArray<FString> ComponentChanges;
        ComponentChanges.Add(TEXT("added interaction component"));
        if (bComponentSaved) { ComponentChanges.Add(TEXT("saved")); }
        AddMutationEvidence(Result, Blueprint, ComponentChanges);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction component added"), Result);
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("create_interaction_component is editor-only"), TEXT("EDITOR_ONLY"));
#endif
        return true;
    }

    if (SubAction == TEXT("configure_interaction_trace"))
    {
        const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
        const FString TraceType = GetJsonStringField(Payload, TEXT("traceType"), TEXT("sphere"));
        const double TraceDistance = GetJsonNumberField(Payload, TEXT("traceDistance"), 200.0);
        const double TraceRadius = GetJsonNumberField(Payload, TEXT("traceRadius"), 50.0);
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

        bool bConfigured = false;
        if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : SCS->GetAllNodes())
            {
                if (!Node || !Node->ComponentClass)
                {
                    continue;
                }
                if (Node->ComponentClass->IsChildOf(USphereComponent::StaticClass()))
                {
                    if (USphereComponent* SphereComp = Cast<USphereComponent>(Node->ComponentTemplate))
                    {
                        SphereComp->SetSphereRadius(static_cast<float>(TraceDistance));
                        SphereComp->SetCollisionProfileName(TEXT("OverlapAll"));
                        SphereComp->SetGenerateOverlapEvents(true);
                        bConfigured = true;
                    }
                }
                else if (Node->ComponentClass->IsChildOf(UBoxComponent::StaticClass()))
                {
                    if (UBoxComponent* BoxComp = Cast<UBoxComponent>(Node->ComponentTemplate))
                    {
                        BoxComp->SetBoxExtent(FVector(static_cast<float>(TraceDistance), static_cast<float>(TraceRadius), static_cast<float>(TraceRadius)));
                        BoxComp->SetCollisionProfileName(TEXT("OverlapAll"));
                        BoxComp->SetGenerateOverlapEvents(true);
                        bConfigured = true;
                    }
                }
            }
        }

        // bConfigured is only true when an SCS node was actually a Sphere/Box component. It used to be
        // reported as success:true with a "configured" message and a changes ledger claiming the trace was
        // configured and saved, while details.configured said false -- the receipt contradicted itself.
        if (!bConfigured)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("%s has no interaction trace component: expected an SCS node whose class is a SphereComponent or BoxComponent. Run create_interaction_component first, or target an interactable that has one."), *ResolvedPath),
                TEXT("INVALID_OBJECT_TYPE"));
            return true;
        }

        FEdGraphPinType FloatType;
        FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        FEdGraphPinType NameType;
        NameType.PinCategory = UEdGraphSchema_K2::PC_Name;
        AddBlueprintVariableIfMissing(Blueprint, TEXT("TraceDistance"), FloatType);
        AddBlueprintVariableIfMissing(Blueprint, TEXT("TraceType"), NameType);

        // The members added above are not on GeneratedClass until it is regenerated, so applying before this
        // compile silently targets nothing (the same defect fixed for the door/chest/switch branches).
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
                ApplyToCdo(TEXT("TraceDistance"), MakeShared<FJsonValueNumber>(TraceDistance));
                ApplyToCdo(TEXT("TraceType"), MakeShared<FJsonValueString>(TraceType));
            }
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("traceType"), TraceType);
        Result->SetNumberField(TEXT("traceDistance"), TraceDistance);
        Result->SetNumberField(TEXT("traceRadius"), TraceRadius);
        Result->SetBoolField(TEXT("configured"), true);
        Result->SetBoolField(TEXT("propertiesApplied"), PropertiesNotApplied == 0);
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        const bool bTraceSaved = McpSafeAssetSave(Blueprint);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        TArray<FString> TraceChanges;
        TraceChanges.Add(TEXT("configured interaction trace"));
        if (bTraceSaved) { TraceChanges.Add(TEXT("saved")); }
        AddMutationEvidence(Result, Blueprint, TraceChanges);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction trace configured"), Result);
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("configure_interaction_trace is editor-only"), TEXT("EDITOR_ONLY"));
#endif
        return true;
    }

    return false;
}
}
