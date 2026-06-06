#include "McpAutomationBridge_ProcessRequestDispatch.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"

namespace McpProcessRequestDispatch
{
namespace
{
using FHandlerMethod = bool (UMcpAutomationBridgeSubsystem::*)(
    const FString&,
    const FString&,
    const TSharedPtr<FJsonObject>&,
    TSharedPtr<FMcpBridgeWebSocket>);

struct FDispatchStep
{
    const TCHAR* Label;
    FHandlerMethod Method;
    const TCHAR* BeforeLog = nullptr;
    const TCHAR* ConsumedLog = nullptr;
    const TCHAR* MissLog = nullptr;
};

bool TryDispatchStep(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FDispatchStep& Step,
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    FString& OutConsumedHandlerLabel)
{
    if (Step.BeforeLog)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("%s"), Step.BeforeLog);
    }

    if ((Bridge->*Step.Method)(RequestId, Action, Payload, RequestingSocket))
    {
        OutConsumedHandlerLabel = Step.Label;
        if (Step.ConsumedLog)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("%s"), Step.ConsumedLog);
        }
        return true;
    }

    if (Step.MissLog)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("%s"), Step.MissLog);
    }
    return false;
}

}

bool DispatchFallbackAutomationRequest(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const FString& Action,
    const FString& LowerAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    FString& OutConsumedHandlerLabel)
{
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
        TEXT("ProcessAutomationRequest: Starting handler dispatch for action='%s'"),
        *Action);

    FString LowerNormalized = LowerAction;
    LowerNormalized.ReplaceInline(TEXT("-"), TEXT("_"));
    LowerNormalized.ReplaceInline(TEXT(" "), TEXT("_"));
    const bool bLooksBlueprint =
        LowerNormalized.StartsWith(TEXT("blueprint_")) ||
        LowerNormalized.StartsWith(TEXT("manage_blueprint")) ||
        LowerNormalized.Contains(TEXT("_scs")) ||
        LowerNormalized.Contains(TEXT("scs_")) ||
        LowerNormalized.Contains(TEXT("scs"));
    if (bLooksBlueprint)
    {
        const FDispatchStep EarlyBlueprint{
            TEXT("HandleBlueprintAction (early)"),
            &UMcpAutomationBridgeSubsystem::HandleBlueprintAction,
            TEXT("ProcessAutomationRequest: Checking HandleBlueprintAction (early)"),
            TEXT("HandleBlueprintAction (early) consumed request")};
        if (TryDispatchStep(Bridge, EarlyBlueprint, RequestId, Action, Payload,
                RequestingSocket, OutConsumedHandlerLabel))
        {
            return true;
        }
    }

    const FDispatchStep Steps[] = {
        {TEXT("HandleExecuteEditorFunction"),
            &UMcpAutomationBridgeSubsystem::HandleExecuteEditorFunction,
            TEXT("ProcessAutomationRequest: About to call HandleExecuteEditorFunction"),
            TEXT("HandleExecuteEditorFunction consumed request"),
            TEXT("ProcessAutomationRequest: HandleExecuteEditorFunction returned false")},
        {TEXT("HandleLevelAction"), &UMcpAutomationBridgeSubsystem::HandleLevelAction,
            TEXT("ProcessAutomationRequest: Checking HandleLevelAction"),
            TEXT("HandleLevelAction consumed request")},
        {TEXT("HandleAssetAction (early)"), &UMcpAutomationBridgeSubsystem::HandleAssetAction,
            TEXT("ProcessAutomationRequest: Checking HandleAssetAction (early)"),
            TEXT("HandleAssetAction (early) consumed request")},
        {TEXT("HandleSetObjectProperty"), &UMcpAutomationBridgeSubsystem::HandleSetObjectProperty},
        {TEXT("HandleGetObjectProperty"), &UMcpAutomationBridgeSubsystem::HandleGetObjectProperty},
        {TEXT("HandleAssetAction"), &UMcpAutomationBridgeSubsystem::HandleAssetAction,
            TEXT("ProcessAutomationRequest: Checking HandleAssetAction"),
            TEXT("HandleAssetAction consumed request")},
        {TEXT("HandleControlActorAction"), &UMcpAutomationBridgeSubsystem::HandleControlActorAction,
            TEXT("ProcessAutomationRequest: Checking HandleControlActorAction"),
            TEXT("HandleControlActorAction consumed request")},
        {TEXT("HandleControlEditorAction"), &UMcpAutomationBridgeSubsystem::HandleControlEditorAction,
            TEXT("ProcessAutomationRequest: Checking HandleControlEditorAction"),
            TEXT("HandleControlEditorAction consumed request")},
        {TEXT("HandleUiAction"), &UMcpAutomationBridgeSubsystem::HandleUiAction,
            TEXT("ProcessAutomationRequest: Checking HandleUiAction"),
            TEXT("HandleUiAction consumed request")},
        {TEXT("HandleBlueprintAction (late)"), &UMcpAutomationBridgeSubsystem::HandleBlueprintAction,
            TEXT("ProcessAutomationRequest: Checking HandleBlueprintAction (late)"),
            TEXT("HandleBlueprintAction (late) consumed request")},
        {TEXT("HandleSequenceAction"), &UMcpAutomationBridgeSubsystem::HandleSequenceAction,
            TEXT("ProcessAutomationRequest: Checking HandleSequenceAction"),
            TEXT("HandleSequenceAction consumed request")},
        {TEXT("HandleEffectAction"), &UMcpAutomationBridgeSubsystem::HandleEffectAction},
        {TEXT("HandleAnimationPhysicsAction"),
            &UMcpAutomationBridgeSubsystem::HandleAnimationPhysicsAction,
            TEXT("ProcessAutomationRequest: Checking HandleAnimationPhysicsAction"),
            TEXT("HandleAnimationPhysicsAction consumed request")},
        {TEXT("HandleAudioAction"), &UMcpAutomationBridgeSubsystem::HandleAudioAction},
        {TEXT("HandleLightingAction"), &UMcpAutomationBridgeSubsystem::HandleLightingAction},
        {TEXT("HandlePerformanceAction"), &UMcpAutomationBridgeSubsystem::HandlePerformanceAction},
        {TEXT("HandleBuildEnvironmentAction"), &UMcpAutomationBridgeSubsystem::HandleBuildEnvironmentAction},
        {TEXT("HandleControlEnvironmentAction"), &UMcpAutomationBridgeSubsystem::HandleControlEnvironmentAction},
        {TEXT("HandleSystemControlAction"), &UMcpAutomationBridgeSubsystem::HandleSystemControlAction},
        {TEXT("HandleConsoleCommandAction"), &UMcpAutomationBridgeSubsystem::HandleConsoleCommandAction},
        {TEXT("HandleInspectAction"), &UMcpAutomationBridgeSubsystem::HandleInspectAction},
        {TEXT("HandleBlueprintGraphAction"), &UMcpAutomationBridgeSubsystem::HandleBlueprintGraphAction},
        {TEXT("HandleNiagaraGraphAction"), &UMcpAutomationBridgeSubsystem::HandleNiagaraGraphAction},
        {TEXT("HandleMaterialGraphAction"), &UMcpAutomationBridgeSubsystem::HandleMaterialGraphAction},
        {TEXT("HandleBehaviorTreeAction"), &UMcpAutomationBridgeSubsystem::HandleBehaviorTreeAction},
        {TEXT("HandleWorldPartitionAction"), &UMcpAutomationBridgeSubsystem::HandleWorldPartitionAction},
        {TEXT("HandleRenderAction"), &UMcpAutomationBridgeSubsystem::HandleRenderAction},
        {TEXT("HandleGeometryAction"), &UMcpAutomationBridgeSubsystem::HandleGeometryAction},
        {TEXT("HandleManageSkeleton"), &UMcpAutomationBridgeSubsystem::HandleManageSkeleton},
        {TEXT("HandleManageMaterialAuthoringAction"),
            &UMcpAutomationBridgeSubsystem::HandleManageMaterialAuthoringAction},
        {TEXT("HandleManageTextureAction"),
            static_cast<FHandlerMethod>(
                &UMcpAutomationBridgeSubsystem::HandleManageTextureAction)},
        {TEXT("HandleManageAnimationAuthoringAction"),
            &UMcpAutomationBridgeSubsystem::HandleManageAnimationAuthoringAction},
        {TEXT("HandleManageAudioAuthoringAction"),
            &UMcpAutomationBridgeSubsystem::HandleManageAudioAuthoringAction},
        {TEXT("HandleManageNiagaraAuthoringAction"),
            &UMcpAutomationBridgeSubsystem::HandleManageNiagaraAuthoringAction},
        {TEXT("HandleManageGASAction"), &UMcpAutomationBridgeSubsystem::HandleManageGASAction},
        {TEXT("HandleManageCharacterAction"), &UMcpAutomationBridgeSubsystem::HandleManageCharacterAction},
        {TEXT("HandleManageCombatAction"), &UMcpAutomationBridgeSubsystem::HandleManageCombatAction},
        {TEXT("HandleManageAIAction"), &UMcpAutomationBridgeSubsystem::HandleManageAIAction},
        {TEXT("HandleManageInventoryAction"), &UMcpAutomationBridgeSubsystem::HandleManageInventoryAction},
        {TEXT("HandleManageInteractionAction"), &UMcpAutomationBridgeSubsystem::HandleManageInteractionAction},
        {TEXT("HandleManageWidgetAuthoringAction"),
            &UMcpAutomationBridgeSubsystem::HandleManageWidgetAuthoringAction},
        {TEXT("HandleManageNetworkingAction"), &UMcpAutomationBridgeSubsystem::HandleManageNetworkingAction},
        {TEXT("HandleManageGameFrameworkAction"),
            &UMcpAutomationBridgeSubsystem::HandleManageGameFrameworkAction},
        {TEXT("HandleManageSessionsAction"), &UMcpAutomationBridgeSubsystem::HandleManageSessionsAction},
        {TEXT("HandleManageLevelStructureAction"),
            &UMcpAutomationBridgeSubsystem::HandleManageLevelStructureAction},
        {TEXT("HandleManageVolumesAction"), &UMcpAutomationBridgeSubsystem::HandleManageVolumesAction},
        {TEXT("HandleManageNavigationAction"), &UMcpAutomationBridgeSubsystem::HandleManageNavigationAction},
        {TEXT("HandleManageSplinesAction"), &UMcpAutomationBridgeSubsystem::HandleManageSplinesAction},
        {TEXT("HandleManagePCGAction"), &UMcpAutomationBridgeSubsystem::HandleManagePCGAction},
        {TEXT("HandlePipelineAction"), &UMcpAutomationBridgeSubsystem::HandlePipelineAction},
        {TEXT("HandleTestAction"), &UMcpAutomationBridgeSubsystem::HandleTestAction},
        {TEXT("HandleLogAction"), &UMcpAutomationBridgeSubsystem::HandleLogAction},
        {TEXT("HandleDebugAction"), &UMcpAutomationBridgeSubsystem::HandleDebugAction},
        {TEXT("HandleAssetQueryAction"), &UMcpAutomationBridgeSubsystem::HandleAssetQueryAction},
        {TEXT("HandleInsightsAction"), &UMcpAutomationBridgeSubsystem::HandleInsightsAction},
    };

    for (const FDispatchStep& Step : Steps)
    {
        if (TryDispatchStep(Bridge, Step, RequestId, Action, Payload,
                RequestingSocket, OutConsumedHandlerLabel))
        {
            return true;
        }
    }
    return false;
}
}
