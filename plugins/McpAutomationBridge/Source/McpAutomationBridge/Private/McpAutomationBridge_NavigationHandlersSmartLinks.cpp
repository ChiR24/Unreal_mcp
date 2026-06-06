#include "McpAutomationBridge_NavigationHandlersPrivate.h"

#if WITH_EDITOR
namespace McpNavigationHandlers
{
bool HandleCreateSmartLink(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"), TEXT("SmartNavLink"));
    FVector Location = GetJsonVectorFieldNav(Payload, TEXT("location"));
    FRotator Rotation = GetJsonRotatorFieldNav(Payload, TEXT("rotation"));
    FVector StartPoint = GetJsonVectorFieldNav(Payload, TEXT("startPoint"), FVector(-100, 0, 0));
    FVector EndPoint = GetJsonVectorFieldNav(Payload, TEXT("endPoint"), FVector(100, 0, 0));

    if (!Payload->HasField(TEXT("location")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("location is required for create_smart_link"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }
    if (!Payload->HasField(TEXT("startPoint")) || !Payload->HasField(TEXT("endPoint")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("startPoint and endPoint are required for create_smart_link to define the navigation link"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }
    if (!IsValidActorName(ActorName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ANavLinkProxy* NavLink = World->SpawnActor<ANavLinkProxy>(Location, Rotation, SpawnParams);
    if (!NavLink)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn NavLinkProxy"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    NavLink->SetActorLabel(*ActorName);
    NavLink->bSmartLinkIsRelevant = true;
    UNavLinkCustomComponent* SmartComp = NavLink->GetSmartLinkComp();
    if (SmartComp)
    {
        SmartComp->SetLinkData(StartPoint, EndPoint, ParseNavLinkDirection(GetJsonStringFieldNav(Payload, TEXT("direction"), TEXT("BothWays"))));
        SmartComp->SetEnabled(true);
    }
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), NavLink->GetActorLabel());
    Result->SetStringField(TEXT("actorPath"), NavLink->GetPathName());
    Result->SetBoolField(TEXT("bSmartLinkIsRelevant"), true);
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Smart NavLink '%s' created"), *ActorName), Result);
    return true;
}

bool HandleConfigureSmartLinkBehavior(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }
    if (!IsValidActorName(ActorName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    ANavLinkProxy* NavLink = FindNavLinkProxyByName(World, ActorName);
    if (!NavLink)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("NavLinkProxy not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    UNavLinkCustomComponent* SmartComp = NavLink->GetSmartLinkComp();
    if (!SmartComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("NavLinkProxy has no smart link component"), nullptr, TEXT("NO_SMART_LINK"));
        return true;
    }

    bool bModified = false;
    if (Payload->HasField(TEXT("linkEnabled")))
    {
        SmartComp->SetEnabled(GetJsonBoolFieldNav(Payload, TEXT("linkEnabled"), true));
        bModified = true;
    }
    if (Payload->HasField(TEXT("enabledAreaClass")))
    {
        FString AreaClassPath = GetJsonStringFieldNav(Payload, TEXT("enabledAreaClass"));
        UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
        if (AreaClass)
        {
            SmartComp->SetEnabledArea(AreaClass);
            bModified = true;
        }
    }
    if (Payload->HasField(TEXT("disabledAreaClass")))
    {
        FString AreaClassPath = GetJsonStringFieldNav(Payload, TEXT("disabledAreaClass"));
        UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
        if (AreaClass)
        {
            SmartComp->SetDisabledArea(AreaClass);
            bModified = true;
        }
    }
    if (Payload->HasField(TEXT("broadcastRadius")) || Payload->HasField(TEXT("broadcastInterval")))
    {
        float Radius = GetJsonNumberFieldNav(Payload, TEXT("broadcastRadius"), 1000.0f);
        float Interval = GetJsonNumberFieldNav(Payload, TEXT("broadcastInterval"), 0.0f);
        SmartComp->SetBroadcastData(Radius, ECC_Pawn, Interval);
        bModified = true;
    }
    if (GetJsonBoolFieldNav(Payload, TEXT("bCreateBoxObstacle"), false))
    {
        FString ObstacleAreaPath = GetJsonStringFieldNav(Payload, TEXT("obstacleAreaClass"), TEXT("/Script/NavigationSystem.NavArea_Null"));
        UClass* ObstacleArea = LoadClass<UNavArea>(nullptr, *ObstacleAreaPath);
        FVector Extent = GetJsonVectorFieldNav(Payload, TEXT("obstacleExtent"), FVector(100, 100, 100));
        FVector Offset = GetJsonVectorFieldNav(Payload, TEXT("obstacleOffset"));
        if (ObstacleArea)
        {
            SmartComp->AddNavigationObstacle(ObstacleArea, Extent, Offset);
            bModified = true;
        }
    }

    if (bModified)
    {
        World->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetBoolField(TEXT("linkEnabled"), SmartComp->IsEnabled());
    Result->SetBoolField(TEXT("modified"), bModified);
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Smart link behavior configured"), Result);
    return true;
}
}
#endif
