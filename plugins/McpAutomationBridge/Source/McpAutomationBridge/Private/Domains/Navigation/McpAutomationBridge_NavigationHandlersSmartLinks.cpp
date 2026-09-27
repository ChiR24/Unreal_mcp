#include "Domains/Navigation/McpAutomationBridge_NavigationHandlersPrivate.h"

namespace McpNavigationHandlers
{
bool HandleCreateSmartLink(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return SpawnNavLink(Self, RequestId, Payload, Socket, true);
}

bool HandleConfigureSmartLinkBehavior(
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
        SmartComp->SetEnabled(GetJsonBoolField(Payload, TEXT("linkEnabled"), true));
        bModified = true;
    }
    if (Payload->HasField(TEXT("enabledAreaClass")))
    {
        FString AreaClassPath = GetJsonStringField(Payload, TEXT("enabledAreaClass"));
        UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
        if (AreaClass)
        {
            SmartComp->SetEnabledArea(AreaClass);
            bModified = true;
        }
    }
    if (Payload->HasField(TEXT("disabledAreaClass")))
    {
        FString AreaClassPath = GetJsonStringField(Payload, TEXT("disabledAreaClass"));
        UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
        if (AreaClass)
        {
            SmartComp->SetDisabledArea(AreaClass);
            bModified = true;
        }
    }
    if (Payload->HasField(TEXT("broadcastRadius")) || Payload->HasField(TEXT("broadcastInterval")))
    {
        float Radius = GetJsonNumberField(Payload, TEXT("broadcastRadius"), 1000.0f);
        float Interval = GetJsonNumberField(Payload, TEXT("broadcastInterval"), 0.0f);
        SmartComp->SetBroadcastData(Radius, ECC_Pawn, Interval);
        bModified = true;
    }
    if (GetJsonBoolField(Payload, TEXT("bCreateBoxObstacle"), false))
    {
        FString ObstacleAreaPath = GetJsonStringField(Payload, TEXT("obstacleAreaClass"), TEXT("/Script/NavigationSystem.NavArea_Null"));
        UClass* ObstacleArea = LoadClass<UNavArea>(nullptr, *ObstacleAreaPath);
        FVector Extent = ExtractVectorField(Payload, TEXT("obstacleExtent"), FVector(100, 100, 100));
        FVector Offset = ExtractVectorField(Payload, TEXT("obstacleOffset"), FVector::ZeroVector);
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
