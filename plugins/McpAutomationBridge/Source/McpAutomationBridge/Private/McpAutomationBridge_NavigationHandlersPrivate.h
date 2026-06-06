#pragma once

#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
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
inline FString GetJsonStringFieldNav(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FString& Default = TEXT(""))
{
    if (!Payload.IsValid())
    {
        return Default;
    }
    FString Value;
    return Payload->TryGetStringField(FieldName, Value) ? Value : Default;
}

inline double GetJsonNumberFieldNav(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, double Default = 0.0)
{
    if (!Payload.IsValid())
    {
        return Default;
    }
    double Value;
    return Payload->TryGetNumberField(FieldName, Value) ? Value : Default;
}

inline bool GetJsonBoolFieldNav(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, bool Default = false)
{
    if (!Payload.IsValid())
    {
        return Default;
    }
    bool Value;
    return Payload->TryGetBoolField(FieldName, Value) ? Value : Default;
}

inline FVector GetJsonVectorFieldNav(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    const FVector& Default = FVector::ZeroVector)
{
    if (!Payload.IsValid())
    {
        return Default;
    }
    const TSharedPtr<FJsonObject>* VecObj;
    if (Payload->TryGetObjectField(FieldName, VecObj) && VecObj->IsValid())
    {
        return FVector(
            GetJsonNumberFieldNav(*VecObj, TEXT("x"), Default.X),
            GetJsonNumberFieldNav(*VecObj, TEXT("y"), Default.Y),
            GetJsonNumberFieldNav(*VecObj, TEXT("z"), Default.Z));
    }
    return Default;
}

inline FRotator GetJsonRotatorFieldNav(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    const FRotator& Default = FRotator::ZeroRotator)
{
    if (!Payload.IsValid())
    {
        return Default;
    }
    const TSharedPtr<FJsonObject>* RotObj;
    if (Payload->TryGetObjectField(FieldName, RotObj) && RotObj->IsValid())
    {
        return FRotator(
            GetJsonNumberFieldNav(*RotObj, TEXT("pitch"), Default.Pitch),
            GetJsonNumberFieldNav(*RotObj, TEXT("yaw"), Default.Yaw),
            GetJsonNumberFieldNav(*RotObj, TEXT("roll"), Default.Roll));
    }
    return Default;
}

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

inline ENavLinkDirection::Type ParseNavLinkDirection(const FString& Direction)
{
    if (Direction == TEXT("LeftToRight"))
    {
        return ENavLinkDirection::LeftToRight;
    }
    if (Direction == TEXT("RightToLeft"))
    {
        return ENavLinkDirection::RightToLeft;
    }
    return ENavLinkDirection::BothWays;
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

bool HandleConfigureNavMeshSettings(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleSetNavAgentProperties(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleRebuildNavigation(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleCreateNavModifierComponent(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleSetNavAreaClass(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleConfigureNavAreaCost(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleCreateNavLinkProxy(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleConfigureNavLink(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleSetNavLinkType(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleCreateSmartLink(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleConfigureSmartLinkBehavior(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleGetNavigationInfo(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
}
#endif
