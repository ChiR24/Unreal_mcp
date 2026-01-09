#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

bool UMcpAutomationBridgeSubsystem::HandlePipelineAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_pipeline"))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = Payload->GetStringField(TEXT("subAction"));

    if (SubAction == TEXT("run_ubt"))
    {
        FString Target;
        Payload->TryGetStringField(TEXT("target"), Target);
        FString Platform;
        Payload->TryGetStringField(TEXT("platform"), Platform);
        FString Configuration;
        Payload->TryGetStringField(TEXT("configuration"), Configuration);
        FString ExtraArgs;
        Payload->TryGetStringField(TEXT("extraArgs"), ExtraArgs);

        // Defense-in-depth: Sanitize ExtraArgs to prevent command injection
        // Block shell metacharacters that could be used for command chaining
        static const TCHAR* DangerousPatterns[] = {
            TEXT(";"), TEXT("&&"), TEXT("||"), TEXT("|"), TEXT("`"),
            TEXT("$("), TEXT("$`"), TEXT("\n"), TEXT("\r"),
            TEXT(">"), TEXT("<"), TEXT(">>"), TEXT("<<")
        };
        for (const TCHAR* Pattern : DangerousPatterns)
        {
            if (ExtraArgs.Contains(Pattern))
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    FString::Printf(TEXT("ExtraArgs contains forbidden pattern: %s"), Pattern), 
                    TEXT("INVALID_ARGS"));
                return true;
            }
        }

        // Construct UBT command line
        // Path to UBT... usually in Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe
        FString UBTPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe"));
        
        FString Params = FString::Printf(TEXT("%s %s %s %s"), *Target, *Platform, *Configuration, *ExtraArgs);

        // Spawn process
        FProcHandle ProcHandle = FPlatformProcess::CreateProc(
            *UBTPath,
            *Params,
            true, // bLaunchDetached
            false, // bLaunchHidden
            false, // bLaunchReallyHidden
            nullptr, // ProcessID
            0, // PriorityModifier
            nullptr, // OptionalWorkingDirectory
            nullptr // PipeWriteChild
        );

        if (ProcHandle.IsValid())
        {
             // We can't easily get the PID on all platforms from the handle immediately without more code,
             // but we know it started.
             SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("UBT process started."));
        }
        else
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to launch UBT."), TEXT("LAUNCH_FAILED"));
        }
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
