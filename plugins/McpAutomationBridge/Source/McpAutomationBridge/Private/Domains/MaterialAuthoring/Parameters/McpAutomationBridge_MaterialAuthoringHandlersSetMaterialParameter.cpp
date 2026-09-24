#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
void ApplyMaterialParameterList(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& AssetPath, const TArray<TSharedPtr<FJsonValue>>& Entries, TSharedPtr<FMcpBridgeWebSocket> Socket, TArray<TSharedPtr<FJsonValue>>& OutResults, TArray<FString>& OutFailed)
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
    const FString Name = GetJsonStringField(One, TEXT("parameterName"));
    const FString ItemId = FString::Printf(TEXT("%s#param%d"), *RequestId, Index);
    Capture.Begin(ItemId);
    HandleSetMaterialParameter(Bridge, ItemId, TEXT("set_material_parameter"), One, Socket);
    const FMcpCapturedResponse Reply = Capture.End(ItemId);
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("parameterName"), Name);
    Entry->SetBoolField(TEXT("applied"), Reply.bSuccess);
    if (!Reply.bSuccess) {
      const FString Why = Reply.bCaptured ? Reply.Message : FString(TEXT("the parameter setter gave no answer"));
      Entry->SetStringField(TEXT("error"), Why);
      OutFailed.Add(FString::Printf(TEXT("%s: %s"), *Name, *Why));
    }
    OutResults.Add(MakeShared<FJsonValueObject>(Entry));
  }
}

bool HandleSetMaterialParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_material_parameter")) {
    // parameters: several values under one consent instead of a describe +
    // execute pair per value.
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (Payload->TryGetArrayField(TEXT("parameters"), Entries) && Entries->Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> Results;
      TArray<FString> Failed;
      ApplyMaterialParameterList(Bridge, RequestId, GetJsonStringField(Payload, TEXT("assetPath")), *Entries, Socket, Results, Failed);
      TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
      Data->SetArrayField(TEXT("parameters"), Results);
      Data->SetNumberField(TEXT("applied"), Results.Num() - Failed.Num());
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
                          FString::Printf(TEXT("Invalid assetPath '%s': contains traversal sequences or invalid root"), *AssetPath),
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
#endif
