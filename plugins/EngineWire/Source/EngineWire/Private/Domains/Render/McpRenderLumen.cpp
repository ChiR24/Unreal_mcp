#include "Domains/Render/McpRenderPrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Foundation/Render/McpConsoleVariableWrite.h"

namespace McpRenderHandlers
{
bool HandleLumenUpdateScene(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // The renderer resets the surface cache and puts the variable back to 0. It ran "r.Lumen.Scene.Recapture", a
    // command no engine version has, and answered "triggered" over the false Exec returned.
    static const TCHAR* const ResetVariable = TEXT("r.LumenScene.SurfaceCache.Reset");
    IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(ResetVariable);
    const FString NotTaken = Variable ? McpSetConsoleVariable(*Variable, TEXT("1")) : FString(TEXT("is not in this engine version"));
    if (!NotTaken.IsEmpty())
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s %s; the Lumen scene was not reset."), ResetVariable, *NotTaken), TEXT("CVAR_UNAVAILABLE"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("variable"), ResetVariable);
    Result->SetBoolField(TEXT("executed"), true);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lumen scene surface cache reset."), Result);
    return true;
}
}
