#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Misc/EngineVersionComparison.h"

#include "AI/NavigationSystemBase.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "NavAreas/NavArea.h"
#include "NavAreas/NavArea_Default.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavLinkCustomComponent.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavModifierComponent.h"
#include "Navigation/NavLinkProxy.h"
#include "NavigationSystem.h"

namespace McpNavigationHandlers
{
inline bool IsValidActorName(const FString& Name)
{
    return !Name.IsEmpty()
        && !Name.Contains(TEXT(".."))
        && !Name.Contains(TEXT("/"))
        && !Name.Contains(TEXT("\\"))
        && !Name.Contains(TEXT(":"));
}

inline bool IsValidNavigationPath(const FString& Path)
{
    return !Path.IsEmpty() && IsValidAssetPath(Path);
}

// direction from the payload, any case, BothWays when absent; false for any other value (it used to mean BothWays).
inline bool ReadNavLinkDirection(const TSharedPtr<FJsonObject>& Payload, ENavLinkDirection::Type& OutDirection)
{
    const FString Direction = GetJsonStringField(Payload, TEXT("direction"), TEXT("BothWays"));
    if (Direction.Equals(TEXT("LeftToRight"), ESearchCase::IgnoreCase))
    {
        OutDirection = ENavLinkDirection::LeftToRight;
        return true;
    }
    if (Direction.Equals(TEXT("RightToLeft"), ESearchCase::IgnoreCase))
    {
        OutDirection = ENavLinkDirection::RightToLeft;
        return true;
    }
    OutDirection = ENavLinkDirection::BothWays;
    return Direction.Equals(TEXT("BothWays"), ESearchCase::IgnoreCase);
}

// A smart link field the component keeps protected (its endpoints, direction and broadcast settings), by property name.
template <typename T>
T* NavLinkCustomField(UNavLinkCustomComponent* SmartComp, const TCHAR* Name)
{
    FProperty* Property = SmartComp ? UNavLinkCustomComponent::StaticClass()->FindPropertyByName(Name) : nullptr;
    return Property ? Property->ContainerPtrToValuePtr<T>(SmartComp) : nullptr;
}

inline ANavLinkProxy* FindNavLinkProxyByName(UWorld* World, const FString& ActorName)
{
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ANavLinkProxy> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            return *It;
        }
    }
    return nullptr;
}

// What a link proxy links, relative to itself.
struct FNavLinkEnds
{
    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    ENavLinkDirection::Type Direction = ENavLinkDirection::BothWays;
};

// Its smart link while that is in use, else its first point link.
FNavLinkEnds ReadNavLinkEnds(ANavLinkProxy* NavLink);

// A proxy is one kind of link at a time: its smart link alone, or its point links alone. A new proxy's default point link
// stayed beside an added one (a second way across, and the one configure then edited), and the smart link never became
// navigation-relevant, which the engine sets only on load or a details-panel edit. The caller updates the nav data.
void ApplyNavLink(ANavLinkProxy* NavLink, bool bSmart, const FNavLinkEnds& Ends);

// linkType, endpoints, direction, point link count and snap radius, and the actor handle for the receipt.
void DescribeNavLink(ANavLinkProxy* NavLink, TSharedPtr<FJsonObject>& Result);

// The NavLinkProxy the payload's actorName names in the editor world; replies and returns null when actorName is
// missing or unsafe, there is no editor world, or nothing matches.
ANavLinkProxy* ResolveNavLinkOrReply(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

// create_nav_link_proxy / create_smart_link: a NavLinkProxy at location linking startPoint to endPoint -- as its point
// link, or (bSmart) through its smart link component alone.
bool SpawnNavLink(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    bool bSmart);

bool HandleConfigureNavMeshSettings(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleSetNavAgentProperties(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleRebuildNavigation(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleCreateNavModifierComponent(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleSetNavAreaClass(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleConfigureNavAreaCost(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleCreateNavLinkProxy(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleConfigureNavLink(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleSetNavLinkType(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleCreateSmartLink(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleConfigureSmartLinkBehavior(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleGetNavigationInfo(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
}
