#include "Domains/MaterialAuthoring/McpMatPrivate.h"

namespace McpMaterialAuthoringHandlers
{
// instances: several instances under the one consent this call carried. Each entry runs through
// this same handler as a captured step over the call's own fields, so every check stays per
// instance; a palette of seven instances used to cost seven describes and seven consents.
static bool CreateMaterialInstanceBatch(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, const TArray<TSharedPtr<FJsonValue>>& Entries, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  TArray<TSharedPtr<FJsonValue>> Results;
  TArray<TSharedPtr<FJsonValue>> Changed;
  TArray<FString> Failed;
  for (int32 Index = 0; Index < Entries.Num(); ++Index) {
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->Values = Payload->Values;
    Item->RemoveField(TEXT("instances"));
    const TSharedPtr<FJsonObject>* Entry = nullptr;
    if (Entries[Index].IsValid() && Entries[Index]->TryGetObject(Entry)) {
      for (TPair<FString, TSharedPtr<FJsonValue>> Pair : (*Entry)->Values) {
        Item->SetField(Pair.Key, Pair.Value);
      }
    }
    const FString StepId = FString::Printf(TEXT("%s#instance%d"), *RequestId, Index);
    const FMcpCapturedResponse Reply = McpCaptureResponse(StepId, [&] {
      HandleCreateMaterialInstance(Bridge, StepId, TEXT("create_material_instance"), Item, Socket);
    });
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
    // An instance made whose parameters then failed exists and was saved: the receipt and created count it, which
    // counting only the entries that fully succeeded left out.
    FString AssetPath;
    if (Row->TryGetStringField(TEXT("assetPath"), AssetPath) && GetJsonBoolField(Row, TEXT("existsAfter"), false)) {
      Changed.Add(MakeShared<FJsonValueString>(AssetPath));
    }
    Results.Add(MakeShared<FJsonValueObject>(Row));
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetArrayField(TEXT("instances"), Results);
  // The receipt's changes list these; without them a palette of six answered changes [].
  Result->SetArrayField(TEXT("changedAssets"), Changed);
  Result->SetNumberField(TEXT("created"), Changed.Num());
  Bridge->SendAutomationResponse(Socket, RequestId, Failed.Num() == 0,
      Failed.Num() == 0 ? FString::Printf(TEXT("Created %d material instances."), Results.Num())
                        : FString::Printf(TEXT("%d of %d material instances failed: %s"), Failed.Num(), Results.Num(), *FString::Join(Failed, TEXT("; "))),
      Result, Failed.Num() == 0 ? FString() : FString(TEXT("INSTANCE_BATCH_INCOMPLETE")));
  return true;
}

bool HandleCreateMaterialInstance(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
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
    // The parent before the new asset's folders are made: a call refused for its parent left them behind.
    // SECURITY: Validate parentMaterial path before loading
    if (!SanitizeMaterialPathOrReply(Bridge, RequestId, Socket, TEXT("parentMaterial"), ParentMaterial))
      return true;

    // Any material interface: a child instance of another instance is how a material family is built, and loading
    // only a UMaterial refused every instance as a parent.
    FString ResolvedParent;
    UMaterialInterface *Parent = McpLoadCanonicalAsset<UMaterialInterface>(ParentMaterial, ResolvedParent);
    if (!Parent) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load parent material (a Material or a Material Instance)."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    // savePath is the published spelling (reading only `path` once dropped it and created the asset under the
    // default folder while reporting success); path is an instances entry's.
    if (!PrepareNewMaterialAsset(Bridge, RequestId, Socket, GetJsonStringField(Payload, TEXT("name")),
                                 McpGetFirstStringField(Payload, {TEXT("savePath"), TEXT("path")}), TEXT("/Game/Materials"),
                                 TEXT("MaterialInstanceConstant"), Name, ValidatedPath, bParentFolderCreated)) {
      return true;
    }

    UMaterialInstanceConstantFactoryNew *Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Parent;

    UPackage *Package = McpCreatePackageOrNull(ValidatedPath);
    if (!Package) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterialInstanceConstant *NewInstance = Cast<UMaterialInstanceConstant>(
        Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(),
                                  Package, FName(*Name),
                                  RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
    if (!NewInstance) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Failed to create material instance."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    NewInstance->PostEditChange();
    NewInstance->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = FinishNewMaterialAsset(NewInstance, Payload, bParentFolderCreated);
    // parameters: the instance comes out already tinted, under the one consent
    // this call carried, instead of a consented set_material_parameter per value.
    const TArray<TSharedPtr<FJsonValue>> *Entries = nullptr;
    if (Payload->TryGetArrayField(TEXT("parameters"), Entries) && Entries->Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> Results;
      TArray<FString> Failed;
      // The instance was saved as it was made; the values are saved once after them. With none written there is no
      // second save, and the first one stands (it answered saved false for an instance on disk).
      FString Written;
      const bool bResaved = ApplyMaterialParameterList(Bridge, RequestId, NewInstance->GetOutermost()->GetName(),
                                                       *Entries, Payload, Socket, Results, Failed, &Written);
      Result->SetBoolField(TEXT("saved"), Written.IsEmpty() ? GetJsonBoolField(Result, TEXT("saved"), false) : bResaved);
      if (Failed.Num() > 0) {
        Result->SetArrayField(TEXT("parameters"), Results);
        Bridge->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Material instance '%s' created, but %d of %d parameters did not apply: %s"),
                            *Name, Failed.Num(), Entries->Num(), *FString::Join(Failed, TEXT("; "))),
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
