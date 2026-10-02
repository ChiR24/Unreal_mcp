#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"

namespace McpMaterialAuthoringHandlers
{
void ApplyMaterialParameterList(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& AssetPath, const TArray<TSharedPtr<FJsonValue>>& Entries, const TSharedPtr<FJsonObject>& Shared, TSharedPtr<FMcpBridgeWebSocket> Socket, TArray<TSharedPtr<FJsonValue>>& OutResults, TArray<FString>& OutFailed, FString* OutChangedAssetPath)
{
  FMcpResponseCaptureRegistry& Capture = FMcpResponseCaptureRegistry::Get();
  for (int32 Index = 0; Index < Entries.Num(); ++Index) {
    TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
    const TSharedPtr<FJsonObject>* EntryObj = nullptr;
    if (Entries[Index].IsValid() && Entries[Index]->TryGetObject(EntryObj)) {
      One->Values = (*EntryObj)->Values;
    }
    One->RemoveField(TEXT("parameters"));
    One->SetStringField(TEXT("assetPath"), AssetPath);
    // The setter reads save from its own payload, so a save:false given once for the whole call has to be
    // copied in; every entry saved the asset whatever the caller said.
    if (Shared.IsValid() && !One->HasField(TEXT("save")) && Shared->HasField(TEXT("save"))) {
      One->SetField(TEXT("save"), Shared->TryGetField(TEXT("save")));
    }
    const FString Name = GetJsonStringField(One, TEXT("parameterName"));
    const FString ItemId = FString::Printf(TEXT("%s#param%d"), *RequestId, Index);
    Capture.Begin(ItemId);
    HandleSetMaterialParameter(Bridge, ItemId, TEXT("set_material_parameter"), One, Socket);
    const FMcpCapturedResponse Reply = Capture.End(ItemId);
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("parameterName"), Name);
    Entry->SetBoolField(TEXT("applied"), Reply.bSuccess);
    if (Reply.bSuccess) {
      // The setter names the asset it wrote (the package path) the way a single parameter's reply does, so the
      // caller's reply can name it too and the receipt lists it.
      if (OutChangedAssetPath && OutChangedAssetPath->IsEmpty() && Reply.Result.IsValid()) {
        Reply.Result->TryGetStringField(TEXT("assetPath"), *OutChangedAssetPath);
      }
    } else {
      const FString Why = Reply.bCaptured ? Reply.Message : FString(TEXT("the parameter setter gave no answer"));
      Entry->SetStringField(TEXT("error"), Why);
      OutFailed.Add(FString::Printf(TEXT("%s: %s"), *Name, *Why));
    }
    OutResults.Add(MakeShared<FJsonValueObject>(Entry));
  }
}

// assets: several materials retuned under the one consent this call carried. Each entry runs
// through this same handler as a captured step, so every check stays per asset; recolouring three
// lamp instances used to cost three describes and three consents.
static bool SetMaterialParameterBatch(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, const TArray<TSharedPtr<FJsonValue>>& Entries, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  TArray<TSharedPtr<FJsonValue>> Rows;
  TArray<TSharedPtr<FJsonValue>> Changed;
  TArray<FString> Failed;
  for (int32 Index = 0; Index < Entries.Num(); ++Index) {
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->Values = Payload->Values;
    Item->RemoveField(TEXT("assets"));
    const TSharedPtr<FJsonObject>* Entry = nullptr;
    if (Entries[Index].IsValid() && Entries[Index]->TryGetObject(Entry)) {
      for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Entry)->Values) {
        Item->SetField(Pair.Key, Pair.Value);
      }
    }
    const FString StepId = FString::Printf(TEXT("%s#asset%d"), *RequestId, Index);
    FMcpResponseCaptureRegistry::Get().Begin(StepId);
    HandleSetMaterialParameter(Bridge, StepId, TEXT("set_material_parameter"), Item, Socket);
    const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(StepId);
    TSharedPtr<FJsonObject> Row = Reply.Result.IsValid() ? Reply.Result : McpHandlerUtils::CreateResultObject();
    const FString AssetPath = GetJsonStringField(Item, TEXT("assetPath"));
    Row->SetStringField(TEXT("assetPath"), AssetPath);
    Row->SetBoolField(TEXT("success"), Reply.bSuccess);
    if (Reply.bSuccess) {
      Changed.Add(MakeShared<FJsonValueString>(AssetPath));
    } else {
      const FString Error = Reply.Message.IsEmpty() ? FString(TEXT("the asset sent no reply")) : Reply.Message;
      Row->SetStringField(TEXT("error"), Error);
      Failed.Add(FString::Printf(TEXT("#%d %s: %s"), Index, *AssetPath, *Error));
    }
    Rows.Add(MakeShared<FJsonValueObject>(Row));
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetArrayField(TEXT("assets"), Rows);
  Result->SetArrayField(TEXT("changedAssets"), Changed);
  Bridge->SendAutomationResponse(Socket, RequestId, Failed.Num() == 0,
      Failed.Num() == 0 ? FString::Printf(TEXT("Set parameters on %d assets."), Rows.Num())
                        : FString::Printf(TEXT("%d of %d assets failed: %s"), Failed.Num(), Rows.Num(), *FString::Join(Failed, TEXT("; "))),
      Result, Failed.Num() == 0 ? FString() : FString(TEXT("ASSET_BATCH_INCOMPLETE")));
  return true;
}

bool HandleSetMaterialParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_material_parameter")) {
    const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
    if (Payload->TryGetArrayField(TEXT("assets"), Assets) && Assets->Num() > 0) {
      return SetMaterialParameterBatch(Bridge, RequestId, Payload, *Assets, Socket);
    }
    // parameters: several values under one consent instead of a describe +
    // execute pair per value.
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (Payload->TryGetArrayField(TEXT("parameters"), Entries) && Entries->Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> Results;
      TArray<FString> Failed;
      FString ChangedAsset;
      ApplyMaterialParameterList(Bridge, RequestId, GetJsonStringField(Payload, TEXT("assetPath")), *Entries, Payload, Socket, Results, Failed, &ChangedAsset);
      TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
      Data->SetArrayField(TEXT("parameters"), Results);
      Data->SetNumberField(TEXT("applied"), Results.Num() - Failed.Num());
      // Without it the reply named no asset, so the receipt's changes and handles were empty for a call that wrote one.
      if (!ChangedAsset.IsEmpty()) {
        Data->SetStringField(TEXT("assetPath"), ChangedAsset);
      }
      if (Failed.Num() > 0) {
        Bridge->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Set %d of %d parameters; %s"), Results.Num() - Failed.Num(), Results.Num(),
                            *FString::Join(Failed, TEXT("; "))),
            Data, TEXT("PARAMETER_BATCH_INCOMPLETE"));
      } else {
        Bridge->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Set %d parameters"), Results.Num()), Data, FString());
      }
      return true;
    }
    FString AssetPath, ParameterName, ParameterType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("parameterType"), ParameterType);

    // SECURITY: Validate assetPath before use (accepts both Materials and Material Functions)
    FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedAssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          McpPathRefusalMessage(TEXT("assetPath"), AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    Payload->SetStringField(TEXT("assetPath"), ValidatedAssetPath);

    // Each type-specific setter below already handles BOTH a material instance
    // and a base material (it edits the named parameter expression's
    // DefaultValue), so the canonical action delegates rather than refusing.
    // parameterType defaults to scalar, matching the TypeScript normalizer.
    const FString Type = ParameterType.IsEmpty() ? TEXT("scalar") : ParameterType.ToLower();

    if (Type == TEXT("scalar") || Type == TEXT("float")) {
      return HandleSetScalarParameterValue(
          Bridge, RequestId, TEXT("set_scalar_parameter_value"), Payload, Socket);
    }
    if (Type == TEXT("vector") || Type == TEXT("color")) {
      return HandleSetVectorParameterValue(
          Bridge, RequestId, TEXT("set_vector_parameter_value"), Payload, Socket);
    }
    if (Type == TEXT("texture")) {
      // The texture setter reads texturePath; a caller using the canonical
      // `value` field passes the texture asset path there instead.
      FString TexturePath, ValueString;
      if ((!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) || TexturePath.IsEmpty()) &&
          Payload->TryGetStringField(TEXT("value"), ValueString) && !ValueString.IsEmpty()) {
        Payload->SetStringField(TEXT("texturePath"), ValueString);
      }
      return HandleSetTextureParameterValue(
          Bridge, RequestId, TEXT("set_texture_parameter_value"), Payload, Socket);
    }

    Bridge->SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Unsupported parameterType '%s'. Use scalar, vector, or texture."), *Type),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  return false;
}
}
