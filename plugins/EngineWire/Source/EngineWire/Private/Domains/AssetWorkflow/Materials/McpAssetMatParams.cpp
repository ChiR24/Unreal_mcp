// Copyright (c) 2024 EngineWire Contributors

#include "Domains/MaterialAuthoring/McpMatPrivate.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#include "Materials/MaterialInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"

bool UEngineWireSubsystem::HandleAddMaterialParameter(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  FString Name;
  Payload->TryGetStringField(TEXT("parameterName"), Name);
  FString Type;
  Payload->TryGetStringField(TEXT("parameterType"), Type);

  if (AssetPath.IsEmpty() || Name.IsEmpty() || Type.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath, parameterName, and parameterType required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!McpSanitizeAssetPathOrReply(this, Socket, RequestId, AssetPath))
    return true;

  UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
  UMaterialFunction *Function = Material ? nullptr : LoadObject<UMaterialFunction>(nullptr, *AssetPath);

  if (!Material && !Function) {
    // This action adds a parameter *expression* to a material graph, and a
    // material instance has no graph. The capability is named
    // edit_material_instance, so callers reach for it with an instance and got
    // "Asset is not a Material or Material Function", which explains nothing.
    // Name the action that does work on instances.
    if (LoadObject<UMaterialInstance>(nullptr, *AssetPath)) {
      SendAutomationError(
          Socket, RequestId,
          TEXT("A material instance has no graph to add a parameter expression to. "
               "Use set_material_parameter to override a parameter on the instance, "
               "or add_parameter on its parent material to introduce a new one."),
          TEXT("INVALID_ASSET_TYPE"));
      return true;
    }
    SendAutomationError(Socket, RequestId,
                        TEXT("Asset is not a Material or Material Function"),
                        TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  // The material authoring adders make, name, group and place the node; this maps its fields onto theirs. It used to
  // make the node by hand and then run LayoutMaterialExpressions, which moved every connected node of the graph onto
  // the engine's grid on each add, with no undo step.
  Type = Type.ToLower();
  const bool bTexture = Type == TEXT("texture");
  const TCHAR *Adder = Type == TEXT("scalar") ? TEXT("add_scalar_parameter")
                     : Type == TEXT("vector") ? TEXT("add_vector_parameter")
                     : Type == TEXT("staticswitch") || Type == TEXT("static_switch") ? TEXT("add_static_switch_parameter")
                     : bTexture ? TEXT("add_texture_sample") : nullptr;
  if (!Adder) {
    SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Unsupported parameter type: %s"), *Type),
        TEXT("INVALID_TYPE"));
    return true;
  }
  TSharedPtr<FJsonObject> Mapped = MakeShared<FJsonObject>();
  Mapped->Values = Payload->Values;
  Mapped->SetStringField(TEXT("assetPath"), AssetPath);
  Mapped->RemoveField(TEXT("value"));
  if (bTexture) {
    // The path is checked here as it always was, before anything is made.
    FString TexPath;
    if (Payload->TryGetStringField(TEXT("value"), TexPath) &&
        !TexPath.IsEmpty()) {
      const FString TexPathAsGiven = TexPath;
      TexPath = SanitizeProjectRelativePath(TexPath);
      if (TexPath.IsEmpty()) {
        SendAutomationResponse(Socket, RequestId, false,
                               McpPathRefusalMessage(TEXT("texture path"), TexPathAsGiven), nullptr,
                               TEXT("SECURITY_VIOLATION"));
        return true;
      }
      Mapped->SetStringField(TEXT("texturePath"), TexPath);
    }
    return McpMaterialAuthoringHandlers::HandleAddTextureSample(this, RequestId, Adder, Mapped, Socket);
  }
  if (const TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("value"))) {
    Mapped->SetField(TEXT("defaultValue"), Value);
  }
  return McpMaterialAuthoringHandlers::HandleAddParameterNode(this, RequestId, Adder, Mapped, Socket);
}

bool UEngineWireSubsystem::HandleListMaterialInstances(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString AssetPath;
  if (!McpReadAssetPathOrReply(this, Socket, RequestId, Payload, AssetPath, /*bMustExist=*/false))
    return true;

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // The material instances whose Parent tag in the asset registry names this asset: only the registry is read,
  // no instance is loaded to ask.

  FARFilter Filter;
  MCP_AR_FILTER_ADD_CLASS_PATH(Filter, TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant"));
  Filter.bRecursiveClasses = true;

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan".
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  TArray<TSharedPtr<FJsonValue>> Instances;

  const FString ParentPackage = FPackageName::ObjectPathToPackageName(AssetPath);
  for (const FAssetData &Asset : AssetList) {
    FString ParentTag;
    if (Asset.GetTagValue(TEXT("Parent"), ParentTag)) {
      // Tag value might be "Material'Path'" or just "Path". Its package, compared whole: a substring match took the
      // instances of M_Base_Old (or of any path holding the text) for those of M_Base. A path-restricted caller
      // hears only of instances inside its prefixes.
      const FString TagPackage = FPackageName::ObjectPathToPackageName(FPackageName::ExportTextPathToObjectPath(ParentTag));
      if (TagPackage.Equals(ParentPackage, ESearchCase::IgnoreCase) &&
          McpCapabilityPrincipal::ActiveRequestPermitsPath(Asset.PackageName.ToString())) {
        Instances.Add(MakeShared<FJsonValueString>(MCP_ASSET_DATA_GET_SOFT_PATH(Asset)));
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("instances"), Instances);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Instances listed"),
                         Resp, FString());
  return true;
}

bool UEngineWireSubsystem::HandleResetInstanceParameters(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString AssetPath;
  if (!McpReadAssetPathOrReply(this, Socket, RequestId, Payload, AssetPath))
    return true;

  UObject *Asset = McpLoadAsset(AssetPath);
  UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(Asset);

  if (!MIC) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Asset is not a Material Instance Constant"),
                        TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  MIC->ClearParameterValuesEditorOnly();
  MIC->PostEditChange();
  MIC->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Instance parameters reset"), Resp, FString());
  return true;
}
