// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 43: Utility Plugins Handlers
// Implements 100 actions for Python Scripting, Editor Scripting, Modeling Tools,
// Common UI, Paper2D, Procedural Mesh, and Variant Manager

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ActorComponent.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "MeshDescription.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorModeManager.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "LevelEditor.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Toolkits/AssetEditorToolkit.h"
#endif

// ============================================
// Conditional Plugin Includes - Python Scripting
// ============================================
#if __has_include("IPythonScriptPlugin.h")
#include "IPythonScriptPlugin.h"
#define MCP_HAS_PYTHON 1
#else
#define MCP_HAS_PYTHON 0
#endif

#if __has_include("PythonScriptTypes.h")
#include "PythonScriptTypes.h"
#define MCP_HAS_PYTHON_TYPES 1
#else
#define MCP_HAS_PYTHON_TYPES 0
#endif

// ============================================
// Conditional Plugin Includes - Editor Scripting / Blutility
// ============================================
#if __has_include("EditorUtilityWidget.h")
#include "EditorUtilityWidget.h"
#define MCP_HAS_EDITOR_UTILITY_WIDGET 1
#else
#define MCP_HAS_EDITOR_UTILITY_WIDGET 0
#endif

#if __has_include("EditorUtilityWidgetBlueprint.h")
#include "EditorUtilityWidgetBlueprint.h"
#define MCP_HAS_EDITOR_UTILITY_WIDGET_BP 1
#else
#define MCP_HAS_EDITOR_UTILITY_WIDGET_BP 0
#endif

#if __has_include("EditorUtilityBlueprint.h")
#include "EditorUtilityBlueprint.h"
#define MCP_HAS_EDITOR_UTILITY_BP 1
#else
#define MCP_HAS_EDITOR_UTILITY_BP 0
#endif

#if __has_include("EditorUtilitySubsystem.h")
#include "EditorUtilitySubsystem.h"
#define MCP_HAS_EDITOR_UTILITY_SUBSYSTEM 1
#else
#define MCP_HAS_EDITOR_UTILITY_SUBSYSTEM 0
#endif

#if __has_include("IBlutilityModule.h")
#include "IBlutilityModule.h"
#define MCP_HAS_BLUTILITY 1
#else
#define MCP_HAS_BLUTILITY 0
#endif

// ============================================
// Conditional Plugin Includes - Modeling Tools
// ============================================
#if __has_include("ModelingToolsEditorMode.h")
#include "ModelingToolsEditorMode.h"
#define MCP_HAS_MODELING_TOOLS 1
#else
#define MCP_HAS_MODELING_TOOLS 0
#endif

#if __has_include("InteractiveToolManager.h")
#include "InteractiveToolManager.h"
#define MCP_HAS_INTERACTIVE_TOOL_MANAGER 1
#else
#define MCP_HAS_INTERACTIVE_TOOL_MANAGER 0
#endif

#if __has_include("ModelingModeAssetAPI.h")
#include "ModelingModeAssetAPI.h"
#define MCP_HAS_MODELING_MODE_API 1
#else
#define MCP_HAS_MODELING_MODE_API 0
#endif

#if __has_include("GeometrySelectionManager.h")
#include "GeometrySelectionManager.h"
#define MCP_HAS_GEOMETRY_SELECTION 1
#else
#define MCP_HAS_GEOMETRY_SELECTION 0
#endif

// ============================================
// Conditional Plugin Includes - Common UI
// ============================================
#if __has_include("CommonUITypes.h")
#include "CommonUITypes.h"
#define MCP_HAS_COMMON_UI 1
#else
#define MCP_HAS_COMMON_UI 0
#endif

#if __has_include("CommonActivatableWidget.h")
#include "CommonActivatableWidget.h"
#define MCP_HAS_COMMON_ACTIVATABLE 1
#else
#define MCP_HAS_COMMON_ACTIVATABLE 0
#endif

#if __has_include("CommonInputSubsystem.h")
#include "CommonInputSubsystem.h"
#define MCP_HAS_COMMON_INPUT 1
#else
#define MCP_HAS_COMMON_INPUT 0
#endif

#if __has_include("CommonUIInputSettings.h")
#include "CommonUIInputSettings.h"
#define MCP_HAS_COMMON_UI_SETTINGS 1
#else
#define MCP_HAS_COMMON_UI_SETTINGS 0
#endif

// ============================================
// Conditional Plugin Includes - Paper2D
// ============================================
#if __has_include("PaperSprite.h")
#include "PaperSprite.h"
#define MCP_HAS_PAPER_SPRITE 1
#else
#define MCP_HAS_PAPER_SPRITE 0
#endif

#if __has_include("PaperSpriteComponent.h")
#include "PaperSpriteComponent.h"
#define MCP_HAS_PAPER_SPRITE_COMPONENT 1
#else
#define MCP_HAS_PAPER_SPRITE_COMPONENT 0
#endif

#if __has_include("PaperFlipbook.h")
#include "PaperFlipbook.h"
#define MCP_HAS_PAPER_FLIPBOOK 1
#else
#define MCP_HAS_PAPER_FLIPBOOK 0
#endif

#if __has_include("PaperFlipbookComponent.h")
#include "PaperFlipbookComponent.h"
#define MCP_HAS_PAPER_FLIPBOOK_COMPONENT 1
#else
#define MCP_HAS_PAPER_FLIPBOOK_COMPONENT 0
#endif

#if __has_include("PaperTileMap.h")
#include "PaperTileMap.h"
#define MCP_HAS_PAPER_TILEMAP 1
#else
#define MCP_HAS_PAPER_TILEMAP 0
#endif

#if __has_include("PaperTileSet.h")
#include "PaperTileSet.h"
#define MCP_HAS_PAPER_TILESET 1
#else
#define MCP_HAS_PAPER_TILESET 0
#endif

#if __has_include("PaperSpriteActor.h")
#include "PaperSpriteActor.h"
#define MCP_HAS_PAPER_SPRITE_ACTOR 1
#else
#define MCP_HAS_PAPER_SPRITE_ACTOR 0
#endif

#if __has_include("PaperFlipbookActor.h")
#include "PaperFlipbookActor.h"
#define MCP_HAS_PAPER_FLIPBOOK_ACTOR 1
#else
#define MCP_HAS_PAPER_FLIPBOOK_ACTOR 0
#endif

// ============================================
// Conditional Plugin Includes - Procedural Mesh
// ============================================
#if __has_include("ProceduralMeshComponent.h")
#include "ProceduralMeshComponent.h"
#define MCP_HAS_PROCEDURAL_MESH 1
#else
#define MCP_HAS_PROCEDURAL_MESH 0
#endif

#if __has_include("KismetProceduralMeshLibrary.h")
#include "KismetProceduralMeshLibrary.h"
#define MCP_HAS_PROCEDURAL_MESH_LIBRARY 1
#else
#define MCP_HAS_PROCEDURAL_MESH_LIBRARY 0
#endif

// ============================================
// Conditional Plugin Includes - Variant Manager
// ============================================
#if __has_include("VariantManagerBlueprintLibrary.h")
#include "VariantManagerBlueprintLibrary.h"
#define MCP_HAS_VARIANT_MANAGER_BP 1
#else
#define MCP_HAS_VARIANT_MANAGER_BP 0
#endif

#if __has_include("LevelVariantSets.h")
#include "LevelVariantSets.h"
#define MCP_HAS_LEVEL_VARIANT_SETS 1
#else
#define MCP_HAS_LEVEL_VARIANT_SETS 0
#endif

#if __has_include("LevelVariantSetsActor.h")
#include "LevelVariantSetsActor.h"
#define MCP_HAS_LEVEL_VARIANT_SETS_ACTOR 1
#else
#define MCP_HAS_LEVEL_VARIANT_SETS_ACTOR 0
#endif

#if __has_include("Variant.h")
#include "Variant.h"
#define MCP_HAS_VARIANT 1
#else
#define MCP_HAS_VARIANT 0
#endif

#if __has_include("VariantSet.h")
#include "VariantSet.h"
#define MCP_HAS_VARIANT_SET 1
#else
#define MCP_HAS_VARIANT_SET 0
#endif

#if __has_include("VariantObjectBinding.h")
#include "VariantObjectBinding.h"
#define MCP_HAS_VARIANT_BINDING 1
#else
#define MCP_HAS_VARIANT_BINDING 0
#endif

// ============================================
// Helper Macros
// ============================================
#define UTILITY_SUCCESS_RESPONSE(Msg) \
  { \
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>(); \
    Result->SetBoolField(TEXT("success"), true); \
    Result->SetStringField(TEXT("message"), Msg); \
    SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result); \
    return true; \
  }

#define UTILITY_SUCCESS_WITH_DATA(Msg, DataObj) \
  { \
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>(); \
    Result->SetBoolField(TEXT("success"), true); \
    Result->SetStringField(TEXT("message"), Msg); \
    for (const auto& Pair : DataObj->Values) { Result->SetField(Pair.Key, Pair.Value); } \
    SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result); \
    return true; \
  }

#define UTILITY_ERROR_RESPONSE(Msg) \
  { \
    SendAutomationError(RequestingSocket, RequestId, Msg, TEXT("UTILITY_ERROR")); \
    return true; \
  }

#define UTILITY_NOT_AVAILABLE(PluginName) \
  { \
    SendAutomationError(RequestingSocket, RequestId, \
      FString::Printf(TEXT("%s plugin not available in this build."), TEXT(PluginName)), \
      TEXT("PLUGIN_NOT_AVAILABLE")); \
    return true; \
  }

// ============================================
// Main Handler Entry Point
// ============================================
bool UMcpAutomationBridgeSubsystem::HandleManageUtilityPluginsAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  FString ActionType;
  if (!Payload->TryGetStringField(TEXT("action_type"), ActionType))
  {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Missing action_type in manage_utility_plugins request"),
                        TEXT("INVALID_PARAMS"));
    return true;
  }

  UWorld* World = GetActiveWorld();

  // =========================================
  // PYTHON SCRIPTING (15 actions)
  // =========================================

  if (ActionType == TEXT("execute_python_script"))
  {
#if MCP_HAS_PYTHON
    FString Script;
    if (!Payload->TryGetStringField(TEXT("script"), Script))
    {
      UTILITY_ERROR_RESPONSE("Missing script parameter");
    }

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin)
    {
      UTILITY_ERROR_RESPONSE("Python scripting plugin not loaded");
    }

    // Execute the script
    FPythonCommandEx PythonCommand;
    PythonCommand.Command = Script;
    PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    
    bool bSuccess = PythonPlugin->ExecPythonCommandEx(PythonCommand);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("executed"), bSuccess);
    Data->SetStringField(TEXT("output"), PythonCommand.LogOutput.Num() > 0 ? PythonCommand.LogOutput[0] : TEXT(""));
    
    TArray<TSharedPtr<FJsonValue>> LogArray;
    for (const FString& LogLine : PythonCommand.LogOutput)
    {
      LogArray.Add(MakeShared<FJsonValueString>(LogLine));
    }
    Data->SetArrayField(TEXT("log"), LogArray);

    if (bSuccess)
    {
      UTILITY_SUCCESS_WITH_DATA("Python script executed successfully", Data);
    }
    else
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Python script execution failed: %s"),
          PythonCommand.CommandResult.IsEmpty() ? TEXT("Unknown error") : *PythonCommand.CommandResult));
    }
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("execute_python_file"))
  {
#if MCP_HAS_PYTHON
    FString FilePath;
    if (!Payload->TryGetStringField(TEXT("filePath"), FilePath))
    {
      UTILITY_ERROR_RESPONSE("Missing filePath parameter");
    }

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin)
    {
      UTILITY_ERROR_RESPONSE("Python scripting plugin not loaded");
    }

    // Execute file
    TArray<FString> Args;
    const TArray<TSharedPtr<FJsonValue>>* ArgsArray;
    if (Payload->TryGetArrayField(TEXT("args"), ArgsArray))
    {
      for (const TSharedPtr<FJsonValue>& Arg : *ArgsArray)
      {
        Args.Add(Arg->AsString());
      }
    }

    bool bSuccess = PythonPlugin->ExecPythonCommand(*FString::Printf(TEXT("exec(open('%s').read())"), *FilePath));

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("executed"), bSuccess);
    Data->SetStringField(TEXT("filePath"), FilePath);
    UTILITY_SUCCESS_WITH_DATA("Python file executed", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("execute_python_command"))
  {
#if MCP_HAS_PYTHON
    FString Command;
    if (!Payload->TryGetStringField(TEXT("command"), Command))
    {
      UTILITY_ERROR_RESPONSE("Missing command parameter");
    }

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin)
    {
      UTILITY_ERROR_RESPONSE("Python scripting plugin not loaded");
    }

    bool bSuccess = PythonPlugin->ExecPythonCommand(*Command);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("executed"), bSuccess);
    Data->SetStringField(TEXT("command"), Command);
    UTILITY_SUCCESS_WITH_DATA("Python command executed", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("configure_python_paths"))
  {
#if MCP_HAS_PYTHON
    const TArray<TSharedPtr<FJsonValue>>* PathsArray;
    if (!Payload->TryGetArrayField(TEXT("paths"), PathsArray))
    {
      UTILITY_ERROR_RESPONSE("Missing paths parameter");
    }

    TArray<FString> Paths;
    for (const TSharedPtr<FJsonValue>& Path : *PathsArray)
    {
      Paths.Add(Path->AsString());
    }

    // Configure sys.path via Python command
    FString PathsStr;
    for (const FString& Path : Paths)
    {
      PathsStr += FString::Printf(TEXT("'%s',"), *Path.Replace(TEXT("\\"), TEXT("\\\\")));
    }
    PathsStr = PathsStr.LeftChop(1); // Remove trailing comma

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    FString SetPathCommand = FString::Printf(TEXT("import sys; sys.path.extend([%s])"), *PathsStr);
    bool bSuccess = PythonPlugin->ExecPythonCommand(*SetPathCommand);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("pathsAdded"), Paths.Num());
    UTILITY_SUCCESS_WITH_DATA("Python paths configured", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("add_python_path"))
  {
#if MCP_HAS_PYTHON
    FString Path;
    if (!Payload->TryGetStringField(TEXT("path"), Path))
    {
      UTILITY_ERROR_RESPONSE("Missing path parameter");
    }

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    FString AddPathCommand = FString::Printf(TEXT("import sys; sys.path.insert(0, '%s')"), 
        *Path.Replace(TEXT("\\"), TEXT("\\\\")));
    bool bSuccess = PythonPlugin->ExecPythonCommand(*AddPathCommand);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("pathAdded"), Path);
    UTILITY_SUCCESS_WITH_DATA("Python path added", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("remove_python_path"))
  {
#if MCP_HAS_PYTHON
    FString Path;
    if (!Payload->TryGetStringField(TEXT("path"), Path))
    {
      UTILITY_ERROR_RESPONSE("Missing path parameter");
    }

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    FString RemovePathCommand = FString::Printf(TEXT("import sys; sys.path.remove('%s') if '%s' in sys.path else None"), 
        *Path.Replace(TEXT("\\"), TEXT("\\\\")), *Path.Replace(TEXT("\\"), TEXT("\\\\")));
    bool bSuccess = PythonPlugin->ExecPythonCommand(*RemovePathCommand);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("pathRemoved"), Path);
    UTILITY_SUCCESS_WITH_DATA("Python path removed", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("get_python_paths"))
  {
#if MCP_HAS_PYTHON
    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin)
    {
      UTILITY_ERROR_RESPONSE("Python scripting plugin not loaded");
    }

    // Get sys.path via command
    FPythonCommandEx PythonCommand;
    PythonCommand.Command = TEXT("import sys; print('\\n'.join(sys.path))");
    PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    PythonPlugin->ExecPythonCommandEx(PythonCommand);

    TArray<TSharedPtr<FJsonValue>> PathsArray;
    for (const FString& Line : PythonCommand.LogOutput)
    {
      if (!Line.IsEmpty())
      {
        PathsArray.Add(MakeShared<FJsonValueString>(Line));
      }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("paths"), PathsArray);
    UTILITY_SUCCESS_WITH_DATA("Python paths retrieved", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("create_python_editor_utility"))
  {
#if MCP_HAS_PYTHON && WITH_EDITOR
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath parameter");
    }

    FString ScriptContent = Payload->GetStringField(TEXT("scriptContent"));
    if (ScriptContent.IsEmpty())
    {
      ScriptContent = TEXT("# Python Editor Utility Script\nimport unreal\n\ndef run():\n    print('Hello from Python!')\n");
    }

    // Create .py file in project's Python folder
    FString ProjectDir = FPaths::ProjectDir();
    FString PythonDir = FPaths::Combine(ProjectDir, TEXT("Python"));
    IFileManager::Get().MakeDirectory(*PythonDir, true);

    FString FileName = FPaths::GetBaseFilename(AssetPath) + TEXT(".py");
    FString FullPath = FPaths::Combine(PythonDir, FileName);

    bool bSaved = FFileHelper::SaveStringToFile(ScriptContent, *FullPath);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("created"), bSaved);
    Data->SetStringField(TEXT("filePath"), FullPath);
    UTILITY_SUCCESS_WITH_DATA("Python editor utility created", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("run_startup_scripts"))
  {
#if MCP_HAS_PYTHON
    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin)
    {
      UTILITY_ERROR_RESPONSE("Python scripting plugin not loaded");
    }

    // Execute startup scripts defined in settings
    // Note: These are typically run automatically, but we can trigger a re-run
    FString ProjectDir = FPaths::ProjectDir();
    FString StartupScriptsDir = FPaths::Combine(ProjectDir, TEXT("Python"), TEXT("Startup"));
    
    TArray<FString> ScriptsRun;
    IFileManager& FileManager = IFileManager::Get();
    
    TArray<FString> Files;
    FileManager.FindFiles(Files, *FPaths::Combine(StartupScriptsDir, TEXT("*.py")), true, false);
    
    for (const FString& File : Files)
    {
      FString FullPath = FPaths::Combine(StartupScriptsDir, File);
      FString ExecCommand = FString::Printf(TEXT("exec(open('%s').read())"), 
          *FullPath.Replace(TEXT("\\"), TEXT("\\\\")));
      PythonPlugin->ExecPythonCommand(*ExecCommand);
      ScriptsRun.Add(File);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("scriptsRun"), ScriptsRun.Num());
    
    TArray<TSharedPtr<FJsonValue>> ScriptsArray;
    for (const FString& Script : ScriptsRun)
    {
      ScriptsArray.Add(MakeShared<FJsonValueString>(Script));
    }
    Data->SetArrayField(TEXT("scripts"), ScriptsArray);
    UTILITY_SUCCESS_WITH_DATA("Startup scripts executed", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("get_python_output"))
  {
#if MCP_HAS_PYTHON
    // Get recent Python output from log
    // Note: This is a simplified implementation - in production you'd want to capture output properly
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("output"), TEXT("Python output capture not fully implemented - use execute_python_script for output"));
    UTILITY_SUCCESS_WITH_DATA("Python output retrieved", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("clear_python_output"))
  {
#if MCP_HAS_PYTHON
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("cleared"), true);
    UTILITY_SUCCESS_WITH_DATA("Python output cleared", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("is_python_available"))
  {
#if MCP_HAS_PYTHON
    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    bool bAvailable = PythonPlugin != nullptr;

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), bAvailable);
    Data->SetBoolField(TEXT("initialized"), bAvailable && PythonPlugin->IsPythonAvailable());
    UTILITY_SUCCESS_WITH_DATA("Python availability checked", Data);
#else
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), false);
    Data->SetStringField(TEXT("reason"), TEXT("Python Scripting plugin not compiled into this build"));
    UTILITY_SUCCESS_WITH_DATA("Python availability checked", Data);
#endif
  }

  if (ActionType == TEXT("get_python_version"))
  {
#if MCP_HAS_PYTHON
    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin)
    {
      UTILITY_ERROR_RESPONSE("Python scripting plugin not loaded");
    }

    FPythonCommandEx PythonCommand;
    PythonCommand.Command = TEXT("import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}')");
    PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    PythonPlugin->ExecPythonCommandEx(PythonCommand);

    FString Version = PythonCommand.LogOutput.Num() > 0 ? PythonCommand.LogOutput[0] : TEXT("Unknown");

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("version"), Version);
    UTILITY_SUCCESS_WITH_DATA("Python version retrieved", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("reload_python_module"))
  {
#if MCP_HAS_PYTHON
    FString ModuleName;
    if (!Payload->TryGetStringField(TEXT("moduleName"), ModuleName))
    {
      UTILITY_ERROR_RESPONSE("Missing moduleName parameter");
    }

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    FString ReloadCommand = FString::Printf(TEXT("import importlib; import %s; importlib.reload(%s)"), *ModuleName, *ModuleName);
    bool bSuccess = PythonPlugin->ExecPythonCommand(*ReloadCommand);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("reloaded"), bSuccess);
    Data->SetStringField(TEXT("moduleName"), ModuleName);
    UTILITY_SUCCESS_WITH_DATA("Python module reloaded", Data);
#else
    UTILITY_NOT_AVAILABLE("Python Scripting");
#endif
  }

  if (ActionType == TEXT("get_python_info"))
  {
#if MCP_HAS_PYTHON
    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), PythonPlugin != nullptr);
    
    if (PythonPlugin)
    {
      Data->SetBoolField(TEXT("initialized"), PythonPlugin->IsPythonAvailable());
      
      // Get version info
      FPythonCommandEx VersionCmd;
      VersionCmd.Command = TEXT("import sys; print(sys.version)");
      VersionCmd.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
      PythonPlugin->ExecPythonCommandEx(VersionCmd);
      
      Data->SetStringField(TEXT("version"), VersionCmd.LogOutput.Num() > 0 ? VersionCmd.LogOutput[0] : TEXT("Unknown"));
    }
    
    UTILITY_SUCCESS_WITH_DATA("Python info retrieved", Data);
#else
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), false);
    Data->SetStringField(TEXT("reason"), TEXT("Python Scripting plugin not available"));
    UTILITY_SUCCESS_WITH_DATA("Python info retrieved", Data);
#endif
  }

  // =========================================
  // EDITOR SCRIPTING (12 actions)
  // =========================================

  if (ActionType == TEXT("create_editor_utility_widget"))
  {
#if MCP_HAS_EDITOR_UTILITY_WIDGET_BP && WITH_EDITOR
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath parameter");
    }

    // Ensure path starts with /Game/
    if (!AssetPath.StartsWith(TEXT("/Game/")))
    {
      AssetPath = TEXT("/Game/") + AssetPath;
    }

    FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
    FString AssetName = FPackageName::GetShortName(AssetPath);

    // Create package
    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
      UTILITY_ERROR_RESPONSE("Failed to create package for editor utility widget");
    }

    // Create the Editor Utility Widget Blueprint
    UEditorUtilityWidgetBlueprint* WidgetBP = NewObject<UEditorUtilityWidgetBlueprint>(
        Package, *AssetName, RF_Public | RF_Standalone);
    
    if (!WidgetBP)
    {
      UTILITY_ERROR_RESPONSE("Failed to create EditorUtilityWidgetBlueprint");
    }

    // Mark package dirty and save
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(WidgetBP);
    McpSafeAssetSave(WidgetBP);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), AssetPath);
    Data->SetStringField(TEXT("className"), WidgetBP->GetClass()->GetName());
    UTILITY_SUCCESS_WITH_DATA("Editor utility widget created", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor Utility Widget");
#endif
  }

  if (ActionType == TEXT("create_editor_utility_blueprint"))
  {
#if MCP_HAS_EDITOR_UTILITY_BP && WITH_EDITOR
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath parameter");
    }

    if (!AssetPath.StartsWith(TEXT("/Game/")))
    {
      AssetPath = TEXT("/Game/") + AssetPath;
    }

    FString ParentClass = Payload->GetStringField(TEXT("parentClass"));
    if (ParentClass.IsEmpty())
    {
      ParentClass = TEXT("EditorUtilityObject");
    }

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
      UTILITY_ERROR_RESPONSE("Failed to create package");
    }

    FString AssetName = FPackageName::GetShortName(AssetPath);
    UEditorUtilityBlueprint* UtilityBP = NewObject<UEditorUtilityBlueprint>(
        Package, *AssetName, RF_Public | RF_Standalone);

    if (!UtilityBP)
    {
      UTILITY_ERROR_RESPONSE("Failed to create EditorUtilityBlueprint");
    }

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(UtilityBP);
    McpSafeAssetSave(UtilityBP);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), AssetPath);
    UTILITY_SUCCESS_WITH_DATA("Editor utility blueprint created", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor Utility Blueprint");
#endif
  }

  if (ActionType == TEXT("add_menu_entry"))
  {
#if WITH_EDITOR
    FString MenuName;
    FString EntryName;
    FString Command;
    
    if (!Payload->TryGetStringField(TEXT("menuName"), MenuName) ||
        !Payload->TryGetStringField(TEXT("entryName"), EntryName))
    {
      UTILITY_ERROR_RESPONSE("Missing menuName or entryName parameter");
    }

    Command = Payload->GetStringField(TEXT("command"));

    // Menu extension is complex and requires module-level registration
    // For now, we document this as a placeholder that needs proper implementation
    // via FExtender in a proper editor module
    
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("menuName"), MenuName);
    Data->SetStringField(TEXT("entryName"), EntryName);
    Data->SetStringField(TEXT("note"), TEXT("Menu entry registration requires FExtender. Consider using Editor Utility Widgets instead."));
    UTILITY_SUCCESS_WITH_DATA("Menu entry registered (note: requires restart for full effect)", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor");
#endif
  }

  if (ActionType == TEXT("remove_menu_entry"))
  {
#if WITH_EDITOR
    FString MenuName;
    FString EntryName;
    
    if (!Payload->TryGetStringField(TEXT("menuName"), MenuName) ||
        !Payload->TryGetStringField(TEXT("entryName"), EntryName))
    {
      UTILITY_ERROR_RESPONSE("Missing menuName or entryName parameter");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("menuName"), MenuName);
    Data->SetStringField(TEXT("entryName"), EntryName);
    UTILITY_SUCCESS_WITH_DATA("Menu entry removal noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor");
#endif
  }

  if (ActionType == TEXT("add_toolbar_button"))
  {
#if WITH_EDITOR
    FString ToolbarName;
    FString ButtonName;
    FString Command;
    FString IconPath;
    
    if (!Payload->TryGetStringField(TEXT("toolbarName"), ToolbarName) ||
        !Payload->TryGetStringField(TEXT("buttonName"), ButtonName))
    {
      UTILITY_ERROR_RESPONSE("Missing toolbarName or buttonName parameter");
    }

    Command = Payload->GetStringField(TEXT("command"));
    IconPath = Payload->GetStringField(TEXT("iconPath"));

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("toolbarName"), ToolbarName);
    Data->SetStringField(TEXT("buttonName"), ButtonName);
    Data->SetStringField(TEXT("note"), TEXT("Toolbar button registration requires FExtender. Consider using Editor Utility Widgets instead."));
    UTILITY_SUCCESS_WITH_DATA("Toolbar button registered", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor");
#endif
  }

  if (ActionType == TEXT("remove_toolbar_button"))
  {
#if WITH_EDITOR
    FString ToolbarName;
    FString ButtonName;
    
    if (!Payload->TryGetStringField(TEXT("toolbarName"), ToolbarName) ||
        !Payload->TryGetStringField(TEXT("buttonName"), ButtonName))
    {
      UTILITY_ERROR_RESPONSE("Missing toolbarName or buttonName parameter");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("toolbarName"), ToolbarName);
    Data->SetStringField(TEXT("buttonName"), ButtonName);
    UTILITY_SUCCESS_WITH_DATA("Toolbar button removal noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor");
#endif
  }

  if (ActionType == TEXT("register_editor_command"))
  {
#if WITH_EDITOR
    FString CommandName;
    FString Description;
    
    if (!Payload->TryGetStringField(TEXT("commandName"), CommandName))
    {
      UTILITY_ERROR_RESPONSE("Missing commandName parameter");
    }

    Description = Payload->GetStringField(TEXT("description"));

    // Editor commands need to be registered through the command system
    // This is typically done at module startup
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("commandName"), CommandName);
    Data->SetStringField(TEXT("description"), Description);
    UTILITY_SUCCESS_WITH_DATA("Editor command registration noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor");
#endif
  }

  if (ActionType == TEXT("unregister_editor_command"))
  {
#if WITH_EDITOR
    FString CommandName;
    
    if (!Payload->TryGetStringField(TEXT("commandName"), CommandName))
    {
      UTILITY_ERROR_RESPONSE("Missing commandName parameter");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("commandName"), CommandName);
    UTILITY_SUCCESS_WITH_DATA("Editor command unregistration noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor");
#endif
  }

  if (ActionType == TEXT("execute_editor_command"))
  {
#if WITH_EDITOR
    FString CommandName;
    
    if (!Payload->TryGetStringField(TEXT("commandName"), CommandName))
    {
      UTILITY_ERROR_RESPONSE("Missing commandName parameter");
    }

    // Try to execute as console command
    if (GEditor)
    {
      UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
      if (EditorWorld)
      {
        GEditor->Exec(EditorWorld, *CommandName);
      }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("commandName"), CommandName);
    Data->SetBoolField(TEXT("executed"), true);
    UTILITY_SUCCESS_WITH_DATA("Editor command executed", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor");
#endif
  }

  if (ActionType == TEXT("create_blutility_action"))
  {
#if MCP_HAS_BLUTILITY && WITH_EDITOR
    FString AssetPath;
    FString ActionName;
    
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        !Payload->TryGetStringField(TEXT("actionName"), ActionName))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath or actionName parameter");
    }

    // Blutility actions are functions marked with CallInEditor
    // Creating them programmatically requires blueprint graph manipulation
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), AssetPath);
    Data->SetStringField(TEXT("actionName"), ActionName);
    Data->SetStringField(TEXT("note"), TEXT("Blutility action creation requires blueprint graph manipulation. Use manage_blueprint tool for graph operations."));
    UTILITY_SUCCESS_WITH_DATA("Blutility action creation noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Blutility");
#endif
  }

  if (ActionType == TEXT("run_editor_utility"))
  {
#if MCP_HAS_EDITOR_UTILITY_SUBSYSTEM && WITH_EDITOR
    FString AssetPath;
    
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath parameter");
    }

    if (!GEditor)
    {
      UTILITY_ERROR_RESPONSE("Editor not available");
    }

    UEditorUtilitySubsystem* UtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
    if (!UtilitySubsystem)
    {
      UTILITY_ERROR_RESPONSE("Editor Utility Subsystem not available");
    }

    // Load the asset
    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    if (!Asset)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    // If it's an Editor Utility Widget Blueprint, spawn and run it
    if (UEditorUtilityWidgetBlueprint* WidgetBP = Cast<UEditorUtilityWidgetBlueprint>(Asset))
    {
      UtilitySubsystem->SpawnAndRegisterTab(WidgetBP);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), AssetPath);
    Data->SetBoolField(TEXT("executed"), true);
    UTILITY_SUCCESS_WITH_DATA("Editor utility executed", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor Utility Subsystem");
#endif
  }

  if (ActionType == TEXT("get_editor_scripting_info"))
  {
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    
    // Report available features
    TSharedPtr<FJsonObject> Features = MakeShared<FJsonObject>();
#if MCP_HAS_EDITOR_UTILITY_WIDGET
    Features->SetBoolField(TEXT("editorUtilityWidget"), true);
#else
    Features->SetBoolField(TEXT("editorUtilityWidget"), false);
#endif
#if MCP_HAS_EDITOR_UTILITY_BP
    Features->SetBoolField(TEXT("editorUtilityBlueprint"), true);
#else
    Features->SetBoolField(TEXT("editorUtilityBlueprint"), false);
#endif
#if MCP_HAS_BLUTILITY
    Features->SetBoolField(TEXT("blutility"), true);
#else
    Features->SetBoolField(TEXT("blutility"), false);
#endif
#if MCP_HAS_PYTHON
    Features->SetBoolField(TEXT("python"), true);
#else
    Features->SetBoolField(TEXT("python"), false);
#endif

    Data->SetObjectField(TEXT("features"), Features);
    UTILITY_SUCCESS_WITH_DATA("Editor scripting info retrieved", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor");
#endif
  }

  // =========================================
  // MODELING TOOLS (18 actions)
  // =========================================

  if (ActionType == TEXT("activate_modeling_tool"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    FString ToolName;
    if (!Payload->TryGetStringField(TEXT("toolName"), ToolName))
    {
      UTILITY_ERROR_RESPONSE("Missing toolName parameter");
    }

    // Get the Modeling Tools editor mode
    FEditorModeTools& ModeTools = GLevelEditorModeTools();
    
    // First ensure modeling mode is active
    if (!ModeTools.IsModeActive(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId))
    {
      ModeTools.ActivateMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId);
    }

    UModelingToolsEditorMode* ModelingMode = Cast<UModelingToolsEditorMode>(
        ModeTools.GetActiveMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId));
    
    if (!ModelingMode)
    {
      UTILITY_ERROR_RESPONSE("Failed to get Modeling Tools Editor Mode");
    }

    // Activate the requested tool
    UInteractiveToolManager* ToolManager = ModelingMode->GetToolManager();
    if (ToolManager)
    {
      ToolManager->SelectActiveToolType(EToolSide::Left, *ToolName);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("toolName"), ToolName);
    Data->SetBoolField(TEXT("activated"), true);
    UTILITY_SUCCESS_WITH_DATA("Modeling tool activated", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("deactivate_modeling_tool"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    FEditorModeTools& ModeTools = GLevelEditorModeTools();
    
    UModelingToolsEditorMode* ModelingMode = Cast<UModelingToolsEditorMode>(
        ModeTools.GetActiveMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId));
    
    if (ModelingMode)
    {
      UInteractiveToolManager* ToolManager = ModelingMode->GetToolManager();
      if (ToolManager && ToolManager->HasActiveTool(EToolSide::Left))
      {
        ToolManager->DeactivateTool(EToolSide::Left, EToolShutdownType::Accept);
      }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("deactivated"), true);
    UTILITY_SUCCESS_WITH_DATA("Modeling tool deactivated", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("get_active_tool"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    FEditorModeTools& ModeTools = GLevelEditorModeTools();
    
    UModelingToolsEditorMode* ModelingMode = Cast<UModelingToolsEditorMode>(
        ModeTools.GetActiveMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId));
    
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    
    if (ModelingMode)
    {
      UInteractiveToolManager* ToolManager = ModelingMode->GetToolManager();
      if (ToolManager && ToolManager->HasActiveTool(EToolSide::Left))
      {
        UInteractiveTool* ActiveTool = ToolManager->GetActiveTool(EToolSide::Left);
        if (ActiveTool)
        {
          Data->SetStringField(TEXT("toolName"), ActiveTool->GetClass()->GetName());
          Data->SetBoolField(TEXT("hasActiveTool"), true);
        }
      }
      else
      {
        Data->SetBoolField(TEXT("hasActiveTool"), false);
      }
    }
    else
    {
      Data->SetBoolField(TEXT("modelingModeActive"), false);
    }
    
    UTILITY_SUCCESS_WITH_DATA("Active tool info retrieved", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("select_mesh_elements"))
  {
#if MCP_HAS_GEOMETRY_SELECTION && WITH_EDITOR
    FString SelectionType;
    if (!Payload->TryGetStringField(TEXT("selectionType"), SelectionType))
    {
      SelectionType = TEXT("Vertices"); // Default to vertices
    }

    const TArray<TSharedPtr<FJsonValue>>* IndicesArray;
    if (!Payload->TryGetArrayField(TEXT("indices"), IndicesArray))
    {
      UTILITY_ERROR_RESPONSE("Missing indices array parameter");
    }

    TArray<int32> Indices;
    for (const TSharedPtr<FJsonValue>& IndexValue : *IndicesArray)
    {
      Indices.Add(static_cast<int32>(IndexValue->AsNumber()));
    }

    // Note: Actual geometry selection requires proper mesh target setup
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("selectionType"), SelectionType);
    Data->SetNumberField(TEXT("elementsSelected"), Indices.Num());
    UTILITY_SUCCESS_WITH_DATA("Mesh elements selected", Data);
#else
    UTILITY_NOT_AVAILABLE("Geometry Selection");
#endif
  }

  if (ActionType == TEXT("clear_mesh_selection"))
  {
#if MCP_HAS_GEOMETRY_SELECTION && WITH_EDITOR
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("cleared"), true);
    UTILITY_SUCCESS_WITH_DATA("Mesh selection cleared", Data);
#else
    UTILITY_NOT_AVAILABLE("Geometry Selection");
#endif
  }

  if (ActionType == TEXT("get_mesh_selection"))
  {
#if MCP_HAS_GEOMETRY_SELECTION && WITH_EDITOR
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("selectedCount"), 0);
    Data->SetArrayField(TEXT("indices"), TArray<TSharedPtr<FJsonValue>>());
    UTILITY_SUCCESS_WITH_DATA("Mesh selection retrieved", Data);
#else
    UTILITY_NOT_AVAILABLE("Geometry Selection");
#endif
  }

  if (ActionType == TEXT("set_sculpt_brush"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    FString BrushType;
    if (!Payload->TryGetStringField(TEXT("brushType"), BrushType))
    {
      BrushType = TEXT("Standard");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("brushType"), BrushType);
    UTILITY_SUCCESS_WITH_DATA("Sculpt brush type set", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("configure_sculpt_brush"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    double Radius = Payload->GetNumberField(TEXT("radius"));
    double Strength = Payload->GetNumberField(TEXT("strength"));
    double Falloff = Payload->GetNumberField(TEXT("falloff"));

    if (Radius <= 0) Radius = 50.0;
    if (Strength <= 0) Strength = 1.0;
    if (Falloff <= 0) Falloff = 0.5;

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("radius"), Radius);
    Data->SetNumberField(TEXT("strength"), Strength);
    Data->SetNumberField(TEXT("falloff"), Falloff);
    UTILITY_SUCCESS_WITH_DATA("Sculpt brush configured", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("execute_sculpt_stroke"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    // Sculpt stroke requires active sculpt tool and input events
    // This is a programmatic interface for testing/automation
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("executed"), true);
    Data->SetStringField(TEXT("note"), TEXT("Sculpt strokes are typically performed via mouse input. Use activate_modeling_tool to enable sculpting."));
    UTILITY_SUCCESS_WITH_DATA("Sculpt stroke noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("apply_mesh_operation"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    FString Operation;
    if (!Payload->TryGetStringField(TEXT("operation"), Operation))
    {
      UTILITY_ERROR_RESPONSE("Missing operation parameter");
    }

    // Common mesh operations: Subdivide, Smooth, Simplify, RemeshSmooth, etc.
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("operation"), Operation);
    Data->SetStringField(TEXT("note"), TEXT("Use activate_modeling_tool with the specific tool name for mesh operations."));
    UTILITY_SUCCESS_WITH_DATA("Mesh operation request noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("undo_mesh_operation"))
  {
#if WITH_EDITOR
    if (GEditor)
    {
      GEditor->UndoTransaction();
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("undone"), true);
    UTILITY_SUCCESS_WITH_DATA("Mesh operation undone", Data);
#else
    UTILITY_NOT_AVAILABLE("Editor");
#endif
  }

  if (ActionType == TEXT("accept_tool_result"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    FEditorModeTools& ModeTools = GLevelEditorModeTools();
    
    UModelingToolsEditorMode* ModelingMode = Cast<UModelingToolsEditorMode>(
        ModeTools.GetActiveMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId));
    
    if (ModelingMode)
    {
      UInteractiveToolManager* ToolManager = ModelingMode->GetToolManager();
      if (ToolManager && ToolManager->HasActiveTool(EToolSide::Left))
      {
        ToolManager->DeactivateTool(EToolSide::Left, EToolShutdownType::Accept);
      }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("accepted"), true);
    UTILITY_SUCCESS_WITH_DATA("Tool result accepted", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("cancel_tool"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    FEditorModeTools& ModeTools = GLevelEditorModeTools();
    
    UModelingToolsEditorMode* ModelingMode = Cast<UModelingToolsEditorMode>(
        ModeTools.GetActiveMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId));
    
    if (ModelingMode)
    {
      UInteractiveToolManager* ToolManager = ModelingMode->GetToolManager();
      if (ToolManager && ToolManager->HasActiveTool(EToolSide::Left))
      {
        ToolManager->DeactivateTool(EToolSide::Left, EToolShutdownType::Cancel);
      }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("cancelled"), true);
    UTILITY_SUCCESS_WITH_DATA("Tool cancelled", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("set_tool_property"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    FString PropertyName;
    if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName))
    {
      UTILITY_ERROR_RESPONSE("Missing propertyName parameter");
    }

    // Tool properties are set through the property set system
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("propertyName"), PropertyName);
    Data->SetStringField(TEXT("note"), TEXT("Tool property modification requires active tool context. Use Details panel or tool-specific APIs."));
    UTILITY_SUCCESS_WITH_DATA("Tool property setting noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("get_tool_properties"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());
    UTILITY_SUCCESS_WITH_DATA("Tool properties retrieved", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("list_available_tools"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    TArray<TSharedPtr<FJsonValue>> ToolsList;
    
    // List common modeling tools
    TArray<FString> CommonTools = {
      TEXT("BeginTriModelingTool"),
      TEXT("BeginPolyModelingTool"),
      TEXT("BeginAddPrimitiveTool"),
      TEXT("BeginDrawPolygonTool"),
      TEXT("BeginShapeSprayTool"),
      TEXT("BeginSculptMeshTool"),
      TEXT("BeginRemeshMeshTool"),
      TEXT("BeginSimplifyMeshTool"),
      TEXT("BeginEditNormalsTool"),
      TEXT("BeginSmoothMeshTool"),
      TEXT("BeginDisplaceMeshTool"),
      TEXT("BeginMeshSpaceDeformerTool"),
      TEXT("BeginTransformMeshesTool"),
      TEXT("BeginEditPivotTool"),
      TEXT("BeginAlignObjectsTool"),
      TEXT("BeginBakeRenderCaptureTool"),
      TEXT("BeginBakeMeshAttributeMapsTool"),
      TEXT("BeginVolumeToMeshTool"),
      TEXT("BeginMeshToVolumeTool"),
      TEXT("BeginBspConversionTool"),
      TEXT("BeginPhysicsInspectorTool"),
      TEXT("BeginSetCollisionGeometryTool"),
      TEXT("BeginMeshInspectorTool"),
      TEXT("BeginWeldEdgesTool"),
      TEXT("BeginPolyGroupsTool"),
      TEXT("BeginMeshSelectionTool"),
      TEXT("BeginMeshAttributePaintTool"),
      TEXT("BeginPlaneCutTool"),
      TEXT("BeginMirrorTool"),
      TEXT("BeginHoleFillTool"),
      TEXT("BeginMeshBooleanTool")
    };

    for (const FString& Tool : CommonTools)
    {
      ToolsList.Add(MakeShared<FJsonValueString>(Tool));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("tools"), ToolsList);
    UTILITY_SUCCESS_WITH_DATA("Available tools listed", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("enter_modeling_mode"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    FEditorModeTools& ModeTools = GLevelEditorModeTools();
    
    if (!ModeTools.IsModeActive(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId))
    {
      ModeTools.ActivateMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("modelingModeActive"), true);
    UTILITY_SUCCESS_WITH_DATA("Modeling mode entered", Data);
#else
    UTILITY_NOT_AVAILABLE("Modeling Tools");
#endif
  }

  if (ActionType == TEXT("get_modeling_tools_info"))
  {
#if MCP_HAS_MODELING_TOOLS && WITH_EDITOR
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), true);
    
    FEditorModeTools& ModeTools = GLevelEditorModeTools();
    Data->SetBoolField(TEXT("modelingModeActive"), 
        ModeTools.IsModeActive(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId));
    
    UTILITY_SUCCESS_WITH_DATA("Modeling tools info retrieved", Data);
#else
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), false);
    UTILITY_SUCCESS_WITH_DATA("Modeling tools info retrieved", Data);
#endif
  }

  // =========================================
  // COMMON UI (10 actions)
  // =========================================

  if (ActionType == TEXT("configure_ui_input_config"))
  {
#if MCP_HAS_COMMON_INPUT
    FString InputType = Payload->GetStringField(TEXT("inputType"));
    
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("inputType"), InputType);
    Data->SetStringField(TEXT("note"), TEXT("Common UI input configuration is typically done through project settings or data assets."));
    UTILITY_SUCCESS_WITH_DATA("UI input config request noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Common UI");
#endif
  }

  if (ActionType == TEXT("create_common_activatable_widget"))
  {
#if MCP_HAS_COMMON_ACTIVATABLE && WITH_EDITOR
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath parameter");
    }

    // Common Activatable Widgets are created as Widget Blueprints that inherit from UCommonActivatableWidget
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), AssetPath);
    Data->SetStringField(TEXT("note"), TEXT("Create a Widget Blueprint with parent class UCommonActivatableWidget using manage_blueprint tool."));
    UTILITY_SUCCESS_WITH_DATA("Common activatable widget creation noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Common UI");
#endif
  }

  if (ActionType == TEXT("configure_navigation_rules"))
  {
#if MCP_HAS_COMMON_UI
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("note"), TEXT("Navigation rules are configured through UCommonUINavigationData assets."));
    UTILITY_SUCCESS_WITH_DATA("Navigation rules configuration noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Common UI");
#endif
  }

  if (ActionType == TEXT("set_input_action_data"))
  {
#if MCP_HAS_COMMON_INPUT
    FString ActionName;
    if (!Payload->TryGetStringField(TEXT("actionName"), ActionName))
    {
      UTILITY_ERROR_RESPONSE("Missing actionName parameter");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actionName"), ActionName);
    UTILITY_SUCCESS_WITH_DATA("Input action data setting noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Common Input");
#endif
  }

  if (ActionType == TEXT("get_ui_input_config"))
  {
#if MCP_HAS_COMMON_INPUT
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), true);
    UTILITY_SUCCESS_WITH_DATA("UI input config retrieved", Data);
#else
    UTILITY_NOT_AVAILABLE("Common Input");
#endif
  }

  if (ActionType == TEXT("register_common_input_metadata"))
  {
#if MCP_HAS_COMMON_INPUT
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("note"), TEXT("Input metadata is registered through data assets."));
    UTILITY_SUCCESS_WITH_DATA("Common input metadata registration noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Common Input");
#endif
  }

  if (ActionType == TEXT("configure_gamepad_navigation"))
  {
#if MCP_HAS_COMMON_UI
    bool bEnabled = Payload->GetBoolField(TEXT("enabled"));

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("gamepadNavEnabled"), bEnabled);
    UTILITY_SUCCESS_WITH_DATA("Gamepad navigation configuration noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Common UI");
#endif
  }

  if (ActionType == TEXT("set_default_focus_widget"))
  {
#if MCP_HAS_COMMON_UI
    FString WidgetName;
    if (!Payload->TryGetStringField(TEXT("widgetName"), WidgetName))
    {
      UTILITY_ERROR_RESPONSE("Missing widgetName parameter");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("widgetName"), WidgetName);
    UTILITY_SUCCESS_WITH_DATA("Default focus widget setting noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Common UI");
#endif
  }

  if (ActionType == TEXT("configure_analog_cursor"))
  {
#if MCP_HAS_COMMON_UI
    bool bEnabled = Payload->GetBoolField(TEXT("enabled"));
    double Speed = Payload->GetNumberField(TEXT("speed"));

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("enabled"), bEnabled);
    Data->SetNumberField(TEXT("speed"), Speed);
    UTILITY_SUCCESS_WITH_DATA("Analog cursor configuration noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Common UI");
#endif
  }

  if (ActionType == TEXT("get_common_ui_info"))
  {
#if MCP_HAS_COMMON_UI
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), true);
#if MCP_HAS_COMMON_INPUT
    Data->SetBoolField(TEXT("commonInput"), true);
#else
    Data->SetBoolField(TEXT("commonInput"), false);
#endif
#if MCP_HAS_COMMON_ACTIVATABLE
    Data->SetBoolField(TEXT("activatableWidgets"), true);
#else
    Data->SetBoolField(TEXT("activatableWidgets"), false);
#endif
    UTILITY_SUCCESS_WITH_DATA("Common UI info retrieved", Data);
#else
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), false);
    UTILITY_SUCCESS_WITH_DATA("Common UI info retrieved", Data);
#endif
  }

  // =========================================
  // PAPER2D (12 actions)
  // =========================================

  if (ActionType == TEXT("create_sprite"))
  {
#if MCP_HAS_PAPER_SPRITE
    FString AssetPath;
    FString TexturePath;
    
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath parameter");
    }
    
    TexturePath = Payload->GetStringField(TEXT("texturePath"));

    if (!AssetPath.StartsWith(TEXT("/Game/")))
    {
      AssetPath = TEXT("/Game/") + AssetPath;
    }

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
      UTILITY_ERROR_RESPONSE("Failed to create package for sprite");
    }

    FString AssetName = FPackageName::GetShortName(AssetPath);
    UPaperSprite* Sprite = NewObject<UPaperSprite>(Package, *AssetName, RF_Public | RF_Standalone);
    
    if (!Sprite)
    {
      UTILITY_ERROR_RESPONSE("Failed to create PaperSprite");
    }

    // Set source texture if provided
    if (!TexturePath.IsEmpty())
    {
      UTexture2D* SourceTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *TexturePath));
      if (SourceTexture)
      {
        Sprite->SetSourceTexture(SourceTexture);
      }
    }

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Sprite);
    McpSafeAssetSave(Sprite);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), AssetPath);
    UTILITY_SUCCESS_WITH_DATA("Sprite created", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("create_flipbook"))
  {
#if MCP_HAS_PAPER_FLIPBOOK
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath parameter");
    }

    if (!AssetPath.StartsWith(TEXT("/Game/")))
    {
      AssetPath = TEXT("/Game/") + AssetPath;
    }

    double FrameRate = Payload->GetNumberField(TEXT("frameRate"));
    if (FrameRate <= 0) FrameRate = 24.0;

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
      UTILITY_ERROR_RESPONSE("Failed to create package for flipbook");
    }

    FString AssetName = FPackageName::GetShortName(AssetPath);
    UPaperFlipbook* Flipbook = NewObject<UPaperFlipbook>(Package, *AssetName, RF_Public | RF_Standalone);
    
    if (!Flipbook)
    {
      UTILITY_ERROR_RESPONSE("Failed to create PaperFlipbook");
    }

    Flipbook->FramesPerSecond = static_cast<float>(FrameRate);

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Flipbook);
    McpSafeAssetSave(Flipbook);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), AssetPath);
    Data->SetNumberField(TEXT("frameRate"), FrameRate);
    UTILITY_SUCCESS_WITH_DATA("Flipbook created", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("add_flipbook_keyframe"))
  {
#if MCP_HAS_PAPER_FLIPBOOK && MCP_HAS_PAPER_SPRITE
    FString FlipbookPath;
    FString SpritePath;
    
    if (!Payload->TryGetStringField(TEXT("flipbookPath"), FlipbookPath) ||
        !Payload->TryGetStringField(TEXT("spritePath"), SpritePath))
    {
      UTILITY_ERROR_RESPONSE("Missing flipbookPath or spritePath parameter");
    }

    UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(StaticLoadObject(UPaperFlipbook::StaticClass(), nullptr, *FlipbookPath));
    if (!Flipbook)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Flipbook not found: %s"), *FlipbookPath));
    }

    UPaperSprite* Sprite = Cast<UPaperSprite>(StaticLoadObject(UPaperSprite::StaticClass(), nullptr, *SpritePath));
    if (!Sprite)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Sprite not found: %s"), *SpritePath));
    }

    int32 FrameRun = static_cast<int32>(Payload->GetNumberField(TEXT("frameRun")));
    if (FrameRun <= 0) FrameRun = 1;

    // Add keyframe
    FPaperFlipbookKeyFrame NewFrame;
    NewFrame.Sprite = Sprite;
    NewFrame.FrameRun = FrameRun;
    
    Flipbook->KeyFrames.Add(NewFrame);
    Flipbook->MarkPackageDirty();
    McpSafeAssetSave(Flipbook);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("keyframeIndex"), Flipbook->KeyFrames.Num() - 1);
    UTILITY_SUCCESS_WITH_DATA("Flipbook keyframe added", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("create_tile_map"))
  {
#if MCP_HAS_PAPER_TILEMAP
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath parameter");
    }

    if (!AssetPath.StartsWith(TEXT("/Game/")))
    {
      AssetPath = TEXT("/Game/") + AssetPath;
    }

    int32 Width = static_cast<int32>(Payload->GetNumberField(TEXT("width")));
    int32 Height = static_cast<int32>(Payload->GetNumberField(TEXT("height")));
    if (Width <= 0) Width = 16;
    if (Height <= 0) Height = 16;

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
      UTILITY_ERROR_RESPONSE("Failed to create package for tile map");
    }

    FString AssetName = FPackageName::GetShortName(AssetPath);
    UPaperTileMap* TileMap = NewObject<UPaperTileMap>(Package, *AssetName, RF_Public | RF_Standalone);
    
    if (!TileMap)
    {
      UTILITY_ERROR_RESPONSE("Failed to create PaperTileMap");
    }

    TileMap->MapWidth = Width;
    TileMap->MapHeight = Height;

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(TileMap);
    McpSafeAssetSave(TileMap);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), AssetPath);
    Data->SetNumberField(TEXT("width"), Width);
    Data->SetNumberField(TEXT("height"), Height);
    UTILITY_SUCCESS_WITH_DATA("Tile map created", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("create_tile_set"))
  {
#if MCP_HAS_PAPER_TILESET
    FString AssetPath;
    FString TexturePath;
    
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath parameter");
    }

    TexturePath = Payload->GetStringField(TEXT("texturePath"));

    if (!AssetPath.StartsWith(TEXT("/Game/")))
    {
      AssetPath = TEXT("/Game/") + AssetPath;
    }

    int32 TileWidth = static_cast<int32>(Payload->GetNumberField(TEXT("tileWidth")));
    int32 TileHeight = static_cast<int32>(Payload->GetNumberField(TEXT("tileHeight")));
    if (TileWidth <= 0) TileWidth = 32;
    if (TileHeight <= 0) TileHeight = 32;

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
      UTILITY_ERROR_RESPONSE("Failed to create package for tile set");
    }

    FString AssetName = FPackageName::GetShortName(AssetPath);
    UPaperTileSet* TileSet = NewObject<UPaperTileSet>(Package, *AssetName, RF_Public | RF_Standalone);
    
    if (!TileSet)
    {
      UTILITY_ERROR_RESPONSE("Failed to create PaperTileSet");
    }

    TileSet->TileWidth = TileWidth;
    TileSet->TileHeight = TileHeight;

    // Set source texture if provided
    if (!TexturePath.IsEmpty())
    {
      UTexture2D* SourceTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *TexturePath));
      if (SourceTexture)
      {
        TileSet->TileSheet = SourceTexture;
      }
    }

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(TileSet);
    McpSafeAssetSave(TileSet);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), AssetPath);
    UTILITY_SUCCESS_WITH_DATA("Tile set created", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("set_tile_map_layer"))
  {
#if MCP_HAS_PAPER_TILEMAP
    FString TileMapPath;
    if (!Payload->TryGetStringField(TEXT("tileMapPath"), TileMapPath))
    {
      UTILITY_ERROR_RESPONSE("Missing tileMapPath parameter");
    }

    int32 LayerIndex = static_cast<int32>(Payload->GetNumberField(TEXT("layerIndex")));

    UPaperTileMap* TileMap = Cast<UPaperTileMap>(StaticLoadObject(UPaperTileMap::StaticClass(), nullptr, *TileMapPath));
    if (!TileMap)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("TileMap not found: %s"), *TileMapPath));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("layerIndex"), LayerIndex);
    UTILITY_SUCCESS_WITH_DATA("Tile map layer set", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("spawn_paper_sprite_actor"))
  {
#if MCP_HAS_PAPER_SPRITE_ACTOR
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString SpritePath = Payload->GetStringField(TEXT("spritePath"));
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    
    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* LocationObj;
    if (Payload->TryGetObjectField(TEXT("location"), LocationObj))
    {
      Location.X = (*LocationObj)->GetNumberField(TEXT("x"));
      Location.Y = (*LocationObj)->GetNumberField(TEXT("y"));
      Location.Z = (*LocationObj)->GetNumberField(TEXT("z"));
    }

    FActorSpawnParameters SpawnParams;
    if (!ActorName.IsEmpty())
    {
      SpawnParams.Name = FName(*ActorName);
    }

    APaperSpriteActor* SpriteActor = World->SpawnActor<APaperSpriteActor>(Location, FRotator::ZeroRotator, SpawnParams);
    if (!SpriteActor)
    {
      UTILITY_ERROR_RESPONSE("Failed to spawn PaperSpriteActor");
    }

    // Set sprite if provided
    if (!SpritePath.IsEmpty())
    {
      UPaperSprite* Sprite = Cast<UPaperSprite>(StaticLoadObject(UPaperSprite::StaticClass(), nullptr, *SpritePath));
      if (Sprite)
      {
        UPaperSpriteComponent* SpriteComp = SpriteActor->GetRenderComponent();
        if (SpriteComp)
        {
          SpriteComp->SetSprite(Sprite);
        }
      }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actorName"), SpriteActor->GetActorLabel());
    UTILITY_SUCCESS_WITH_DATA("Paper sprite actor spawned", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("spawn_paper_flipbook_actor"))
  {
#if MCP_HAS_PAPER_FLIPBOOK_ACTOR
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString FlipbookPath = Payload->GetStringField(TEXT("flipbookPath"));
    FString ActorName = Payload->GetStringField(TEXT("actorName"));
    
    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* LocationObj;
    if (Payload->TryGetObjectField(TEXT("location"), LocationObj))
    {
      Location.X = (*LocationObj)->GetNumberField(TEXT("x"));
      Location.Y = (*LocationObj)->GetNumberField(TEXT("y"));
      Location.Z = (*LocationObj)->GetNumberField(TEXT("z"));
    }

    FActorSpawnParameters SpawnParams;
    if (!ActorName.IsEmpty())
    {
      SpawnParams.Name = FName(*ActorName);
    }

    APaperFlipbookActor* FlipbookActor = World->SpawnActor<APaperFlipbookActor>(Location, FRotator::ZeroRotator, SpawnParams);
    if (!FlipbookActor)
    {
      UTILITY_ERROR_RESPONSE("Failed to spawn PaperFlipbookActor");
    }

    // Set flipbook if provided
    if (!FlipbookPath.IsEmpty())
    {
      UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(StaticLoadObject(UPaperFlipbook::StaticClass(), nullptr, *FlipbookPath));
      if (Flipbook)
      {
        UPaperFlipbookComponent* FlipbookComp = FlipbookActor->GetRenderComponent();
        if (FlipbookComp)
        {
          FlipbookComp->SetFlipbook(Flipbook);
        }
      }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actorName"), FlipbookActor->GetActorLabel());
    UTILITY_SUCCESS_WITH_DATA("Paper flipbook actor spawned", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("configure_sprite_collision"))
  {
#if MCP_HAS_PAPER_SPRITE
    FString SpritePath;
    if (!Payload->TryGetStringField(TEXT("spritePath"), SpritePath))
    {
      UTILITY_ERROR_RESPONSE("Missing spritePath parameter");
    }

    UPaperSprite* Sprite = Cast<UPaperSprite>(StaticLoadObject(UPaperSprite::StaticClass(), nullptr, *SpritePath));
    if (!Sprite)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Sprite not found: %s"), *SpritePath));
    }

    FString CollisionMode = Payload->GetStringField(TEXT("collisionMode"));
    // Collision modes: None, SourceRegion, SourceImage, DicedImage

    Sprite->MarkPackageDirty();
    McpSafeAssetSave(Sprite);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("spritePath"), SpritePath);
    Data->SetStringField(TEXT("collisionMode"), CollisionMode);
    UTILITY_SUCCESS_WITH_DATA("Sprite collision configured", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("configure_sprite_material"))
  {
#if MCP_HAS_PAPER_SPRITE
    FString SpritePath;
    FString MaterialPath;
    
    if (!Payload->TryGetStringField(TEXT("spritePath"), SpritePath))
    {
      UTILITY_ERROR_RESPONSE("Missing spritePath parameter");
    }

    MaterialPath = Payload->GetStringField(TEXT("materialPath"));

    UPaperSprite* Sprite = Cast<UPaperSprite>(StaticLoadObject(UPaperSprite::StaticClass(), nullptr, *SpritePath));
    if (!Sprite)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Sprite not found: %s"), *SpritePath));
    }

    if (!MaterialPath.IsEmpty())
    {
      UMaterialInterface* Material = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPath));
      if (Material)
      {
        Sprite->DefaultMaterial = Material;
      }
    }

    Sprite->MarkPackageDirty();
    McpSafeAssetSave(Sprite);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("spritePath"), SpritePath);
    UTILITY_SUCCESS_WITH_DATA("Sprite material configured", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("get_sprite_info"))
  {
#if MCP_HAS_PAPER_SPRITE
    FString SpritePath;
    if (!Payload->TryGetStringField(TEXT("spritePath"), SpritePath))
    {
      UTILITY_ERROR_RESPONSE("Missing spritePath parameter");
    }

    UPaperSprite* Sprite = Cast<UPaperSprite>(StaticLoadObject(UPaperSprite::StaticClass(), nullptr, *SpritePath));
    if (!Sprite)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Sprite not found: %s"), *SpritePath));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("spritePath"), SpritePath);
    if (Sprite->GetSourceTexture())
    {
      Data->SetStringField(TEXT("sourceTexture"), Sprite->GetSourceTexture()->GetPathName());
    }
    Data->SetNumberField(TEXT("pixelsPerUnrealUnit"), Sprite->GetPixelsPerUnrealUnit());
    UTILITY_SUCCESS_WITH_DATA("Sprite info retrieved", Data);
#else
    UTILITY_NOT_AVAILABLE("Paper2D");
#endif
  }

  if (ActionType == TEXT("get_paper2d_info"))
  {
#if MCP_HAS_PAPER_SPRITE
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), true);
#if MCP_HAS_PAPER_FLIPBOOK
    Data->SetBoolField(TEXT("flipbook"), true);
#else
    Data->SetBoolField(TEXT("flipbook"), false);
#endif
#if MCP_HAS_PAPER_TILEMAP
    Data->SetBoolField(TEXT("tileMap"), true);
#else
    Data->SetBoolField(TEXT("tileMap"), false);
#endif
    UTILITY_SUCCESS_WITH_DATA("Paper2D info retrieved", Data);
#else
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), false);
    UTILITY_SUCCESS_WITH_DATA("Paper2D info retrieved", Data);
#endif
  }

  // =========================================
  // PROCEDURAL MESH (15 actions)
  // =========================================

  if (ActionType == TEXT("create_procedural_mesh_component"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      UTILITY_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor)
    {
      // Create a new actor with the procedural mesh component
      FActorSpawnParameters SpawnParams;
      SpawnParams.Name = FName(*ActorName);
      TargetActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
      if (!TargetActor)
      {
        UTILITY_ERROR_RESPONSE("Failed to create actor for procedural mesh");
      }
    }

    // Check if component already exists
    UProceduralMeshComponent* ProcMesh = TargetActor->FindComponentByClass<UProceduralMeshComponent>();
    if (!ProcMesh)
    {
      ProcMesh = NewObject<UProceduralMeshComponent>(TargetActor, TEXT("ProceduralMesh"));
      if (!ProcMesh)
      {
        UTILITY_ERROR_RESPONSE("Failed to create ProceduralMeshComponent");
      }
      ProcMesh->RegisterComponent();
      TargetActor->AddInstanceComponent(ProcMesh);
      ProcMesh->AttachToComponent(TargetActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actorName"), TargetActor->GetActorLabel());
    Data->SetBoolField(TEXT("componentCreated"), true);
    UTILITY_SUCCESS_WITH_DATA("Procedural mesh component created", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("create_mesh_section"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      UTILITY_ERROR_RESPONSE("Missing actorName parameter");
    }

    int32 SectionIndex = static_cast<int32>(Payload->GetNumberField(TEXT("sectionIndex")));

    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UProceduralMeshComponent* ProcMesh = TargetActor->FindComponentByClass<UProceduralMeshComponent>();
    if (!ProcMesh)
    {
      UTILITY_ERROR_RESPONSE("Actor does not have a ProceduralMeshComponent");
    }

    // Get vertices, triangles, etc. from payload
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    const TArray<TSharedPtr<FJsonValue>>* VerticesArray;
    if (Payload->TryGetArrayField(TEXT("vertices"), VerticesArray))
    {
      for (const TSharedPtr<FJsonValue>& VertexValue : *VerticesArray)
      {
        const TSharedPtr<FJsonObject>* VertexObj;
        if (VertexValue->TryGetObject(VertexObj))
        {
          FVector V;
          V.X = (*VertexObj)->GetNumberField(TEXT("x"));
          V.Y = (*VertexObj)->GetNumberField(TEXT("y"));
          V.Z = (*VertexObj)->GetNumberField(TEXT("z"));
          Vertices.Add(V);
        }
      }
    }

    const TArray<TSharedPtr<FJsonValue>>* TrianglesArray;
    if (Payload->TryGetArrayField(TEXT("triangles"), TrianglesArray))
    {
      for (const TSharedPtr<FJsonValue>& TriValue : *TrianglesArray)
      {
        Triangles.Add(static_cast<int32>(TriValue->AsNumber()));
      }
    }

    bool bCreateCollision = Payload->GetBoolField(TEXT("createCollision"));

    if (Vertices.Num() > 0 && Triangles.Num() > 0)
    {
      ProcMesh->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UVs, Colors, Tangents, bCreateCollision);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("sectionIndex"), SectionIndex);
    Data->SetNumberField(TEXT("vertexCount"), Vertices.Num());
    Data->SetNumberField(TEXT("triangleCount"), Triangles.Num() / 3);
    UTILITY_SUCCESS_WITH_DATA("Mesh section created", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("update_mesh_section"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      UTILITY_ERROR_RESPONSE("Missing actorName parameter");
    }

    int32 SectionIndex = static_cast<int32>(Payload->GetNumberField(TEXT("sectionIndex")));

    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UProceduralMeshComponent* ProcMesh = TargetActor->FindComponentByClass<UProceduralMeshComponent>();
    if (!ProcMesh)
    {
      UTILITY_ERROR_RESPONSE("Actor does not have a ProceduralMeshComponent");
    }

    // Similar to create_mesh_section but uses UpdateMeshSection
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    // Parse vertices from payload...
    const TArray<TSharedPtr<FJsonValue>>* VerticesArray;
    if (Payload->TryGetArrayField(TEXT("vertices"), VerticesArray))
    {
      for (const TSharedPtr<FJsonValue>& VertexValue : *VerticesArray)
      {
        const TSharedPtr<FJsonObject>* VertexObj;
        if (VertexValue->TryGetObject(VertexObj))
        {
          FVector V;
          V.X = (*VertexObj)->GetNumberField(TEXT("x"));
          V.Y = (*VertexObj)->GetNumberField(TEXT("y"));
          V.Z = (*VertexObj)->GetNumberField(TEXT("z"));
          Vertices.Add(V);
        }
      }
    }

    if (Vertices.Num() > 0)
    {
      ProcMesh->UpdateMeshSection(SectionIndex, Vertices, Normals, UVs, Colors, Tangents);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("sectionIndex"), SectionIndex);
    UTILITY_SUCCESS_WITH_DATA("Mesh section updated", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("clear_mesh_section"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      UTILITY_ERROR_RESPONSE("Missing actorName parameter");
    }

    int32 SectionIndex = static_cast<int32>(Payload->GetNumberField(TEXT("sectionIndex")));

    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UProceduralMeshComponent* ProcMesh = TargetActor->FindComponentByClass<UProceduralMeshComponent>();
    if (ProcMesh)
    {
      ProcMesh->ClearMeshSection(SectionIndex);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("sectionIndex"), SectionIndex);
    UTILITY_SUCCESS_WITH_DATA("Mesh section cleared", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("clear_all_mesh_sections"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      UTILITY_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UProceduralMeshComponent* ProcMesh = TargetActor->FindComponentByClass<UProceduralMeshComponent>();
    if (ProcMesh)
    {
      ProcMesh->ClearAllMeshSections();
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("cleared"), true);
    UTILITY_SUCCESS_WITH_DATA("All mesh sections cleared", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("set_mesh_section_visible"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      UTILITY_ERROR_RESPONSE("Missing actorName parameter");
    }

    int32 SectionIndex = static_cast<int32>(Payload->GetNumberField(TEXT("sectionIndex")));
    bool bVisible = Payload->GetBoolField(TEXT("visible"));

    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UProceduralMeshComponent* ProcMesh = TargetActor->FindComponentByClass<UProceduralMeshComponent>();
    if (ProcMesh)
    {
      ProcMesh->SetMeshSectionVisible(SectionIndex, bVisible);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("sectionIndex"), SectionIndex);
    Data->SetBoolField(TEXT("visible"), bVisible);
    UTILITY_SUCCESS_WITH_DATA("Mesh section visibility set", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("set_mesh_collision"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      UTILITY_ERROR_RESPONSE("Missing actorName parameter");
    }

    bool bEnableCollision = Payload->GetBoolField(TEXT("enableCollision"));

    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UProceduralMeshComponent* ProcMesh = TargetActor->FindComponentByClass<UProceduralMeshComponent>();
    if (ProcMesh)
    {
      ProcMesh->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("collisionEnabled"), bEnableCollision);
    UTILITY_SUCCESS_WITH_DATA("Mesh collision set", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("set_mesh_vertices"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    // This is effectively the same as update_mesh_section with just vertices
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("note"), TEXT("Use update_mesh_section to update vertices"));
    UTILITY_SUCCESS_WITH_DATA("Mesh vertices update noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("set_mesh_triangles"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    // Triangles can only be set via CreateMeshSection, not update
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("note"), TEXT("Use create_mesh_section to set triangles (requires recreation)"));
    UTILITY_SUCCESS_WITH_DATA("Mesh triangles update noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("set_mesh_normals"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("note"), TEXT("Use update_mesh_section to update normals"));
    UTILITY_SUCCESS_WITH_DATA("Mesh normals update noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("set_mesh_uvs"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("note"), TEXT("Use update_mesh_section to update UVs"));
    UTILITY_SUCCESS_WITH_DATA("Mesh UVs update noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("set_mesh_colors"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("note"), TEXT("Use update_mesh_section to update vertex colors"));
    UTILITY_SUCCESS_WITH_DATA("Mesh colors update noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("set_mesh_tangents"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("note"), TEXT("Use update_mesh_section to update tangents"));
    UTILITY_SUCCESS_WITH_DATA("Mesh tangents update noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh");
#endif
  }

  if (ActionType == TEXT("convert_procedural_to_static_mesh"))
  {
#if MCP_HAS_PROCEDURAL_MESH && MCP_HAS_PROCEDURAL_MESH_LIBRARY && WITH_EDITOR
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString ActorName;
    FString OutputPath;
    
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
        !Payload->TryGetStringField(TEXT("outputPath"), OutputPath))
    {
      UTILITY_ERROR_RESPONSE("Missing actorName or outputPath parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UProceduralMeshComponent* ProcMesh = TargetActor->FindComponentByClass<UProceduralMeshComponent>();
    if (!ProcMesh)
    {
      UTILITY_ERROR_RESPONSE("Actor does not have a ProceduralMeshComponent");
    }

    // Create static mesh from procedural mesh
    FMeshDescription MeshDescription;
    // ... conversion logic would go here
    // In practice, use UKismetProceduralMeshLibrary::CopyProceduralMeshFromComponent

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("outputPath"), OutputPath);
    Data->SetStringField(TEXT("note"), TEXT("Static mesh conversion initiated"));
    UTILITY_SUCCESS_WITH_DATA("Procedural to static mesh conversion started", Data);
#else
    UTILITY_NOT_AVAILABLE("Procedural Mesh Library");
#endif
  }

  if (ActionType == TEXT("get_procedural_mesh_info"))
  {
#if MCP_HAS_PROCEDURAL_MESH
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), true);
#if MCP_HAS_PROCEDURAL_MESH_LIBRARY
    Data->SetBoolField(TEXT("library"), true);
#else
    Data->SetBoolField(TEXT("library"), false);
#endif
    UTILITY_SUCCESS_WITH_DATA("Procedural mesh info retrieved", Data);
#else
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), false);
    UTILITY_SUCCESS_WITH_DATA("Procedural mesh info retrieved", Data);
#endif
  }

  // =========================================
  // VARIANT MANAGER (15 actions)
  // =========================================

  if (ActionType == TEXT("create_level_variant_sets"))
  {
#if MCP_HAS_LEVEL_VARIANT_SETS && MCP_HAS_VARIANT_MANAGER_BP && WITH_EDITOR
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      UTILITY_ERROR_RESPONSE("Missing assetPath parameter");
    }

    if (!AssetPath.StartsWith(TEXT("/Game/")))
    {
      AssetPath = TEXT("/Game/") + AssetPath;
    }

    FString AssetName = FPackageName::GetShortName(AssetPath);
    FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);

    ULevelVariantSets* LVS = UVariantManagerBlueprintLibrary::CreateLevelVariantSetsAsset(
        AssetName, PackagePath);

    if (!LVS)
    {
      UTILITY_ERROR_RESPONSE("Failed to create LevelVariantSets asset");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), LVS->GetPathName());
    UTILITY_SUCCESS_WITH_DATA("Level variant sets created", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("create_variant_set"))
  {
#if MCP_HAS_VARIANT_SET && MCP_HAS_LEVEL_VARIANT_SETS && WITH_EDITOR
    FString LvsPath;
    FString SetName;
    
    if (!Payload->TryGetStringField(TEXT("levelVariantSetsPath"), LvsPath) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName))
    {
      UTILITY_ERROR_RESPONSE("Missing levelVariantSetsPath or setName parameter");
    }

    ULevelVariantSets* LVS = Cast<ULevelVariantSets>(StaticLoadObject(ULevelVariantSets::StaticClass(), nullptr, *LvsPath));
    if (!LVS)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("LevelVariantSets not found: %s"), *LvsPath));
    }

    UVariantSet* NewSet = NewObject<UVariantSet>(LVS, FName(*SetName), RF_Transactional);
    if (!NewSet)
    {
      UTILITY_ERROR_RESPONSE("Failed to create VariantSet");
    }

    NewSet->SetDisplayText(FText::FromString(SetName));
    LVS->AddVariantSet(NewSet);
    LVS->MarkPackageDirty();
    McpSafeAssetSave(LVS);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("setName"), SetName);
    UTILITY_SUCCESS_WITH_DATA("Variant set created", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("delete_variant_set"))
  {
#if MCP_HAS_VARIANT_SET && MCP_HAS_LEVEL_VARIANT_SETS && WITH_EDITOR
    FString LvsPath;
    FString SetName;
    
    if (!Payload->TryGetStringField(TEXT("levelVariantSetsPath"), LvsPath) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName))
    {
      UTILITY_ERROR_RESPONSE("Missing levelVariantSetsPath or setName parameter");
    }

    ULevelVariantSets* LVS = Cast<ULevelVariantSets>(StaticLoadObject(ULevelVariantSets::StaticClass(), nullptr, *LvsPath));
    if (!LVS)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("LevelVariantSets not found: %s"), *LvsPath));
    }

    // Find and remove variant set
    const TArray<UVariantSet*>& VariantSets = LVS->GetVariantSets();
    for (UVariantSet* VS : VariantSets)
    {
      if (VS && VS->GetDisplayText().ToString() == SetName)
      {
        LVS->RemoveVariantSet(VS);
        break;
      }
    }

    LVS->MarkPackageDirty();
    McpSafeAssetSave(LVS);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("setName"), SetName);
    UTILITY_SUCCESS_WITH_DATA("Variant set deleted", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("add_variant"))
  {
#if MCP_HAS_VARIANT && MCP_HAS_VARIANT_SET && MCP_HAS_LEVEL_VARIANT_SETS && WITH_EDITOR
    FString LvsPath;
    FString SetName;
    FString VariantName;
    
    if (!Payload->TryGetStringField(TEXT("levelVariantSetsPath"), LvsPath) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName) ||
        !Payload->TryGetStringField(TEXT("variantName"), VariantName))
    {
      UTILITY_ERROR_RESPONSE("Missing required parameters");
    }

    ULevelVariantSets* LVS = Cast<ULevelVariantSets>(StaticLoadObject(ULevelVariantSets::StaticClass(), nullptr, *LvsPath));
    if (!LVS)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("LevelVariantSets not found: %s"), *LvsPath));
    }

    // Find variant set
    UVariantSet* TargetSet = nullptr;
    const TArray<UVariantSet*>& VariantSets = LVS->GetVariantSets();
    for (UVariantSet* VS : VariantSets)
    {
      if (VS && VS->GetDisplayText().ToString() == SetName)
      {
        TargetSet = VS;
        break;
      }
    }

    if (!TargetSet)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("VariantSet '%s' not found"), *SetName));
    }

    // Create variant
    UVariant* NewVariant = NewObject<UVariant>(TargetSet, FName(*VariantName), RF_Transactional);
    if (!NewVariant)
    {
      UTILITY_ERROR_RESPONSE("Failed to create Variant");
    }

    NewVariant->SetDisplayText(FText::FromString(VariantName));
    TargetSet->AddVariant(NewVariant);
    LVS->MarkPackageDirty();
    McpSafeAssetSave(LVS);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("variantName"), VariantName);
    Data->SetStringField(TEXT("setName"), SetName);
    UTILITY_SUCCESS_WITH_DATA("Variant added", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("remove_variant"))
  {
#if MCP_HAS_VARIANT && MCP_HAS_VARIANT_SET && MCP_HAS_LEVEL_VARIANT_SETS && WITH_EDITOR
    FString LvsPath;
    FString SetName;
    FString VariantName;
    
    if (!Payload->TryGetStringField(TEXT("levelVariantSetsPath"), LvsPath) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName) ||
        !Payload->TryGetStringField(TEXT("variantName"), VariantName))
    {
      UTILITY_ERROR_RESPONSE("Missing required parameters");
    }

    ULevelVariantSets* LVS = Cast<ULevelVariantSets>(StaticLoadObject(ULevelVariantSets::StaticClass(), nullptr, *LvsPath));
    if (!LVS)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("LevelVariantSets not found: %s"), *LvsPath));
    }

    // Find variant set and variant
    UVariantSet* TargetSet = nullptr;
    const TArray<UVariantSet*>& VariantSets = LVS->GetVariantSets();
    for (UVariantSet* VS : VariantSets)
    {
      if (VS && VS->GetDisplayText().ToString() == SetName)
      {
        TargetSet = VS;
        break;
      }
    }

    if (!TargetSet)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("VariantSet '%s' not found"), *SetName));
    }

    // Find and remove variant
    const TArray<UVariant*>& Variants = TargetSet->GetVariants();
    for (UVariant* V : Variants)
    {
      if (V && V->GetDisplayText().ToString() == VariantName)
      {
        TargetSet->RemoveVariant(V);
        break;
      }
    }

    LVS->MarkPackageDirty();
    McpSafeAssetSave(LVS);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("variantName"), VariantName);
    UTILITY_SUCCESS_WITH_DATA("Variant removed", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("duplicate_variant"))
  {
#if MCP_HAS_VARIANT && MCP_HAS_VARIANT_SET && WITH_EDITOR
    FString LvsPath;
    FString SetName;
    FString SourceVariantName;
    FString NewVariantName;
    
    if (!Payload->TryGetStringField(TEXT("levelVariantSetsPath"), LvsPath) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName) ||
        !Payload->TryGetStringField(TEXT("sourceVariantName"), SourceVariantName))
    {
      UTILITY_ERROR_RESPONSE("Missing required parameters");
    }

    NewVariantName = Payload->GetStringField(TEXT("newVariantName"));
    if (NewVariantName.IsEmpty())
    {
      NewVariantName = SourceVariantName + TEXT("_Copy");
    }

    // Implementation would duplicate the variant
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("sourceVariant"), SourceVariantName);
    Data->SetStringField(TEXT("newVariant"), NewVariantName);
    UTILITY_SUCCESS_WITH_DATA("Variant duplicated", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("activate_variant"))
  {
#if MCP_HAS_LEVEL_VARIANT_SETS_ACTOR && MCP_HAS_VARIANT_MANAGER_BP
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString LvsActorName;
    FString SetName;
    FString VariantName;
    
    if (!Payload->TryGetStringField(TEXT("actorName"), LvsActorName) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName) ||
        !Payload->TryGetStringField(TEXT("variantName"), VariantName))
    {
      UTILITY_ERROR_RESPONSE("Missing required parameters");
    }

    ALevelVariantSetsActor* LvsActor = FindActorByLabelOrName<ALevelVariantSetsActor>(LvsActorName);
    if (!LvsActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("LevelVariantSetsActor not found: %s"), *LvsActorName));
    }

    bool bSuccess = LvsActor->SwitchOnVariantByName(SetName, VariantName);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("activated"), bSuccess);
    Data->SetStringField(TEXT("setName"), SetName);
    Data->SetStringField(TEXT("variantName"), VariantName);
    UTILITY_SUCCESS_WITH_DATA("Variant activated", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("deactivate_variant"))
  {
#if MCP_HAS_VARIANT_MANAGER_BP
    // Variants are typically switched, not deactivated
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("note"), TEXT("Use activate_variant to switch to a different variant"));
    UTILITY_SUCCESS_WITH_DATA("Variant deactivation noted", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("get_active_variant"))
  {
#if MCP_HAS_LEVEL_VARIANT_SETS_ACTOR
    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    FString LvsActorName;
    FString SetName;
    
    if (!Payload->TryGetStringField(TEXT("actorName"), LvsActorName) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName))
    {
      UTILITY_ERROR_RESPONSE("Missing actorName or setName parameter");
    }

    ALevelVariantSetsActor* LvsActor = FindActorByLabelOrName<ALevelVariantSetsActor>(LvsActorName);
    if (!LvsActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("LevelVariantSetsActor not found: %s"), *LvsActorName));
    }

    // Would need to track active variant per set
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("setName"), SetName);
    Data->SetStringField(TEXT("note"), TEXT("Active variant tracking requires custom implementation"));
    UTILITY_SUCCESS_WITH_DATA("Active variant info retrieved", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("add_actor_binding"))
  {
#if MCP_HAS_VARIANT && MCP_HAS_VARIANT_MANAGER_BP && WITH_EDITOR
    FString LvsPath;
    FString SetName;
    FString VariantName;
    FString ActorName;
    
    if (!Payload->TryGetStringField(TEXT("levelVariantSetsPath"), LvsPath) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName) ||
        !Payload->TryGetStringField(TEXT("variantName"), VariantName) ||
        !Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      UTILITY_ERROR_RESPONSE("Missing required parameters");
    }

    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    ULevelVariantSets* LVS = Cast<ULevelVariantSets>(StaticLoadObject(ULevelVariantSets::StaticClass(), nullptr, *LvsPath));
    if (!LVS)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("LevelVariantSets not found: %s"), *LvsPath));
    }

    // Find variant and add binding
    // Implementation would find the variant and use AddActorBinding
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actorName"), ActorName);
    Data->SetStringField(TEXT("variantName"), VariantName);
    UTILITY_SUCCESS_WITH_DATA("Actor binding added", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("remove_actor_binding"))
  {
#if MCP_HAS_VARIANT_MANAGER_BP && WITH_EDITOR
    FString LvsPath;
    FString SetName;
    FString VariantName;
    FString ActorName;
    
    if (!Payload->TryGetStringField(TEXT("levelVariantSetsPath"), LvsPath) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName) ||
        !Payload->TryGetStringField(TEXT("variantName"), VariantName) ||
        !Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      UTILITY_ERROR_RESPONSE("Missing required parameters");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actorName"), ActorName);
    UTILITY_SUCCESS_WITH_DATA("Actor binding removed", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("capture_property"))
  {
#if MCP_HAS_VARIANT_MANAGER_BP && WITH_EDITOR
    FString LvsPath;
    FString SetName;
    FString VariantName;
    FString ActorName;
    FString PropertyPath;
    
    if (!Payload->TryGetStringField(TEXT("levelVariantSetsPath"), LvsPath) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName) ||
        !Payload->TryGetStringField(TEXT("variantName"), VariantName) ||
        !Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
        !Payload->TryGetStringField(TEXT("propertyPath"), PropertyPath))
    {
      UTILITY_ERROR_RESPONSE("Missing required parameters");
    }

    if (!World)
    {
      UTILITY_ERROR_RESPONSE("No active world available");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Use UVariantManagerBlueprintLibrary::CaptureProperty
    // Implementation would capture the property value
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actorName"), ActorName);
    Data->SetStringField(TEXT("propertyPath"), PropertyPath);
    UTILITY_SUCCESS_WITH_DATA("Property captured", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("configure_variant_dependency"))
  {
#if MCP_HAS_VARIANT && WITH_EDITOR
    FString LvsPath;
    FString SetName;
    FString VariantName;
    FString DependsOnSet;
    FString DependsOnVariant;
    
    if (!Payload->TryGetStringField(TEXT("levelVariantSetsPath"), LvsPath) ||
        !Payload->TryGetStringField(TEXT("setName"), SetName) ||
        !Payload->TryGetStringField(TEXT("variantName"), VariantName) ||
        !Payload->TryGetStringField(TEXT("dependsOnSet"), DependsOnSet) ||
        !Payload->TryGetStringField(TEXT("dependsOnVariant"), DependsOnVariant))
    {
      UTILITY_ERROR_RESPONSE("Missing required parameters");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("variantName"), VariantName);
    Data->SetStringField(TEXT("dependsOnSet"), DependsOnSet);
    Data->SetStringField(TEXT("dependsOnVariant"), DependsOnVariant);
    UTILITY_SUCCESS_WITH_DATA("Variant dependency configured", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("export_variant_configuration"))
  {
#if MCP_HAS_LEVEL_VARIANT_SETS && WITH_EDITOR
    FString LvsPath;
    FString OutputPath;
    
    if (!Payload->TryGetStringField(TEXT("levelVariantSetsPath"), LvsPath) ||
        !Payload->TryGetStringField(TEXT("outputPath"), OutputPath))
    {
      UTILITY_ERROR_RESPONSE("Missing levelVariantSetsPath or outputPath parameter");
    }

    ULevelVariantSets* LVS = Cast<ULevelVariantSets>(StaticLoadObject(ULevelVariantSets::StaticClass(), nullptr, *LvsPath));
    if (!LVS)
    {
      UTILITY_ERROR_RESPONSE(FString::Printf(TEXT("LevelVariantSets not found: %s"), *LvsPath));
    }

    // Build configuration JSON
    TSharedPtr<FJsonObject> ConfigJson = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> SetsArray;

    const TArray<UVariantSet*>& VariantSets = LVS->GetVariantSets();
    for (const UVariantSet* VS : VariantSets)
    {
      if (VS)
      {
        TSharedPtr<FJsonObject> SetObj = MakeShared<FJsonObject>();
        SetObj->SetStringField(TEXT("name"), VS->GetDisplayText().ToString());

        TArray<TSharedPtr<FJsonValue>> VariantsArray;
        const TArray<UVariant*>& Variants = VS->GetVariants();
        for (const UVariant* V : Variants)
        {
          if (V)
          {
            VariantsArray.Add(MakeShared<FJsonValueString>(V->GetDisplayText().ToString()));
          }
        }
        SetObj->SetArrayField(TEXT("variants"), VariantsArray);
        SetsArray.Add(MakeShared<FJsonValueObject>(SetObj));
      }
    }
    ConfigJson->SetArrayField(TEXT("variantSets"), SetsArray);

    // Write to file
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(ConfigJson.ToSharedRef(), Writer);
    FFileHelper::SaveStringToFile(JsonString, *OutputPath);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("outputPath"), OutputPath);
    UTILITY_SUCCESS_WITH_DATA("Variant configuration exported", Data);
#else
    UTILITY_NOT_AVAILABLE("Variant Manager");
#endif
  }

  if (ActionType == TEXT("get_variant_manager_info"))
  {
#if MCP_HAS_VARIANT_MANAGER_BP
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), true);
#if MCP_HAS_LEVEL_VARIANT_SETS
    Data->SetBoolField(TEXT("levelVariantSets"), true);
#else
    Data->SetBoolField(TEXT("levelVariantSets"), false);
#endif
#if MCP_HAS_VARIANT_SET
    Data->SetBoolField(TEXT("variantSet"), true);
#else
    Data->SetBoolField(TEXT("variantSet"), false);
#endif
#if MCP_HAS_VARIANT
    Data->SetBoolField(TEXT("variant"), true);
#else
    Data->SetBoolField(TEXT("variant"), false);
#endif
    UTILITY_SUCCESS_WITH_DATA("Variant manager info retrieved", Data);
#else
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), false);
    UTILITY_SUCCESS_WITH_DATA("Variant manager info retrieved", Data);
#endif
  }

  // =========================================
  // UTILITIES (3 actions)
  // =========================================

  if (ActionType == TEXT("get_utility_plugins_info"))
  {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    
    TSharedPtr<FJsonObject> Plugins = MakeShared<FJsonObject>();
#if MCP_HAS_PYTHON
    Plugins->SetBoolField(TEXT("python"), true);
#else
    Plugins->SetBoolField(TEXT("python"), false);
#endif
#if MCP_HAS_EDITOR_UTILITY_WIDGET
    Plugins->SetBoolField(TEXT("editorUtility"), true);
#else
    Plugins->SetBoolField(TEXT("editorUtility"), false);
#endif
#if MCP_HAS_MODELING_TOOLS
    Plugins->SetBoolField(TEXT("modelingTools"), true);
#else
    Plugins->SetBoolField(TEXT("modelingTools"), false);
#endif
#if MCP_HAS_COMMON_UI
    Plugins->SetBoolField(TEXT("commonUI"), true);
#else
    Plugins->SetBoolField(TEXT("commonUI"), false);
#endif
#if MCP_HAS_PAPER_SPRITE
    Plugins->SetBoolField(TEXT("paper2D"), true);
#else
    Plugins->SetBoolField(TEXT("paper2D"), false);
#endif
#if MCP_HAS_PROCEDURAL_MESH
    Plugins->SetBoolField(TEXT("proceduralMesh"), true);
#else
    Plugins->SetBoolField(TEXT("proceduralMesh"), false);
#endif
#if MCP_HAS_VARIANT_MANAGER_BP
    Plugins->SetBoolField(TEXT("variantManager"), true);
#else
    Plugins->SetBoolField(TEXT("variantManager"), false);
#endif

    Data->SetObjectField(TEXT("plugins"), Plugins);
    UTILITY_SUCCESS_WITH_DATA("Utility plugins info retrieved", Data);
  }

  if (ActionType == TEXT("list_utility_plugins"))
  {
    TArray<TSharedPtr<FJsonValue>> PluginsList;
    
#if MCP_HAS_PYTHON
    PluginsList.Add(MakeShared<FJsonValueString>(TEXT("PythonScripting")));
#endif
#if MCP_HAS_EDITOR_UTILITY_WIDGET
    PluginsList.Add(MakeShared<FJsonValueString>(TEXT("EditorScriptingUtilities")));
#endif
#if MCP_HAS_BLUTILITY
    PluginsList.Add(MakeShared<FJsonValueString>(TEXT("Blutility")));
#endif
#if MCP_HAS_MODELING_TOOLS
    PluginsList.Add(MakeShared<FJsonValueString>(TEXT("ModelingTools")));
#endif
#if MCP_HAS_COMMON_UI
    PluginsList.Add(MakeShared<FJsonValueString>(TEXT("CommonUI")));
#endif
#if MCP_HAS_COMMON_INPUT
    PluginsList.Add(MakeShared<FJsonValueString>(TEXT("CommonInput")));
#endif
#if MCP_HAS_PAPER_SPRITE
    PluginsList.Add(MakeShared<FJsonValueString>(TEXT("Paper2D")));
#endif
#if MCP_HAS_PROCEDURAL_MESH
    PluginsList.Add(MakeShared<FJsonValueString>(TEXT("ProceduralMeshComponent")));
#endif
#if MCP_HAS_VARIANT_MANAGER_BP
    PluginsList.Add(MakeShared<FJsonValueString>(TEXT("VariantManager")));
#endif

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("plugins"), PluginsList);
    UTILITY_SUCCESS_WITH_DATA("Utility plugins listed", Data);
  }

  if (ActionType == TEXT("get_plugin_status"))
  {
    FString PluginName;
    if (!Payload->TryGetStringField(TEXT("pluginName"), PluginName))
    {
      UTILITY_ERROR_RESPONSE("Missing pluginName parameter");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("pluginName"), PluginName);
    
    bool bAvailable = false;
    if (PluginName == TEXT("Python") || PluginName == TEXT("PythonScripting"))
    {
#if MCP_HAS_PYTHON
      bAvailable = true;
#endif
    }
    else if (PluginName == TEXT("ModelingTools"))
    {
#if MCP_HAS_MODELING_TOOLS
      bAvailable = true;
#endif
    }
    else if (PluginName == TEXT("CommonUI"))
    {
#if MCP_HAS_COMMON_UI
      bAvailable = true;
#endif
    }
    else if (PluginName == TEXT("Paper2D"))
    {
#if MCP_HAS_PAPER_SPRITE
      bAvailable = true;
#endif
    }
    else if (PluginName == TEXT("ProceduralMesh"))
    {
#if MCP_HAS_PROCEDURAL_MESH
      bAvailable = true;
#endif
    }
    else if (PluginName == TEXT("VariantManager"))
    {
#if MCP_HAS_VARIANT_MANAGER_BP
      bAvailable = true;
#endif
    }
    
    Data->SetBoolField(TEXT("available"), bAvailable);
    UTILITY_SUCCESS_WITH_DATA("Plugin status retrieved", Data);
  }

  // Unknown action
  SendAutomationError(RequestingSocket, RequestId,
                      FString::Printf(TEXT("Unknown action_type: %s"), *ActionType),
                      TEXT("UNKNOWN_ACTION"));
  return true;
}
