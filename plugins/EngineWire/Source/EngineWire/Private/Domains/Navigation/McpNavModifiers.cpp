#include "Domains/Navigation/McpNavPrivate.h"
#include "Foundation/BridgeHelpers/Blueprints/McpScsLookup.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace
{
// create_nav_modifier_component without a Blueprint: an instance NavModifierComponent on a placed actor.
bool AddNavModifierToPlacedActor(
    UEngineWireSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& ActorName, const FString& ComponentName, UClass* AreaClass, const FVector& FailsafeExtent)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    AActor* Actor = World ? FindActorByNameInWorldForMcp(World, ActorName, true) : nullptr;
    if (!Actor)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Actor not found in the editor world: %s"), *ActorName), TEXT("NOT_FOUND"));
        return true;
    }
    if (FindObject<UObject>(Actor, *ComponentName))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Component '%s' already exists on %s"), *ComponentName, *ActorName), TEXT("ALREADY_EXISTS"));
        return true;
    }
    // One undo step, opened before the component is made.
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add Nav Modifier")), EMcpMutationDurability::EditorStateOnly, {Actor});
    Actor->Modify();
    UNavModifierComponent* ModComp = NewObject<UNavModifierComponent>(Actor, UNavModifierComponent::StaticClass(), FName(*ComponentName), RF_Transactional);
    ModComp->FailsafeExtent = FailsafeExtent;
    if (AreaClass)
    {
        ModComp->SetAreaClass(AreaClass);
    }
    Actor->AddInstanceComponent(ModComp);
    ModComp->RegisterComponent();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("componentName"), ModComp->GetName());
    Result->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    McpHandlerUtils::AddVerification(Result, Actor);
    Transaction.DescribeInto(Result);
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("NavModifierComponent '%s' added to actor %s"), *ComponentName, *Actor->GetActorLabel()), Result);
    return true;
}
}

namespace McpNavigationHandlers
{
bool HandleCreateNavModifierComponent(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("NavModifier"));
    FString AreaClassPath = GetJsonStringField(Payload, TEXT("areaClass"));
    FVector FailsafeExtent = ExtractVectorField(Payload, TEXT("failsafeExtent"), FVector(100, 100, 100));

    if (BlueprintPath.IsEmpty() && ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Pass blueprintPath (a Blueprint asset) or actorName (a placed actor)"), TEXT("MISSING_PARAM"));
        return true;
    }
    if (!BlueprintPath.IsEmpty() && !IsValidNavigationPath(BlueprintPath))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid blueprintPath: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }
    if (!AreaClassPath.IsEmpty() && !IsValidNavigationPath(AreaClassPath))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid areaClass: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }
    // Resolve the area class up front so an unknown class is refused instead of
    // silently falling back to the default area.
    UClass* ResolvedAreaClass = nullptr;
    if (!AreaClassPath.IsEmpty())
    {
        ResolvedAreaClass = McpLoadNavAreaClass(AreaClassPath);
        if (!ResolvedAreaClass)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("areaClass not found or not a UNavArea subclass: %s"), *AreaClassPath), TEXT("INVALID_AREA_CLASS"));
            return true;
        }
    }
    if (BlueprintPath.IsEmpty())
    {
        return AddNavModifierToPlacedActor(Self, RequestId, Socket, ActorName, ComponentName, ResolvedAreaClass, FailsafeExtent);
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_BP"));
        return true;
    }

    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Component '%s' already exists"), *ComponentName), TEXT("ALREADY_EXISTS"));
            return true;
        }
    }

    USCS_Node* NewNode = McpAddScsNode(SCS, UNavModifierComponent::StaticClass(), *ComponentName);
    if (!NewNode)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to create SCS node"), TEXT("CREATE_FAILED"));
        return true;
    }

    UNavModifierComponent* ModComp = Cast<UNavModifierComponent>(NewNode->ComponentTemplate);
    if (ModComp)
    {
        ModComp->FailsafeExtent = FailsafeExtent;
        if (ResolvedAreaClass)
        {
            ModComp->AreaClass = ResolvedAreaClass;
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    const bool bSaved = GetJsonBoolField(Payload, TEXT("save"), false) && McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("componentName"), NewNode->GetVariableName().ToString());
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Result, Blueprint);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("NavModifierComponent '%s' added to Blueprint"), *ComponentName), Result);
    return true;
}

bool HandleSetNavAreaClass(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    FString AreaClassPath = GetJsonStringField(Payload, TEXT("areaClass"));

    if (ActorName.IsEmpty() || AreaClassPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and areaClass are required"), TEXT("MISSING_PARAM"));
        return true;
    }
    if (!IsValidActorName(ActorName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }
    if (!IsValidNavigationPath(AreaClassPath))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid areaClass: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No editor world available"), TEXT("NO_WORLD"));
        return true;
    }

    AActor* TargetActor = FindActorByNameInWorldForMcp(World, ActorName, true);
    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("NOT_FOUND"));
        return true;
    }

    UNavModifierComponent* ModComp = nullptr;
    TArray<UNavModifierComponent*> Components;
    TargetActor->GetComponents<UNavModifierComponent>(Components);
    if (!ComponentName.IsEmpty())
    {
        for (UNavModifierComponent* Comp : Components)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                ModComp = Comp;
                break;
            }
        }
        if (!ModComp)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("NavModifierComponent '%s' not found on actor"), *ComponentName), TEXT("NO_COMPONENT"));
            return true;
        }
    }
    else if (Components.Num() > 0)
    {
        ModComp = Components[0];
    }

    if (!ModComp)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No NavModifierComponent found on actor"), TEXT("NO_COMPONENT"));
        return true;
    }

    UClass* AreaClass = McpLoadNavAreaClass(AreaClassPath);
    if (!AreaClass)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("NavArea class not found: %s"), *AreaClassPath), TEXT("INVALID_CLASS"));
        return true;
    }

    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Nav Area Class")), EMcpMutationDurability::EditorStateOnly, {ModComp});
    ModComp->Modify();
    ModComp->SetAreaClass(AreaClass);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("componentName"), ModComp->GetName());
    Result->SetStringField(TEXT("areaClass"), AreaClassPath);
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Transaction.DescribeInto(Result);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Nav area class set"), Result);
    return true;
}
}
