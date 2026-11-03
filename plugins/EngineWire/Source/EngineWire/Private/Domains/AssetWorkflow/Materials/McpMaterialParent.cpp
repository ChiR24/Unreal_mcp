// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AssetWorkflow/Materials/McpMaterialParent.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"

#include "CoreGlobals.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/Change.h"
#include "Misc/ITransaction.h"

namespace McpMaterialParent
{
namespace
{
// SetParentInternal compiles anew only when the parent changes, so the instance is first left with none: from none,
// To always reads as a change. The update context redraws what uses the instance, as the instance editor's does.
void Rebuild(UMaterialInstanceConstant* Instance, UMaterialInterface* To)
{
  FMaterialUpdateContext Context;
  Instance->Parent = nullptr;
  Instance->SetParentEditorOnly(To, true);
  Instance->PostEditChange();
  Context.AddMaterialInstance(Instance);
}

// Undo replays a transaction's records last to first and redo first to last, so this runs before the instance's saved
// state on undo and after it on redo: either way it rebuilds for the parent named, whatever Parent reads by then.
class FParentChange final : public FCommandChange
{
public:
  FParentChange(UMaterialInterface* InOld, UMaterialInterface* InNew) : Old(InOld), New(InNew) {}

  virtual void Apply(UObject* Object) override
  {
    if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Object)) Rebuild(Instance, New.Get());
  }

  virtual void Revert(UObject* Object) override
  {
    if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Object)) Rebuild(Instance, Old.Get());
  }

  virtual FString ToString() const override { return TEXT("Material instance parent"); }

private:
  // The instance's saved state in the same step holds both parents, so they outlive the step.
  TWeakObjectPtr<UMaterialInterface> Old;
  TWeakObjectPtr<UMaterialInterface> New;
};
} // namespace

UMaterialInterface* HeldParent(const UObject* Object)
{
  const UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Object);
  return Instance ? Instance->Parent.Get() : nullptr;
}

FString Refusal(const UMaterialInstanceConstant* Instance, const UMaterialInterface* To)
{
  if (!To) {
    return FString();
  }
  if (!To->IsA<UMaterial>() && !To->IsA<UMaterialInstanceConstant>()) {
    return FString::Printf(TEXT("%s is a %s: a material instance's parent is a Material or a Material Instance asset."),
                           *To->GetPathName(), *To->GetClass()->GetName());
  }
  if (To == Instance) {
    return FString::Printf(TEXT("%s cannot be its own parent."), *Instance->GetPathName());
  }
  const UMaterialInstance* ToInstance = Cast<UMaterialInstance>(To);
  if (ToInstance && ToInstance->IsChildOf(Instance)) {
    return FString::Printf(TEXT("%s is a child of %s: the instance would become its own parent."), *To->GetPathName(),
                           *Instance->GetPathName());
  }
  return FString();
}

void SetParent(UMaterialInstanceConstant* Instance, UMaterialInterface* Old, UMaterialInterface* To)
{
  Rebuild(Instance, To);
  if (GUndo) {
    GUndo->StoreUndo(Instance, MakeUnique<FParentChange>(Old, To));
  }
}

bool ApplyWrittenParent(UObject* Object, const FString& PropertyPath, UMaterialInterface* Old, FString& OutError)
{
  UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Object);
  if (!Instance || !PropertyPath.Equals(TEXT("Parent"), ESearchCase::IgnoreCase)) {
    return true;
  }
  UMaterialInterface* To = Instance->Parent;
  OutError = Refusal(Instance, To);
  if (!OutError.IsEmpty()) {
    Instance->Parent = Old;
    return false;
  }
  SetParent(Instance, Old, To);
  return true;
}

bool HandleSetParent(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                     TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  auto Fail = [&](const FString& Message, const TCHAR* Code) {
    Bridge->SendAutomationError(Socket, RequestId, Message, Code);
    return true;
  };
  const FString AssetText = GetJsonStringField(Payload, TEXT("assetPath"));
  const FString ParentText = GetJsonStringField(Payload, TEXT("parentPath"));
  if (AssetText.IsEmpty() || ParentText.IsEmpty()) {
    return Fail(TEXT("assetPath and parentPath are required."), TEXT("INVALID_ARGUMENT"));
  }
  const FString AssetPath = SanitizeProjectRelativePath(AssetText);
  const FString ParentPath = SanitizeProjectRelativePath(ParentText);
  if (AssetPath.IsEmpty()) return Fail(McpPathRefusalMessage(TEXT("assetPath"), AssetText), TEXT("SECURITY_VIOLATION"));
  if (ParentPath.IsEmpty()) return Fail(McpPathRefusalMessage(TEXT("parentPath"), ParentText), TEXT("SECURITY_VIOLATION"));

  UObject* Asset = McpLoadAsset(AssetPath);
  UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Asset);
  if (!Asset) return Fail(FString::Printf(TEXT("No asset at %s."), *AssetText), TEXT("ASSET_NOT_FOUND"));
  if (!Instance) {
    return Fail(FString::Printf(TEXT("%s is a %s, not a material instance."), *AssetText, *Asset->GetClass()->GetName()),
                TEXT("INVALID_ASSET_TYPE"));
  }
  if (Instance->GetOutermost()->GetName().StartsWith(TEXT("/Engine/"))) {
    return Fail(TEXT("Engine content is not changed: duplicate the instance into /Game first."), TEXT("ENGINE_CONTENT"));
  }
  UMaterialInterface* To = Cast<UMaterialInterface>(McpLoadAsset(ParentPath));
  if (!To) return Fail(FString::Printf(TEXT("No material at %s."), *ParentText), TEXT("PARENT_NOT_FOUND"));
  const FString Refused = Refusal(Instance, To);
  if (!Refused.IsEmpty()) return Fail(Refused, TEXT("INVALID_PARENT"));

  UMaterialInterface* Old = Instance->Parent;
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  {
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Material Instance Parent")),
                                            EMcpMutationDurability::EditorStateOnly, {Instance});
    SetParent(Instance, Old, To);
    Transaction.DescribeInto(Result);
  }
  Instance->MarkPackageDirty();
  const bool bSaved = McpSafeAssetSave(Instance);
  Result->SetStringField(TEXT("assetPath"), Instance->GetPathName());
  Result->SetStringField(TEXT("parent"), To->GetPathName());
  Result->SetStringField(TEXT("previousParent"), Old ? Old->GetPathName() : FString());
  Result->SetBoolField(TEXT("ownShaders"), Instance->bHasStaticPermutationResource);
  Result->SetBoolField(TEXT("saved"), bSaved);
  McpHandlerUtils::AddVerification(Result, Instance);
  Bridge->SendAutomationResponse(Socket, RequestId, true,
                                 FString::Printf(TEXT("%s now takes %s as its parent."), *Instance->GetName(), *To->GetName()),
                                 Result);
  return true;
}
} // namespace McpMaterialParent
