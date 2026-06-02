#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

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
#include "IPythonScriptPlugin.h"
#include "Misc/FileHelper.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleSystemControlAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // The sub-action is in the payload's "action" field
  FString SubAction;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }

  const FString Lower = SubAction.ToLower();

  // Check if this handler should process this sub-action
  if (!Lower.StartsWith(TEXT("run_ubt")) &&
      !Lower.StartsWith(TEXT("run_tests")) &&
      !Lower.StartsWith(TEXT("test_progress")) &&
      !Lower.StartsWith(TEXT("test_stale")) &&
      Lower != TEXT("export_asset") &&
      Lower != TEXT("start_session") &&
      Lower != TEXT("validate_assets") &&
      Lower != TEXT("execute_python")) {
    return false; // Not handled by this function
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("System control payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  if (Lower == TEXT("start_session")) {
    return HandleInsightsAction(RequestId, TEXT("manage_insights"), Payload, RequestingSocket);
  }

  if (Lower == TEXT("validate_assets")) {
    TArray<FString> PathsToValidate;

    const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("paths"), PathsArray) && PathsArray) {
      for (const TSharedPtr<FJsonValue>& PathValue : *PathsArray) {
        if (PathValue.IsValid() && PathValue->Type == EJson::String) {
          FString Path = PathValue->AsString();
          Path.TrimStartAndEndInline();
          if (!Path.IsEmpty()) {
            PathsToValidate.Add(Path);
          }
        }
      }
    }

    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) ||
        Payload->TryGetStringField(TEXT("path"), SinglePath)) {
      SinglePath.TrimStartAndEndInline();
      if (!SinglePath.IsEmpty()) {
        PathsToValidate.AddUnique(SinglePath);
      }
    }

    if (PathsToValidate.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("validate_assets requires paths, assetPath, or path"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const bool bRecursive = Payload->HasField(TEXT("recursive"))
      ? GetJsonBoolField(Payload, TEXT("recursive"))
      : true;
    TArray<TSharedPtr<FJsonValue>> Results;
    bool bAllValid = true;

    auto AddValidationResult = [&](const FString& OriginalPath, bool bSuccess,
                                   const FString& Kind, const FString& Message,
                                   int32 AssetCount = INDEX_NONE) {
      TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
      Item->SetStringField(TEXT("path"), OriginalPath);
      Item->SetBoolField(TEXT("success"), bSuccess);
      Item->SetBoolField(TEXT("isValid"), bSuccess);
      Item->SetStringField(TEXT("kind"), Kind);
      Item->SetStringField(TEXT("message"), Message);
      if (AssetCount != INDEX_NONE) {
        Item->SetNumberField(TEXT("assetCount"), AssetCount);
      }
      Results.Add(MakeShared<FJsonValueObject>(Item));
      bAllValid = bAllValid && bSuccess;
    };

    for (const FString& RawPath : PathsToValidate) {
      FString Path = RawPath;
      if (Path.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
        Path = FString::Printf(TEXT("/Game%s"), *Path.RightChop(8));
      }

      const FString SafePath = SanitizeProjectRelativePath(Path);
      if (SafePath.IsEmpty()) {
        AddValidationResult(RawPath, false, TEXT("invalid"),
                            TEXT("Invalid or unsafe asset path"));
        continue;
      }

      if (UEditorAssetLibrary::DoesAssetExist(SafePath)) {
        UObject* Asset = UEditorAssetLibrary::LoadAsset(SafePath);
        AddValidationResult(SafePath, Asset != nullptr, TEXT("asset"),
                            Asset ? TEXT("Asset loaded successfully") : TEXT("Asset exists but failed to load"));
        continue;
      }

      if (UEditorAssetLibrary::DoesDirectoryExist(SafePath)) {
        TArray<FString> Assets = UEditorAssetLibrary::ListAssets(SafePath, bRecursive, false);
        AddValidationResult(SafePath, true, TEXT("directory"), TEXT("Directory exists"), Assets.Num());
        continue;
      }

      AddValidationResult(SafePath, false, TEXT("missing"), TEXT("Asset or directory not found"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bAllValid);
    Result->SetBoolField(TEXT("isValid"), bAllValid);
    Result->SetArrayField(TEXT("results"), Results);
    Result->SetNumberField(TEXT("checkedCount"), Results.Num());

    SendAutomationResponse(RequestingSocket, RequestId, bAllValid,
                           bAllValid ? TEXT("Asset validation completed")
                                     : TEXT("Asset validation failed"),
                           Result, bAllValid ? FString() : TEXT("VALIDATION_FAILED"));
    return true;
  }

  if (Lower == TEXT("run_ubt")) {
    // Extract optional parameters
    FString Target;
    Payload->TryGetStringField(TEXT("target"), Target);

    FString Platform;
    Payload->TryGetStringField(TEXT("platform"), Platform);

    FString Configuration;
    Payload->TryGetStringField(TEXT("configuration"), Configuration);

    FString AdditionalArgs;
    Payload->TryGetStringField(TEXT("additionalArgs"), AdditionalArgs);
    if (AdditionalArgs.IsEmpty()) {
      Payload->TryGetStringField(TEXT("arguments"), AdditionalArgs);
    }

    Target.TrimStartAndEndInline();
    Platform.TrimStartAndEndInline();
    Configuration.TrimStartAndEndInline();
    AdditionalArgs.TrimStartAndEndInline();

    auto ValidateBuildToken = [&](const FString& Value, const TCHAR* FieldName) -> bool {
      if (!McpIsSafeUbtPositionalToken(Value)) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid %s for run_ubt: %s must be a positional token"), FieldName, *Value),
            TEXT("INVALID_ARGUMENT"));
        return false;
      }
      return true;
    };

    if (Target.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Target is required for run_ubt"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (Platform.IsEmpty()) {
#if PLATFORM_WINDOWS
      Platform = TEXT("Win64");
#elif PLATFORM_MAC
      Platform = TEXT("Mac");
#else
      Platform = TEXT("Linux");
#endif
    }

    if (Configuration.IsEmpty()) {
      Configuration = TEXT("Development");
    }

    if (!ValidateBuildToken(Target, TEXT("target")) ||
        !ValidateBuildToken(Platform, TEXT("platform")) ||
        !ValidateBuildToken(Configuration, TEXT("configuration"))) {
      return true;
    }

    if (!McpIsAllowedUbtPlatform(Platform)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Platform is not allowed for run_ubt: %s"), *Platform),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!McpIsAllowedUbtConfiguration(Configuration)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Configuration is not allowed for run_ubt: %s"), *Configuration),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!McpIsSafeUbtArgumentList(AdditionalArgs)) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("additionalArgs contains unsafe UBT argument characters"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Build UBT path
    FString EngineDir = FPaths::EngineDir();
    FString UBTPath;

#if PLATFORM_WINDOWS
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Build.bat"));
#elif PLATFORM_MAC
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Mac/Build.sh"));
#else
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Linux/Build.sh"));
#endif

    if (!FPaths::FileExists(UBTPath)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("UBT not found at: %s"), *UBTPath),
                          TEXT("UBT_NOT_FOUND"));
      return true;
    }

    // Build command line arguments
    FString Arguments;

    // Target (project or engine target)
    Arguments += Target + TEXT(" ");

    // Platform
    if (!Platform.IsEmpty()) {
      Arguments += Platform + TEXT(" ");
    } else {
#if PLATFORM_WINDOWS
      Arguments += TEXT("Win64 ");
#elif PLATFORM_MAC
      Arguments += TEXT("Mac ");
#else
      Arguments += TEXT("Linux ");
#endif
    }

    // Configuration
    if (!Configuration.IsEmpty()) {
      Arguments += Configuration + TEXT(" ");
    } else {
      Arguments += TEXT("Development ");
    }

    const FString ProjectPath = FPaths::GetProjectFilePath();
    if (!ProjectPath.IsEmpty()) {
      Arguments += FString::Printf(TEXT("-Project=\"%s\" "), *ProjectPath);
    }

    // Additional args
    if (!AdditionalArgs.IsEmpty()) {
      Arguments += AdditionalArgs;
    }

    // Use FMonitoredProcess for non-blocking execution with output capture
    // For simplicity, we'll use a synchronous approach with timeout
    int32 ReturnCode = -1;
    FString StdOut;
    FString StdErr;

    // Note: FPlatformProcess::ExecProcess is simpler but blocks
    // Using CreateProc with pipes for better control
    void* ReadPipe = nullptr;
    void* WritePipe = nullptr;
    FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

    FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
        *UBTPath,
        *Arguments,
        false,  // bLaunchDetached
        true,   // bLaunchHidden
        true,   // bLaunchReallyHidden
        nullptr, // OutProcessID
        0,      // PriorityModifier
        nullptr, // OptionalWorkingDirectory
        WritePipe // PipeWriteChild
    );

    if (!ProcessHandle.IsValid()) {
      FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to launch UBT process"),
                          TEXT("PROCESS_LAUNCH_FAILED"));
      return true;
    }

    // Read output with timeout (30 seconds max wait, but check periodically)
    const double TimeoutSeconds = 300.0; // 5 minute timeout for builds
    const double StartTime = FPlatformTime::Seconds();

    while (FPlatformProcess::IsProcRunning(ProcessHandle)) {
      // Read available output
      FString NewOutput = FPlatformProcess::ReadPipe(ReadPipe);
      if (!NewOutput.IsEmpty()) {
        StdOut += NewOutput;
      }

      // Check timeout
      if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds) {
        FPlatformProcess::TerminateProc(ProcessHandle, true);
        FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("output"), StdOut);
        Result->SetBoolField(TEXT("timedOut"), true);

        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("UBT process timed out"), Result,
                               TEXT("TIMEOUT"));
        return true;
      }

      // Small sleep to avoid busy waiting
      FPlatformProcess::Sleep(0.1f);
    }

    // Read any remaining output
    FString FinalOutput = FPlatformProcess::ReadPipe(ReadPipe);
    if (!FinalOutput.IsEmpty()) {
      StdOut += FinalOutput;
    }

    // Get return code
    FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
    FPlatformProcess::CloseProc(ProcessHandle);
    FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("output"), StdOut);
    Result->SetNumberField(TEXT("returnCode"), ReturnCode);
    Result->SetStringField(TEXT("ubtPath"), UBTPath);
    Result->SetStringField(TEXT("arguments"), Arguments);

    if (ReturnCode == 0) {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("UBT completed successfully"), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("UBT failed with code %d"), ReturnCode),
                             Result, TEXT("UBT_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("run_tests")) {
    // Extract test filter
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);

    FString TestName;
    Payload->TryGetStringField(TEXT("test"), TestName);

    // If specific test name provided, use it as filter
    if (!TestName.IsEmpty() && Filter.IsEmpty()) {
      Filter = TestName;
    }
    Filter.TrimStartAndEndInline();
    if (!McpIsSafeAutomationTestFilter(Filter)) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Test filter contains unsafe characters"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Build automation test command
    FString TestCommand;
    if (Filter.IsEmpty()) {
      // Run all tests
      TestCommand = TEXT("automation RunAll");
    } else {
      // Run filtered tests
      TestCommand = FString::Printf(TEXT("automation RunTests %s"), *Filter);
    }

    // Execute the automation command
    if (GEngine && GEditor && GEditor->GetEditorWorldContext().World()) {
      GEngine->Exec(GEditor->GetEditorWorldContext().World(), *TestCommand);

      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("command"), TestCommand);
      Result->SetStringField(TEXT("filter"), Filter);

      // Note: Automation tests run asynchronously in UE.
      // The command starts the tests, but results come later via automation framework.
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Automation tests started. Check Output Log for results."),
                             Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor world not available for running tests"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    }
    return true;
  } else   if (Lower == TEXT("test_progress_protocol")) {
    // Test action for heartbeat/progress protocol
    // Simulates a long-running operation with progress updates
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("test_progress_protocol: Handler called successfully"));
    int32 Steps = 5;
    Payload->TryGetNumberField(TEXT("steps"), Steps);
    Steps = FMath::Clamp(Steps, 1, 20);

    float StepDurationMs = 500.0f;
    Payload->TryGetNumberField(TEXT("stepDurationMs"), StepDurationMs);
    StepDurationMs = FMath::Clamp(StepDurationMs, 100.0f, 5000.0f);

    bool bSendProgress = true;
    if (Payload->HasField(TEXT("sendProgress"))) {
      bSendProgress = Payload->GetBoolField(TEXT("sendProgress"));
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("test_progress_protocol: Starting %d steps, %.0fms each, progress=%s"),
           Steps, StepDurationMs, bSendProgress ? TEXT("true") : TEXT("false"));

    for (int32 i = 1; i <= Steps; i++) {
      // Send progress update before each step
      if (bSendProgress) {
        float Percent = (static_cast<float>(i) / static_cast<float>(Steps)) * 100.0f;
        FString StatusMsg = FString::Printf(TEXT("Step %d/%d"), i, Steps);
        SendProgressUpdate(RequestId, Percent, StatusMsg, true);
      }

      // Simulate work by sleeping
      FPlatformProcess::Sleep(StepDurationMs / 1000.0f);
    }

    // Send final progress indicating completion
    if (bSendProgress) {
      SendProgressUpdate(RequestId, 100.0f, TEXT("Complete"), false);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("stepDurationMs"), StepDurationMs);
    Result->SetBoolField(TEXT("progressSent"), bSendProgress);
    Result->SetStringField(TEXT("message"), TEXT("Progress protocol test completed"));

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Progress protocol test completed successfully"), Result);
    return true;
  } else if (Lower == TEXT("test_stale_progress")) {
    // Test action for stale progress detection
    // Sends the same progress percent multiple times to trigger stale detection
    int32 StaleCount = 5;
    Payload->TryGetNumberField(TEXT("staleCount"), StaleCount);
    StaleCount = FMath::Clamp(StaleCount, 1, 10);

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("test_stale_progress: Sending %d stale updates"), StaleCount);

    // Send same progress repeatedly to trigger stale detection
    for (int32 i = 0; i < StaleCount; i++) {
      FString StatusMsg = FString::Printf(TEXT("Stale update %d/%d"), i + 1, StaleCount);
      SendProgressUpdate(RequestId, 50.0f, StatusMsg, true);  // Always 50%
      FPlatformProcess::Sleep(0.1f);  // 100ms between updates
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("staleUpdatesSent"), StaleCount);
    Result->SetBoolField(TEXT("staleDetectionExpected"), true);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Stale progress test completed"), Result);
    return true;
  } else if (Lower == TEXT("export_asset")) {
    // Export asset to FBX/OBJ/other format
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString ExportPath;
    Payload->TryGetStringField(TEXT("exportPath"), ExportPath);

    if (AssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("assetPath is required for export"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SafeAssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid asset path for export"),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    if (ExportPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("exportPath is required for export"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString SafeExportPath = SanitizeProjectFilePath(ExportPath);
    if (SafeExportPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe export path: %s"), *ExportPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    FString AbsoluteExportPath = FPaths::ProjectDir() / SafeExportPath;
    FPaths::MakeStandardFilename(AbsoluteExportPath);

    // CRITICAL: Convert to absolute path for proper comparison
    AbsoluteExportPath = FPaths::ConvertRelativePathToFull(AbsoluteExportPath);
    FPaths::NormalizeFilename(AbsoluteExportPath);

    FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(NormalizedProjectDir);
    if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
      NormalizedProjectDir += TEXT("/");
    }

    // SECURITY: Verify the resolved absolute path is within project bounds
    if (!AbsoluteExportPath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Export path escapes project directory: %s"), *ExportPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Ensure export directory exists
    FString ExportDir = FPaths::GetPath(AbsoluteExportPath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*ExportDir)) {
      PlatformFile.CreateDirectoryTree(*ExportDir);
    }

    // Load the asset
    UObject* Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
    if (!Asset) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Failed to load asset: %s"), *SafeAssetPath),
                          TEXT("LOAD_FAILED"));
      return true;
    }

    // Determine export format from file extension
    FString Extension = FPaths::GetExtension(AbsoluteExportPath).ToLower();

    // Try generic asset export via AssetTools
    bool bExportSuccess = false;
    FString ExportError;

    // CRITICAL FIX: Use AssetTools ExportAssets with explicit export path
    // This performs automated export without showing modal dialogs
    // The bPromptForIndividualFilenames=false suppresses file dialogs
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    IAssetTools& AssetTools = AssetToolsModule.Get();

    // Use ExportAssets with explicit path - this suppresses dialogs for automated export
    // The asset will be exported with its original name to the specified directory
    TArray<UObject*> AssetsToExport;
    AssetsToExport.Add(Asset);

    // ExportAssets exports to the specified directory with the asset's name
    // For custom filename, we need to rename temporarily or use UExporter directly
    AssetTools.ExportAssets(AssetsToExport, ExportDir);

    // Check if file was created
    FString ExpectedExportPath = ExportDir / FPaths::GetBaseFilename(SafeAssetPath) + TEXT(".") + Extension;
    if (FPaths::FileExists(ExpectedExportPath))
    {
      bExportSuccess = true;
    }
    else
    {
      // Try with the actual requested filename
      bExportSuccess = FPaths::FileExists(AbsoluteExportPath);
    }

    if (!bExportSuccess)
    {
      // Fallback: Use UExporter::ExportToFile directly with Prompt=false
      UExporter* Exporter = nullptr;

      // Find appropriate exporter for the asset type and extension
      for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt) {
        UClass* CurrentClass = *ClassIt;
        if (CurrentClass->IsChildOf(UExporter::StaticClass()) && !CurrentClass->HasAnyClassFlags(CLASS_Abstract)) {
          UExporter* DefaultExporter = Cast<UExporter>(CurrentClass->GetDefaultObject());
          if (DefaultExporter && DefaultExporter->SupportedClass) {
            if (Asset->GetClass()->IsChildOf(DefaultExporter->SupportedClass)) {
              if (DefaultExporter->PreferredFormatIndex < DefaultExporter->FormatExtension.Num()) {
                FString PreferredExt = DefaultExporter->FormatExtension[DefaultExporter->PreferredFormatIndex].ToLower();
                if (PreferredExt == Extension || PreferredExt.Contains(Extension)) {
                  Exporter = DefaultExporter;
                  break;
                }
              }
              if (!Exporter) {
                Exporter = DefaultExporter;
              }
            }
          }
        }
      }

      if (Exporter) {
        // ExportToFile signature: (Object, Exporter, Filename, InSelectedOnly, NoReplaceIdentical, Prompt)
        // The last parameter (Prompt=false) should suppress dialogs for most exporters
        int32 ExportResult = UExporter::ExportToFile(Asset, Exporter, *AbsoluteExportPath, false, false, false);
        bExportSuccess = (ExportResult != 0);
      }

      if (!bExportSuccess) {
        ExportError = FString::Printf(TEXT("Export failed for asset type '%s' and format '%s'"),
                                       *Asset->GetClass()->GetName(), *Extension);
      }
    }

    if (bExportSuccess) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      AddAssetVerification(Result, Asset);
      Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
      Result->SetStringField(TEXT("exportPath"), AbsoluteExportPath);
      Result->SetStringField(TEXT("format"), Extension);
      Result->SetBoolField(TEXT("success"), true);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Asset exported to: %s"), *AbsoluteExportPath),
                             Result);
    } else {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
      Result->SetStringField(TEXT("exportPath"), AbsoluteExportPath);
      Result->SetStringField(TEXT("format"), Extension);
      Result->SetStringField(TEXT("error"), ExportError);

      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Export failed: %s"), *ExportError),
                             Result, TEXT("EXPORT_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("execute_python")) {
    // Execute Python code with stdout/stderr capture via temp file wrapper
    FString Code;
    Payload->TryGetStringField(TEXT("code"), Code);
    FString File;
    Payload->TryGetStringField(TEXT("file"), File);

    const bool bHasCode = !Code.TrimStartAndEnd().IsEmpty();
    const bool bHasFile = !File.TrimStartAndEnd().IsEmpty();

    if (!bHasCode && !bHasFile) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("'code' or 'file' parameter is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (bHasCode && bHasFile) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Provide either 'code' or 'file', not both"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Enforce maximum code size (1 MB)
    static const int32 MaxCodeSize = 1048576;
    if (Code.Len() > MaxCodeSize) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Python code exceeds maximum size (%d bytes)"), MaxCodeSize),
                          TEXT("CODE_TOO_LARGE"));
      return true;
    }

    // Temp paths — GUID in filenames for concurrency safety
    FString TempDir = FPaths::ProjectSavedDir() / TEXT("Temp/MCP_Python");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*TempDir)) {
      PlatformFile.CreateDirectoryTree(*TempDir);
    }

    FString SafeId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FString ScriptPath = TempDir / FString::Printf(TEXT("mcp_exec_%s.py"), *SafeId);
    FString OutputPath = TempDir / FString::Printf(TEXT("output_%s.txt"), *SafeId);
    FString ErrorPath  = TempDir / FString::Printf(TEXT("error_%s.txt"), *SafeId);
    FString StatusPath = TempDir / FString::Printf(TEXT("status_%s.txt"), *SafeId);
    FString CodePath   = TempDir / FString::Printf(TEXT("code_%s.py"), *SafeId);

    // RAII-style scope guard for temp file cleanup
    struct FTempFileCleanup {
      TArray<FString> Paths;
      ~FTempFileCleanup() {
        IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
        for (const FString& P : Paths) {
          PF.DeleteFile(*P);
        }
      }
    } Cleanup;
    Cleanup.Paths.Add(ScriptPath);
    Cleanup.Paths.Add(OutputPath);
    Cleanup.Paths.Add(ErrorPath);
    Cleanup.Paths.Add(StatusPath);
    Cleanup.Paths.Add(CodePath);

    // Normalize paths for Python (forward slashes)
    auto NormalizePyPath = [](const FString& Path) -> FString {
      return Path.Replace(TEXT("\\"), TEXT("/"));
    };
    FString PyOutputPath = NormalizePyPath(OutputPath);
    FString PyErrorPath  = NormalizePyPath(ErrorPath);
    FString PyStatusPath = NormalizePyPath(StatusPath);

    // Build Python wrapper
    FString Wrapper;
    auto AppendPythonExec = [&Wrapper](const FString& PyScriptPath,
                                       const FString& PyScriptDir) {
      Wrapper += FString::Printf(TEXT("    _script_path = r'%s'\n"), *PyScriptPath);
      Wrapper += FString::Printf(TEXT("    _script_dir = r'%s'\n"), *PyScriptDir);
      Wrapper += TEXT("    _exec_globals = globals()\n");
      Wrapper += TEXT("    _exec_globals['__name__'] = '__main__'\n");
      Wrapper += TEXT("    _exec_globals['__file__'] = _script_path\n");
      Wrapper += TEXT("    _exec_globals['__package__'] = None\n");
      Wrapper += TEXT("    _exec_globals['__cached__'] = None\n");
      Wrapper += TEXT("    if _script_dir and _script_dir not in sys.path:\n");
      Wrapper += TEXT("        sys.path.insert(0, _script_dir)\n");
      Wrapper += FString::Printf(TEXT("    exec(compile(_user_code, r'%s', 'exec'), _exec_globals)\n"), *PyScriptPath);
    };
    Wrapper += TEXT("import sys\nimport traceback\n\n");
    Wrapper += FString::Printf(TEXT("_out = open(r'%s', 'w', encoding='utf-8')\n"), *PyOutputPath);
    Wrapper += FString::Printf(TEXT("_err = open(r'%s', 'w', encoding='utf-8')\n"), *PyErrorPath);
    Wrapper += TEXT("_old_out, _old_err = sys.stdout, sys.stderr\n");
    Wrapper += TEXT("sys.stdout, sys.stderr = _out, _err\n\n");
    Wrapper += TEXT("_success = True\n");
    Wrapper += TEXT("try:\n");

    if (bHasCode) {
      if (!FFileHelper::SaveStringToFile(Code, *CodePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Failed to write temp code file: %s"), *CodePath),
                            TEXT("FILE_WRITE_FAILED"));
        return true;
      }
      FString PyCodePath = NormalizePyPath(CodePath);
      FString PyCodeDir = NormalizePyPath(FPaths::GetPath(CodePath));
      Wrapper += FString::Printf(TEXT("    with open(r'%s', 'r', encoding='utf-8') as _f:\n"), *PyCodePath);
      Wrapper += TEXT("        _user_code = _f.read()\n");
      AppendPythonExec(PyCodePath, PyCodeDir);
    } else {
      // SECURITY: Sanitize file path to prevent directory traversal
      FString SafeFilePath = SanitizeProjectFilePath(File);
      if (SafeFilePath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Invalid or unsafe file path: %s"), *File),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }

      // Resolve absolute path and verify it stays within project directory
      FString AbsoluteFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / SafeFilePath);
      FPaths::NormalizeFilename(AbsoluteFilePath);

      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }

      if (!AbsoluteFilePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("File path escapes project directory: %s"), *File),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }

      // Resolve symlinks and re-validate (prevents symlink escape attacks)
      FString ResolvedPath = FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForRead(*AbsoluteFilePath);
      if (!ResolvedPath.IsEmpty())
      {
        FPaths::NormalizeFilename(ResolvedPath);
        if (!ResolvedPath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase))
        {
          SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Resolved file path escapes project directory (symlink detected)"),
                              TEXT("SECURITY_VIOLATION"));
          return true;
        }
        AbsoluteFilePath = ResolvedPath;
      }

      // Use absolute path in Python wrapper (forward slashes)
      FString PyFilePath = NormalizePyPath(AbsoluteFilePath);
      FString PyFileDir = NormalizePyPath(FPaths::GetPath(AbsoluteFilePath));
      Wrapper += FString::Printf(TEXT("    with open(r'%s', 'r', encoding='utf-8') as _f:\n"), *PyFilePath);
      Wrapper += TEXT("        _user_code = _f.read()\n");
      AppendPythonExec(PyFilePath, PyFileDir);
    }

    Wrapper += TEXT("except:\n");
    Wrapper += TEXT("    traceback.print_exc()\n");
    Wrapper += TEXT("    _success = False\n");
    Wrapper += TEXT("finally:\n");
    Wrapper += TEXT("    sys.stdout, sys.stderr = _old_out, _old_err\n");
    Wrapper += TEXT("    _out.close()\n");
    Wrapper += TEXT("    _err.close()\n");
    Wrapper += FString::Printf(TEXT("    with open(r'%s', 'w') as _sf:\n"), *PyStatusPath);
    Wrapper += TEXT("        _sf.write('1' if _success else '0')\n");

    // Write wrapper to disk
    if (!FFileHelper::SaveStringToFile(Wrapper, *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Failed to write temp script: %s"), *ScriptPath),
                          TEXT("FILE_WRITE_FAILED"));
      return true;
    }

    SendProgressUpdate(RequestId, 0.0f, TEXT("Executing Python script"), true, CurrentRequestOrigin);

    IPythonScriptPlugin* PythonPlugin = FModuleManager::LoadModulePtr<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
    if (!PythonPlugin) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Python Editor Script Plugin module is not loaded"),
                          TEXT("PYTHON_NOT_AVAILABLE"));
      return true;
    }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
    if (!PythonPlugin->IsPythonInitialized()) {
      PythonPlugin->ForceEnablePythonAtRuntime();
    }
    if (!PythonPlugin->IsPythonInitialized()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Python Editor Script Plugin is not initialized yet"),
                          TEXT("PYTHON_NOT_AVAILABLE"));
      return true;
    }
#else
    // UE 5.0-5.5 IPythonScriptPlugin does not expose initialization helpers.
    // Loading the module and executing through ExecPythonCommandEx is the
    // compatible path for those versions.
#endif

    // Execute through PythonScriptPlugin directly. The console "py" command can
    // defer file loading on a fresh editor startup, racing temp-file cleanup.
    static constexpr double MaxPythonExecutionSeconds = 60.0;
    FPythonCommandEx PythonCommand;
    PythonCommand.Command = FString::Printf(TEXT("\"%s\""), *ScriptPath);
    PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
    PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Private;
    PythonCommand.Flags |= EPythonCommandFlags::Unattended;
    double ExecStartTime = FPlatformTime::Seconds();
    bool bPythonCommandSucceeded = PythonPlugin->ExecPythonCommandEx(PythonCommand);
    double ExecElapsed = FPlatformTime::Seconds() - ExecStartTime;
    SendProgressUpdate(RequestId, 90.0f, TEXT("Collecting Python output"), true, CurrentRequestOrigin);
    if (ExecElapsed > MaxPythonExecutionSeconds) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Python execution took %.1fs (exceeds %.1fs threshold). "
                  "Consider running long scripts via 'file' parameter in a separate process."),
             ExecElapsed, MaxPythonExecutionSeconds);
    }

    // Read results
    FString Output, Error, Status;
    FFileHelper::LoadFileToString(Output, *OutputPath);
    FFileHelper::LoadFileToString(Error, *ErrorPath);
    FFileHelper::LoadFileToString(Status, *StatusPath);
    if (!bPythonCommandSucceeded && Status.TrimStartAndEnd().IsEmpty()) {
      FString PythonError = PythonCommand.CommandResult;
      for (const FPythonLogOutputEntry& LogOutput : PythonCommand.LogOutput) {
        if (!PythonError.IsEmpty()) {
          PythonError += TEXT("\n");
        }
        PythonError += FString::Printf(TEXT("[%s] %s"), LexToString(LogOutput.Type), *LogOutput.Output);
      }
      if (!PythonError.IsEmpty()) {
        if (!Error.IsEmpty()) {
          Error += TEXT("\n");
        }
        Error += PythonError;
      }
    }

    // Cleanup happens automatically via FTempFileCleanup destructor

    bool bSuccess = bPythonCommandSucceeded && Status.TrimStartAndEnd().Equals(TEXT("1"));
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("output"), Output.TrimEnd());
    Result->SetStringField(TEXT("error"), Error.TrimEnd());

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                           bSuccess ? TEXT("Python executed successfully") : TEXT("Python execution failed"),
                           Result, bSuccess ? FString() : TEXT("PYTHON_ERROR"));
    return true;
  }

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("System control actions require editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
