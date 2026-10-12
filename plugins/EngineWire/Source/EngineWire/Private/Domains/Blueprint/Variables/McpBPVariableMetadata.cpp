#include "Domains/Blueprint/McpBPActionContext.h"
#include "Core/Module/EngineWireGlobals.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/ScopeExit.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
TArray<FString> McpReadVariableNames(const TSharedPtr<FJsonObject> &Payload) {
  TArray<FString> Names;
  const TArray<TSharedPtr<FJsonValue>> *NameList = nullptr;
  if (Payload.IsValid() && Payload->TryGetArrayField(TEXT("variableNames"), NameList)) {
    for (const TSharedPtr<FJsonValue> &Name : *NameList) {
      if (Name.IsValid() && Name->Type == EJson::String && !Name->AsString().IsEmpty()) {
        Names.AddUnique(Name->AsString());
      }
    }
  }
  FString Single;
  if (Payload.IsValid() && Payload->TryGetStringField(TEXT("variableName"), Single) && !Single.IsEmpty()) {
    Names.AddUnique(Single);
  }
  return Names;
}

void McpMatchVariableNames(const UBlueprint *Blueprint, TArray<FString> &InOutNames, TArray<FString> &OutMissing,
                           TArray<FString> &OutKnown) {
  for (const FBPVariableDescription &Desc : Blueprint->NewVariables) {
    OutKnown.Add(Desc.VarName.ToString());
  }
  for (FString &Name : InOutNames) {
    const FString *Match = OutKnown.FindByPredicate([&Name](const FString &Candidate) {
      return Candidate.Equals(Name, ESearchCase::IgnoreCase);
    });
    if (Match) {
      Name = *Match;
    } else {
      OutMissing.Add(Name);
    }
  }
}

bool HandleBlueprintSetVariableMetadata(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("set_variable_metadata"))) {
    UE_LOG(
        LogEngineWireSubsystem, Verbose,
        TEXT("Entered blueprint_set_variable_metadata handler: RequestId=%s"),
        *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_set_variable_metadata requires a blueprint path."),
          nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    // variableNames: the same metadata on several variables in one call (five
    // ExposeOnSpawn flags used to be five calls, five compiles, five saves).
    TArray<FString> VarNames = McpReadVariableNames(LocalPayload);
    if (VarNames.Num() == 0) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("variableName or variableNames required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const TSharedPtr<FJsonValue> MetaVal =
        LocalPayload->TryGetField(TEXT("metadata"));
    const TSharedPtr<FJsonObject> MetaObjPtr =
        MetaVal.IsValid() && MetaVal->Type == EJson::Object
            ? MetaVal->AsObject()
            : nullptr;
    if (!MetaObjPtr.IsValid()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("metadata object required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }


    FString Normalized;
    FString LoadErr;
    UBlueprint *Blueprint = LoadBlueprintAsset(Path, Normalized, LoadErr);
    if (!Blueprint) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      if (!LoadErr.IsEmpty()) {
        Err->SetStringField(TEXT("error"), LoadErr);
      }
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to load blueprint"), Err,
                             TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    const FString RegistryKey = Normalized.IsEmpty() ? Path : Normalized;

    TArray<FString> Missing;
    TArray<FString> Known;
    McpMatchVariableNames(Blueprint, VarNames, Missing, Known);
    TArray<FName> VarFNames;
    for (const FString &Name : VarNames) {
      VarFNames.Add(FName(*Name));
    }

    if (Missing.Num() > 0) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(TEXT("error"), TEXT("Variable not found"));
      const FString Message = FString::Printf(TEXT("Variable not found: %s. The Blueprint's variables: %s."),
          *FString::Join(Missing, TEXT(", ")), Known.Num() > 0 ? *FString::Join(Known, TEXT(", ")) : TEXT("<none>"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Message, Err,
                             TEXT("VARIABLE_NOT_FOUND"));
      return true;
    }
    const FString VarName = VarNames[0];

    Blueprint->Modify();

    TArray<FString> AppliedKeys;
    for (const auto &Pair : MetaObjPtr->Values) {
      if (!Pair.Value.IsValid()) {
        continue;
      }

      const FString KeyStr(*Pair.Key);
      const FString ValueStr =
          McpHandlerUtils::JsonValueToString(Pair.Value);
      const FName MetaKey = FEngineWire_ResolveMetadataKey(KeyStr);

      for (const FName &Var : VarFNames) {
        if (ValueStr.IsEmpty()) {
          FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(Blueprint, Var, nullptr, MetaKey);
        } else {
          FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, Var, nullptr, MetaKey, ValueStr);
        }
        UE_LOG(LogEngineWireSubsystem, Log, TEXT("Metadata '%s'='%s' on variable '%s'"),
               *MetaKey.ToString(), *ValueStr, *Var.ToString());
      }

      AppliedKeys.Add(MetaKey.ToString());
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

    const TSharedPtr<FJsonObject> Snapshot =
        FEngineWire_BuildBlueprintSnapshot(Blueprint, RegistryKey);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
    Resp->SetStringField(TEXT("variableName"), VarName);
    TArray<TSharedPtr<FJsonValue>> VarNamesJson;
    for (const FString &Name : VarNames) {
      VarNamesJson.Add(MakeShared<FJsonValueString>(Name));
    }
    Resp->SetArrayField(TEXT("variableNames"), VarNamesJson);
    Resp->SetBoolField(TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> AppliedKeysJson;
    for (const FString &Key : AppliedKeys) {
      AppliedKeysJson.Add(MakeShared<FJsonValueString>(Key));
    }
    Resp->SetArrayField(TEXT("appliedKeys"), AppliedKeysJson);
    if (Snapshot.IsValid() && Snapshot->HasField(TEXT("metadata"))) {
      Resp->SetObjectField(TEXT("metadata"),
                           Snapshot->GetObjectField(TEXT("metadata")));
    }
    // Without assetPath the receipt listed no change though the Blueprint was modified and saved.
    McpHandlerUtils::AddVerification(Resp, Blueprint);

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Variable metadata applied"), Resp, FString());

    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
