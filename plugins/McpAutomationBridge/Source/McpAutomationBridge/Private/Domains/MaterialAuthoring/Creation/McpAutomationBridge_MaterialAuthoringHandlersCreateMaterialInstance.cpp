#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
// instances: several instances under the one consent this call carried. Each entry runs through
// this same handler as a captured step over the call's own fields, so every check stays per
// instance; a palette of seven instances used to cost seven describes and seven consents.
static bool CreateMaterialInstanceBatch(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, const TArray<TSharedPtr<FJsonValue>>& Entries, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  TArray<TSharedPtr<FJsonValue>> Results;
  TArray<FString> Failed;
  for (int32 Index = 0; Index < Entries.Num(); ++Index) {
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->Values = Payload->Values;
    Item->RemoveField(TEXT("instances"));
    const TSharedPtr<FJsonObject>* Entry = nullptr;
    if (Entries[Index].IsValid() && Entries[Index]->TryGetObject(Entry)) {
      for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Entry)->Values) {
        Item->SetField(Pair.Key, Pair.Value);
      }
    }
    const FString StepId = FString::Printf(TEXT("%s#instance%d"), *RequestId, Index);
    FMcpResponseCaptureRegistry::Get().Begin(StepId);
    HandleCreateMaterialInstance(Bridge, StepId, TEXT("create_material_instance"), Item, Socket);
    const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(StepId);
    TSharedPtr<FJsonObject> Row = Reply.Result.IsValid() ? Reply.Result : McpHandlerUtils::CreateResultObject();
    const FString Name = GetJsonStringField(Item, TEXT("name"));
    Row->SetStringField(TEXT("name"), Name);
    Row->SetBoolField(TEXT("success"), Reply.bSuccess);
    if (!Reply.bSuccess) {
      const FString Error = Reply.Message.IsEmpty() ? FString(TEXT("the instance sent no reply")) : Reply.Message;
      Row->SetStringField(TEXT("error"), Error);
      Row->SetStringField(TEXT("errorCode"), Reply.ErrorCode);
      Failed.Add(FString::Printf(TEXT("#%d %s: %s"), Index, *Name, *Error));
    }
    Results.Add(MakeShared<FJsonValueObject>(Row));
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetArrayField(TEXT("instances"), Results);
  Result->SetNumberField(TEXT("created"), Results.Num() - Failed.Num());
  Bridge->SendAutomationResponse(Socket, RequestId, Failed.Num() == 0,
      Failed.Num() == 0 ? FString::Printf(TEXT("Created %d material instances."), Results.Num())
                        : FString::Printf(TEXT("%d of %d material instances failed: %s"), Failed.Num(), Results.Num(), *FString::Join(Failed, TEXT("; "))),
      Result, Failed.Num() == 0 ? FString() : FString(TEXT("INSTANCE_BATCH_INCOMPLETE")));
  return true;
}

bool HandleCreateMaterialInstance(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("create_material_instance")) {
    const TArray<TSharedPtr<FJsonValue>>* Instances = nullptr;
    if (Payload->TryGetArrayField(TEXT("instances"), Instances) && Instances->Num() > 0) {
      return CreateMaterialInstanceBatch(Bridge, RequestId, Payload, *Instances, Socket);
    }
    FString Name, ValidatedPath, ParentMaterial;
    bool bParentFolderCreated = false;
    if (!Payload->TryGetStringField(TEXT("parentMaterial"), ParentMaterial) ||
        ParentMaterial.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parentMaterial'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    // savePath is the published spelling (reading only `path` once dropped it and created
    // the asset under the default folder while reporting success); path is the legacy one.
    FString RequestedPath = GetJsonStringField(Payload, TEXT("savePath"));
    if (RequestedPath.IsEmpty()) {
      RequestedPath = GetJsonStringField(Payload, TEXT("path"));
    }
    if (!PrepareNewMaterialAsset(Bridge, RequestId, Socket, GetJsonStringField(Payload, TEXT("name")),
                                 RequestedPath, TEXT("/Game/Materials"), TEXT("MaterialInstanceConstant"),
                                 Name, ValidatedPath, bParentFolderCreated)) {
      return true;
    }
    // SECURITY: Validate parentMaterial path before loading
    FString ValidatedParentPath = SanitizeProjectRelativePath(ParentMaterial);
    if (ValidatedParentPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          McpPathRefusalMessage(TEXT("parentMaterial"), ParentMaterial),
                          TEXT("INVALID_PATH"));
      return true;
    }
    ParentMaterial = ValidatedParentPath;

    UMaterial *Parent = LoadObject<UMaterial>(nullptr, *ParentMaterial);
    if (!Parent) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load parent material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialInstanceConstantFactoryNew *Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Parent;

    UPackage *Package = CreatePackage(*ValidatedPath);
    if (!Package) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterialInstanceConstant *NewInstance = Cast<UMaterialInstanceConstant>(
        Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(),
                                  Package, FName(*Name),
                                  RF_Public | RF_Standalone, nullptr, GWarn));
    if (!NewInstance) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Failed to create material instance."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    NewInstance->PostEditChange();
    NewInstance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      McpSafeAssetSave(NewInstance);
    }

    FAssetRegistryModule::AssetCreated(NewInstance);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewInstance);
    // parameters: the instance comes out already tinted, under the one consent
    // this call carried, instead of a consented set_material_parameter per value.
    const TArray<TSharedPtr<FJsonValue>> *Entries = nullptr;
    if (Payload->TryGetArrayField(TEXT("parameters"), Entries) && Entries->Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> Results;
      TArray<FString> Failed;
      ApplyMaterialParameterList(Bridge, RequestId, NewInstance->GetOutermost()->GetName(), *Entries, Payload, Socket, Results, Failed);
      Result->SetArrayField(TEXT("parameters"), Results);
      if (Failed.Num() > 0) {
        Bridge->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Material instance '%s' created, but %d of %d parameters did not apply: %s"),
                            *Name, Failed.Num(), Results.Num(), *FString::Join(Failed, TEXT("; "))),
            Result, TEXT("PARAMETER_BATCH_INCOMPLETE"));
        return true;
      }
    }
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material instance '%s' created."), *Name), Result);
    return true;
  }

  return false;
}
}
