#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Misc/OutputDevice.h"
#include "Async/Async.h"

// Define a custom output device to capture logs and stream them via the bridge
class FMcpLogOutputDevice : public FOutputDevice
{
public:
    FMcpLogOutputDevice(UMcpAutomationBridgeSubsystem* InSubsystem) 
        : Subsystem(InSubsystem) 
    {
    }

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
    {
        if (!Subsystem || !Subsystem->IsValidLowLevel())
        {
            return;
        }

        // Filter out very verbose logs if needed, but for now allow all
        // Prevent infinite recursion if our own logging causes more logging
        if (Category == LogMcpAutomationBridgeSubsystem.GetCategoryName())
        {
            return; 
        }

        FString VerbosityString;
        switch (Verbosity)
        {
            case ELogVerbosity::Fatal: VerbosityString = TEXT("Fatal"); break;
            case ELogVerbosity::Error: VerbosityString = TEXT("Error"); break;
            case ELogVerbosity::Warning: VerbosityString = TEXT("Warning"); break;
            case ELogVerbosity::Display: VerbosityString = TEXT("Display"); break;
            case ELogVerbosity::Log: VerbosityString = TEXT("Log"); break;
            case ELogVerbosity::Verbose: VerbosityString = TEXT("Verbose"); break;
            case ELogVerbosity::VeryVerbose: VerbosityString = TEXT("VeryVerbose"); break;
            default: VerbosityString = TEXT("Log"); break;
        }

        FString Message = FString(V);
        FString CategoryString = Category.ToString();

        // Dispatch to game thread to ensure safe socket sending if not already there
        // Actually, SendRawMessage might be thread safe, but let's be safe.
        // Copy data for lambda capture
        const FString PayloadJson = FString::Printf(TEXT("{\"event\":\"log\",\"category\":\"%s\",\"verbosity\":\"%s\",\"message\":\"%s\"}"), 
            *CategoryString, *VerbosityString, *Message.ReplaceCharWithEscapedChar());

        // Use a weak pointer to the subsystem to avoid crashing if it's destroyed
        TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(Subsystem);

        AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, PayloadJson]()
        {
            if (UMcpAutomationBridgeSubsystem* StrongSubsystem = WeakSubsystem.Get())
            {
               StrongSubsystem->SendRawMessage(PayloadJson);
            }
        });
    }

private:
    UMcpAutomationBridgeSubsystem* Subsystem;
};

bool UMcpAutomationBridgeSubsystem::HandleLogAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_logs"))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = Payload->GetStringField(TEXT("subAction"));

    if (SubAction == TEXT("subscribe"))
    {
        if (!LogCaptureDevice.IsValid())
        {
            LogCaptureDevice = MakeShared<FMcpLogOutputDevice>(this);
            GLog->AddOutputDevice(LogCaptureDevice.Get());
            UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("Log streaming enabled by client request."));
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Subscribed to editor logs."));
        return true;
    }
    else if (SubAction == TEXT("unsubscribe"))
    {
        if (LogCaptureDevice.IsValid())
        {
            GLog->RemoveOutputDevice(LogCaptureDevice.Get());
            LogCaptureDevice.Reset();
            UE_LOG(LogMcpAutomationBridgeSubsystem, Display, TEXT("Log streaming disabled by client request."));
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Unsubscribed from editor logs."));
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
