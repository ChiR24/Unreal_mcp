// McpAutomationBridge_SystemControlHandlers.cpp
// Handles system_control, console_command, and inspect actions

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Math/UnrealMathUtility.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Misc/OutputDeviceNull.h"
#include "EditorAssetLibrary.h"
#include "GeneralProjectSettings.h"
#include "EditorValidatorSubsystem.h"
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#endif
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleSystemControlAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (!Payload.IsValid())
  {
    return false;
  }

  // Get the sub-action from the payload
  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("action"), SubAction))
  {
    // No sub-action means this isn't meant for us
    return false;
  }

  const FString LowerSubAction = SubAction.ToLower();
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

  // =========================================================================
  // batch_execute - Execute multiple operations in sequence by enqueuing them
  // =========================================================================
  if (LowerSubAction == TEXT("batch_execute"))
  {
    const TArray<TSharedPtr<FJsonValue>>* RequestsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("requests"), RequestsArray) || !RequestsArray)
    {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("batch_execute requires 'requests' array"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    int32 EnqueuedCount = 0;
    for (int32 i = 0; i < RequestsArray->Num(); ++i)
    {
      const TSharedPtr<FJsonValue>& RequestValue = (*RequestsArray)[i];
      if (!RequestValue.IsValid() || RequestValue->Type != EJson::Object) continue;

      const TSharedPtr<FJsonObject> RequestObj = RequestValue->AsObject();
      FString SubRequestId, SubActionName;
      if (!RequestObj->TryGetStringField(TEXT("requestId"), SubRequestId)) 
        SubRequestId = RequestId + TEXT("_") + FString::FromInt(i);
      
      // The action name is often in the 'action' field of the sub-request
      if (!RequestObj->TryGetStringField(TEXT("action"), SubActionName))
        continue;

      // Create a pending request and add it to the queue.
      FPendingAutomationRequest P;
      P.RequestId = SubRequestId;
      P.Action = SubActionName;
      P.Payload = RequestObj;
      P.RequestingSocket = RequestingSocket;

      {
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        PendingAutomationRequests.Add(MoveTemp(P));
        bPendingRequestsScheduled = true;
      }
      EnqueuedCount++;
    }

    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
    ResultPayload->SetBoolField(TEXT("success"), true);
    ResultPayload->SetNumberField(TEXT("enqueuedCount"), EnqueuedCount);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Batch requests enqueued for sequential execution"), ResultPayload);
    return true;
  }

  // =========================================================================
  // validate_asset - Validate a single asset exists
  // =========================================================================
  if (LowerSubAction == TEXT("validate_asset"))
  {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("validate_asset requires 'assetPath'"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

#if WITH_EDITOR
    bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);
    
    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
    ResultPayload->SetBoolField(TEXT("success"), true);
    ResultPayload->SetBoolField(TEXT("exists"), bExists);
    ResultPayload->SetStringField(TEXT("assetPath"), AssetPath);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           bExists ? TEXT("Asset exists") : TEXT("Asset not found"),
                           ResultPayload);
#else
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
#endif
    return true;
  }

  // Profile commands
  if (LowerSubAction == TEXT("profile")) {
    FString ProfileType;
    bool bEnabled = true;
    Payload->TryGetStringField(TEXT("profileType"), ProfileType);
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    FString Command;
    if (ProfileType.ToLower() == TEXT("cpu")) {
      Command = bEnabled ? TEXT("stat cpu") : TEXT("stat cpu");
    } else if (ProfileType.ToLower() == TEXT("gpu")) {
      Command = bEnabled ? TEXT("stat gpu") : TEXT("stat gpu");
    } else if (ProfileType.ToLower() == TEXT("memory")) {
      Command = bEnabled ? TEXT("stat memory") : TEXT("stat memory");
    } else if (ProfileType.ToLower() == TEXT("fps")) {
      Command = bEnabled ? TEXT("stat fps") : TEXT("stat fps");
    }

    if (!Command.IsEmpty()) {
      GEngine->Exec(nullptr, *Command);
      Result->SetStringField(TEXT("command"), Command);
      Result->SetBoolField(TEXT("enabled"), bEnabled);
      SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Executed profile command: %s"), *Command),
          Result, FString());
      return true;
    }
  }

  // Show FPS
  if (LowerSubAction == TEXT("show_fps")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    FString Command = bEnabled ? TEXT("stat fps") : TEXT("stat fps");
    GEngine->Exec(nullptr, *Command);
    Result->SetStringField(TEXT("command"), Command);
    Result->SetBoolField(TEXT("enabled"), bEnabled);
    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("FPS display %s"),
                        bEnabled ? TEXT("enabled") : TEXT("disabled")),
        Result, FString());
    return true;
  }

  // Set quality
  if (LowerSubAction == TEXT("set_quality")) {
    FString Category;
    int32 Level = 1;
    Payload->TryGetStringField(TEXT("category"), Category);
    Payload->TryGetNumberField(TEXT("level"), Level);

    if (!Category.IsEmpty()) {
      FString Command = FString::Printf(TEXT("sg.%s %d"), *Category, Level);
      GEngine->Exec(nullptr, *Command);
      Result->SetStringField(TEXT("command"), Command);
      Result->SetStringField(TEXT("category"), Category);
      Result->SetNumberField(TEXT("level"), Level);
      SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Set quality %s to %d"), *Category, Level),
          Result, FString());
      return true;
    }
  }

  // Screenshot
  if (LowerSubAction == TEXT("screenshot")) {
    FString Filename = TEXT("screenshot");
    Payload->TryGetStringField(TEXT("filename"), Filename);

    FString Command = FString::Printf(TEXT("screenshot %s"), *Filename);
    GEngine->Exec(nullptr, *Command);
    Result->SetStringField(TEXT("command"), Command);
    Result->SetStringField(TEXT("filename"), Filename);
    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Screenshot captured: %s"), *Filename), Result,
        FString());
    return true;
  }

  if (LowerSubAction == TEXT("get_project_settings")) {
#if WITH_EDITOR
    FString Category;
    Payload->TryGetStringField(TEXT("category"), Category);
    const UGeneralProjectSettings *ProjectSettings =
        GetDefault<UGeneralProjectSettings>();
    TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();
    if (ProjectSettings) {
      SettingsObj->SetStringField(TEXT("projectName"),
                                  ProjectSettings->ProjectName);
      SettingsObj->SetStringField(TEXT("companyName"),
                                  ProjectSettings->CompanyName);
      SettingsObj->SetStringField(TEXT("projectVersion"),
                                  ProjectSettings->ProjectVersion);
      SettingsObj->SetStringField(TEXT("description"),
                                  ProjectSettings->Description);
    }

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("category"),
                        Category.IsEmpty() ? TEXT("Project") : Category);
    Out->SetObjectField(TEXT("settings"), SettingsObj);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Project settings retrieved"), Out, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("get_project_settings requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  if (LowerSubAction == TEXT("get_engine_version")) {
#if WITH_EDITOR
    const FEngineVersion &EngineVer = FEngineVersion::Current();
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("version"), EngineVer.ToString());
    Out->SetNumberField(TEXT("major"), EngineVer.GetMajor());
    Out->SetNumberField(TEXT("minor"), EngineVer.GetMinor());
    Out->SetNumberField(TEXT("patch"), EngineVer.GetPatch());
    const bool bIs56OrAbove =
        (EngineVer.GetMajor() > 5) ||
        (EngineVer.GetMajor() == 5 && EngineVer.GetMinor() >= 6);
    Out->SetBoolField(TEXT("isUE56OrAbove"), bIs56OrAbove);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Engine version retrieved"), Out, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("get_engine_version requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  if (LowerSubAction == TEXT("get_feature_flags")) {
#if WITH_EDITOR
    bool bUnrealEditor = false;
    bool bLevelEditor = false;
    bool bEditorActor = false;

    if (GEditor) {
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
      if (GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) bUnrealEditor = true;
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
      if (GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) bLevelEditor = true;
#endif
#if __has_include("Subsystems/EditorActorSubsystem.h")
      if (GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) bEditorActor = true;
#endif
    }

    TSharedPtr<FJsonObject> SubsystemsObj = MakeShared<FJsonObject>();
    SubsystemsObj->SetBoolField(TEXT("unrealEditor"), bUnrealEditor);
    SubsystemsObj->SetBoolField(TEXT("levelEditor"), bLevelEditor);
    SubsystemsObj->SetBoolField(TEXT("editorActor"), bEditorActor);

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetObjectField(TEXT("subsystems"), SubsystemsObj);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Feature flags retrieved"), Out, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("get_feature_flags requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  if (LowerSubAction == TEXT("set_project_setting")) {
#if WITH_EDITOR
    FString Section;
    FString Key;
    FString Value;
    FString ConfigName;

    if (!Payload->TryGetStringField(TEXT("section"), Section) ||
        !Payload->TryGetStringField(TEXT("key"), Key) ||
        !Payload->TryGetStringField(TEXT("value"), Value)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Missing section, key, or value"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!Payload->TryGetStringField(TEXT("configName"), ConfigName) ||
        ConfigName.IsEmpty()) {
      ConfigName = GGameIni;
    } else if (ConfigName == TEXT("Engine")) {
      ConfigName = GEngineIni;
    } else if (ConfigName == TEXT("Input")) {
      ConfigName = GInputIni;
    } else if (ConfigName == TEXT("Game")) {
      ConfigName = GGameIni;
    }

    if (!GConfig) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("GConfig not available"), nullptr,
                             TEXT("ENGINE_ERROR"));
      return true;
    }

    GConfig->SetString(*Section, *Key, *Value, ConfigName);
    GConfig->Flush(false, ConfigName);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Project setting set: [%s] %s = %s"), *Section,
                        *Key, *Value),
        nullptr);
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("set_project_setting requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  if (LowerSubAction == TEXT("validate_assets")) {
#if WITH_EDITOR
    const TArray<TSharedPtr<FJsonValue>> *PathsPtr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("paths"), PathsPtr) || !PathsPtr) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("paths array required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TArray<FString> AssetPaths;
    for (const auto &Val : *PathsPtr) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }

    if (AssetPaths.Num() == 0) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No paths provided"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (GEditor) {
      if (UEditorValidatorSubsystem *Validator =
              GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>()) {
        FValidateAssetsSettings Settings;
        Settings.bSkipExcludedDirectories = true;
        Settings.bShowIfNoFailures = false;
        Settings.ValidationUsecase = EDataValidationUsecase::Script;

        TArray<FAssetData> AssetsToValidate;
        for (const FString &Path : AssetPaths) {
          if (UEditorAssetLibrary::DoesDirectoryExist(Path)) {
            TArray<FString> FoundAssets =
                UEditorAssetLibrary::ListAssets(Path, true);
            for (const FString &AssetPath : FoundAssets) {
              FAssetData AssetData =
                  UEditorAssetLibrary::FindAssetData(AssetPath);
              if (AssetData.IsValid()) {
                AssetsToValidate.Add(AssetData);
              }
            }
          } else {
            FAssetData SpecificAsset = UEditorAssetLibrary::FindAssetData(Path);
            if (SpecificAsset.IsValid()) {
              AssetsToValidate.AddUnique(SpecificAsset);
            }
          }
        }

        if (AssetsToValidate.Num() == 0) {
          Result->SetBoolField(TEXT("success"), true);
          Result->SetStringField(TEXT("message"),
                                 TEXT("No assets found to validate"));
          SendAutomationResponse(RequestingSocket, RequestId, true,
                                 TEXT("Validation skipped (no assets)"), Result,
                                 FString());
          return true;
        }

        FValidateAssetsResults ValidationResults;
        int32 NumChecked = Validator->ValidateAssetsWithSettings(
            AssetsToValidate, Settings, ValidationResults);

        Result->SetNumberField(TEXT("checkedCount"), NumChecked);
        Result->SetNumberField(TEXT("failedCount"),
                               ValidationResults.NumInvalid);
        Result->SetNumberField(TEXT("warningCount"),
                               ValidationResults.NumWarnings);
        Result->SetNumberField(TEXT("skippedCount"),
                               ValidationResults.NumSkipped);

        bool bOverallSuccess = (ValidationResults.NumInvalid == 0);
        Result->SetStringField(
            TEXT("result"), bOverallSuccess ? TEXT("Valid") : TEXT("Invalid"));

        SendAutomationResponse(RequestingSocket, RequestId, true,
                               bOverallSuccess ? TEXT("Validation Passed")
                                               : TEXT("Validation Failed"),
                               Result, FString());
        return true;
      }
    }
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("validate_assets requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  if (LowerSubAction == TEXT("engine_quit")) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Engine quit command is disabled for safety"),
                           nullptr, TEXT("NOT_ALLOWED"));
    return true;
  }

  // Unknown sub-action: return false to allow other handlers (e.g.
  // HandleUiAction) to attempt handling it.
  return HandleUiAction(RequestId, Action, Payload, RequestingSocket);
}

bool UMcpAutomationBridgeSubsystem::HandleConsoleCommandAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!Action.Equals(TEXT("console_command"), ESearchCase::IgnoreCase)) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Console command requires valid payload"),
                           nullptr, TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Command;
  if (!Payload->TryGetStringField(TEXT("command"), Command)) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Console command requires command parameter"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Block dangerous commands (Defense-in-Depth)
  FString LowerCommand = Command.ToLower();

  // 1. Explicit command blocking
  TArray<FString> ExplicitBlockedCommands = {
      TEXT("quit"),    TEXT("exit"),   TEXT("crash"),     TEXT("shutdown"),
      TEXT("restart"), TEXT("reboot"), TEXT("debug exec")};

  for (const FString &Blocked : ExplicitBlockedCommands) {
    if (LowerCommand.Equals(Blocked) ||
        LowerCommand.StartsWith(Blocked + TEXT(" "))) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Command '%s' is explicitly blocked for safety"),
                          *Command),
          nullptr, TEXT("COMMAND_BLOCKED"));
      return true;
    }
  }

  // 2. Token-based blocking
  TArray<FString> ForbiddenTokens = {TEXT("rm "),
                                     TEXT("rm-"),
                                     TEXT("del "),
                                     TEXT("format "),
                                     TEXT("rmdir"),
                                     TEXT("mklink"),
                                     TEXT("copy "),
                                     TEXT("move "),
                                     TEXT("start \""),
                                     TEXT("system("),
                                     TEXT("import os"),
                                     TEXT("import subprocess"),
                                     TEXT("subprocess."),
                                     TEXT("os.system"),
                                     TEXT("exec("),
                                     TEXT("eval("),
                                     TEXT("__import__"),
                                     TEXT("import sys"),
                                     TEXT("import importlib"),
                                     TEXT("with open"),
                                     TEXT("open(")};

  for (const FString &Token : ForbiddenTokens) {
    if (LowerCommand.Contains(Token)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(
              TEXT("Command '%s' contains forbidden token '%s' and is blocked"),
              *Command, *Token),
          nullptr, TEXT("COMMAND_BLOCKED"));
      return true;
    }
  }

  // 3. Block Chaining
  if (LowerCommand.Contains(TEXT("&&")) || LowerCommand.Contains(TEXT("||"))) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Command chaining is blocked for safety"),
                           nullptr, TEXT("COMMAND_BLOCKED"));
    return true;
  }

  // 4. Block line breaks
  if (LowerCommand.Contains(TEXT("\n")) || LowerCommand.Contains(TEXT("\r"))) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Multi-line commands are blocked for safety"),
                           nullptr, TEXT("COMMAND_BLOCKED"));
    return true;
  }

  // 5. Block semicolon and pipe
  if (LowerCommand.Contains(TEXT(";")) || LowerCommand.Contains(TEXT("|"))) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Command chaining with semicolon or pipe is blocked for safety"),
                           nullptr, TEXT("COMMAND_BLOCKED"));
    return true;
  }

  // 6. Block backticks
  if (LowerCommand.Contains(TEXT("`"))) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Commands containing backticks are blocked for safety"),
                           nullptr, TEXT("COMMAND_BLOCKED"));
    return true;
  }

  // Execute the command
  try {
    UWorld *TargetWorld = nullptr;
#if WITH_EDITOR
    if (GEditor) {
      // Prefer PIE world if active, otherwise Editor world
      TargetWorld = GEditor->PlayWorld;
      if (!TargetWorld) {
        TargetWorld = GetActiveWorld();
      }
    }
#endif

    if (!TargetWorld && GEngine) {
      TargetWorld = GetActiveWorld();
    }

    GEngine->Exec(TargetWorld, *Command);

    TSharedPtr<FJsonObject> CommandResult = MakeShared<FJsonObject>();
    CommandResult->SetStringField(TEXT("command"), Command);
    CommandResult->SetBoolField(TEXT("executed"), true);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Executed console command: %s"), *Command), CommandResult,
        FString());
    return true;
  } catch (...) {
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Failed to execute command: %s"), *Command),
        nullptr, TEXT("EXECUTION_FAILED"));
    return true;
  }
}

bool UMcpAutomationBridgeSubsystem::HandleInspectAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!Action.Equals(TEXT("inspect"), ESearchCase::IgnoreCase)) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Inspect action requires valid payload"),
                           nullptr, TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("action"), SubAction)) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Inspect action requires action parameter"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString LowerSub = SubAction.ToLower();
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

  // Inspect object
  if (LowerSub == TEXT("inspect_object")) {
    FString ObjectPath;
    if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("inspect_object requires objectPath parameter"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UObject *TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
    if (!TargetObject) {
      if (AActor *FoundActor = FindActorByLabelOrName<AActor>(GetActiveWorld(), ObjectPath)) {
        TargetObject = FoundActor;
        ObjectPath = FoundActor->GetPathName();
      }
    }
    if (!TargetObject) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Object not found: %s"), *ObjectPath), nullptr,
          TEXT("OBJECT_NOT_FOUND"));
      return true;
    }

    Result->SetStringField(TEXT("objectPath"), ObjectPath);
    Result->SetStringField(TEXT("objectName"), TargetObject->GetName());
    Result->SetStringField(TEXT("objectClass"),
                           TargetObject->GetClass()->GetName());
    Result->SetStringField(TEXT("objectType"),
                           TargetObject->GetClass()->GetFName().ToString());

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Inspected object: %s"), *ObjectPath), Result,
        FString());
    return true;
  }

  // Get property
  if (LowerSub == TEXT("get_property")) {
    FString ObjectPath;
    FString PropertyName;

    if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
        !Payload->TryGetStringField(TEXT("propertyName"), PropertyName)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("get_property requires objectPath and propertyName parameters"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UObject *TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
    if (!TargetObject) {
      if (AActor *FoundActor = FindActorByLabelOrName<AActor>(GetActiveWorld(), ObjectPath)) {
        TargetObject = FoundActor;
        ObjectPath = FoundActor->GetPathName();
      }
    }
    if (!TargetObject) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Object not found: %s"), *ObjectPath), nullptr,
          TEXT("OBJECT_NOT_FOUND"));
      return true;
    }

    UClass *ObjectClass = TargetObject->GetClass();
    FProperty *Property = ObjectClass->FindPropertyByName(*PropertyName);

    if (!Property) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Property not found: %s"), *PropertyName),
          nullptr, TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }

    Result->SetStringField(TEXT("objectPath"), ObjectPath);
    Result->SetStringField(TEXT("propertyName"), PropertyName);
    Result->SetStringField(TEXT("propertyType"),
                           Property->GetClass()->GetName());

    FString ValueText;
    const void *ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
    Property->ExportTextItem_Direct(ValueText, ValuePtr, nullptr, TargetObject,
                                    PPF_None);
    Result->SetStringField(TEXT("value"), ValueText);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Retrieved property: %s.%s"),
                                           *ObjectPath, *PropertyName),
                           Result, FString());
    return true;
  }

  // Set property
  if (LowerSub == TEXT("set_property")) {
    FString ObjectPath;
    FString PropertyName;

    if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
        !Payload->TryGetStringField(TEXT("propertyName"), PropertyName)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("set_property requires objectPath and propertyName parameters"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TArray<FString> ProtectedProperties = {TEXT("Class"), TEXT("Outer"),
                                           TEXT("Archetype"), TEXT("Linker"),
                                           TEXT("LinkerIndex")};
    if (ProtectedProperties.Contains(PropertyName)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(
              TEXT("Modification of critical property '%s' is blocked"),
              *PropertyName),
          nullptr, TEXT("PROPERTY_BLOCKED"));
      return true;
    }

    UObject *TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
    if (!TargetObject) {
      if (AActor *FoundActor = FindActorByLabelOrName<AActor>(GetActiveWorld(), ObjectPath)) {
        TargetObject = FoundActor;
        ObjectPath = FoundActor->GetPathName();
      }
    }
    if (!TargetObject) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Object not found: %s"), *ObjectPath), nullptr,
          TEXT("OBJECT_NOT_FOUND"));
      return true;
    }

    FString PropertyValue;
    if (!Payload->TryGetStringField(TEXT("value"), PropertyValue)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("set_property requires 'value' field"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FProperty *FoundProperty =
        TargetObject->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!FoundProperty) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Property '%s' not found on object '%s'"),
                          *PropertyName, *ObjectPath),
          nullptr, TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }

    bool bSuccess = false;
    FString ErrorMessage;
    void *PropAddr = FoundProperty->ContainerPtrToValuePtr<void>(TargetObject);

    if (FStrProperty *StrProp = CastField<FStrProperty>(FoundProperty)) {
      StrProp->SetPropertyValue(PropAddr, PropertyValue);
      bSuccess = true;
    } else if (FFloatProperty *FloatProp =
                   CastField<FFloatProperty>(FoundProperty)) {
      float Value = FCString::Atof(*PropertyValue);
      FloatProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FDoubleProperty *DoubleProp =
                   CastField<FDoubleProperty>(FoundProperty)) {
      double Value = FCString::Atod(*PropertyValue);
      DoubleProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FIntProperty *IntProp = CastField<FIntProperty>(FoundProperty)) {
      int32 Value = FCString::Atoi(*PropertyValue);
      IntProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FInt64Property *Int64Prop =
                   CastField<FInt64Property>(FoundProperty)) {
      int64 Value = FCString::Atoi64(*PropertyValue);
      Int64Prop->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FBoolProperty *BoolProp =
                   CastField<FBoolProperty>(FoundProperty)) {
      bool Value = PropertyValue.ToBool();
      BoolProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FObjectProperty *ObjProp =
                   CastField<FObjectProperty>(FoundProperty)) {
      UObject *ObjValue = FindObject<UObject>(nullptr, *PropertyValue);
      if (ObjValue || PropertyValue.IsEmpty()) {
        ObjProp->SetPropertyValue(PropAddr, ObjValue);
        bSuccess = true;
      } else {
        ErrorMessage = FString::Printf(
            TEXT("Object property requires valid object path, got: %s"),
            *PropertyValue);
      }
    } else if (FStructProperty *StructProp =
                   CastField<FStructProperty>(FoundProperty)) {
      FString StructName =
          StructProp->Struct ? StructProp->Struct->GetName() : FString();

      const TSharedPtr<FJsonObject> *JsonObjValue = nullptr;
      if (Payload->TryGetObjectField(TEXT("value"), JsonObjValue) &&
          JsonObjValue->IsValid()) {
        if (StructName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase)) {
          FVector *Vec = static_cast<FVector *>(PropAddr);
          double X = 0, Y = 0, Z = 0;
          if (!(*JsonObjValue)->TryGetNumberField(TEXT("X"), X)) (*JsonObjValue)->TryGetNumberField(TEXT("x"), X);
          if (!(*JsonObjValue)->TryGetNumberField(TEXT("Y"), Y)) (*JsonObjValue)->TryGetNumberField(TEXT("y"), Y);
          if (!(*JsonObjValue)->TryGetNumberField(TEXT("Z"), Z)) (*JsonObjValue)->TryGetNumberField(TEXT("z"), Z);
          *Vec = FVector(X, Y, Z);
          bSuccess = true;
        } else if (StructName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase)) {
          FRotator *Rot = static_cast<FRotator *>(PropAddr);
          double Pitch = 0, Yaw = 0, Roll = 0;
          if (!(*JsonObjValue)->TryGetNumberField(TEXT("Pitch"), Pitch)) (*JsonObjValue)->TryGetNumberField(TEXT("pitch"), Pitch);
          if (!(*JsonObjValue)->TryGetNumberField(TEXT("Yaw"), Yaw)) (*JsonObjValue)->TryGetNumberField(TEXT("yaw"), Yaw);
          if (!(*JsonObjValue)->TryGetNumberField(TEXT("Roll"), Roll)) (*JsonObjValue)->TryGetNumberField(TEXT("roll"), Roll);
          *Rot = FRotator(Pitch, Yaw, Roll);
          bSuccess = true;
        }
      }

      if (!bSuccess && !PropertyValue.IsEmpty() && StructProp->Struct) {
        const TCHAR *ImportResult = StructProp->Struct->ImportText(
            *PropertyValue, PropAddr, nullptr, PPF_None, GLog, StructName);
        bSuccess = (ImportResult != nullptr);
      }
    }

    if (bSuccess) {
      Result->SetStringField(TEXT("objectPath"), ObjectPath);
      Result->SetStringField(TEXT("propertyName"), PropertyName);
      Result->SetStringField(TEXT("value"), PropertyValue);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Property set successfully"), Result,
                             FString());
    } else {
      Result->SetStringField(TEXT("objectPath"), ObjectPath);
      Result->SetStringField(TEXT("propertyName"), PropertyName);
      Result->SetStringField(TEXT("error"), ErrorMessage);
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to set property"), Result,
                             TEXT("PROPERTY_SET_FAILED"));
    }
    return true;
  }

  // Get bounding box
  if (LowerSub == TEXT("get_bounding_box")) {
    FString ActorName, ObjectPath;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);

    if (ActorName.IsEmpty() && ObjectPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("get_bounding_box requires actorName or objectPath"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    AActor *TargetActor = FindActorByLabelOrName<AActor>(GetActiveWorld(), ActorName.IsEmpty() ? ObjectPath : ActorName);
    if (TargetActor) {
      FBox Box = TargetActor->GetComponentsBoundingBox(true);
      FVector Origin = Box.GetCenter();
      FVector Extent = Box.GetExtent();
      TSharedPtr<FJsonObject> BoxObj = MakeShared<FJsonObject>();
      TSharedPtr<FJsonObject> OrgObj = MakeShared<FJsonObject>();
      OrgObj->SetNumberField(TEXT("x"), Origin.X); OrgObj->SetNumberField(TEXT("y"), Origin.Y); OrgObj->SetNumberField(TEXT("z"), Origin.Z);
      BoxObj->SetObjectField(TEXT("origin"), OrgObj);
      TSharedPtr<FJsonObject> ExtObj = MakeShared<FJsonObject>();
      ExtObj->SetNumberField(TEXT("x"), Extent.X); ExtObj->SetNumberField(TEXT("y"), Extent.Y); ExtObj->SetNumberField(TEXT("z"), Extent.Z);
      BoxObj->SetObjectField(TEXT("extent"), ExtObj);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bounding box retrieved"), BoxObj, FString());
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Object not found"), nullptr, TEXT("OBJECT_NOT_FOUND"));
    }
    return true;
  }

  // Get components
  if (LowerSub == TEXT("get_components")) {
    FString ObjectPath;
    if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath)) {
      Payload->TryGetStringField(TEXT("actorName"), ObjectPath);
    }

    if (ObjectPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("get_components requires objectPath or actorName"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    AActor *FoundActor = FindActorByLabelOrName<AActor>(GetActiveWorld(), ObjectPath);
    if (!FoundActor) {
      if (UObject *Asset = UEditorAssetLibrary::LoadAsset(ObjectPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Asset)) {
          if (BP->GeneratedClass) {
            FoundActor = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());
          }
        }
      }
    }

    if (!FoundActor) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Actor or Blueprint not found: %s"),
                          *ObjectPath),
          nullptr, TEXT("OBJECT_NOT_FOUND"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> ComponentList;
    for (UActorComponent *Comp : FoundActor->GetComponents()) {
      if (!Comp) continue;
      TSharedPtr<FJsonObject> CompData = MakeShared<FJsonObject>();
      CompData->SetStringField(TEXT("name"), Comp->GetName());
      CompData->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
      CompData->SetStringField(TEXT("path"), Comp->GetPathName());
      ComponentList.Add(MakeShared<FJsonValueObject>(CompData));
    }

    TSharedPtr<FJsonObject> ComponentsResult = MakeShared<FJsonObject>();
    ComponentsResult->SetArrayField(TEXT("components"), ComponentList);
    ComponentsResult->SetNumberField(TEXT("count"), ComponentList.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Actor components retrieved"), ComponentsResult,
                           FString());
    return true;
  }

  // Find by class
  if (LowerSub == TEXT("find_by_class")) {
    FString ClassName;
    if (!Payload->TryGetStringField(TEXT("className"), ClassName)) {
      Payload->TryGetStringField(TEXT("classPath"), ClassName);
    }

#if WITH_EDITOR
    if (GEditor) {
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      if (ActorSS) {
        TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
        TArray<TSharedPtr<FJsonValue>> Matches;
        for (AActor *Actor : Actors) {
          if (!Actor) continue;
          if (ClassName.IsEmpty() || Actor->GetClass()->GetName().Contains(ClassName) || 
              Actor->GetClass()->GetPathName().Contains(ClassName)) {
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
            Entry->SetStringField(TEXT("path"), Actor->GetPathName());
            Entry->SetStringField(TEXT("class"), Actor->GetClass()->GetPathName());
            Matches.Add(MakeShared<FJsonValueObject>(Entry));
          }
        }
        Result->SetArrayField(TEXT("actors"), Matches);
        Result->SetNumberField(TEXT("count"), Matches.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Found actors by class"), Result,
                               FString());
        return true;
      }
    }
#endif
    return true;
  }

  // Inspect class
  if (LowerSub == TEXT("inspect_class")) {
    FString ClassPath;
    if (!Payload->TryGetStringField(TEXT("classPath"), ClassPath)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("classPath required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UClass *ResolvedClass = ResolveClassByName(ClassPath);
    if (!ResolvedClass) {
      if (UObject *Found = StaticLoadObject(UObject::StaticClass(), nullptr, *ClassPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Found)) ResolvedClass = BP->GeneratedClass;
        else if (UClass *C = Cast<UClass>(Found)) ResolvedClass = C;
      }
    }

    if (ResolvedClass) {
      Result->SetStringField(TEXT("className"), ResolvedClass->GetName());
      Result->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
      if (ResolvedClass->GetSuperClass())
        Result->SetStringField(TEXT("parentClass"),
                               ResolvedClass->GetSuperClass()->GetName());

      TArray<TSharedPtr<FJsonValue>> Props;
      for (TFieldIterator<FProperty> PropIt(ResolvedClass); PropIt; ++PropIt) {
        FProperty *Prop = *PropIt;
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("name"), Prop->GetName());
        P->SetStringField(TEXT("type"), Prop->GetClass()->GetName());
        Props.Add(MakeShared<FJsonValueObject>(P));
      }
      Result->SetArrayField(TEXT("properties"), Props);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Class inspected"), Result, FString());
      return true;
    }

    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Class not found"), nullptr,
                           TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  return true;
}
