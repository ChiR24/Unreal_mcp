#include "Domains/Navigation/McpNavPrivate.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace McpNavigationHandlers
{
bool HandleCreateSmartLink(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return SpawnNavLink(Self, RequestId, Payload, Socket, true);
}

bool HandleSetNavLinkType(
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
    // Any linkType but "smart" used to mean simple, so "Smart" turned the smart link off.
    const FString LinkType = GetJsonStringField(Payload, TEXT("linkType"));
    const bool bSmart = LinkType.Equals(TEXT("smart"), ESearchCase::IgnoreCase);
    if (!bSmart && !LinkType.Equals(TEXT("simple"), ESearchCase::IgnoreCase))
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("linkType must be simple or smart."), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    const bool bChanged = NavLink->bSmartLinkIsRelevant != bSmart;
    if (bChanged)
    {
        // The endpoints and direction carry over from one kind of link to the other.
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Nav Link Type")), EMcpMutationDurability::EditorStateOnly,
            {NavLink, NavLink->GetSmartLinkComp()});
        ApplyNavLink(NavLink, bSmart, ReadNavLinkEnds(NavLink));
        FNavigationSystem::UpdateActorData(*NavLink);
        NavLink->MarkPackageDirty();
        Transaction.DescribeInto(Result);
    }
    else
    {
        McpHandlerUtils::MarkNoAssetsChanged(Result);
    }
    DescribeNavLink(NavLink, Result);
    Result->SetBoolField(TEXT("changed"), bChanged);
    Self->SendAutomationResponse(Socket, RequestId, true, bChanged
        ? FString::Printf(TEXT("NavLink type set to %s"), bSmart ? TEXT("smart") : TEXT("simple"))
        : FString(TEXT("NavLink already had that type; nothing changed")), Result);
    return true;
}

bool HandleConfigureSmartLinkBehavior(
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
    UNavLinkCustomComponent* SmartComp = NavLink->GetSmartLinkComp();
    if (!SmartComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("NavLinkProxy has no smart link component"), nullptr, TEXT("NO_SMART_LINK"));
        return true;
    }

    // An area class that did not load was skipped while the call still answered success; every value is checked first.
    const auto LoadArea = [&Payload](const TCHAR* Field, const TCHAR* Default, UClass*& OutArea)
    {
        const FString Path = GetJsonStringField(Payload, Field, Default);
        OutArea = Path.IsEmpty() ? nullptr : LoadClass<UNavArea>(nullptr, *Path);
        return OutArea != nullptr;
    };
    UClass* EnabledArea = nullptr;
    UClass* DisabledArea = nullptr;
    UClass* ObstacleArea = nullptr;
    const bool bObstacle = GetJsonBoolField(Payload, TEXT("bCreateBoxObstacle"), false);
    const bool bBroadcast = Payload->HasField(TEXT("broadcastRadius")) || Payload->HasField(TEXT("broadcastInterval"));
    const TCHAR* BadField = nullptr;
    if (Payload->HasField(TEXT("enabledAreaClass")) && !LoadArea(TEXT("enabledAreaClass"), TEXT(""), EnabledArea))
    {
        BadField = TEXT("enabledAreaClass");
    }
    else if (Payload->HasField(TEXT("disabledAreaClass")) && !LoadArea(TEXT("disabledAreaClass"), TEXT(""), DisabledArea))
    {
        BadField = TEXT("disabledAreaClass");
    }
    else if (bObstacle && !LoadArea(TEXT("obstacleAreaClass"), TEXT("/Script/NavigationSystem.NavArea_Null"), ObstacleArea))
    {
        BadField = TEXT("obstacleAreaClass");
    }
    if (BadField)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("%s is not a NavArea class: %s. Nothing was changed."),
            BadField, *GetJsonStringField(Payload, BadField)), nullptr, TEXT("INVALID_AREA_CLASS"));
        return true;
    }
    if (!Payload->HasField(TEXT("linkEnabled")) && !EnabledArea && !DisabledArea && !bBroadcast && !bObstacle)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Nothing to set: send linkEnabled, enabledAreaClass, disabledAreaClass, "
            "broadcastRadius, broadcastInterval or bCreateBoxObstacle true."), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Configure Smart Link")), EMcpMutationDurability::EditorStateOnly, {SmartComp});
        if (Payload->HasField(TEXT("linkEnabled")))
        {
            SmartComp->SetEnabled(GetJsonBoolField(Payload, TEXT("linkEnabled"), true));
        }
        if (EnabledArea)
        {
            SmartComp->SetEnabledArea(EnabledArea);
        }
        if (DisabledArea)
        {
            SmartComp->SetDisabledArea(DisabledArea);
        }
        if (bBroadcast)
        {
            // The setter takes radius, channel and interval together: the one left out keeps its value, where it was
            // reset to 1000 or 0 and the channel to Pawn.
            const float* Radius = NavLinkCustomField<float>(SmartComp, TEXT("BroadcastRadius"));
            const float* Interval = NavLinkCustomField<float>(SmartComp, TEXT("BroadcastInterval"));
            const TEnumAsByte<ECollisionChannel>* Channel = NavLinkCustomField<TEnumAsByte<ECollisionChannel>>(SmartComp, TEXT("BroadcastChannel"));
            SmartComp->SetBroadcastData(
                static_cast<float>(GetJsonNumberField(Payload, TEXT("broadcastRadius"), Radius ? *Radius : 1000.0f)),
                Channel ? Channel->GetValue() : ECC_Pawn,
                static_cast<float>(GetJsonNumberField(Payload, TEXT("broadcastInterval"), Interval ? *Interval : 0.0f)));
        }
        if (bObstacle)
        {
            SmartComp->AddNavigationObstacle(ObstacleArea, ExtractVectorField(Payload, TEXT("obstacleExtent"), FVector(100, 100, 100)),
                ExtractVectorField(Payload, TEXT("obstacleOffset"), FVector::ZeroVector));
        }
        NavLink->MarkPackageDirty();
        Transaction.DescribeInto(Result);
    }

    Result->SetStringField(TEXT("actorName"), McpActorRef(NavLink));
    Result->SetBoolField(TEXT("linkEnabled"), SmartComp->IsEnabled());
    Result->SetBoolField(TEXT("smartLinkInUse"), NavLink->bSmartLinkIsRelevant);
    Result->SetStringField(TEXT("enabledAreaClass"), GetPathNameSafe(SmartComp->GetEnabledArea().Get()));
    Result->SetStringField(TEXT("disabledAreaClass"), GetPathNameSafe(SmartComp->GetDisabledArea().Get()));
    if (const float* Radius = NavLinkCustomField<float>(SmartComp, TEXT("BroadcastRadius")))
    {
        Result->SetNumberField(TEXT("broadcastRadius"), *Radius);
    }
    if (const float* Interval = NavLinkCustomField<float>(SmartComp, TEXT("BroadcastInterval")))
    {
        Result->SetNumberField(TEXT("broadcastInterval"), *Interval);
    }
    McpHandlerUtils::AddVerification(Result, NavLink);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Smart link behavior configured"), Result);
    return true;
}
}
