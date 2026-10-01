// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringMainInputs.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#include "EditorAssetLibrary.h"
// EMaterialDomain's own header exists only on the engines that split it out of
// Material.h; on 5.0 it is declared in Materials/Material.h, included below.
#if __has_include("MaterialDomain.h")
#include "MaterialDomain.h"
#endif
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString AssetPathAsGiven = AssetPath;
  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           McpPathRefusalMessage(TEXT("assetPath"), AssetPathAsGiven), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!McpAssetExists(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = McpLoadAsset(AssetPath);
  UMaterialInterface *Material = Cast<UMaterialInterface>(Asset);

  if (!Material) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material"), nullptr,
                           TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  // Ensure material is compiled
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Material->EnsureIsComplete();
#else
  // UE 5.0: Force compilation by accessing the material resource
  Material->GetMaterial();
#endif

  TSharedPtr<FJsonObject> Stats = McpHandlerUtils::CreateResultObject();

  // Get actual shading model from the material
  FString ShadingModelStr = TEXT("Unknown");
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    if (const UEnum *ShadingEnum = StaticEnum<EMaterialShadingModel>()) {
      ShadingModelStr = ShadingEnum->GetNameStringByValue(
          static_cast<int64>(BaseMat->GetShadingModels().GetFirstShadingModel()));
      ShadingModelStr.RemoveFromStart(TEXT("MSM_"));
    }
  }
  Stats->SetStringField(TEXT("shadingModel"), ShadingModelStr);

  // Blend mode and material domain via the shared UEnum lookup. Each field is
  // only emitted when the enum lookup yields a real name, so a lookup failure
  // leaves the field absent instead of publishing an invented label.
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    if (const UEnum *BlendModeEnum = StaticEnum<EBlendMode>()) {
      const FString BlendModeName = BlendModeEnum->GetNameStringByValue(
          static_cast<int64>(BaseMat->GetBlendMode()));
      if (!BlendModeName.IsEmpty()) {
        Stats->SetStringField(TEXT("blendMode"), BlendModeName);
      }
    }
    if (const UEnum *DomainEnum = StaticEnum<EMaterialDomain>()) {
      const FString DomainName = DomainEnum->GetNameStringByValue(
          static_cast<int64>(BaseMat->MaterialDomain));
      if (!DomainName.IsEmpty()) {
        Stats->SetStringField(TEXT("materialDomain"), DomainName);
      }
    }
  }

  // Graph census: one bounded pass over the material's expression list counts
  // nodes, texture samples, parameters (scalar/vector/texture) and static
  // switches. samplerCount keeps its legacy meaning (texture samples) beside
  // the explicitly named textureSampleCount so existing readers stay valid.
  int32 SamplerCount = 0;
  int32 NodeCount = 0;
  int32 ScalarParameterCount = 0;
  int32 VectorParameterCount = 0;
  int32 TextureParameterCount = 0;
  int32 StaticSwitchCount = 0;
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(BaseMat)) {
      if (!Expr) {
        continue;
      }
      NodeCount++;
      if (Expr->IsA<UMaterialExpressionTextureSample>()) {
        SamplerCount++;
      }
      if (Expr->IsA<UMaterialExpressionTextureSampleParameter>()) {
        TextureParameterCount++;
      }
      if (Expr->IsA<UMaterialExpressionScalarParameter>()) {
        ScalarParameterCount++;
      }
      if (Expr->IsA<UMaterialExpressionVectorParameter>()) {
        VectorParameterCount++;
      }
      if (Expr->IsA<UMaterialExpressionStaticSwitchParameter>() ||
          Expr->IsA<UMaterialExpressionStaticBoolParameter>()) {
        StaticSwitchCount++;
      }
    }
  }
  Stats->SetNumberField(TEXT("nodeCount"), NodeCount);
  Stats->SetNumberField(TEXT("textureSampleCount"), SamplerCount);
  Stats->SetNumberField(TEXT("samplerCount"), SamplerCount);
  Stats->SetNumberField(TEXT("scalarParameterCount"), ScalarParameterCount);
  Stats->SetNumberField(TEXT("vectorParameterCount"), VectorParameterCount);
  Stats->SetNumberField(TEXT("textureParameterCount"), TextureParameterCount);
  Stats->SetNumberField(TEXT("staticSwitchCount"), StaticSwitchCount);
  Stats->SetBoolField(TEXT("hasStaticSwitches"), StaticSwitchCount > 0);

  // Advertise the root output node identity: connect_nodes addresses the
  // material result node only as "Main"; stats is the always-reachable read that
  // must surface it along with its accepted inputs.
  Stats->SetStringField(TEXT("resultNode"), TEXT("Main"));
  TArray<TSharedPtr<FJsonValue>> ResultNodeInputs;
  // The same pin list connect_nodes accepts on "Main".
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    ForEachMainMaterialInput(BaseMat, [&](const TCHAR *Input, FExpressionInput &) {
      ResultNodeInputs.Add(MakeShared<FJsonValueString>(Input));
    });
  }
  Stats->SetArrayField(TEXT("resultNodeInputs"), ResultNodeInputs);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  // `details` is the field the asset.get_material_stats output contract declares.
  Resp->SetObjectField(TEXT("details"), Stats);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material stats retrieved"), Resp, FString());
  return true;
}
