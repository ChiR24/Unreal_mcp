#include "Domains/Navigation/McpAutomationBridge_NavigationHandlersPrivate.h"

namespace
{
// create_nav_modifier_component without a Blueprint: an instance NavModifierComponent on a placed actor.
bool AddNavModifierToPlacedActor(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& ActorName, const FString& ComponentName, UClass* AreaClass, const FVector& FailsafeExtent)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    AActor* Actor = World ? FindActorByNameInWorldForMcp(World, ActorName, true) : nullptr;
    if (!Actor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found in the editor world: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    if (FindObject<UObject>(Actor, *ComponentName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Component '%s' already exists on %s"), *ComponentName, *ActorName), nullptr, TEXT("ALREADY_EXISTS"));
        return true;
    }
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
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("NavModifierComponent '%s' added to actor %s"), *ComponentName, *Actor->GetActorLabel()), Result);
    return true;
}
}

namespace McpNavigationHandlers
{
bool HandleCreateNavModifierComponent(
    UMcpAutomationBridgeSubsystem* Self,
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
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Pass blueprintPath (a Blueprint asset) or actorName (a placed actor)"), nullptr, TEXT("MISSING_PARAM"));
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
    // silently falling back to the default area (dogfood #61).
    UClass* ResolvedAreaClass = nullptr;
    if (!AreaClassPath.IsEmpty())
    {
        ResolvedAreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
        if (!ResolvedAreaClass)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("areaClass not found or not a UNavArea subclass: %s"), *AreaClassPath), nullptr, TEXT("INVALID_AREA_CLASS"));
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
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Blueprint has no SimpleConstructionScript"), nullptr, TEXT("INVALID_BP"));
        return true;
    }

    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Component '%s' already exists"), *ComponentName), nullptr, TEXT("ALREADY_EXISTS"));
            return true;
        }
    }

    USCS_Node* NewNode = SCS->CreateNode(UNavModifierComponent::StaticClass(), *ComponentName);
    if (!NewNode)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create SCS node"), nullptr, TEXT("CREATE_FAILED"));
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

    SCS->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (GetJsonBoolField(Payload, TEXT("save"), false))
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    McpHandlerUtils::AddVerification(Result, Blueprint);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("NavModifierComponent '%s' added to Blueprint"), *ComponentName), Result);
    return true;
}

bool HandleSetNavAreaClass(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    FString AreaClassPath = GetJsonStringField(Payload, TEXT("areaClass"));

    if (ActorName.IsEmpty() || AreaClassPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("actorName and areaClass are required"), nullptr, TEXT("MISSING_PARAM"));
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

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AActor* TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }
    if (!TargetActor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
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
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("NavModifierComponent '%s' not found on actor"), *ComponentName), nullptr, TEXT("NO_COMPONENT"));
            return true;
        }
    }
    else if (Components.Num() > 0)
    {
        ModComp = Components[0];
    }

    if (!ModComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No NavModifierComponent found on actor"), nullptr, TEXT("NO_COMPONENT"));
        return true;
    }

    UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
    if (!AreaClass)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("NavArea class not found: %s"), *AreaClassPath), nullptr, TEXT("INVALID_CLASS"));
        return true;
    }

    ModComp->Modify();
    ModComp->SetAreaClass(AreaClass);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("componentName"), ModComp->GetName());
    Result->SetStringField(TEXT("areaClass"), AreaClassPath);
    McpHandlerUtils::AddVerification(Result, TargetActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Nav area class set"), Result);
    return true;
}
}
