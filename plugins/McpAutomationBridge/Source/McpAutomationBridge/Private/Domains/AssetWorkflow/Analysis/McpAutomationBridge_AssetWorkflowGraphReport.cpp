// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialInstance.h"

bool UMcpAutomationBridgeSubsystem::HandleAnalyzeGraph(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("analyze_graph"), ESearchCase::IgnoreCase)) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("analyze_graph payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString AssetPathAsGiven = AssetPath;
  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        McpPathRefusalMessage(TEXT("assetPath"), AssetPathAsGiven),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // This resolved with a bare synchronous LoadObject, which is both unbounded
  // and unobservable: a path naming no real asset paid for a full load miss,
  // and a cold Blueprint pulled its whole dependency closure (parent class,
  // component templates, compile-on-load) onto the game thread. Witnessed
  // returning NOTHING until the 300 s transport timeout with no log line to
  // attribute it to. The registry answers existence without loading anything,
  // so a bad path now fails in microseconds and a still-scanning registry is
  // refused rather than raced; the Display logs make any remaining stall name
  // the asset that caused it. Every path below sends a response — a silent
  // stall was the defect, not merely a slow one.
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("analyze_graph: resolving '%s'"), *AssetPath);

  IAssetRegistry &AssetRegistry =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
  if (AssetRegistry.IsLoadingAssets()) {
    SendAutomationError(
        Socket, RequestId,
        TEXT("Asset registry is still scanning; retry once the initial scan completes."),
        TEXT("ASSET_REGISTRY_BUSY"));
    return true;
  }

  FString PackageName = AssetPath;
  int32 DotIndex = INDEX_NONE;
  if (PackageName.FindChar(TEXT('.'), DotIndex)) {
    PackageName = PackageName.Left(DotIndex);
  }

  TArray<FAssetData> PackageAssets;
  AssetRegistry.GetAssetsByPackageName(FName(*PackageName), PackageAssets);
  if (PackageAssets.Num() == 0) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Analysis needs the live object, but only a cold asset should pay for a
  // load, and by here it is known to exist.
  UObject *Asset = PackageAssets[0].FastGetAsset(/*bLoad=*/false);
  if (!Asset) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("analyze_graph: '%s' is not resident; loading synchronously"), *AssetPath);
    Asset = PackageAssets[0].FastGetAsset(/*bLoad=*/true);
  }
  if (!Asset) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset could not be loaded: %s"), *AssetPath),
                        TEXT("ASSET_LOAD_FAILED"));
    return true;
  }
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("analyze_graph: resolved '%s' as %s"), *AssetPath, *Asset->GetClass()->GetName());

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, Asset);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());

  // Check if it's a material
  UMaterial *Material = Cast<UMaterial>(Asset);
  UMaterialInstance *MaterialInstance = Cast<UMaterialInstance>(Asset);

  if (Material || MaterialInstance) {
    // Analyze material graph
    UMaterial *BaseMaterial = Material ? Material : MaterialInstance->GetBaseMaterial();

    // Get expressions count
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    const TArray<TObjectPtr<UMaterialExpression>> *Expressions = nullptr;
    if (Material && Material->GetEditorOnlyData()) {
      Expressions = &Material->GetEditorOnlyData()->ExpressionCollection.Expressions;
    }
#else
    // UE 5.0: Direct access, but also uses TObjectPtr
    const TArray<TObjectPtr<UMaterialExpression>> *Expressions = nullptr;
    if (Material) {
      Expressions = &Material->Expressions;
    }
#endif

    int32 NodeCount = Expressions ? Expressions->Num() : 0;
    int32 ParameterCount = 0;
    int32 TextureSampleCount = 0;
    TArray<FString> ParameterNames;

    if (Expressions) {
      for (UMaterialExpression *Expr : *Expressions) {
        if (!Expr) continue;
        if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
          ParameterCount++;
          ParameterNames.Add(Param->ParameterName.ToString());
        }
        // Texture parameters derive from TextureSample, not UMaterialExpressionParameter (dogfood #207).
        else if (UMaterialExpressionTextureSampleParameter *TexParam = Cast<UMaterialExpressionTextureSampleParameter>(Expr)) {
          ParameterCount++;
          ParameterNames.Add(TexParam->ParameterName.ToString());
        }
        if (Cast<UMaterialExpressionTextureSample>(Expr)) {
          TextureSampleCount++;
        }
      }
    }

    Result->SetStringField(TEXT("graphType"), TEXT("Material"));
    Result->SetNumberField(TEXT("nodeCount"), NodeCount);
    Result->SetNumberField(TEXT("parameterCount"), ParameterCount);
    Result->SetNumberField(TEXT("textureSampleCount"), TextureSampleCount);

    // Add parameter names
    TArray<TSharedPtr<FJsonValue>> ParamArray;
    for (const FString &ParamName : ParameterNames) {
      ParamArray.Add(MakeShared<FJsonValueString>(ParamName));
    }
    Result->SetArrayField(TEXT("parameters"), ParamArray);

    // Material properties
    Result->SetBoolField(TEXT("isMaterialInstance"), MaterialInstance != nullptr);
    if (Material) {
      Result->SetBoolField(TEXT("isTwoSided"), Material->TwoSided);
      Result->SetBoolField(TEXT("isMasked"), Material->IsMasked());
      Result->SetStringField(TEXT("blendMode"),
                             StaticEnum<EBlendMode>()->GetNameStringByValue((int64)Material->GetBlendMode()));
      // Get shading model name from the first selected model
      FString ShadingModelName = TEXT("Unknown");
      FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
      if (ShadingModels.HasShadingModel(MSM_DefaultLit)) ShadingModelName = TEXT("DefaultLit");
      else if (ShadingModels.HasShadingModel(MSM_Subsurface)) ShadingModelName = TEXT("Subsurface");
      else if (ShadingModels.HasShadingModel(MSM_Unlit)) ShadingModelName = TEXT("Unlit");
      else if (ShadingModels.HasShadingModel(MSM_ClearCoat)) ShadingModelName = TEXT("ClearCoat");
      else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) ShadingModelName = TEXT("SubsurfaceProfile");
      else if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin)) ShadingModelName = TEXT("PreintegratedSkin");
      Result->SetStringField(TEXT("shadingModel"), ShadingModelName);
    }

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material graph analyzed"), Result, FString());
    return true;
  }

  // Check if it's a blueprint
  UBlueprint *Blueprint = Cast<UBlueprint>(Asset);
  if (Blueprint) {
    TArray<UEdGraph *> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);

    int32 TotalNodes = 0;
    TArray<TSharedPtr<FJsonValue>> GraphInfoArray;

    for (UEdGraph *Graph : AllGraphs) {
      if (!Graph) continue;
      TSharedPtr<FJsonObject> GraphInfo = McpHandlerUtils::CreateResultObject();
      GraphInfo->SetStringField(TEXT("name"), Graph->GetName());
      GraphInfo->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());
      TotalNodes += Graph->Nodes.Num();
      GraphInfoArray.Add(MakeShared<FJsonValueObject>(GraphInfo));
    }

    Result->SetStringField(TEXT("graphType"), TEXT("Blueprint"));
    Result->SetStringField(TEXT("blueprintType"), Blueprint->BlueprintType == BPTYPE_Interface ? TEXT("Interface") :
                           Blueprint->BlueprintType == BPTYPE_MacroLibrary ? TEXT("MacroLibrary") :
                           Blueprint->BlueprintType == BPTYPE_FunctionLibrary ? TEXT("FunctionLibrary") : TEXT("Class"));
    Result->SetNumberField(TEXT("totalNodes"), TotalNodes);
    Result->SetNumberField(TEXT("graphCount"), AllGraphs.Num());
    Result->SetArrayField(TEXT("graphs"), GraphInfoArray);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Blueprint graph analyzed"), Result, FString());
    return true;
  }

  // Graphs this action does not walk: name the reader instead of claiming there is no graph.
  struct FMcpGraphReader { const TCHAR *ClassName; const TCHAR *Tool; const TCHAR *Action; };
  static const FMcpGraphReader Readers[] = {
      {TEXT("MetaSoundSource"), TEXT("manage_audio"), TEXT("get_metasound_graph")},
      {TEXT("MetaSoundPatch"), TEXT("manage_audio"), TEXT("get_metasound_graph")},
      {TEXT("NiagaraSystem"), TEXT("manage_effect"), TEXT("get_niagara_info")},
      {TEXT("NiagaraEmitter"), TEXT("manage_effect"), TEXT("get_niagara_info")},
      {TEXT("BehaviorTree"), TEXT("manage_ai"), TEXT("get_tree")}};
  const FString ClassName = Asset->GetClass()->GetName();
  for (const FMcpGraphReader &Reader : Readers) {
    if (ClassName != Reader.ClassName) {
      continue;
    }
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("assetPath"), AssetPath);
    TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
    Next->SetStringField(TEXT("operation"), TEXT("execute"));
    Next->SetStringField(TEXT("tool"), Reader.Tool);
    Next->SetStringField(TEXT("action"), Reader.Action);
    Next->SetObjectField(TEXT("params"), Params);
    const FString Message = FString::Printf(TEXT("A %s graph is read by %s %s; nextCall reads this one."),
                                            *ClassName, Reader.Tool, Reader.Action);
    Result->SetStringField(TEXT("graphType"), ClassName);
    Result->SetStringField(TEXT("message"), Message);
    Result->SetObjectField(TEXT("nextCall"), Next);
    SendAutomationResponse(Socket, RequestId, true, Message, Result, FString());
    return true;
  }

  // Generic asset - no graph
  Result->SetStringField(TEXT("graphType"), TEXT("None"));
  Result->SetStringField(TEXT("message"), TEXT("Asset does not have a graph structure"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("No graph to analyze for this asset type"), Result, FString());
  return true;
}
