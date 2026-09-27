#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace McpRenderHandlers
{
bool HandleLumenUpdateScene(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (GEditor)
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
            GEngine->Exec(World, TEXT("r.Lumen.Scene.Recapture"));

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("action"), TEXT("manage_render"));
            Result->SetStringField(TEXT("subAction"), TEXT("lumen_update_scene"));
            Result->SetStringField(TEXT("command"), TEXT("r.Lumen.Scene.Recapture"));
            Result->SetBoolField(TEXT("executed"), true);

            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Lumen scene recapture triggered."), Result);
            return true;
        }
    }

    Subsystem->SendAutomationError(RequestingSocket, RequestId,
        TEXT("Could not execute command (no world context)."), TEXT("EXECUTION_FAILED"));
    return true;
}
}
