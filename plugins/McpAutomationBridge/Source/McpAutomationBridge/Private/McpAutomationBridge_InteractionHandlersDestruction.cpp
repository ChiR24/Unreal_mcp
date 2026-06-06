#include "McpAutomationBridge_InteractionHandlersPrivate.h"

namespace
{
bool ConfigureDestructionTag(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString& SuccessMessage,
    const FString& EditorOnlyMessage,
    const FName& TagName)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: actorName"), TEXT("MISSING_PARAMETER"));
        return true;
    }
#if WITH_EDITOR
    AActor* TargetActor = nullptr;
    if (!McpInteractionHandlers::FindEditorActorByName(ActorName, TargetActor))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world available"), TEXT("NO_WORLD"));
        return true;
    }
    if (!TargetActor)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found: ") + ActorName, TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    TargetActor->Modify();
    TargetActor->Tags.AddUnique(TagName);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetStringField(TEXT("tagAdded"), TagName.ToString());
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, SuccessMessage, Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, EditorOnlyMessage, TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}

namespace McpInteractionHandlers
{
bool HandleDestructionAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("setup_destructible_mesh"))
    {
        return ConfigureDestructionTag(Subsystem, RequestId, Payload, RequestingSocket, TEXT("Destructible mesh setup configured"), TEXT("setup_destructible_mesh is editor-only"), TEXT("MCP_DestructibleMeshConfigured"));
    }
    if (SubAction == TEXT("configure_destruction_levels"))
    {
        return ConfigureDestructionTag(Subsystem, RequestId, Payload, RequestingSocket, TEXT("Destruction levels configured"), TEXT("configure_destruction_levels is editor-only"), TEXT("MCP_DestructionLevelsConfigured"));
    }
    if (SubAction == TEXT("configure_destruction_effects"))
    {
        return ConfigureDestructionTag(Subsystem, RequestId, Payload, RequestingSocket, TEXT("Destruction effects configured"), TEXT("configure_destruction_effects is editor-only"), TEXT("MCP_DestructionEffectsConfigured"));
    }
    if (SubAction == TEXT("configure_destruction_damage"))
    {
        return ConfigureDestructionTag(Subsystem, RequestId, Payload, RequestingSocket, TEXT("Destruction damage configured"), TEXT("configure_destruction_damage is editor-only"), TEXT("MCP_DestructionDamageConfigured"));
    }
    if (SubAction != TEXT("add_destruction_component"))
    {
        return false;
    }

    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    const FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("DestructionComponent"));
#if WITH_EDITOR
    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("NO_SCS"));
        return true;
    }

    USCS_Node* Node = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
    if (!Node)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create destruction component"), TEXT("COMPONENT_CREATE_FAILED"));
        return true;
    }
    SCS->AddNode(Node);

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
    AddBlueprintVariableIfMissing(Blueprint, TEXT("Health"), FloatType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("MaxHealth"), FloatType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("bIsDestroyed"), BoolType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("DestructionStage"), IntType);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    TArray<TSharedPtr<FJsonValue>> AddedVars;
    AddedVars.Add(MakeShared<FJsonValueString>(TEXT("Health")));
    AddedVars.Add(MakeShared<FJsonValueString>(TEXT("MaxHealth")));
    AddedVars.Add(MakeShared<FJsonValueString>(TEXT("bIsDestroyed")));
    AddedVars.Add(MakeShared<FJsonValueString>(TEXT("DestructionStage")));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("componentAdded"), true);
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Destruction component added"), Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("add_destruction_component is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
