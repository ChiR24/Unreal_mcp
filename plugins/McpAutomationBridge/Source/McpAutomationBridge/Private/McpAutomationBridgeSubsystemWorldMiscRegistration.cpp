#include "McpAutomationBridgeSubsystem.h"

#include "MCP/McpConsolidatedActionRouting.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#define MCP_REGISTER_DIRECT(ActionName, MethodName) RegisterHandler(TEXT(ActionName), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return MethodName(R, A, P, S); })

void UMcpAutomationBridgeSubsystem::RegisterWorldAndMiscHandlers()
{
    MCP_REGISTER_DIRECT("manage_game_framework", HandleManageGameFrameworkAction);
    MCP_REGISTER_DIRECT("manage_sessions", HandleManageSessionsAction);

    RegisterHandler(
        TEXT("manage_level_structure"),
        [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S)
        {
            const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
            if (McpConsolidatedActions::IsVolumeAction(SubAction))
            {
                return HandleManageVolumesAction(R, TEXT("manage_volumes"), P, S);
            }
            return HandleManageLevelStructureAction(R, A, P, S);
        });

    MCP_REGISTER_DIRECT("manage_volumes", HandleManageVolumesAction);
    MCP_REGISTER_DIRECT("manage_navigation", HandleManageNavigationAction);
    MCP_REGISTER_DIRECT("manage_misc", HandleMiscAction);
    MCP_REGISTER_DIRECT("create_camera", HandleMiscAction);
    MCP_REGISTER_DIRECT("set_camera_fov", HandleMiscAction);
    MCP_REGISTER_DIRECT("set_viewport_resolution", HandleMiscAction);
    MCP_REGISTER_DIRECT("set_game_speed", HandleMiscAction);
    MCP_REGISTER_DIRECT("create_bookmark", HandleMiscAction);

    RegisterHandler(
        TEXT("check_pie_state"),
        [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S)
        {
#if WITH_EDITOR
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            bool bIsInPIE = false;
            FString PieState = TEXT("stopped");
            if (GEditor && GEditor->PlayWorld)
            {
                bIsInPIE = true;
                PieState = GEditor->PlayWorld->IsPaused() ? TEXT("paused") : TEXT("playing");
            }
            Result->SetBoolField(TEXT("isInPIE"), bIsInPIE);
            Result->SetStringField(TEXT("pieState"), PieState);
            SendAutomationResponse(
                S,
                R,
                true,
                bIsInPIE ? TEXT("PIE is active") : TEXT("PIE is not active"),
                Result);
            return true;
#else
            SendAutomationError(
                S,
                R,
                TEXT("PIE state check requires editor build"),
                TEXT("NOT_AVAILABLE"));
            return true;
#endif
        });
}

#undef MCP_REGISTER_DIRECT
