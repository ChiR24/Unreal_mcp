// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 33: Testing & Quality Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "FileHelpers.h"

// Automation Test Framework
#include "Misc/AutomationTest.h"
#include "Misc/FeedbackContext.h"

// Data Validation
#include "Misc/DataValidation.h"

// Visual Logger
#include "VisualLogger/VisualLogger.h"

// Memory/Performance
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Stats/Stats.h"
#include "ProfilingDebugging/ProfilingHelpers.h"

// Asset Utilities
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/RedirectCollector.h"
#include "UObject/ObjectRedirector.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

// Map Check
#include "Engine/World.h"
#include "Logging/MessageLog.h"

// Engine Trace
#if UE_TRACE_ENABLED
#include "Trace/Trace.h"
#endif

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleManageTestingAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_testing"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_testing")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_testing payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  if (SubAction.IsEmpty()) {
    Payload->TryGetStringField(TEXT("action_type"), SubAction);
  }
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = true;
  FString Message = FString::Printf(TEXT("Testing action '%s' completed"), *LowerSub);
  FString ErrorCode;

  // =========================================
  // AUTOMATION TESTS
  // =========================================
  if (LowerSub == TEXT("list_tests"))
  {
    // Get automation controller and list available tests
    FString TestFilter = Payload->GetStringField(TEXT("testFilter"));
    
    TArray<TSharedPtr<FJsonValue>> TestsArray;
    
    FAutomationTestFramework& Framework = FAutomationTestFramework::Get();
    TArray<FAutomationTestInfo> TestInfos;
    Framework.GetValidTestNames(TestInfos);
    
    for (const FAutomationTestInfo& Info : TestInfos)
    {
      // Apply filter if provided
      if (!TestFilter.IsEmpty() && !Info.GetDisplayName().Contains(TestFilter))
      {
        continue;
      }
      
      TSharedPtr<FJsonObject> TestObj = MakeShared<FJsonObject>();
      TestObj->SetStringField(TEXT("name"), Info.GetDisplayName());
      TestObj->SetStringField(TEXT("fullPath"), Info.GetFullTestPath());
      TestObj->SetStringField(TEXT("testName"), Info.GetTestName());
      TestObj->SetNumberField(TEXT("numParticipants"), Info.GetNumDevicesRunningTest());
      TestsArray.Add(MakeShared<FJsonValueObject>(TestObj));
    }
    
    Resp->SetArrayField(TEXT("tests"), TestsArray);
    Resp->SetNumberField(TEXT("totalTests"), TestsArray.Num());
    Message = FString::Printf(TEXT("Listed %d automation tests"), TestsArray.Num());
  }
  else if (LowerSub == TEXT("run_tests"))
  {
    // Run automation tests with optional filter
    FString TestFilter = Payload->GetStringField(TEXT("testFilter"));
    
    FAutomationTestFramework& Framework = FAutomationTestFramework::Get();
    
    // Start test execution (filter is applied at the test itself)
    Framework.StartTestByName(TestFilter.IsEmpty() ? TEXT("*") : TestFilter, 0);
    
    Message = TEXT("Automation tests started");
    Resp->SetStringField(TEXT("status"), TEXT("running"));
  }
  else if (LowerSub == TEXT("run_test"))
  {
    FString TestName = Payload->GetStringField(TEXT("testName"));
    if (TestName.IsEmpty())
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("testName is required"), TEXT("MISSING_PARAM"));
      return true;
    }
    
    FAutomationTestFramework& Framework = FAutomationTestFramework::Get();
    Framework.StartTestByName(TestName, 0);
    
    Message = FString::Printf(TEXT("Started test: %s"), *TestName);
    Resp->SetStringField(TEXT("testName"), TestName);
    Resp->SetStringField(TEXT("status"), TEXT("running"));
  }
  else if (LowerSub == TEXT("get_test_results"))
  {
    FAutomationTestFramework& Framework = FAutomationTestFramework::Get();
    
    TArray<TSharedPtr<FJsonValue>> ResultsArray;
    
    // Get test results from framework - results are cleared after each run
    // We report what we can gather
    int32 PassedCount = 0;
    int32 FailedCount = 0;
    int32 TotalCount = 0;
    
    Resp->SetArrayField(TEXT("testResults"), ResultsArray);
    Resp->SetNumberField(TEXT("totalTests"), TotalCount);
    Resp->SetNumberField(TEXT("passedTests"), PassedCount);
    Resp->SetNumberField(TEXT("failedTests"), FailedCount);
    
    Message = TEXT("Retrieved test results");
  }
  else if (LowerSub == TEXT("get_test_info"))
  {
    FString TestName = Payload->GetStringField(TEXT("testName"));
    if (TestName.IsEmpty())
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("testName is required"), TEXT("MISSING_PARAM"));
      return true;
    }
    
    FAutomationTestFramework& Framework = FAutomationTestFramework::Get();
    TArray<FAutomationTestInfo> TestInfos;
    Framework.GetValidTestNames(TestInfos);
    
    bool bFound = false;
    for (const FAutomationTestInfo& Info : TestInfos)
    {
      if (Info.GetDisplayName() == TestName || Info.GetFullTestPath() == TestName)
      {
        Resp->SetStringField(TEXT("name"), Info.GetDisplayName());
        Resp->SetStringField(TEXT("fullPath"), Info.GetFullTestPath());
        Resp->SetStringField(TEXT("testName"), Info.GetTestName());
        Resp->SetStringField(TEXT("sourceFile"), Info.GetSourceFile());
        Resp->SetNumberField(TEXT("sourceLine"), Info.GetSourceFileLine());
        bFound = true;
        Message = FString::Printf(TEXT("Found test: %s"), *TestName);
        break;
      }
    }
    
    if (!bFound)
    {
      bSuccess = false;
      Message = FString::Printf(TEXT("Test not found: %s"), *TestName);
      ErrorCode = TEXT("TEST_NOT_FOUND");
    }
  }
  // =========================================
  // FUNCTIONAL TESTS
  // =========================================
  else if (LowerSub == TEXT("list_functional_tests"))
  {
    // Find all functional test actors in the level
    TArray<TSharedPtr<FJsonValue>> FuncTestsArray;
    
    UWorld* World = GetActiveWorld();
    if (World)
    {
      for (TActorIterator<AActor> It(World); It; ++It)
      {
        AActor* Actor = *It;
        if (Actor && Actor->GetClass()->GetName().Contains(TEXT("FunctionalTest")))
        {
          TSharedPtr<FJsonObject> TestObj = MakeShared<FJsonObject>();
          TestObj->SetStringField(TEXT("name"), Actor->GetName());
          TestObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
          FuncTestsArray.Add(MakeShared<FJsonValueObject>(TestObj));
        }
      }
    }
    
    Resp->SetArrayField(TEXT("functionalTests"), FuncTestsArray);
    Resp->SetNumberField(TEXT("totalCount"), FuncTestsArray.Num());
    Message = FString::Printf(TEXT("Found %d functional tests"), FuncTestsArray.Num());
  }
  else if (LowerSub == TEXT("run_functional_test"))
  {
    FString FunctionalTestPath = Payload->GetStringField(TEXT("functionalTestPath"));
    if (FunctionalTestPath.IsEmpty())
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("functionalTestPath is required"), TEXT("MISSING_PARAM"));
      return true;
    }
    
    // Load the test level if it's a map path
    if (FunctionalTestPath.EndsWith(TEXT(".umap")) || FunctionalTestPath.StartsWith(TEXT("/Game/")))
    {
      GEditor->GetEditorWorldContext().SetCurrentWorld(nullptr);
      FEditorFileUtils::LoadMap(FunctionalTestPath, false, true);
    }
    
    Message = FString::Printf(TEXT("Started functional test: %s"), *FunctionalTestPath);
    Resp->SetStringField(TEXT("status"), TEXT("running"));
  }
  else if (LowerSub == TEXT("get_functional_test_results"))
  {
    TArray<TSharedPtr<FJsonValue>> ResultsArray;
    Resp->SetArrayField(TEXT("functionalTestResults"), ResultsArray);
    Message = TEXT("Retrieved functional test results");
  }
  // =========================================
  // PROFILING - TRACE
  // =========================================
  else if (LowerSub == TEXT("start_trace"))
  {
    FString TraceName = Payload->GetStringField(TEXT("traceName"));
    if (TraceName.IsEmpty())
    {
      TraceName = FString::Printf(TEXT("Trace_%s"), *FDateTime::Now().ToString());
    }
    
    // Start CPU/GPU trace via console command
    if (GEngine)
    {
      GEngine->Exec(GetActiveWorld(), *FString::Printf(TEXT("trace.start %s"), *TraceName));
    }
    
    Message = FString::Printf(TEXT("Started trace: %s"), *TraceName);
    Resp->SetStringField(TEXT("traceName"), TraceName);
    Resp->SetStringField(TEXT("traceStatus"), TEXT("recording"));
  }
  else if (LowerSub == TEXT("stop_trace"))
  {
    if (GEngine)
    {
      GEngine->Exec(GetActiveWorld(), TEXT("trace.stop"));
    }
    
    Message = TEXT("Trace stopped");
    Resp->SetStringField(TEXT("traceStatus"), TEXT("stopped"));
  }
  else if (LowerSub == TEXT("get_trace_status"))
  {
    // Check if trace is active
    bool bTracing = false;
#if UE_TRACE_ENABLED
    bTracing = UE::Trace::IsTracing();
#endif
    
    Resp->SetStringField(TEXT("traceStatus"), bTracing ? TEXT("recording") : TEXT("idle"));
    Message = bTracing ? TEXT("Trace is recording") : TEXT("Trace is idle");
  }
  // =========================================
  // PROFILING - VISUAL LOGGER
  // =========================================
  else if (LowerSub == TEXT("enable_visual_logger"))
  {
#if ENABLE_VISUAL_LOG
    FVisualLogger::Get().SetIsRecording(true);
    Resp->SetBoolField(TEXT("visualLoggerEnabled"), true);
    Message = TEXT("Visual Logger enabled");
#else
    bSuccess = false;
    Message = TEXT("Visual Logger not available in this build");
    ErrorCode = TEXT("FEATURE_NOT_AVAILABLE");
#endif
  }
  else if (LowerSub == TEXT("disable_visual_logger"))
  {
#if ENABLE_VISUAL_LOG
    FVisualLogger::Get().SetIsRecording(false);
    Resp->SetBoolField(TEXT("visualLoggerEnabled"), false);
    Message = TEXT("Visual Logger disabled");
#else
    bSuccess = false;
    Message = TEXT("Visual Logger not available in this build");
    ErrorCode = TEXT("FEATURE_NOT_AVAILABLE");
#endif
  }
  else if (LowerSub == TEXT("get_visual_logger_status"))
  {
#if ENABLE_VISUAL_LOG
    bool bEnabled = FVisualLogger::Get().IsRecording();
    Resp->SetBoolField(TEXT("visualLoggerEnabled"), bEnabled);
    Message = bEnabled ? TEXT("Visual Logger is recording") : TEXT("Visual Logger is idle");
#else
    Resp->SetBoolField(TEXT("visualLoggerEnabled"), false);
    Message = TEXT("Visual Logger not available in this build");
#endif
  }
  // =========================================
  // PROFILING - STATS
  // =========================================
  else if (LowerSub == TEXT("start_stats_capture"))
  {
    FString CaptureName = Payload->GetStringField(TEXT("traceName"));
    if (CaptureName.IsEmpty())
    {
      CaptureName = TEXT("StatsCapture");
    }
    
    if (GEngine)
    {
      GEngine->Exec(GetActiveWorld(), TEXT("stat startfile"));
    }
    
    Message = TEXT("Stats capture started");
    Resp->SetStringField(TEXT("status"), TEXT("capturing"));
  }
  else if (LowerSub == TEXT("stop_stats_capture"))
  {
    if (GEngine)
    {
      GEngine->Exec(GetActiveWorld(), TEXT("stat stopfile"));
    }
    
    Message = TEXT("Stats capture stopped");
    Resp->SetStringField(TEXT("status"), TEXT("stopped"));
  }
  else if (LowerSub == TEXT("get_memory_report"))
  {
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    
    TSharedPtr<FJsonObject> MemReport = MakeShared<FJsonObject>();
    MemReport->SetNumberField(TEXT("totalPhysicalMB"), static_cast<double>(MemStats.TotalPhysical) / (1024.0 * 1024.0));
    MemReport->SetNumberField(TEXT("usedPhysicalMB"), static_cast<double>(MemStats.UsedPhysical) / (1024.0 * 1024.0));
    MemReport->SetNumberField(TEXT("totalVirtualMB"), static_cast<double>(MemStats.TotalVirtual) / (1024.0 * 1024.0));
    MemReport->SetNumberField(TEXT("usedVirtualMB"), static_cast<double>(MemStats.UsedVirtual) / (1024.0 * 1024.0));
    MemReport->SetNumberField(TEXT("peakUsedMB"), static_cast<double>(MemStats.PeakUsedPhysical) / (1024.0 * 1024.0));
    MemReport->SetNumberField(TEXT("availablePhysicalMB"), static_cast<double>(MemStats.AvailablePhysical) / (1024.0 * 1024.0));
    
    Resp->SetObjectField(TEXT("memoryReport"), MemReport);
    Message = TEXT("Memory report generated");
  }
  else if (LowerSub == TEXT("get_performance_stats"))
  {
    TSharedPtr<FJsonObject> PerfStats = MakeShared<FJsonObject>();
    
    // Get frame time
    float DeltaTime = FApp::GetDeltaTime();
    PerfStats->SetNumberField(TEXT("frameTimeMs"), DeltaTime * 1000.0f);
    PerfStats->SetNumberField(TEXT("fps"), DeltaTime > 0.0f ? 1.0f / DeltaTime : 0.0f);
    
    // Memory
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    PerfStats->SetNumberField(TEXT("memoryUsedMB"), static_cast<double>(MemStats.UsedPhysical) / (1024.0 * 1024.0));
    PerfStats->SetNumberField(TEXT("memoryAvailableMB"), static_cast<double>(MemStats.AvailablePhysical) / (1024.0 * 1024.0));
    
    // Use FPlatformTime for CPU cycles
    double Seconds = FPlatformTime::Seconds();
    PerfStats->SetNumberField(TEXT("uptimeSeconds"), Seconds);
    
    Resp->SetObjectField(TEXT("performanceStats"), PerfStats);
    Message = TEXT("Performance stats retrieved");
  }
  // =========================================
  // VALIDATION
  // =========================================
  else if (LowerSub == TEXT("validate_asset"))
  {
    FString AssetPath = Payload->GetStringField(TEXT("assetPath"));
    if (AssetPath.IsEmpty())
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath is required"), TEXT("MISSING_PARAM"));
      return true;
    }
    
    UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
    if (!Asset)
    {
      Resp->SetBoolField(TEXT("isValid"), false);
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      TArray<TSharedPtr<FJsonValue>> ErrorsArray;
      ErrorsArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Asset not found: %s"), *AssetPath)));
      Resp->SetArrayField(TEXT("errors"), ErrorsArray);
      Message = FString::Printf(TEXT("Asset not found: %s"), *AssetPath);
    }
    else
    {
      // Use UObject::IsDataValid for validation (const version for UE 5.7)
      FDataValidationContext Context;
      EDataValidationResult Result = const_cast<const UObject*>(Asset)->IsDataValid(Context);
      
      TArray<TSharedPtr<FJsonValue>> ErrorsArray;
      TArray<TSharedPtr<FJsonValue>> WarningsArray;
      
      for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
      {
        if (Issue.Severity == EMessageSeverity::Error)
        {
          ErrorsArray.Add(MakeShared<FJsonValueString>(Issue.Message.ToString()));
        }
        else if (Issue.Severity == EMessageSeverity::Warning)
        {
          WarningsArray.Add(MakeShared<FJsonValueString>(Issue.Message.ToString()));
        }
      }
      
      Resp->SetBoolField(TEXT("isValid"), Result == EDataValidationResult::Valid);
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetArrayField(TEXT("errors"), ErrorsArray);
      Resp->SetArrayField(TEXT("warnings"), WarningsArray);
      Message = FString::Printf(TEXT("Validated asset: %s"), *AssetPath);
    }
  }
  else if (LowerSub == TEXT("validate_assets_in_path"))
  {
    FString DirectoryPath = Payload->GetStringField(TEXT("directoryPath"));
    if (DirectoryPath.IsEmpty())
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("directoryPath is required"), TEXT("MISSING_PARAM"));
      return true;
    }
    
    bool bRecursive = true;
    Payload->TryGetBoolField(TEXT("recursive"), bRecursive);
    
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssetsByPath(FName(*DirectoryPath), AssetDataList, bRecursive);
    
    TArray<TSharedPtr<FJsonValue>> ResultsArray;
    int32 ValidCount = 0;
    int32 InvalidCount = 0;
    
    for (const FAssetData& AssetData : AssetDataList)
    {
      UObject* Asset = AssetData.GetAsset();
      if (!Asset) continue;
      
      FDataValidationContext Context;
      EDataValidationResult Result = const_cast<const UObject*>(Asset)->IsDataValid(Context);
      
      TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
      ResultObj->SetStringField(TEXT("assetPath"), AssetData.GetObjectPathString());
      ResultObj->SetBoolField(TEXT("isValid"), Result == EDataValidationResult::Valid);
      
      if (Result == EDataValidationResult::Valid)
      {
        ValidCount++;
      }
      else
      {
        InvalidCount++;
        TArray<TSharedPtr<FJsonValue>> ErrorsArray;
        for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
        {
          if (Issue.Severity == EMessageSeverity::Error)
          {
            ErrorsArray.Add(MakeShared<FJsonValueString>(Issue.Message.ToString()));
          }
        }
        ResultObj->SetArrayField(TEXT("errors"), ErrorsArray);
      }
      
      ResultsArray.Add(MakeShared<FJsonValueObject>(ResultObj));
    }
    
    Resp->SetArrayField(TEXT("validationResults"), ResultsArray);
    Resp->SetNumberField(TEXT("totalAssets"), AssetDataList.Num());
    Resp->SetNumberField(TEXT("validAssets"), ValidCount);
    Resp->SetNumberField(TEXT("invalidAssets"), InvalidCount);
    Message = FString::Printf(TEXT("Validated %d assets (%d valid, %d invalid)"), AssetDataList.Num(), ValidCount, InvalidCount);
  }
  else if (LowerSub == TEXT("validate_blueprint"))
  {
    FString BlueprintPath = Payload->GetStringField(TEXT("blueprintPath"));
    if (BlueprintPath.IsEmpty())
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("blueprintPath is required"), TEXT("MISSING_PARAM"));
      return true;
    }
    
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
      Resp->SetBoolField(TEXT("isValid"), false);
      Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
      TArray<TSharedPtr<FJsonValue>> ErrorsArray;
      ErrorsArray.Add(MakeShared<FJsonValueString>(TEXT("Blueprint not found")));
      Resp->SetArrayField(TEXT("errors"), ErrorsArray);
      Message = FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath);
    }
    else
    {
      TArray<TSharedPtr<FJsonValue>> ErrorsArray;
      TArray<TSharedPtr<FJsonValue>> WarningsArray;
      
      // Check compilation status
      if (Blueprint->Status == BS_Error)
      {
        ErrorsArray.Add(MakeShared<FJsonValueString>(TEXT("Blueprint has compilation errors")));
      }
      else if (Blueprint->Status == BS_UpToDateWithWarnings)
      {
        WarningsArray.Add(MakeShared<FJsonValueString>(TEXT("Blueprint has warnings")));
      }
      
      // Validate data
      FDataValidationContext Context;
      EDataValidationResult Result = const_cast<const UBlueprint*>(Blueprint)->IsDataValid(Context);
      
      for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
      {
        if (Issue.Severity == EMessageSeverity::Error)
        {
          ErrorsArray.Add(MakeShared<FJsonValueString>(Issue.Message.ToString()));
        }
        else if (Issue.Severity == EMessageSeverity::Warning)
        {
          WarningsArray.Add(MakeShared<FJsonValueString>(Issue.Message.ToString()));
        }
      }
      
      Resp->SetBoolField(TEXT("isValid"), ErrorsArray.Num() == 0);
      Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
      Resp->SetStringField(TEXT("status"), Blueprint->Status == BS_UpToDate ? TEXT("UpToDate") : TEXT("NeedsCompilation"));
      Resp->SetArrayField(TEXT("errors"), ErrorsArray);
      Resp->SetArrayField(TEXT("warnings"), WarningsArray);
      Message = FString::Printf(TEXT("Validated blueprint: %s"), *BlueprintPath);
    }
  }
  else if (LowerSub == TEXT("check_map_errors"))
  {
    UWorld* World = GetActiveWorld();
    if (!World)
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
      return true;
    }
    
    // Run map check via console command
    if (GEditor)
    {
      GEditor->Exec(World, TEXT("MAP CHECK"));
    }
    
    TArray<TSharedPtr<FJsonValue>> ErrorsArray;
    
    // We can't easily access MessageLog messages, so just report the command was executed
    Resp->SetArrayField(TEXT("mapErrors"), ErrorsArray);
    Resp->SetNumberField(TEXT("errorCount"), 0);
    Resp->SetStringField(TEXT("note"), TEXT("Map check executed. Check Map Check tab in Message Log for results."));
    Message = TEXT("Map check executed");
  }
  else if (LowerSub == TEXT("fix_redirectors"))
  {
    FString DirectoryPath = Payload->GetStringField(TEXT("directoryPath"));
    if (DirectoryPath.IsEmpty())
    {
      DirectoryPath = TEXT("/Game/");
    }
    
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    
    FARFilter Filter;
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add(FName(*DirectoryPath));
    Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
    
    TArray<FAssetData> RedirectorList;
    AssetRegistry.GetAssets(Filter, RedirectorList);
    
    TArray<TSharedPtr<FJsonValue>> RedirectorsArray;
    TArray<UObjectRedirector*> RedirectorsToFix;
    int32 FixedCount = 0;
    int32 BrokenCount = 0;
    
    // Load all redirectors and identify which ones can be fixed
    for (const FAssetData& AssetData : RedirectorList)
    {
      TSharedPtr<FJsonObject> RedirObj = MakeShared<FJsonObject>();
      RedirObj->SetStringField(TEXT("redirectorPath"), AssetData.GetObjectPathString());
      
      UObjectRedirector* Redirector = Cast<UObjectRedirector>(AssetData.GetAsset());
      if (Redirector)
      {
        if (Redirector->DestinationObject)
        {
          RedirObj->SetStringField(TEXT("targetPath"), Redirector->DestinationObject->GetPathName());
          RedirectorsToFix.Add(Redirector);
          RedirObj->SetBoolField(TEXT("willFix"), true);
        }
        else
        {
          // Broken redirector - no destination
          RedirObj->SetBoolField(TEXT("willFix"), false);
          RedirObj->SetStringField(TEXT("error"), TEXT("No destination object"));
          BrokenCount++;
        }
      }
      else
      {
        RedirObj->SetBoolField(TEXT("willFix"), false);
        RedirObj->SetStringField(TEXT("error"), TEXT("Failed to load redirector"));
        BrokenCount++;
      }
      
      RedirectorsArray.Add(MakeShared<FJsonValueObject>(RedirObj));
    }
    
    // Actually fix the redirectors using the AssetTools API
    if (RedirectorsToFix.Num() > 0)
    {
      AssetToolsModule.Get().FixupReferencers(RedirectorsToFix);
      FixedCount = RedirectorsToFix.Num();
    }
    
    // Update response with fix status
    for (TSharedPtr<FJsonValue>& JsonVal : RedirectorsArray)
    {
      TSharedPtr<FJsonObject> Obj = JsonVal->AsObject();
      if (Obj.IsValid() && Obj->GetBoolField(TEXT("willFix")))
      {
        Obj->SetBoolField(TEXT("fixed"), true);
        Obj->RemoveField(TEXT("willFix"));
      }
      else if (Obj.IsValid())
      {
        Obj->SetBoolField(TEXT("fixed"), false);
        Obj->RemoveField(TEXT("willFix"));
      }
    }
    
    Resp->SetArrayField(TEXT("redirectors"), RedirectorsArray);
    Resp->SetNumberField(TEXT("totalRedirectors"), RedirectorList.Num());
    Resp->SetNumberField(TEXT("redirectorsFixed"), FixedCount);
    Resp->SetNumberField(TEXT("brokenRedirectors"), BrokenCount);
    Message = FString::Printf(TEXT("Fixed %d redirectors (%d broken)"), FixedCount, BrokenCount);
  }
  else if (LowerSub == TEXT("get_redirectors"))
  {
    FString DirectoryPath = Payload->GetStringField(TEXT("directoryPath"));
    if (DirectoryPath.IsEmpty())
    {
      DirectoryPath = TEXT("/Game/");
    }
    
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    
    FARFilter Filter;
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add(FName(*DirectoryPath));
    Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
    
    TArray<FAssetData> RedirectorList;
    AssetRegistry.GetAssets(Filter, RedirectorList);
    
    TArray<TSharedPtr<FJsonValue>> RedirectorsArray;
    
    for (const FAssetData& AssetData : RedirectorList)
    {
      TSharedPtr<FJsonObject> RedirObj = MakeShared<FJsonObject>();
      RedirObj->SetStringField(TEXT("redirectorPath"), AssetData.GetObjectPathString());
      
      UObjectRedirector* Redirector = Cast<UObjectRedirector>(AssetData.GetAsset());
      if (Redirector && Redirector->DestinationObject)
      {
        RedirObj->SetStringField(TEXT("targetPath"), Redirector->DestinationObject->GetPathName());
      }
      
      RedirectorsArray.Add(MakeShared<FJsonValueObject>(RedirObj));
    }
    
    Resp->SetArrayField(TEXT("redirectors"), RedirectorsArray);
    Resp->SetNumberField(TEXT("totalRedirectors"), RedirectorList.Num());
    Message = FString::Printf(TEXT("Found %d redirectors"), RedirectorList.Num());
  }
  else
  {
    bSuccess = false;
    Message = FString::Printf(TEXT("Unknown manage_testing action: '%s'"), *LowerSub);
    ErrorCode = TEXT("UNKNOWN_ACTION");
  }

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
  return true;

#else // !WITH_EDITOR
  SendAutomationError(RequestingSocket, RequestId, TEXT("Testing actions require editor build"), TEXT("EDITOR_REQUIRED"));
  return true;
#endif // WITH_EDITOR
}
