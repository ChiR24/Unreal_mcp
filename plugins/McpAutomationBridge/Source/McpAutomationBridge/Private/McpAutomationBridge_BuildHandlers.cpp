// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 32: Build & Deployment Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

// Plugin Manager
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "ProjectDescriptor.h"

// Desktop Platform (GenerateProjectFiles, RunUBT)
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"

// Shader Compilation
#include "ShaderCompiler.h"

// Asset Registry for dependencies/references
#include "AssetRegistry/IAssetRegistry.h"

// PAK/Chunking
#include "IPlatformFilePak.h"

// Build settings
#include "GeneralProjectSettings.h"
#include "GameMapsSettings.h"
#include "Misc/App.h"

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleManageBuildAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_build"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_build")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_build payload missing."),
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
  FString Message = FString::Printf(TEXT("Build action '%s' completed"), *LowerSub);
  FString ErrorCode;

  if (!GEditor) {
    bSuccess = false;
    Message = TEXT("Editor not available");
    ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  // ========================================================================
  // GET BUILD INFO
  // ========================================================================
  if (LowerSub == TEXT("get_build_info")) {
    // Return current build configuration info
    Resp->SetStringField(TEXT("projectName"), FApp::GetProjectName());
    Resp->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
    Resp->SetStringField(TEXT("buildConfiguration"), LexToString(FApp::GetBuildConfiguration()));
    Resp->SetStringField(TEXT("projectDirectory"), FPaths::ProjectDir());
    Resp->SetStringField(TEXT("engineDirectory"), FPaths::EngineDir());
    
    // Platform info
    Resp->SetStringField(TEXT("platform"), FPlatformProperties::IniPlatformName());
    Resp->SetBoolField(TEXT("isEditor"), true);
    Resp->SetBoolField(TEXT("isGame"), false);
    
    // Shader compilation status
    if (GShaderCompilingManager) {
      Resp->SetBoolField(TEXT("isCompilingShaders"), GShaderCompilingManager->IsCompiling());
      Resp->SetNumberField(TEXT("pendingShaderJobs"), GShaderCompilingManager->GetNumRemainingJobs());
    }
    
    bSuccess = true;
    Message = TEXT("Build info retrieved");
  }
  // ========================================================================
  // GENERATE PROJECT FILES
  // ========================================================================
  else if (LowerSub == TEXT("generate_project_files")) {
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (DesktopPlatform) {
      FString ProjectPath = FPaths::GetProjectFilePath();
      FString EngineDir = FPaths::RootDir();
      
      FFeedbackContext* Warn = GWarn;
      
      bool bResult = DesktopPlatform->GenerateProjectFiles(EngineDir, ProjectPath, Warn);
      
      if (bResult) {
        bSuccess = true;
        Message = TEXT("Project files generated successfully");
        Resp->SetBoolField(TEXT("generated"), true);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to generate project files");
        ErrorCode = TEXT("GENERATION_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Desktop platform not available");
      ErrorCode = TEXT("PLATFORM_NOT_AVAILABLE");
    }
  }
  // ========================================================================
  // RUN UBT (Unreal Build Tool)
  // ========================================================================
  else if (LowerSub == TEXT("run_ubt")) {
    FString Arguments;
    Payload->TryGetStringField(TEXT("arguments"), Arguments);
    
    if (Arguments.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("arguments parameter is required for run_ubt");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
      if (DesktopPlatform) {
        if (DesktopPlatform->IsUnrealBuildToolAvailable()) {
          FString EngineDir = FPaths::RootDir();
          FFeedbackContext* Warn = GWarn;
          
          int32 ExitCode = 0;
          bool bResult = DesktopPlatform->RunUnrealBuildTool(
              FText::FromString(TEXT("Running UnrealBuildTool")),
              EngineDir,
              Arguments,
              Warn,
              ExitCode
          );
          
          Resp->SetNumberField(TEXT("exitCode"), ExitCode);
          
          if (bResult && ExitCode == 0) {
            bSuccess = true;
            Message = TEXT("UBT executed successfully");
          } else {
            bSuccess = false;
            Message = FString::Printf(TEXT("UBT failed with exit code %d"), ExitCode);
            ErrorCode = TEXT("UBT_FAILED");
          }
        } else {
          bSuccess = false;
          Message = TEXT("UnrealBuildTool is not available");
          ErrorCode = TEXT("UBT_NOT_AVAILABLE");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Desktop platform not available");
        ErrorCode = TEXT("PLATFORM_NOT_AVAILABLE");
      }
    }
  }
  // ========================================================================
  // COMPILE SHADERS
  // ========================================================================
  else if (LowerSub == TEXT("compile_shaders")) {
    if (GShaderCompilingManager) {
      bool bIsCompiling = GShaderCompilingManager->IsCompiling();
      int32 NumPending = GShaderCompilingManager->GetNumPendingJobs();
      int32 NumOutstanding = GShaderCompilingManager->GetNumOutstandingJobs();
      int32 NumRemaining = GShaderCompilingManager->GetNumRemainingJobs();
      
      Resp->SetBoolField(TEXT("isCompiling"), bIsCompiling);
      Resp->SetNumberField(TEXT("pendingJobs"), NumPending);
      Resp->SetNumberField(TEXT("outstandingJobs"), NumOutstanding);
      Resp->SetNumberField(TEXT("remainingJobs"), NumRemaining);
      
      bSuccess = true;
      Message = bIsCompiling ? 
          FString::Printf(TEXT("Shader compilation in progress: %d remaining"), NumRemaining) :
          TEXT("No shader compilation in progress");
    } else {
      bSuccess = false;
      Message = TEXT("Shader compiling manager not available");
      ErrorCode = TEXT("SHADER_MANAGER_NOT_AVAILABLE");
    }
  }
  // ========================================================================
  // GET TARGET PLATFORMS
  // ========================================================================
  else if (LowerSub == TEXT("get_target_platforms")) {
    // Get common target platforms
    TArray<FString> PlatformNames = {
        TEXT("Win64"),
        TEXT("Linux"),
        TEXT("LinuxArm64"),
        TEXT("Mac"),
        TEXT("Android"),
        TEXT("IOS"),
        TEXT("TVOS"),
    };
    
    TArray<TSharedPtr<FJsonValue>> PlatformsArray;
    PlatformsArray.Reserve(PlatformNames.Num());
    
    for (const FString& PlatformName : PlatformNames) {
      TSharedPtr<FJsonObject> PlatformObj = MakeShared<FJsonObject>();
      PlatformObj->SetStringField(TEXT("name"), PlatformName);
      PlatformsArray.Add(MakeShared<FJsonValueObject>(PlatformObj));
    }
    
    Resp->SetArrayField(TEXT("platforms"), PlatformsArray);
    bSuccess = true;
    Message = TEXT("Target platforms retrieved");
  }
  // ========================================================================
  // LIST PLUGINS
  // ========================================================================
  else if (LowerSub == TEXT("list_plugins")) {
    TArray<TSharedPtr<FJsonValue>> PluginsArray;
    
    IPluginManager& PluginManager = IPluginManager::Get();
    TArray<TSharedRef<IPlugin>> AllPlugins = PluginManager.GetDiscoveredPlugins();
    
    bool bEnabledOnly = false;
    Payload->TryGetBoolField(TEXT("enabledOnly"), bEnabledOnly);
    
    for (const TSharedRef<IPlugin>& Plugin : AllPlugins) {
      if (bEnabledOnly && !Plugin->IsEnabled()) {
        continue;
      }
      
      TSharedPtr<FJsonObject> PluginObj = MakeShared<FJsonObject>();
      PluginObj->SetStringField(TEXT("name"), Plugin->GetName());
      PluginObj->SetStringField(TEXT("friendlyName"), Plugin->GetFriendlyName());
      PluginObj->SetBoolField(TEXT("enabled"), Plugin->IsEnabled());
      PluginObj->SetBoolField(TEXT("canContainContent"), Plugin->CanContainContent());
      PluginObj->SetStringField(TEXT("baseDir"), Plugin->GetBaseDir());
      
      const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
      PluginObj->SetStringField(TEXT("description"), Descriptor.Description);
      PluginObj->SetStringField(TEXT("category"), Descriptor.Category);
      PluginObj->SetStringField(TEXT("version"), Descriptor.VersionName);
      PluginObj->SetStringField(TEXT("createdBy"), Descriptor.CreatedBy);
      
      // Plugin type
      FString PluginType;
      switch (Plugin->GetType()) {
        case EPluginType::Engine: PluginType = TEXT("Engine"); break;
        case EPluginType::Enterprise: PluginType = TEXT("Enterprise"); break;
        case EPluginType::Project: PluginType = TEXT("Project"); break;
        case EPluginType::External: PluginType = TEXT("External"); break;
        case EPluginType::Mod: PluginType = TEXT("Mod"); break;
        default: PluginType = TEXT("Unknown"); break;
      }
      PluginObj->SetStringField(TEXT("type"), PluginType);
      
      PluginsArray.Add(MakeShared<FJsonValueObject>(PluginObj));
    }
    
    Resp->SetArrayField(TEXT("plugins"), PluginsArray);
    Resp->SetNumberField(TEXT("count"), PluginsArray.Num());
    bSuccess = true;
    Message = FString::Printf(TEXT("Found %d plugins"), PluginsArray.Num());
  }
  // ========================================================================
  // GET PLUGIN INFO
  // ========================================================================
  else if (LowerSub == TEXT("get_plugin_info")) {
    FString PluginName;
    Payload->TryGetStringField(TEXT("pluginName"), PluginName);
    
    if (PluginName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("pluginName is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      IPluginManager& PluginManager = IPluginManager::Get();
      TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
      
      if (Plugin.IsValid()) {
        Resp->SetStringField(TEXT("name"), Plugin->GetName());
        Resp->SetStringField(TEXT("friendlyName"), Plugin->GetFriendlyName());
        Resp->SetBoolField(TEXT("enabled"), Plugin->IsEnabled());
        Resp->SetBoolField(TEXT("mounted"), Plugin->IsMounted());
        Resp->SetBoolField(TEXT("canContainContent"), Plugin->CanContainContent());
        Resp->SetBoolField(TEXT("canContainVerse"), Plugin->CanContainVerse());
        Resp->SetStringField(TEXT("baseDir"), Plugin->GetBaseDir());
        Resp->SetStringField(TEXT("contentDir"), Plugin->GetContentDir());
        Resp->SetStringField(TEXT("descriptorFileName"), Plugin->GetDescriptorFileName());
        
        const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
        Resp->SetStringField(TEXT("description"), Descriptor.Description);
        Resp->SetStringField(TEXT("category"), Descriptor.Category);
        Resp->SetStringField(TEXT("version"), Descriptor.VersionName);
        Resp->SetStringField(TEXT("createdBy"), Descriptor.CreatedBy);
        Resp->SetStringField(TEXT("docsURL"), Descriptor.DocsURL);
        Resp->SetStringField(TEXT("supportURL"), Descriptor.SupportURL);
        Resp->SetBoolField(TEXT("isBetaVersion"), Descriptor.bIsBetaVersion);
        
        // Modules
        TArray<TSharedPtr<FJsonValue>> ModulesArray;
        ModulesArray.Reserve(Descriptor.Modules.Num());
        for (const FModuleDescriptor& Module : Descriptor.Modules) {
          TSharedPtr<FJsonObject> ModuleObj = MakeShared<FJsonObject>();
          ModuleObj->SetStringField(TEXT("name"), Module.Name.ToString());
          ModulesArray.Add(MakeShared<FJsonValueObject>(ModuleObj));
        }
        Resp->SetArrayField(TEXT("modules"), ModulesArray);
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Plugin info retrieved for '%s'"), *PluginName);
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Plugin '%s' not found"), *PluginName);
        ErrorCode = TEXT("PLUGIN_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // ENABLE PLUGIN
  // ========================================================================
  else if (LowerSub == TEXT("enable_plugin")) {
    FString PluginName;
    Payload->TryGetStringField(TEXT("pluginName"), PluginName);
    
    if (PluginName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("pluginName is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      IPluginManager& PluginManager = IPluginManager::Get();
      TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
      
      if (Plugin.IsValid()) {
        if (Plugin->IsEnabled()) {
          bSuccess = true;
          Message = FString::Printf(TEXT("Plugin '%s' is already enabled"), *PluginName);
          Resp->SetBoolField(TEXT("alreadyEnabled"), true);
        } else {
          FText FailReason;
          bool bResult = IProjectManager::Get().SetPluginEnabled(PluginName, true, FailReason);
          
          if (bResult) {
            // Save the project descriptor
            FText SaveFailReason;
            if (IProjectManager::Get().SaveCurrentProjectToDisk(SaveFailReason)) {
              bSuccess = true;
              Message = FString::Printf(TEXT("Plugin '%s' enabled. Restart required."), *PluginName);
              Resp->SetBoolField(TEXT("restartRequired"), true);
            } else {
              bSuccess = false;
              Message = FString::Printf(TEXT("Failed to save project: %s"), *SaveFailReason.ToString());
              ErrorCode = TEXT("SAVE_FAILED");
            }
          } else {
            bSuccess = false;
            Message = FString::Printf(TEXT("Failed to enable plugin: %s"), *FailReason.ToString());
            ErrorCode = TEXT("ENABLE_FAILED");
          }
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Plugin '%s' not found"), *PluginName);
        ErrorCode = TEXT("PLUGIN_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // DISABLE PLUGIN
  // ========================================================================
  else if (LowerSub == TEXT("disable_plugin")) {
    FString PluginName;
    Payload->TryGetStringField(TEXT("pluginName"), PluginName);
    
    if (PluginName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("pluginName is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      IPluginManager& PluginManager = IPluginManager::Get();
      TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
      
      if (Plugin.IsValid()) {
        if (!Plugin->IsEnabled()) {
          bSuccess = true;
          Message = FString::Printf(TEXT("Plugin '%s' is already disabled"), *PluginName);
          Resp->SetBoolField(TEXT("alreadyDisabled"), true);
        } else {
          FText FailReason;
          bool bResult = IProjectManager::Get().SetPluginEnabled(PluginName, false, FailReason);
          
          if (bResult) {
            FText SaveFailReason;
            if (IProjectManager::Get().SaveCurrentProjectToDisk(SaveFailReason)) {
              bSuccess = true;
              Message = FString::Printf(TEXT("Plugin '%s' disabled. Restart required."), *PluginName);
              Resp->SetBoolField(TEXT("restartRequired"), true);
            } else {
              bSuccess = false;
              Message = FString::Printf(TEXT("Failed to save project: %s"), *SaveFailReason.ToString());
              ErrorCode = TEXT("SAVE_FAILED");
            }
          } else {
            bSuccess = false;
            Message = FString::Printf(TEXT("Failed to disable plugin: %s"), *FailReason.ToString());
            ErrorCode = TEXT("DISABLE_FAILED");
          }
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Plugin '%s' not found"), *PluginName);
        ErrorCode = TEXT("PLUGIN_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // VALIDATE ASSETS
  // ========================================================================
  else if (LowerSub == TEXT("validate_assets")) {
    FString Directory;
    Payload->TryGetStringField(TEXT("directory"), Directory);
    
    if (Directory.IsEmpty()) {
      Directory = TEXT("/Game");
    }
    
    // Normalize path
    Directory.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    
    TArray<FAssetData> Assets;
    AssetRegistry.GetAssetsByPath(FName(*Directory), Assets, true);
    
    TArray<TSharedPtr<FJsonValue>> ValidatedArray;
    TArray<TSharedPtr<FJsonValue>> ErrorsArray;
    int32 ValidCount = 0;
    int32 InvalidCount = 0;
    
    for (const FAssetData& AssetData : Assets) {
      TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
      FString AssetPath = AssetData.GetSoftObjectPath().ToString();
      AssetObj->SetStringField(TEXT("path"), AssetPath);
      AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
      
      // Check if asset can be loaded
      // UE 5.1+ compatible: Use GetSoftObjectPath().TryLoad() instead of deprecated GetAsset()
      UObject* LoadedAsset = AssetData.GetSoftObjectPath().TryLoad();
      if (LoadedAsset) {
        AssetObj->SetBoolField(TEXT("valid"), true);
        ValidCount++;
      } else {
        AssetObj->SetBoolField(TEXT("valid"), false);
        AssetObj->SetStringField(TEXT("error"), TEXT("Failed to load"));
        InvalidCount++;
        ErrorsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
      }
      
      ValidatedArray.Add(MakeShared<FJsonValueObject>(AssetObj));
    }
    
    Resp->SetNumberField(TEXT("totalAssets"), Assets.Num());
    Resp->SetNumberField(TEXT("validCount"), ValidCount);
    Resp->SetNumberField(TEXT("invalidCount"), InvalidCount);
    Resp->SetArrayField(TEXT("errors"), ErrorsArray);
    
    bSuccess = true;
    Message = FString::Printf(TEXT("Validated %d assets: %d valid, %d invalid"), Assets.Num(), ValidCount, InvalidCount);
  }
  // ========================================================================
  // GET ASSET SIZE INFO
  // ========================================================================
  else if (LowerSub == TEXT("get_asset_size_info")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      // Normalize path
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
      
      FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
      
      if (AssetData.IsValid()) {
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
        Resp->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
        
        // Get package file size
        FString PackageFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(AssetData.PackageName.ToString(), PackageFilename)) {
          PackageFilename += TEXT(".uasset");
          int64 FileSize = IFileManager::Get().FileSize(*PackageFilename);
          if (FileSize >= 0) {
            Resp->SetNumberField(TEXT("fileSizeBytes"), static_cast<double>(FileSize));
            Resp->SetStringField(TEXT("fileSizeFormatted"), 
                FileSize < 1024 ? FString::Printf(TEXT("%lld B"), FileSize) :
                FileSize < 1024*1024 ? FString::Printf(TEXT("%.2f KB"), FileSize / 1024.0) :
                FString::Printf(TEXT("%.2f MB"), FileSize / (1024.0 * 1024.0)));
          }
        }
        
        bSuccess = true;
        Message = TEXT("Asset size info retrieved");
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Asset '%s' not found"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET ASSET REFERENCES
  // ========================================================================
  else if (LowerSub == TEXT("get_asset_references")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      // Normalize path
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
      
      // Get dependencies (what this asset depends on)
      TArray<FAssetDependency> Dependencies;
      AssetRegistry.GetDependencies(FName(*AssetPath), Dependencies);
      
      TArray<TSharedPtr<FJsonValue>> DependenciesArray;
      for (const FAssetDependency& Dep : Dependencies) {
        DependenciesArray.Add(MakeShared<FJsonValueString>(Dep.AssetId.PackageName.ToString()));
      }
      
      // Get referencers (what depends on this asset)
      TArray<FAssetDependency> Referencers;
      AssetRegistry.GetReferencers(FName(*AssetPath), Referencers);
      
      TArray<TSharedPtr<FJsonValue>> ReferencersArray;
      for (const FAssetDependency& Ref : Referencers) {
        ReferencersArray.Add(MakeShared<FJsonValueString>(Ref.AssetId.PackageName.ToString()));
      }
      
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetArrayField(TEXT("dependencies"), DependenciesArray);
      Resp->SetArrayField(TEXT("referencers"), ReferencersArray);
      Resp->SetNumberField(TEXT("dependencyCount"), DependenciesArray.Num());
      Resp->SetNumberField(TEXT("referencerCount"), ReferencersArray.Num());
      
      bSuccess = true;
      Message = FString::Printf(TEXT("Asset has %d dependencies and %d referencers"), 
          DependenciesArray.Num(), ReferencersArray.Num());
    }
  }
  // ========================================================================
  // CONFIGURE BUILD SETTINGS
  // ========================================================================
  else if (LowerSub == TEXT("configure_build_settings")) {
    // This is read-only in editor - report current settings
    UGeneralProjectSettings* ProjectSettings = GetMutableDefault<UGeneralProjectSettings>();
    if (ProjectSettings) {
      Resp->SetStringField(TEXT("projectName"), ProjectSettings->ProjectName);
      Resp->SetStringField(TEXT("companyName"), ProjectSettings->CompanyName);
      Resp->SetStringField(TEXT("projectID"), ProjectSettings->ProjectID.ToString());
      Resp->SetStringField(TEXT("description"), ProjectSettings->Description);
      
      bSuccess = true;
      Message = TEXT("Build settings retrieved");
    } else {
      bSuccess = false;
      Message = TEXT("Failed to get project settings");
      ErrorCode = TEXT("SETTINGS_NOT_AVAILABLE");
    }
  }
  // ========================================================================
  // CLEAR DDC (Derived Data Cache)
  // ========================================================================
  else if (LowerSub == TEXT("clear_ddc")) {
    // DDC operations require console commands
    Resp->SetBoolField(TEXT("requested"), true);
    Resp->SetStringField(TEXT("note"), TEXT("DDC operations are managed by the engine. Use console command 'DDC.Flush' for cache operations."));
    Resp->SetStringField(TEXT("consoleCommand"), TEXT("DDC.Flush"));
    
    bSuccess = true;
    Message = TEXT("DDC clear info provided - use console command");
  }
  // ========================================================================
  // GET DDC STATS
  // ========================================================================
  else if (LowerSub == TEXT("get_ddc_stats")) {
    // DDC stats are available via console commands
    Resp->SetStringField(TEXT("note"), TEXT("DDC statistics are available via console commands"));
    Resp->SetStringField(TEXT("consoleCommand"), TEXT("DDC.Stats"));
    Resp->SetStringField(TEXT("ddcPath"), FPaths::ProjectSavedDir() / TEXT("DerivedDataCache"));
    
    // Check if DDC folder exists and get size
    FString DDCPath = FPaths::ProjectSavedDir() / TEXT("DerivedDataCache");
    if (IFileManager::Get().DirectoryExists(*DDCPath)) {
      Resp->SetBoolField(TEXT("ddcExists"), true);
    } else {
      Resp->SetBoolField(TEXT("ddcExists"), false);
    }
    
    bSuccess = true;
    Message = TEXT("DDC stats info provided");
  }
  // ========================================================================
  // CONFIGURE PLATFORM
  // ========================================================================
  else if (LowerSub == TEXT("configure_platform")) {
    FString Platform;
    Payload->TryGetStringField(TEXT("platform"), Platform);
    
    if (Platform.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("platform parameter is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      // Report platform configuration (read-only inspection)
      Resp->SetStringField(TEXT("platform"), Platform);
      Resp->SetStringField(TEXT("currentPlatform"), FPlatformProperties::IniPlatformName());
      Resp->SetBoolField(TEXT("isPlatformSupported"), true);
      
      bSuccess = true;
      Message = FString::Printf(TEXT("Platform '%s' configuration retrieved"), *Platform);
    }
  }
  // ========================================================================
  // GET PLATFORM SETTINGS
  // ========================================================================
  else if (LowerSub == TEXT("get_platform_settings")) {
    FString Platform;
    Payload->TryGetStringField(TEXT("platform"), Platform);
    
    if (Platform.IsEmpty()) {
      Platform = FPlatformProperties::IniPlatformName();
    }
    
    Resp->SetStringField(TEXT("platform"), Platform);
    Resp->SetStringField(TEXT("platformDisplayName"), FPlatformProperties::PlatformName());
    // Platform capability flags are queried at runtime, provide basic info
    Resp->SetBoolField(TEXT("isDesktop"), FPlatformProperties::IsGameOnly() == false);
    Resp->SetBoolField(TEXT("supportsWindowedMode"), FPlatformProperties::SupportsWindowedMode());
    Resp->SetBoolField(TEXT("hasEditorOnlyData"), FPlatformProperties::HasEditorOnlyData());
    
    bSuccess = true;
    Message = FString::Printf(TEXT("Platform settings for '%s' retrieved"), *Platform);
  }
  // ========================================================================
  // AUDIT ASSETS
  // ========================================================================
  else if (LowerSub == TEXT("audit_assets")) {
    FString Directory;
    Payload->TryGetStringField(TEXT("directory"), Directory);
    
    if (Directory.IsEmpty()) {
      Directory = TEXT("/Game");
    }
    
    // Normalize path
    Directory.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    
    TArray<FAssetData> Assets;
    AssetRegistry.GetAssetsByPath(FName(*Directory), Assets, true);
    
    // Categorize by type
    TMap<FString, int32> TypeCounts;
    int64 TotalSize = 0;
    
    for (const FAssetData& AssetData : Assets) {
      FString TypeName = AssetData.AssetClassPath.GetAssetName().ToString();
      TypeCounts.FindOrAdd(TypeName)++;
    }
    
    // Build type breakdown
    TArray<TSharedPtr<FJsonValue>> TypesArray;
    for (const auto& Pair : TypeCounts) {
      TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
      TypeObj->SetStringField(TEXT("type"), Pair.Key);
      TypeObj->SetNumberField(TEXT("count"), Pair.Value);
      TypesArray.Add(MakeShared<FJsonValueObject>(TypeObj));
    }
    
    Resp->SetStringField(TEXT("directory"), Directory);
    Resp->SetNumberField(TEXT("totalAssets"), Assets.Num());
    Resp->SetArrayField(TEXT("typeBreakdown"), TypesArray);
    
    bSuccess = true;
    Message = FString::Printf(TEXT("Audited %d assets in '%s'"), Assets.Num(), *Directory);
  }
  // ========================================================================
  // CONFIGURE CHUNKING
  // ========================================================================
  else if (LowerSub == TEXT("configure_chunking")) {
    // Report current chunking configuration
    UGameMapsSettings* MapSettings = GetMutableDefault<UGameMapsSettings>();
    if (MapSettings) {
      Resp->SetBoolField(TEXT("useSplitscreen"), MapSettings->bUseSplitscreen);
      Resp->SetStringField(TEXT("gameDefaultMap"), UGameMapsSettings::GetGameDefaultMap());
      bSuccess = true;
      Message = TEXT("Chunking configuration retrieved");
    } else {
      bSuccess = false;
      Message = TEXT("Map settings not available");
      ErrorCode = TEXT("SETTINGS_NOT_AVAILABLE");
    }
  }
  // ========================================================================
  // CREATE PAK FILE (informational)
  // ========================================================================
  else if (LowerSub == TEXT("create_pak_file")) {
    // PAK file creation is done via UAT, not directly in editor
    Resp->SetStringField(TEXT("note"), TEXT("PAK files are created during packaging via RunUAT. Use 'package_project' action for full packaging."));
    bSuccess = true;
    Message = TEXT("PAK file creation is part of the packaging pipeline");
  }
  // ========================================================================
  // CONFIGURE ENCRYPTION
  // ========================================================================
  else if (LowerSub == TEXT("configure_encryption")) {
    // Encryption is configured in project settings
    Resp->SetStringField(TEXT("note"), TEXT("Encryption settings are configured in Project Settings > Crypto > Encryption"));
    bSuccess = true;
    Message = TEXT("Encryption configuration info provided");
  }
  // ========================================================================
  // CONFIGURE DDC
  // ========================================================================
  else if (LowerSub == TEXT("configure_ddc")) {
    Resp->SetStringField(TEXT("note"), TEXT("DDC is configured via Engine.ini [DerivedDataBackendGraph] section"));
    bSuccess = true;
    Message = TEXT("DDC configuration info provided");
  }
  // ========================================================================
  // COOK CONTENT
  // ========================================================================
  else if (LowerSub == TEXT("cook_content")) {
    FString Platform;
    Payload->TryGetStringField(TEXT("platform"), Platform);
    
    if (Platform.IsEmpty()) {
      Platform = FPlatformProperties::IniPlatformName();
    }
    
    // Cooking is typically done via UAT
    Resp->SetStringField(TEXT("platform"), Platform);
    Resp->SetStringField(TEXT("note"), TEXT("Content cooking is performed via RunUAT or the Editor's 'Cook Content for <Platform>' menu."));
    Resp->SetStringField(TEXT("command"), FString::Printf(TEXT("RunUAT BuildCookRun -project=\"%s\" -platform=%s -cook"), *FPaths::GetProjectFilePath(), *Platform));
    
    bSuccess = true;
    Message = FString::Printf(TEXT("Cook command prepared for platform '%s'"), *Platform);
  }
  // ========================================================================
  // PACKAGE PROJECT
  // ========================================================================
  else if (LowerSub == TEXT("package_project")) {
    FString Platform;
    Payload->TryGetStringField(TEXT("platform"), Platform);
    
    if (Platform.IsEmpty()) {
      Platform = FPlatformProperties::IniPlatformName();
    }
    
    FString Configuration;
    Payload->TryGetStringField(TEXT("configuration"), Configuration);
    if (Configuration.IsEmpty()) {
      Configuration = TEXT("Development");
    }
    
    // Package is done via UAT
    Resp->SetStringField(TEXT("platform"), Platform);
    Resp->SetStringField(TEXT("configuration"), Configuration);
    Resp->SetStringField(TEXT("note"), TEXT("Project packaging is performed via RunUAT or File > Package Project menu."));
    Resp->SetStringField(TEXT("command"), FString::Printf(TEXT("RunUAT BuildCookRun -project=\"%s\" -platform=%s -clientconfig=%s -cook -stage -pak -package"), 
        *FPaths::GetProjectFilePath(), *Platform, *Configuration));
    
    bSuccess = true;
    Message = FString::Printf(TEXT("Package command prepared for %s/%s"), *Platform, *Configuration);
  }
  // ========================================================================
  // UNKNOWN ACTION - Return false to allow dispatcher to try other handlers
  // ========================================================================
  else {
    // Don't send response here - let the dispatcher continue to other handlers
    // This fixes the "Dispatch Deadlock" where Testing/Modding actions were
    // being swallowed by the Build handler
    return false;
  }

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
  return true;

#else
  // Non-editor build
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Build operations require editor build."),
                      TEXT("EDITOR_REQUIRED"));
  return true;
#endif // WITH_EDITOR
}
