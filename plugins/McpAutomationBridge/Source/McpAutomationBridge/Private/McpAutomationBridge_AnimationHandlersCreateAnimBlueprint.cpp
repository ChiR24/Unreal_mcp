#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Modules/ModuleManager.h"

bool UMcpAutomationBridgeSubsystem::HandleCreateAnimBlueprint(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_animation_blueprint"),
                    ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_animation_blueprint payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString BlueprintName;
  if (!Payload->TryGetStringField(TEXT("name"), BlueprintName) ||
      BlueprintName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SkeletonPath;
  Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

  FString MeshPath;
  Payload->TryGetStringField(TEXT("meshPath"), MeshPath);

  FString SavePath;
  if (!Payload->TryGetStringField(TEXT("savePath"), SavePath) ||
      SavePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("savePath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USkeleton *Skeleton = nullptr;
  if (!SkeletonPath.IsEmpty()) {
    if (UEditorAssetLibrary::DoesAssetExist(SkeletonPath)) {
      Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
    }

    if (!Skeleton) {
      const FString SkelMessage =
          FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
      SendAutomationError(RequestingSocket, RequestId, SkelMessage,
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
  } else if (!MeshPath.IsEmpty()) {
    if (UEditorAssetLibrary::DoesAssetExist(MeshPath)) {
      if (USkeletalMesh *Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath)) {
        Skeleton = Mesh->GetSkeleton();
      }
    }

    if (!Skeleton) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Could not infer skeleton from meshPath, and "
                               "skeletonPath was not provided"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    SkeletonPath = Skeleton->GetPathName();
  } else {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("skeletonPath or meshPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString FullPath = FString::Printf(TEXT("%s/%s"), *SavePath, *BlueprintName);

  UAnimBlueprintFactory *Factory = NewObject<UAnimBlueprintFactory>();
  Factory->TargetSkeleton = Skeleton;
  Factory->BlueprintType = BPTYPE_Normal;
  Factory->ParentClass = UAnimInstance::StaticClass();

  if (!Factory) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create animation blueprint factory"),
                        TEXT("FACTORY_FAILED"));
    return true;
  }

  FString PackagePath = SavePath;
  FString AssetName = BlueprintName;
  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
  UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
      AssetName, PackagePath, UAnimBlueprint::StaticClass(), Factory);
  UAnimBlueprint *AnimBlueprint = Cast<UAnimBlueprint>(NewAsset);

  if (!AnimBlueprint) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create animation blueprint"),
                        TEXT("ASSET_CREATION_FAILED"));
    return true;
  }

  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

  bool bShouldSave = true;
  Payload->TryGetBoolField(TEXT("save"), bShouldSave);
  if (bShouldSave)
  {
    McpSafeOperations::McpSafeAssetSave(AnimBlueprint);
  }

  FAssetRegistryModule::AssetCreated(AnimBlueprint);


  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("blueprintPath"), AnimBlueprint->GetPathName());
  Resp->SetStringField(TEXT("blueprintName"), BlueprintName);
  Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
  Resp->SetStringField(TEXT("createdClass"), AnimBlueprint->GetClass()->GetPathName());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Animation blueprint created successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("create_animation_blueprint requires editor build"), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
