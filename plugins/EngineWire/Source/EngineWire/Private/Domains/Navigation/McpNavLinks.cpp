#include "Domains/Navigation/McpNavPrivate.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace McpNavigationHandlers
{
FNavLinkEnds ReadNavLinkEnds(ANavLinkProxy* NavLink)
{
    FNavLinkEnds Ends;
    UNavLinkCustomComponent* SmartComp = NavLink->GetSmartLinkComp();
    const FVector* Start = NavLinkCustomField<FVector>(SmartComp, TEXT("LinkRelativeStart"));
    const FVector* End = NavLinkCustomField<FVector>(SmartComp, TEXT("LinkRelativeEnd"));
    const TEnumAsByte<ENavLinkDirection::Type>* Direction = NavLinkCustomField<TEnumAsByte<ENavLinkDirection::Type>>(SmartComp, TEXT("LinkDirection"));
    if (NavLink->bSmartLinkIsRelevant && Start && End && Direction)
    {
        Ends.Start = *Start;
        Ends.End = *End;
        Ends.Direction = Direction->GetValue();
    }
    else if (NavLink->PointLinks.Num() > 0)
    {
        Ends.Start = NavLink->PointLinks[0].Left;
        Ends.End = NavLink->PointLinks[0].Right;
        Ends.Direction = NavLink->PointLinks[0].Direction;
    }
    return Ends;
}

void ApplyNavLink(ANavLinkProxy* NavLink, bool bSmart, const FNavLinkEnds& Ends)
{
    UNavLinkCustomComponent* SmartComp = NavLink->GetSmartLinkComp();
    NavLink->bSmartLinkIsRelevant = bSmart && SmartComp;
    if (NavLink->bSmartLinkIsRelevant)
    {
        NavLink->PointLinks.Reset();
        SmartComp->SetLinkData(Ends.Start, Ends.End, Ends.Direction);
    }
    else
    {
        if (NavLink->PointLinks.Num() == 0)
        {
            FNavigationLink Link;
            Link.SetAreaClass(UNavArea_Default::StaticClass());
            NavLink->PointLinks.Add(Link);
        }
        NavLink->PointLinks[0].Left = Ends.Start;
        NavLink->PointLinks[0].Right = Ends.End;
        NavLink->PointLinks[0].Direction = Ends.Direction;
    }
    if (SmartComp)
    {
        SmartComp->SetNavigationRelevancy(NavLink->bSmartLinkIsRelevant);
    }
}

void DescribeNavLink(ANavLinkProxy* NavLink, TSharedPtr<FJsonObject>& Result)
{
    const FNavLinkEnds Ends = ReadNavLinkEnds(NavLink);
    const TCHAR* Directions[] = {TEXT("BothWays"), TEXT("LeftToRight"), TEXT("RightToLeft")};
    Result->SetStringField(TEXT("actorName"), McpActorRef(NavLink));
    Result->SetStringField(TEXT("linkType"), NavLink->bSmartLinkIsRelevant ? TEXT("smart") : TEXT("simple"));
    Result->SetNumberField(TEXT("pointLinkCount"), NavLink->PointLinks.Num());
    Result->SetObjectField(TEXT("startPoint"), McpHandlerUtils::VectorToJson(Ends.Start));
    Result->SetObjectField(TEXT("endPoint"), McpHandlerUtils::VectorToJson(Ends.End));
    Result->SetStringField(TEXT("direction"), Directions[FMath::Clamp(static_cast<int32>(Ends.Direction), 0, 2)]);
    if (!NavLink->bSmartLinkIsRelevant && NavLink->PointLinks.Num() > 0)
    {
        Result->SetNumberField(TEXT("snapRadius"), NavLink->PointLinks[0].SnapRadius);
    }
    McpHandlerUtils::AddVerification(Result, NavLink);
}

ANavLinkProxy* ResolveNavLinkOrReply(
    UEngineWireSubsystem* Self,
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
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    bool bSmart)
{
    const auto Fail = [&](const FString& Message, const TCHAR* Code)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, Message, nullptr, Code);
        return true;
    };
    const TCHAR* Action = bSmart ? TEXT("create_smart_link") : TEXT("create_nav_link_proxy");
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FNavLinkEnds Ends;
    Ends.Start = ExtractVectorField(Payload, TEXT("startPoint"), FVector::ZeroVector);
    Ends.End = ExtractVectorField(Payload, TEXT("endPoint"), FVector::ZeroVector);
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!Payload->HasField(TEXT("location")))
    {
        return Fail(FString::Printf(TEXT("location is required for %s"), Action), TEXT("MISSING_PARAM"));
    }
    if (!Payload->HasField(TEXT("startPoint")) || !Payload->HasField(TEXT("endPoint")))
    {
        return Fail(FString::Printf(TEXT("startPoint and endPoint are required for %s to define the navigation link"), Action), TEXT("MISSING_PARAM"));
    }
    if (!ActorName.IsEmpty() && !IsValidActorName(ActorName))
    {
        return Fail(TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), TEXT("SECURITY_VIOLATION"));
    }
    if (!ReadNavLinkDirection(Payload, Ends.Direction))
    {
        return Fail(TEXT("direction must be BothWays, LeftToRight or RightToLeft."), TEXT("INVALID_ARGUMENT"));
    }
    if (!World)
    {
        return Fail(TEXT("No editor world available"), TEXT("NO_WORLD"));
    }
    // A second actor under one label left every later call by that name on whichever came first.
    if (!ActorName.IsEmpty() && FindActorByNameInWorldForMcp(World, ActorName, true))
    {
        return Fail(FString::Printf(TEXT("An actor named %s is already in the level; pick another actorName. Nothing was created."), *ActorName), TEXT("NAME_TAKEN"));
    }

    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Create Nav Link")), EMcpMutationDurability::EditorStateOnly, TArray<UObject*>());
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ANavLinkProxy* NavLink = World->SpawnActor<ANavLinkProxy>(ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector),
        ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator), SpawnParams);
    if (!NavLink)
    {
        return Fail(TEXT("Failed to spawn NavLinkProxy"), TEXT("SPAWN_FAILED"));
    }
    NavLink->SetActorLabel(ActorName.IsEmpty() ? NavLink->GetName() : ActorName);
    ApplyNavLink(NavLink, bSmart, Ends);
    FNavigationSystem::UpdateActorData(*NavLink);
    NavLink->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    DescribeNavLink(NavLink, Result);
    Result->SetStringField(TEXT("actorPath"), NavLink->GetPathName());
    Transaction.DescribeInto(Result);
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%s '%s' created"), bSmart ? TEXT("Smart nav link") : TEXT("NavLinkProxy"), *McpActorRef(NavLink)), Result);
    return true;
}

bool HandleCreateNavLinkProxy(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return SpawnNavLink(Self, RequestId, Payload, Socket, false);
}

bool HandleConfigureNavLink(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    ANavLinkProxy* NavLink = ResolveNavLinkOrReply(Self, RequestId, Payload, Socket);
    if (!NavLink)
    {
        return true;
    }
    // direction and snapRadius were read only beside a start or end point, so a call with either alone changed nothing.
    FNavLinkEnds Ends = ReadNavLinkEnds(NavLink);
    const bool bSnap = Payload->HasField(TEXT("snapRadius"));
    FString Error;
    if (!bSnap && !Payload->HasField(TEXT("startPoint")) && !Payload->HasField(TEXT("endPoint")) && !Payload->HasField(TEXT("direction")))
    {
        Error = TEXT("Nothing to set: send startPoint, endPoint, direction or snapRadius.");
    }
    else if (Payload->HasField(TEXT("direction")) && !ReadNavLinkDirection(Payload, Ends.Direction))
    {
        Error = TEXT("direction must be BothWays, LeftToRight or RightToLeft.");
    }
    else if (bSnap && NavLink->bSmartLinkIsRelevant)
    {
        Error = TEXT("snapRadius belongs to point links, and this proxy uses its smart link; set_nav_link_type simple first.");
    }
    if (!Error.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false, Error, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Configure Nav Link")), EMcpMutationDurability::EditorStateOnly,
        {NavLink, NavLink->GetSmartLinkComp()});
    Ends.Start = ExtractVectorField(Payload, TEXT("startPoint"), Ends.Start);
    Ends.End = ExtractVectorField(Payload, TEXT("endPoint"), Ends.End);
    ApplyNavLink(NavLink, NavLink->bSmartLinkIsRelevant, Ends);
    if (bSnap)
    {
        NavLink->PointLinks[0].SnapRadius = static_cast<float>(GetJsonNumberField(Payload, TEXT("snapRadius"), 30.0));
    }
    FNavigationSystem::UpdateActorData(*NavLink);
    NavLink->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    DescribeNavLink(NavLink, Result);
    Transaction.DescribeInto(Result);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("NavLink configured"), Result);
    return true;
}
}
