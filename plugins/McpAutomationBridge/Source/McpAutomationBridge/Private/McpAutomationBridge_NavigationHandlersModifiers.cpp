#include "McpAutomationBridge_NavigationHandlersPrivate.h"

#if WITH_EDITOR
namespace McpNavigationHandlers
{
bool HandleCreateNavModifierComponent(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringFieldNav(Payload, TEXT("blueprintPath"));
    FString ComponentName = GetJsonStringFieldNav(Payload, TEXT("componentName"), TEXT("NavModifier"));
    FString AreaClassPath = GetJsonStringFieldNav(Payload, TEXT("areaClass"));
    FVector FailsafeExtent = GetJsonVectorFieldNav(Payload, TEXT("failsafeExtent"), FVector(100, 100, 100));

    if (BlueprintPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("blueprintPath is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }
    if (!IsValidNavigationPath(BlueprintPath))
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
        if (!AreaClassPath.IsEmpty())
        {
            UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
            if (AreaClass)
            {
                ModComp->AreaClass = AreaClass;
            }
        }
    }

    SCS->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (GetJsonBoolFieldNav(Payload, TEXT("save"), false))
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetBoolField(TEXT("existsAfter"), true);
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
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringFieldNav(Payload, TEXT("componentName"));
    FString AreaClassPath = GetJsonStringFieldNav(Payload, TEXT("areaClass"));

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

    ModComp->SetAreaClass(AreaClass);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("areaClass"), AreaClassPath);
    McpHandlerUtils::AddVerification(Result, TargetActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Nav area class set"), Result);
    return true;
}

bool HandleConfigureNavAreaCost(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AreaClassPath = GetJsonStringFieldNav(Payload, TEXT("areaClass"));
    double AreaCost = GetJsonNumberFieldNav(Payload, TEXT("areaCost"), 1.0);
    if (AreaClassPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("areaClass is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }
    if (!IsValidNavigationPath(AreaClassPath))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid areaClass: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
    if (!AreaClass)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("NavArea class not found: %s"), *AreaClassPath), nullptr, TEXT("INVALID_CLASS"));
        return true;
    }

    UNavArea* AreaCDO = AreaClass->GetDefaultObject<UNavArea>();
    if (!AreaCDO)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Could not get NavArea CDO"), nullptr, TEXT("CDO_FAILED"));
        return true;
    }

    AreaCDO->DefaultCost = AreaCost;

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("areaClass"), AreaClassPath);
    Result->SetNumberField(TEXT("areaCost"), AreaCost);
    Result->SetNumberField(TEXT("fixedAreaEnteringCost"), AreaCDO->GetFixedAreaEnteringCost());
    Result->SetBoolField(TEXT("existsAfter"), true);

    FString Message = TEXT("Nav area cost configured");
    if (Payload->HasField(TEXT("fixedAreaEnteringCost")))
    {
        Message = TEXT("Nav area cost configured (note: fixedAreaEnteringCost is read-only and was not modified)");
        Result->SetBoolField(TEXT("fixedAreaEnteringCostIgnored"), true);
    }

    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}
}
#endif
