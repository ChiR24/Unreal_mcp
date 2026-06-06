#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_EditorFunctionHandlersShared.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Misc/Base64.h"
#include "Serialization/JsonSerializer.h"

namespace McpEditorFunctionHandlers {

bool HandleBlueprintComponentFunction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FN, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // GET_BLUEPRINT_CDO: best-effort CDO/class info for a Blueprint asset
  if (FN == TEXT("GET_BLUEPRINT_CDO")) {
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (BlueprintPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                                 TEXT("blueprintPath required"),
                                 TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath)) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Blueprint not found"), nullptr,
                                    TEXT("NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    UObject *Obj = UEditorAssetLibrary::LoadAsset(BlueprintPath);
    if (!Obj) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Blueprint not found"), nullptr,
                                    TEXT("NOT_FOUND"));
      return true;
    }

    if (UBlueprint *BP = Cast<UBlueprint>(Obj)) {
      if (BP->GeneratedClass) {
        UClass *Gen = BP->GeneratedClass;
        Out->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Out->SetStringField(TEXT("classPath"), Gen->GetPathName());
        Out->SetStringField(TEXT("className"), Gen->GetName());
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                      TEXT("Blueprint CDO info"), Out,
                                      FString());
        return true;
      }
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Blueprint/GeneratedClass not available"), nullptr,
          TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    if (UClass *C = Cast<UClass>(Obj)) {
      Out->SetStringField(TEXT("classPath"), C->GetPathName());
      Out->SetStringField(TEXT("className"), C->GetName());
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                    TEXT("Class info"), Out, FString());
      return true;
    }

    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Blueprint/GeneratedClass not available"), nullptr,
        TEXT("INVALID_BLUEPRINT"));
    return true;
  }

  if (FN == TEXT("BLUEPRINT_ADD_COMPONENT")) {
    const TSharedPtr<FJsonObject> *Params = nullptr;
    TSharedPtr<FJsonObject> LocalParams = McpHandlerUtils::CreateResultObject();
    if (Payload->TryGetObjectField(TEXT("params"), Params) && Params &&
        (*Params).IsValid()) {
      LocalParams = *Params;
    } else if (Payload->HasField(TEXT("payloadBase64"))) {
      FString Enc;
      Payload->TryGetStringField(TEXT("payloadBase64"), Enc);
      if (!Enc.IsEmpty()) {
        TArray<uint8> DecodedBytes;
        if (FBase64::Decode(Enc, DecodedBytes) && DecodedBytes.Num() > 0) {
          DecodedBytes.Add(0);
          const ANSICHAR *Utf8 =
              reinterpret_cast<const ANSICHAR *>(DecodedBytes.GetData());
          FString Decoded = FString(UTF8_TO_TCHAR(Utf8));
          TSharedPtr<FJsonObject> Parsed =
              McpHandlerUtils::CreateResultObject();
          TSharedRef<TJsonReader<>> Reader =
              TJsonReaderFactory<>::Create(Decoded);
          if (FJsonSerializer::Deserialize(Reader, Parsed) &&
              Parsed.IsValid()) {
            LocalParams = Parsed;
          }
        }
      }
    }

    FString TargetBP;
    LocalParams->TryGetStringField(TEXT("blueprintPath"), TargetBP);
    if (TargetBP.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                                 TEXT("blueprintPath required"),
                                 TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TSharedPtr<FJsonObject> SCSPayload = McpHandlerUtils::CreateResultObject();
    SCSPayload->SetStringField(TEXT("blueprintPath"), TargetBP);

    TArray<TSharedPtr<FJsonValue>> Ops;
    TSharedPtr<FJsonObject> Op = McpHandlerUtils::CreateResultObject();
    Op->SetStringField(TEXT("type"), TEXT("add_component"));
    FString Name;
    LocalParams->TryGetStringField(TEXT("componentName"), Name);
    if (!Name.IsEmpty())
      Op->SetStringField(TEXT("componentName"), Name);
    FString Class;
    LocalParams->TryGetStringField(TEXT("componentClass"), Class);
    if (!Class.IsEmpty())
      Op->SetStringField(TEXT("componentClass"), Class);
    FString AttachTo;
    LocalParams->TryGetStringField(TEXT("attachTo"), AttachTo);
    if (!AttachTo.IsEmpty())
      Op->SetStringField(TEXT("attachTo"), AttachTo);
    Ops.Add(MakeShared<FJsonValueObject>(Op));
    SCSPayload->SetArrayField(TEXT("operations"), Ops);

    return FMcpEditorFunctionHandlerAccess::HandleBlueprintAction(
        Bridge, RequestId, TEXT("blueprint_modify_scs"), SCSPayload,
        RequestingSocket);
  }

  return false;
}

} // namespace McpEditorFunctionHandlers
#endif
