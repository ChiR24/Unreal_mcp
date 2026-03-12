// =============================================================================
// McpAutomationBridge_SystemControlHandlers.cpp
// =============================================================================
// MCP Automation Bridge - System Control & Build Automation Handlers
// 
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
// 
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: system_control (via 'action' field in payload)
//   - run_ubt: Execute UnrealBuildTool with target/platform/config
//   - run_tests: Run automation tests via console command
//   - test_progress_protocol: Test progress update protocol
//   - test_stale_progress: Test stale progress detection
//   - export_asset: Export asset to FBX/OBJ/other format
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: PlatformProcess, FileManager, Paths, App
//   - Editor: EditorAssetLibrary, AssetTools, Exporter
// 
// Notes:
//   - UBT runs with 5-minute timeout for builds
//   - Export uses AssetTools::ExportAssets for dialog-free operation
//   - Tests run asynchronously; check Output Log for results
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/MonitoredProcess.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Exporters/Exporter.h"
#include "Misc/FileHelper.h"
#endif

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSystemControlAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Extract sub-action from payload's "action" field
    FString SubAction;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("action"), SubAction);
    }

    const FString Lower = SubAction.ToLower();

    // Check if this handler should process this sub-action
    if (!Lower.StartsWith(TEXT("run_ubt")) &&
        !Lower.StartsWith(TEXT("run_tests")) &&
        !Lower.StartsWith(TEXT("test_progress")) &&
        !Lower.StartsWith(TEXT("test_stale")) &&
        Lower != TEXT("export_asset"))
    {
        return false;  // Not handled by this function
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("System control payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // -------------------------------------------------------------------------
    // run_ubt: Execute UnrealBuildTool
    // -------------------------------------------------------------------------
    if (Lower == TEXT("run_ubt"))
    {
        FString Target;
        Payload->TryGetStringField(TEXT("target"), Target);

        FString Platform;
        Payload->TryGetStringField(TEXT("platform"), Platform);

        FString Configuration;
        Payload->TryGetStringField(TEXT("configuration"), Configuration);

        FString AdditionalArgs;
        Payload->TryGetStringField(TEXT("additionalArgs"), AdditionalArgs);

        // Build UBT path (platform-specific)
        const FString EngineDir = FPaths::EngineDir();
        FString UBTPath;

#if PLATFORM_WINDOWS
        UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Build.bat"));
#else
        UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Build.sh"));
#endif

        if (!FPaths::FileExists(UBTPath))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("UBT not found at: %s"), *UBTPath),
                TEXT("UBT_NOT_FOUND"));
            return true;
        }

        // Build command line arguments
        FString Arguments;

        // Target (project or engine target)
        if (!Target.IsEmpty())
        {
            Arguments += Target + TEXT(" ");
        }
        else
        {
            // Default to current project
            FString ProjectPath = FPaths::GetProjectFilePath();
            if (!ProjectPath.IsEmpty())
            {
                Arguments += FString::Printf(TEXT("-project=\"%s\" "), *ProjectPath);
            }
        }

        // Platform
        if (!Platform.IsEmpty())
        {
            Arguments += Platform + TEXT(" ");
        }
        else
        {
#if PLATFORM_WINDOWS
            Arguments += TEXT("Win64 ");
#elif PLATFORM_MAC
            Arguments += TEXT("Mac ");
#else
            Arguments += TEXT("Linux ");
#endif
        }

        // Configuration
        if (!Configuration.IsEmpty())
        {
            Arguments += Configuration + TEXT(" ");
        }
        else
        {
            Arguments += TEXT("Development ");
        }

        // Additional args
        if (!AdditionalArgs.IsEmpty())
        {
            Arguments += AdditionalArgs;
        }

        // Create pipes for output capture
        void* ReadPipe = nullptr;
        void* WritePipe = nullptr;
        FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

        FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
            *UBTPath,
            *Arguments,
            false,   // bLaunchDetached
            true,    // bLaunchHidden
            true,    // bLaunchReallyHidden
            nullptr, // OutProcessID
            0,       // PriorityModifier
            nullptr, // OptionalWorkingDirectory
            WritePipe
        );

        if (!ProcessHandle.IsValid())
        {
            FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to launch UBT process"), TEXT("PROCESS_LAUNCH_FAILED"));
            return true;
        }

        // Read output with timeout (5 minutes for builds)
        constexpr double TimeoutSeconds = 300.0;
        const double StartTime = FPlatformTime::Seconds();
        FString StdOut;

        while (FPlatformProcess::IsProcRunning(ProcessHandle))
        {
            FString NewOutput = FPlatformProcess::ReadPipe(ReadPipe);
            if (!NewOutput.IsEmpty())
            {
                StdOut += NewOutput;
            }

            // Check timeout
            if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
            {
                FPlatformProcess::TerminateProc(ProcessHandle, true);
                FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

                TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
                Result->SetStringField(TEXT("output"), StdOut);
                Result->SetBoolField(TEXT("timedOut"), true);

                SendAutomationResponse(RequestingSocket, RequestId, false,
                    TEXT("UBT process timed out"), Result, TEXT("TIMEOUT"));
                return true;
            }

            FPlatformProcess::Sleep(0.1f);
        }

        // Read remaining output
        FString FinalOutput = FPlatformProcess::ReadPipe(ReadPipe);
        if (!FinalOutput.IsEmpty())
        {
            StdOut += FinalOutput;
        }

        // Get return code
        int32 ReturnCode = -1;
        FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
        FPlatformProcess::CloseProc(ProcessHandle);
        FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("output"), StdOut);
        Result->SetNumberField(TEXT("returnCode"), ReturnCode);
        Result->SetStringField(TEXT("ubtPath"), UBTPath);
        Result->SetStringField(TEXT("arguments"), Arguments);

        if (ReturnCode == 0)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("UBT completed successfully"), Result);
        }
        else
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("UBT failed with code %d"), ReturnCode),
                Result, TEXT("UBT_FAILED"));
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // run_tests: Run automation tests via console command
    // -------------------------------------------------------------------------
    if (Lower == TEXT("run_tests"))
    {
        FString Filter;
        Payload->TryGetStringField(TEXT("filter"), Filter);

        FString TestName;
        Payload->TryGetStringField(TEXT("test"), TestName);

        // If specific test name provided, use it as filter
        if (!TestName.IsEmpty() && Filter.IsEmpty())
        {
            Filter = TestName;
        }

        // Build automation test command
        FString TestCommand;
        if (Filter.IsEmpty())
        {
            TestCommand = TEXT("automation RunAll");
        }
        else
        {
            TestCommand = FString::Printf(TEXT("automation RunTests %s"), *Filter);
        }

        // Execute the automation command
        if (GEngine && GEditor && GEditor->GetEditorWorldContext().World())
        {
            GEngine->Exec(GEditor->GetEditorWorldContext().World(), *TestCommand);

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("command"), TestCommand);
            Result->SetStringField(TEXT("filter"), Filter);

            // Note: Automation tests run asynchronously
            SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Automation tests started. Check Output Log for results."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Editor world not available for running tests"),
                TEXT("EDITOR_NOT_AVAILABLE"));
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // test_progress_protocol: Test progress update protocol
    // -------------------------------------------------------------------------
    if (Lower == TEXT("test_progress_protocol"))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
            TEXT("test_progress_protocol: Handler called successfully"));

        int32 Steps = 5;
        Payload->TryGetNumberField(TEXT("steps"), Steps);
        Steps = FMath::Clamp(Steps, 1, 20);

        float StepDurationMs = 500.0f;
        Payload->TryGetNumberField(TEXT("stepDurationMs"), StepDurationMs);
        StepDurationMs = FMath::Clamp(StepDurationMs, 100.0f, 5000.0f);

        bool bSendProgress = true;
        if (Payload->HasField(TEXT("sendProgress")))
        {
            bSendProgress = Payload->GetBoolField(TEXT("sendProgress"));
        }

        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
            TEXT("test_progress_protocol: Starting %d steps, %.0fms each, progress=%s"),
            Steps, StepDurationMs, bSendProgress ? TEXT("true") : TEXT("false"));

        for (int32 i = 1; i <= Steps; i++)
        {
            if (bSendProgress)
            {
                float Percent = (static_cast<float>(i) / static_cast<float>(Steps)) * 100.0f;
                FString StatusMsg = FString::Printf(TEXT("Step %d/%d"), i, Steps);
                SendProgressUpdate(RequestId, Percent, StatusMsg, true);
            }

            FPlatformProcess::Sleep(StepDurationMs / 1000.0f);
        }

        if (bSendProgress)
        {
            SendProgressUpdate(RequestId, 100.0f, TEXT("Complete"), false);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("steps"), Steps);
        Result->SetNumberField(TEXT("stepDurationMs"), StepDurationMs);
        Result->SetBoolField(TEXT("progressSent"), bSendProgress);
        Result->SetStringField(TEXT("message"), TEXT("Progress protocol test completed"));

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Progress protocol test completed successfully"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // test_stale_progress: Test stale progress detection
    // -------------------------------------------------------------------------
    if (Lower == TEXT("test_stale_progress"))
    {
        int32 StaleCount = 5;
        Payload->TryGetNumberField(TEXT("staleCount"), StaleCount);
        StaleCount = FMath::Clamp(StaleCount, 1, 10);

        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
            TEXT("test_stale_progress: Sending %d stale updates"), StaleCount);

        // Send same progress repeatedly to trigger stale detection
        for (int32 i = 0; i < StaleCount; i++)
        {
            FString StatusMsg = FString::Printf(TEXT("Stale update %d/%d"), i + 1, StaleCount);
            SendProgressUpdate(RequestId, 50.0f, StatusMsg, true);  // Always 50%
            FPlatformProcess::Sleep(0.1f);  // 100ms between updates
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("staleUpdatesSent"), StaleCount);
        Result->SetBoolField(TEXT("staleDetectionExpected"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Stale progress test completed"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // export_asset: Export asset to FBX/OBJ/other format
    // -------------------------------------------------------------------------
    if (Lower == TEXT("export_asset"))
    {
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        FString ExportPath;
        Payload->TryGetStringField(TEXT("exportPath"), ExportPath);

        if (AssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("assetPath is required for export"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (ExportPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("exportPath is required for export"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Check if asset exists
        if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Ensure export directory exists
        FString ExportDir = FPaths::GetPath(ExportPath);
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (!PlatformFile.DirectoryExists(*ExportDir))
        {
            PlatformFile.CreateDirectoryTree(*ExportDir);
        }

        // Load the asset
        UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (!Asset)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath),
                TEXT("LOAD_FAILED"));
            return true;
        }

        // Determine export format from file extension
        FString Extension = FPaths::GetExtension(ExportPath).ToLower();

        bool bExportSuccess = false;
        FString ExportError;

        // Use AssetTools for automated export (suppresses dialogs)
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        IAssetTools& AssetTools = AssetToolsModule.Get();

        TArray<UObject*> AssetsToExport;
        AssetsToExport.Add(Asset);
        AssetTools.ExportAssets(AssetsToExport, ExportDir);

        // Check if file was created
        FString ExpectedExportPath = ExportDir / FPaths::GetBaseFilename(AssetPath) + TEXT(".") + Extension;
        if (FPaths::FileExists(ExpectedExportPath))
        {
            bExportSuccess = true;
        }
        else
        {
            bExportSuccess = FPaths::FileExists(ExportPath);
        }

        // Fallback: Use UExporter directly
        if (!bExportSuccess)
        {
            UExporter* Exporter = nullptr;

            // Find appropriate exporter for the asset type
            for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
            {
                UClass* CurrentClass = *ClassIt;
                if (CurrentClass->IsChildOf(UExporter::StaticClass()) && 
                    !CurrentClass->HasAnyClassFlags(CLASS_Abstract))
                {
                    UExporter* DefaultExporter = Cast<UExporter>(CurrentClass->GetDefaultObject());
                    if (DefaultExporter && DefaultExporter->SupportedClass)
                    {
                        if (Asset->GetClass()->IsChildOf(DefaultExporter->SupportedClass))
                        {
                            if (DefaultExporter->PreferredFormatIndex < DefaultExporter->FormatExtension.Num())
                            {
                                FString PreferredExt = DefaultExporter->FormatExtension[DefaultExporter->PreferredFormatIndex].ToLower();
                                if (PreferredExt == Extension || PreferredExt.Contains(Extension))
                                {
                                    Exporter = DefaultExporter;
                                    break;
                                }
                            }
                            if (!Exporter)
                            {
                                Exporter = DefaultExporter;
                            }
                        }
                    }
                }
            }

            if (Exporter)
            {
                int32 ExportResult = UExporter::ExportToFile(Asset, Exporter, *ExportPath, false, false, false);
                bExportSuccess = (ExportResult != 0);
            }

            if (!bExportSuccess)
            {
                ExportError = FString::Printf(TEXT("Export failed for asset type '%s' and format '%s'"),
                    *Asset->GetClass()->GetName(), *Extension);
            }
        }

        if (bExportSuccess)
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpHandlerUtils::AddVerification(Result, Asset);
            Result->SetStringField(TEXT("assetPath"), AssetPath);
            Result->SetStringField(TEXT("exportPath"), ExportPath);
            Result->SetStringField(TEXT("format"), Extension);
            Result->SetBoolField(TEXT("success"), true);

            SendAutomationResponse(RequestingSocket, RequestId, true,
                FString::Printf(TEXT("Asset exported to: %s"), *ExportPath), Result);
        }
        else
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("assetPath"), AssetPath);
            Result->SetStringField(TEXT("exportPath"), ExportPath);
            Result->SetStringField(TEXT("format"), Extension);
            Result->SetStringField(TEXT("error"), ExportError);

            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Export failed: %s"), *ExportError),
                Result, TEXT("EXPORT_FAILED"));
        }
        return true;
    }

    return false;

#else
    // Non-editor build
    SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("System control actions require editor build"),
        nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
