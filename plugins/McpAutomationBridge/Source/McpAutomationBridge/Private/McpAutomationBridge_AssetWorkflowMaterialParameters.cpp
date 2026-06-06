// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridge_AssetWorkflowMaterialHost.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/Texture.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialParameter(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Type;
  Payload->TryGetStringField(TEXT("type"), Type);

  if (AssetPath.IsEmpty() || Name.IsEmpty() || Type.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("assetPath, name, and type required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(AssetPath, Material, Function);

  if (!Material && !Function) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material or Material Function"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                : static_cast<UObject*>(Function);

  UMaterialExpression *NewExpression = nullptr;
  Type = Type.ToLower();

  // Asymmetric creation paths by design:
  // - UMaterial: UMaterialEditingLibrary::CreateMaterialExpression handles
  //   graph registration, undo transactions, and editor-only data setup.
  // - UMaterialFunction: UMaterialEditingLibrary only supports UMaterial, so we
  //   use NewObject + manual add to the expression collection. This is
  //   intentional due to API limitations — CreateMaterialExpression does not
  //   accept UMaterialFunction as a host.
  auto CreateExpr = [&](UClass* ExprClass) -> UMaterialExpression* {
    if (Material) {
      return UMaterialEditingLibrary::CreateMaterialExpression(Material, ExprClass);
    }
    UMaterialExpression* Expr = NewObject<UMaterialExpression>(HostOuter, ExprClass, NAME_None, RF_Transactional);
    if (Expr) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      Function->GetEditorOnlyData()->ExpressionCollection.AddExpression(Expr);
#else
      Function->FunctionExpressions.Add(Expr);
#endif
    }
    return Expr;
  };

  if (Type == TEXT("scalar")) {
    NewExpression = CreateExpr(UMaterialExpressionScalarParameter::StaticClass());
    if (UMaterialExpressionScalarParameter *ScalarParam =
            Cast<UMaterialExpressionScalarParameter>(NewExpression)) {
      ScalarParam->ParameterName = FName(*Name);
      double Val = 0.0;
      if (Payload->TryGetNumberField(TEXT("value"), Val)) {
        ScalarParam->DefaultValue = (float)Val;
      }
    }
  } else if (Type == TEXT("vector")) {
    NewExpression = CreateExpr(UMaterialExpressionVectorParameter::StaticClass());
    if (UMaterialExpressionVectorParameter *VectorParam =
            Cast<UMaterialExpressionVectorParameter>(NewExpression)) {
      VectorParam->ParameterName = FName(*Name);
      const TSharedPtr<FJsonObject> *VecObj;
      if (Payload->TryGetObjectField(TEXT("value"), VecObj)) {
        double R = 0, G = 0, B = 0, A = 1;
        (*VecObj)->TryGetNumberField(TEXT("r"), R);
        (*VecObj)->TryGetNumberField(TEXT("g"), G);
        (*VecObj)->TryGetNumberField(TEXT("b"), B);
        (*VecObj)->TryGetNumberField(TEXT("a"), A);
        VectorParam->DefaultValue =
            FLinearColor((float)R, (float)G, (float)B, (float)A);
      }
    }
  } else if (Type == TEXT("texture")) {
    NewExpression = CreateExpr(UMaterialExpressionTextureSampleParameter2D::StaticClass());
    if (UMaterialExpressionTextureSampleParameter2D *TexParam =
            Cast<UMaterialExpressionTextureSampleParameter2D>(NewExpression)) {
      TexParam->ParameterName = FName(*Name);
      FString TexPath;
      if (Payload->TryGetStringField(TEXT("value"), TexPath) &&
          !TexPath.IsEmpty()) {
        TexPath = SanitizeProjectRelativePath(TexPath);
        if (TexPath.IsEmpty()) {
          SendAutomationResponse(Socket, RequestId, false,
                                 TEXT("Invalid texture path"), nullptr,
                                 TEXT("SECURITY_VIOLATION"));
          return true;
        }
        UTexture *Tex = LoadObject<UTexture>(nullptr, *TexPath);
        if (Tex) {
          TexParam->Texture = Tex;
        }
      }
    }
  } else if (Type == TEXT("staticswitch") || Type == TEXT("static_switch")) {
    NewExpression = CreateExpr(UMaterialExpressionStaticSwitchParameter::StaticClass());
    if (UMaterialExpressionStaticSwitchParameter *SwitchParam =
            Cast<UMaterialExpressionStaticSwitchParameter>(NewExpression)) {
      SwitchParam->ParameterName = FName(*Name);
      bool Val = false;
      if (Payload->TryGetBoolField(TEXT("value"), Val)) {
        SwitchParam->DefaultValue = Val;
      }
    }
  } else {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Unsupported parameter type: %s"), *Type), nullptr,
        TEXT("INVALID_TYPE"));
    return true;
  }

  if (NewExpression) {
    if (Material) {
      UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
      UMaterialEditingLibrary::RecompileMaterial(Material);
      Material->MarkPackageDirty();
    } else {
      FinalizeHost(nullptr, Function);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetStringField(TEXT("parameterName"), Name);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Parameter added"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create parameter expression"),
                           nullptr, TEXT("CREATE_FAILED"));
  }

  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleListMaterialInstances(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Find all assets that are Material Instances and have this asset as parent
  // Note: This can be expensive if we scan all assets.
  // Optimization: Use GetReferencers? Or just filter by class and check parent.
  // Since we can't easily query by "Parent" tag efficiently without iterating,
  // we'll try a filtered query.

  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                           TEXT("MaterialInstanceConstant")));
#else
  Filter.ClassNames.Add(FName(TEXT("MaterialInstanceConstant")));
#endif
  Filter.bRecursiveClasses = true;

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  TArray<TSharedPtr<FJsonValue>> Instances;

  // We need to check the parent. Loading the asset is safest but slow.
  // Checking tags is faster. MICs usually have "Parent" tag.
  for (const FAssetData &Asset : AssetList) {
    // Check tag first
    FString ParentTag;
    if (Asset.GetTagValue(TEXT("Parent"), ParentTag)) {
      // Tag value might be "Material'Path'" or just "Path"
      // It's usually formatted string.
      if (ParentTag.Contains(AssetPath)) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.GetSoftObjectPath().ToString()));
#else
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.ToSoftObjectPath().ToString()));
#endif
      }
    } else {
      // Fallback: load asset (slow, but accurate)
      // Only do this if tag is missing? Or maybe skip to avoid perf hit.
      // Let's rely on tag for now.
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("instances"), Instances);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Instances listed"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleResetInstanceParameters(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(Asset);

  if (!MIC) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material Instance Constant"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
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
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
