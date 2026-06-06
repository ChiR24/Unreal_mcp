#include "McpAutomationBridge_NavigationHandlersPrivate.h"

#if WITH_EDITOR
namespace McpNavigationHandlers
{
bool HandleCreateNavLinkProxy(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"), TEXT("NavLinkProxy"));
    FVector Location = GetJsonVectorFieldNav(Payload, TEXT("location"));
    FRotator Rotation = GetJsonRotatorFieldNav(Payload, TEXT("rotation"));
    FVector StartPoint = GetJsonVectorFieldNav(Payload, TEXT("startPoint"), FVector(-100, 0, 0));
    FVector EndPoint = GetJsonVectorFieldNav(Payload, TEXT("endPoint"), FVector(100, 0, 0));

    if (!Payload->HasField(TEXT("location")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("location is required for create_nav_link_proxy"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }
    if (!Payload->HasField(TEXT("startPoint")) || !Payload->HasField(TEXT("endPoint")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("startPoint and endPoint are required for create_nav_link_proxy to define the navigation link"), nullptr, TEXT("MISSING_PARAM"));
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
    FNavigationLink NewLink;
    NewLink.Left = StartPoint;
    NewLink.Right = EndPoint;
    NewLink.Direction = ParseNavLinkDirection(GetJsonStringFieldNav(Payload, TEXT("direction"), TEXT("BothWays")));
    NavLink->PointLinks.Add(NewLink);
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), NavLink->GetActorLabel());
    Result->SetStringField(TEXT("actorPath"), NavLink->GetPathName());
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("NavLinkProxy '%s' created"), *ActorName), Result);
    return true;
}

bool HandleConfigureNavLink(
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

    bool bModified = false;
    if (Payload->HasField(TEXT("startPoint")) || Payload->HasField(TEXT("endPoint")))
    {
        if (NavLink->PointLinks.Num() == 0)
        {
            NavLink->PointLinks.Add(FNavigationLink());
        }

        FNavigationLink& Link = NavLink->PointLinks[0];
        if (Payload->HasField(TEXT("startPoint")))
        {
            Link.Left = GetJsonVectorFieldNav(Payload, TEXT("startPoint"));
            bModified = true;
        }
        if (Payload->HasField(TEXT("endPoint")))
        {
            Link.Right = GetJsonVectorFieldNav(Payload, TEXT("endPoint"));
            bModified = true;
        }
        if (Payload->HasField(TEXT("direction")))
        {
            Link.Direction = ParseNavLinkDirection(GetJsonStringFieldNav(Payload, TEXT("direction"), TEXT("BothWays")));
            bModified = true;
        }
        if (Payload->HasField(TEXT("snapRadius")))
        {
            Link.SnapRadius = GetJsonNumberFieldNav(Payload, TEXT("snapRadius"), 30.0f);
            bModified = true;
        }
    }

    if (bModified)
    {
        World->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetBoolField(TEXT("modified"), bModified);
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("NavLink configured"), Result);
    return true;
}

bool HandleSetNavLinkType(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"));
    FString LinkType = GetJsonStringFieldNav(Payload, TEXT("linkType"), TEXT("simple"));
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

    bool bSmartLink = (LinkType == TEXT("smart"));
    NavLink->bSmartLinkIsRelevant = bSmartLink;
    if (bSmartLink)
    {
        UNavLinkCustomComponent* SmartComp = NavLink->GetSmartLinkComp();
        if (SmartComp)
        {
            SmartComp->SetEnabled(true);
        }
    }
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("linkType"), LinkType);
    Result->SetBoolField(TEXT("bSmartLinkIsRelevant"), NavLink->bSmartLinkIsRelevant);
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("NavLink type set to %s"), *LinkType), Result);
    return true;
}
}
#endif
