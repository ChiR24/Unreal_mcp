#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Paths.h"
#include "NavAreas/NavArea.h"
#include "NavAreas/NavArea_Default.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavModifierComponent.h"
#include "NavModifierVolume.h"
#include "Domains/Volume/McpAutomationBridge_VolumeGeometry.h"

namespace
{
// A short name (Null, Obstacle, Default, or NavArea_*) or a class path; null when it names no NavArea.
UClass* ResolveNavModifierAreaClass(const FString& Name)
{
    if (Name.Equals(TEXT("NavArea_Null"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("Null"), ESearchCase::IgnoreCase))
    {
        return UNavArea_Null::StaticClass();
    }
    if (Name.Equals(TEXT("NavArea_Obstacle"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("Obstacle"), ESearchCase::IgnoreCase))
    {
        return UNavArea_Obstacle::StaticClass();
    }
    if (Name.Equals(TEXT("NavArea_Default"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
    {
        return UNavArea_Default::StaticClass();
    }
    return LoadClass<UNavArea>(nullptr, *Name);
}

// No blueprintPath: place a NavModifierVolume in the editor world instead of editing an asset.
bool SpawnNavModifierVolume(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                            TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UClass* AreaClass)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world available"), TEXT("NO_WORLD"));
        return true;
    }
    const FVector Extent = ExtractVectorField(Payload, TEXT("extent"), FVector(200.0, 200.0, 100.0));
    if (Extent.X <= 0.0 || Extent.Y <= 0.0 || Extent.Z <= 0.0)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("extent must be positive on every axis"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    ANavModifierVolume* Volume = VolumeHelpers::SpawnVolumeActor<ANavModifierVolume>(World,
        GetJsonStringField(Payload, TEXT("actorName")), ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector),
        FRotator::ZeroRotator, Extent);
    if (!Volume)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn NavModifierVolume"), TEXT("CREATION_FAILED"));
        return true;
    }
    Volume->SetAreaClass(AreaClass);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), Volume->GetActorLabel());
    Result->SetStringField(TEXT("areaClass"), AreaClass->GetPathName());
    McpHandlerUtils::AddVerification(Result, Volume);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("NavModifierVolume created"), Result);
    return true;
}
}

namespace McpAIHandlers
{
// Implements the "create_nav_modifier" action.
bool HandleCreateNavModifier(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Resolve the area before touching anything: an unknown class used to be ignored and the
    // default area reported as applied.
    const bool bFailsafe = GetJsonBoolField(Payload, TEXT("failsafeToDefaultNavmesh"));
    UClass* AppliedAreaClass = bFailsafe ? UNavArea_Default::StaticClass() : UNavArea_Obstacle::StaticClass();
    const FString AreaClassName = GetJsonStringField(Payload, TEXT("areaClass"));
    if (!AreaClassName.IsEmpty())
    {
        AppliedAreaClass = ResolveNavModifierAreaClass(AreaClassName);
        if (!AppliedAreaClass)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("areaClass not found or not a NavArea: %s"), *AreaClassName), TEXT("INVALID_AREA_CLASS"));
            return true;
        }
    }

    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    if (BlueprintPath.IsEmpty())
    {
        return SpawnNavModifierVolume(Self, RequestId, Payload, RequestingSocket, AppliedAreaClass);
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return true;
    }

    if (!Blueprint->SimpleConstructionScript)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
        return true;
    }

    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    if (ComponentName.IsEmpty())
    {
        ComponentName = TEXT("NavModifierComponent");
    }

    // Create nav modifier component
    USCS_Node* NavModNode = Blueprint->SimpleConstructionScript->CreateNode(
        UNavModifierComponent::StaticClass(), *ComponentName);
    if (!NavModNode)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create nav modifier node"), TEXT("CREATION_FAILED"));
        return true;
    }

    Blueprint->SimpleConstructionScript->AddNode(NavModNode);
    if (UNavModifierComponent* NavModComp = Cast<UNavModifierComponent>(NavModNode->ComponentTemplate))
    {
        NavModComp->SetAreaClass(AppliedAreaClass);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> NavModResult = McpHandlerUtils::CreateResultObject();
    NavModResult->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    NavModResult->SetStringField(TEXT("componentName"), ComponentName);
    // Report the class actually applied (dogfood #61); UNavModifierComponent has no getter on 5.7.
    NavModResult->SetStringField(TEXT("areaClass"), AppliedAreaClass->GetPathName());

    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Nav modifier component created"), NavModResult);
    return true;
}
}
