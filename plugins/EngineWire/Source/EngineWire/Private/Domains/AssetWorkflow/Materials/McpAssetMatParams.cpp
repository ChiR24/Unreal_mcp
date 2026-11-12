// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AssetWorkflow/Materials/McpAssetMatHost.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#include "Materials/MaterialInstance.h"
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

bool UEngineWireSubsystem::HandleAddMaterialParameter(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  // The published capability schema declares `parameterName`/`parameterType`
  // with additionalProperties:false, so `name`/`type` cannot be sent through
  // the gateway at all. Reading only `name`/`type` here left the capability
  // uncallable by ANY input: the schema-correct call failed in the handler,
  // the handler-correct call failed schema validation, and sending both was
  // rejected as an undeclared parameter. Prefer the contract spelling and keep
  // `name`/`type` as the legacy WebSocket fallback.
  FString Name;
  if (!Payload->TryGetStringField(TEXT("parameterName"), Name) || Name.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), Name);
  }
  FString Type;
  if (!Payload->TryGetStringField(TEXT("parameterType"), Type) || Type.IsEmpty()) {
    Payload->TryGetStringField(TEXT("type"), Type);
  }

  if (AssetPath.IsEmpty() || Name.IsEmpty() || Type.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("assetPath, parameterName, and parameterType required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!McpSanitizeAssetPathOrReply(this, Socket, RequestId, AssetPath))
    return true;

  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(AssetPath, Material, Function);

  if (!Material && !Function) {
    // This action adds a parameter *expression* to a material graph, and a
    // material instance has no graph. The capability is named
    // edit_material_instance, so callers reach for it with an instance and got
    // "Asset is not a Material or Material Function", which explains nothing.
    // Name the action that does work on instances.
    if (LoadObject<UMaterialInstance>(nullptr, *AssetPath)) {
      SendAutomationResponse(
          Socket, RequestId, false,
          TEXT("A material instance has no graph to add a parameter expression to. "
               "Use set_material_parameter to override a parameter on the instance, "
               "or add_parameter on its parent material to introduce a new one."),
          nullptr, TEXT("INVALID_ASSET_TYPE"));
      return true;
    }
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material or Material Function"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                : static_cast<UObject*>(Function);

  UMaterialExpression *NewExpression = nullptr;
  Type = Type.ToLower();

  // A material's node is made by UMaterialEditingLibrary::CreateMaterialExpression (graph registration, undo,
  // editor-only data); a material function's is made here by hand and added to its expression list.
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
        const FString TexPathAsGiven = TexPath;
        TexPath = SanitizeProjectRelativePath(TexPath);
        if (TexPath.IsEmpty()) {
          SendAutomationResponse(Socket, RequestId, false,
                                 McpPathRefusalMessage(TEXT("texture path"), TexPathAsGiven), nullptr,
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.GetSoftObjectPath().ToString()));
#else
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.ToSoftObjectPath().ToString()));
#endif
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
}
