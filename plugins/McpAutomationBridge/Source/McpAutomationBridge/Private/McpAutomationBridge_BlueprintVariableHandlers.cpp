#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridge_BlueprintHandlers_Common.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Misc/ScopeExit.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleBlueprintVariableAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {

  const FString AlphaNumLower = Action.ToLower();
  const TSharedPtr<FJsonObject> LocalPayload = Payload;

  auto ActionMatchesPattern = [&](const FString &Pattern) {
    return Action.Equals(Pattern, ESearchCase::IgnoreCase);
  };

  // 1. Add Variable
  if (ActionMatchesPattern(TEXT("blueprint_add_variable")) ||
      ActionMatchesPattern(TEXT("add_variable")) ||
      AlphaNumLower.Contains(TEXT("blueprintaddvariable")) ||
      AlphaNumLower.Contains(TEXT("addvariable"))) {
    
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_variable requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    FString VarName;
    LocalPayload->TryGetStringField(TEXT("variableName"), VarName);
    if (VarName.TrimStartAndEnd().IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("variableName required"), nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString VarType;
    LocalPayload->TryGetStringField(TEXT("variableType"), VarType);
    FString Category;
    LocalPayload->TryGetStringField(TEXT("category"), Category);
    const bool bReplicated = LocalPayload->HasField(TEXT("isReplicated")) ? LocalPayload->GetBoolField(TEXT("isReplicated")) : false;
    const bool bPublic = LocalPayload->HasField(TEXT("isPublic")) ? LocalPayload->GetBoolField(TEXT("isPublic")) : false;

#if WITH_EDITOR
    FString NormPath;
    FindBlueprintNormalizedPath(Path, NormPath);
    const FString RegKey = !NormPath.IsEmpty() ? NormPath : Path;

    if (GBlueprintBusySet.Contains(RegKey)) {
      SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Blueprint %s is busy"), *RegKey), TEXT("BLUEPRINT_BUSY"));
      return true;
    }

    GBlueprintBusySet.Add(RegKey);
    ON_SCOPE_EXIT { GBlueprintBusySet.Remove(RegKey); };

    FString LocalNormalized;
    FString LocalLoadError;
    UBlueprint *Blueprint = LoadBlueprintAsset(Path, LocalNormalized, LocalLoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load blueprint"), TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    FEdGraphPinType PinType = FMcpAutomationBridge_MakePinType(VarType);
    
    // Check if exists
    for (const FBPVariableDescription &Existing : Blueprint->NewVariables) {
      if (Existing.VarName == FName(*VarName)) {
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable already exists"), nullptr);
        return true;
      }
    }

    Blueprint->Modify();
    FBPVariableDescription NewVar;
    NewVar.VarName = FName(*VarName);
    NewVar.VarGuid = FGuid::NewGuid();
    NewVar.FriendlyName = VarName;
    NewVar.Category = Category.IsEmpty() ? FText::GetEmpty() : FText::FromString(Category);
    NewVar.VarType = PinType;
    NewVar.PropertyFlags |= CPF_Edit | CPF_BlueprintVisible;
    if (bReplicated) NewVar.PropertyFlags |= CPF_Net;
    if (!bPublic) NewVar.PropertyFlags |= CPF_BlueprintReadOnly;

    Blueprint->NewVariables.Add(NewVar);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    const bool bSaved = McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetBoolField(TEXT("saved"), bSaved);
    Response->SetStringField(TEXT("variableName"), VarName);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable added"), Response);
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor required"), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  // 2. Set Default Value
  if (ActionMatchesPattern(TEXT("blueprint_set_default")) ||
      AlphaNumLower.Contains(TEXT("blueprintsetdefault"))) {
    FString Path = ResolveBlueprintRequestedPath();
    FString PropertyName;
    LocalPayload->TryGetStringField(TEXT("propertyName"), PropertyName);
    const TSharedPtr<FJsonValue> ValueField = LocalPayload->TryGetField(TEXT("value"));

#if WITH_EDITOR
    UBlueprint *Blueprint = LoadBlueprintAsset(Path);
    if (Blueprint && Blueprint->GeneratedClass) {
      UObject *CDO = Blueprint->GeneratedClass->GetDefaultObject();
      FProperty *Property = CDO->GetClass()->FindPropertyByName(*PropertyName);
      if (Property) {
        FString ConversionError;
        if (ApplyJsonValueToProperty(CDO, Property, ValueField, ConversionError)) {
          FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
          FKismetEditorUtilities::CompileBlueprint(Blueprint);
          McpSafeAssetSave(Blueprint);
          SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Default value set"), nullptr);
          return true;
        }
      }
    }
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to set default"), TEXT("ERROR"));
    return true;
#endif
  }

  // 3. Rename Variable
  if (ActionMatchesPattern(TEXT("blueprint_rename_variable")) ||
      AlphaNumLower.Contains(TEXT("blueprintrenamevariable"))) {
    FString Path = ResolveBlueprintRequestedPath();
    FString OldName, NewName;
    LocalPayload->TryGetStringField(TEXT("oldName"), OldName);
    LocalPayload->TryGetStringField(TEXT("newName"), NewName);

#if WITH_EDITOR
    UBlueprint *Blueprint = LoadBlueprintAsset(Path);
    if (Blueprint) {
      FBlueprintEditorUtils::RenameMemberVariable(Blueprint, FName(*OldName), FName(*NewName));
      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
      FKismetEditorUtilities::CompileBlueprint(Blueprint);
      McpSafeAssetSave(Blueprint);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable renamed"), nullptr);
      return true;
    }
#endif
  }

  // 4. Remove Variable
  if (ActionMatchesPattern(TEXT("blueprint_remove_variable")) ||
      AlphaNumLower.Contains(TEXT("blueprintremovevariable"))) {
    FString Path = ResolveBlueprintRequestedPath();
    FString VarName;
    LocalPayload->TryGetStringField(TEXT("variableName"), VarName);

#if WITH_EDITOR
    UBlueprint *Blueprint = LoadBlueprintAsset(Path);
    if (Blueprint) {
      FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*VarName));
      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
      FKismetEditorUtilities::CompileBlueprint(Blueprint);
      McpSafeAssetSave(Blueprint);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable removed"), nullptr);
      return true;
    }
#endif
  }

  return false;
}
