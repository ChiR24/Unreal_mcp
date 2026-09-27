#include "Domains/Navigation/McpAutomationBridge_NavigationHandlersPrivate.h"

namespace McpNavigationHandlers
{
ANavLinkProxy* ResolveNavLinkOrReply(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
    }
    else if (!IsValidActorName(ActorName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
    }
    else if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
    }
    else if (ANavLinkProxy* NavLink = FindNavLinkProxyByName(World, ActorName))
    {
        return NavLink;
    }
    else
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("NavLinkProxy not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
    }
    return nullptr;
}

bool SpawnNavLink(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    bool bSmart)
{
    const TCHAR* Action = bSmart ? TEXT("create_smart_link") : TEXT("create_nav_link_proxy");
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), bSmart ? TEXT("SmartNavLink") : TEXT("NavLinkProxy"));
    FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector StartPoint = ExtractVectorField(Payload, TEXT("startPoint"), FVector(-100, 0, 0));
    FVector EndPoint = ExtractVectorField(Payload, TEXT("endPoint"), FVector(100, 0, 0));

    if (!Payload->HasField(TEXT("location")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("location is required for %s"), Action), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }
    if (!Payload->HasField(TEXT("startPoint")) || !Payload->HasField(TEXT("endPoint")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("startPoint and endPoint are required for %s to define the navigation link"), Action), nullptr, TEXT("MISSING_PARAM"));
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
    const ENavLinkDirection::Type Direction = ParseNavLinkDirection(GetJsonStringField(Payload, TEXT("direction"), TEXT("BothWays")));
    if (bSmart)
    {
        NavLink->bSmartLinkIsRelevant = true;
        if (UNavLinkCustomComponent* SmartComp = NavLink->GetSmartLinkComp())
        {
            SmartComp->SetLinkData(StartPoint, EndPoint, Direction);
            SmartComp->SetEnabled(true);
        }
    }
    else
    {
        FNavigationLink NewLink;
        NewLink.Left = StartPoint;
        NewLink.Right = EndPoint;
        NewLink.Direction = Direction;
        NavLink->PointLinks.Add(NewLink);
    }
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), McpActorRef(NavLink));
    Result->SetStringField(TEXT("actorPath"), NavLink->GetPathName());
    if (bSmart)
    {
        Result->SetBoolField(TEXT("bSmartLinkIsRelevant"), true);
    }
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true,
        bSmart ? FString::Printf(TEXT("Smart NavLink '%s' created"), *ActorName)
               : FString::Printf(TEXT("NavLinkProxy '%s' created"), *ActorName), Result);
    return true;
}

bool HandleCreateNavLinkProxy(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return SpawnNavLink(Self, RequestId, Payload, Socket, false);
}

bool HandleConfigureNavLink(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    ANavLinkProxy* NavLink = ResolveNavLinkOrReply(Self, RequestId, Payload, Socket);
    if (!NavLink)
    {
        return true;
    }
    UWorld* World = NavLink->GetWorld();
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

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
            Link.Left = ExtractVectorField(Payload, TEXT("startPoint"), FVector::ZeroVector);
            bModified = true;
        }
        if (Payload->HasField(TEXT("endPoint")))
        {
            Link.Right = ExtractVectorField(Payload, TEXT("endPoint"), FVector::ZeroVector);
            bModified = true;
        }
        if (Payload->HasField(TEXT("direction")))
        {
            Link.Direction = ParseNavLinkDirection(GetJsonStringField(Payload, TEXT("direction"), TEXT("BothWays")));
            bModified = true;
        }
        if (Payload->HasField(TEXT("snapRadius")))
        {
            Link.SnapRadius = GetJsonNumberField(Payload, TEXT("snapRadius"), 30.0f);
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
    ANavLinkProxy* NavLink = ResolveNavLinkOrReply(Self, RequestId, Payload, Socket);
    if (!NavLink)
    {
        return true;
    }
    UWorld* World = NavLink->GetWorld();
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString LinkType = GetJsonStringField(Payload, TEXT("linkType"), TEXT("simple"));

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
