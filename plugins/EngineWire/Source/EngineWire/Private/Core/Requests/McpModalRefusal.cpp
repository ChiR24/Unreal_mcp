// A request that arrives while a modal dialog is up ("Save changes?", a message box) used to wait out the client's
// timeout on the native door: Slate's modal loop runs ModalLoopTickEvent, the platform and the widgets, but never
// FTSTicker::GetCoreTicker(), which drives the queue drain and the native heartbeat. The WebSocket door was answered
// at once only because its frames reach the game thread as tasks, which the modal loop's render sync does run.
// The modal tick now answers every queued request EDITOR_MODAL_ACTIVE, naming the dialog, without running it.
#include "EngineWireSubsystem.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "MCP/Transport/McpNativeTransport.h"
#include "Widgets/SWindow.h"

int32 UEngineWireSubsystem::AnswerQueuedRequestsDuringModal(const FString& DialogTitle)
{
    // Taken out under the lock and answered outside it: SendAutomationResponse reaches the transports, which take
    // their own locks. A request already in a running drain's batch is not in this list and is never touched.
    TArray<FPendingAutomationRequest> Taken;
    {
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        Taken = MoveTemp(PendingAutomationRequests);
        PendingAutomationRequests.Reset();
    }
    for (const FPendingAutomationRequest& Request : Taken)
    {
        // The failure path hands back a consumed consent grant and never records an idempotency key, so the same
        // call succeeds unchanged once the dialog is closed.
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("modalWindowTitle"), DialogTitle);
        SendAutomationResponse(
            Request.RequestingSocket, Request.RequestId, false,
            FString::Printf(TEXT("The editor is waiting on the dialog '%s'; nothing ran. Close that dialog in the editor, then send the same call again."),
                            *DialogTitle),
            Result, TEXT("EDITOR_MODAL_ACTIVE"), Request.Origin);
    }
    return Taken.Num();
}

void UEngineWireSubsystem::OnModalLoopTick(float /*DeltaTime*/)
{
    const TSharedPtr<SWindow> Modal = FSlateApplication::Get().GetActiveModalWindow();
    if (!Modal.IsValid())
    {
        return;
    }
    // The game thread is alive inside the dialog's loop: keeping the native heartbeat current lets new calls queue and
    // get this precise answer instead of the generic EDITOR_BLOCKED a stalled heartbeat gives after 15 s.
    if (NativeTransport)
    {
        NativeTransport->CleanupStaleRequests();
    }
    AnswerQueuedRequestsDuringModal(Modal->GetTitle().ToString());
}
