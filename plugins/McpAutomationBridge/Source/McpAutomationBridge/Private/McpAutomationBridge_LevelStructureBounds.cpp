#include "McpAutomationBridge_LevelStructureActions.h"
#include "McpAutomationBridge_LevelStructureEditorWorld.h"
#include "McpAutomationBridge_LevelStructurePayload.h"

#include "Engine/LevelScriptActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleConfigureLevelBounds(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    bool bAutoCalculateBounds = GetJsonBoolField(Payload, TEXT("bAutoCalculateBounds"), false);

    // Check if bounds parameters are provided
    TSharedPtr<FJsonObject> BoundsOriginJson = GetObjectField(Payload, TEXT("boundsOrigin"));
    TSharedPtr<FJsonObject> BoundsExtentJson = GetObjectField(Payload, TEXT("boundsExtent"));

    // If not auto-calculating, boundsOrigin and boundsExtent must be provided
    if (!bAutoCalculateBounds)
    {
        if (!BoundsOriginJson.IsValid() || !BoundsExtentJson.IsValid())
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                TEXT("boundsOrigin and boundsExtent are required when bAutoCalculateBounds is false"),
                nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }

    FVector BoundsOrigin = LevelStructureHelpers::GetVectorFromJson(BoundsOriginJson);
    FVector BoundsExtent = LevelStructureHelpers::GetVectorFromJson(BoundsExtentJson, FVector(10000.0));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Get or create level bounds
    FBox WorldBounds;
    if (bAutoCalculateBounds)
    {
        // Calculate bounds from all actors
        WorldBounds = FBox(ForceInit);
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor && !Actor->IsA(ALevelScriptActor::StaticClass()))
            {
                FBox ActorBounds = Actor->GetComponentsBoundingBox();
                if (ActorBounds.IsValid)
                {
                    WorldBounds += ActorBounds;
                }
            }
        }
    }
    else
    {
        WorldBounds = FBox(BoundsOrigin - BoundsExtent, BoundsOrigin + BoundsExtent);
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, World);

    TSharedPtr<FJsonObject> OriginJson = McpHandlerUtils::CreateResultObject();
    OriginJson->SetNumberField(TEXT("x"), WorldBounds.GetCenter().X);
    OriginJson->SetNumberField(TEXT("y"), WorldBounds.GetCenter().Y);
    OriginJson->SetNumberField(TEXT("z"), WorldBounds.GetCenter().Z);
    ResponseJson->SetObjectField(TEXT("boundsOrigin"), OriginJson);

    TSharedPtr<FJsonObject> ExtentJson = McpHandlerUtils::CreateResultObject();
    ExtentJson->SetNumberField(TEXT("x"), WorldBounds.GetExtent().X);
    ExtentJson->SetNumberField(TEXT("y"), WorldBounds.GetExtent().Y);
    ExtentJson->SetNumberField(TEXT("z"), WorldBounds.GetExtent().Z);
    ResponseJson->SetObjectField(TEXT("boundsExtent"), ExtentJson);

    FString Message = TEXT("Configured level bounds");
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

// ============================================================================
// World Partition Handlers (6 actions)
// ============================================================================

}
#endif
