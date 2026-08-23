// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialStats(
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
    FMaterialShadingModelField ShadingModels = BaseMat->GetShadingModels();
    // Check shading models using HasShadingModel - prioritize common ones
    if (ShadingModels.HasShadingModel(MSM_Unlit)) {
      ShadingModelStr = TEXT("Unlit");
    } else if (ShadingModels.HasShadingModel(MSM_DefaultLit)) {
      ShadingModelStr = TEXT("DefaultLit");
    } else if (ShadingModels.HasShadingModel(MSM_Subsurface)) {
      ShadingModelStr = TEXT("Subsurface");
    } else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) {
      ShadingModelStr = TEXT("SubsurfaceProfile");
    } else if (ShadingModels.HasShadingModel(MSM_ClearCoat)) {
      ShadingModelStr = TEXT("ClearCoat");
    } else if (ShadingModels.HasShadingModel(MSM_TwoSidedFoliage)) {
      ShadingModelStr = TEXT("TwoSidedFoliage");
    } else if (ShadingModels.HasShadingModel(MSM_Hair)) {
      ShadingModelStr = TEXT("Hair");
    } else if (ShadingModels.HasShadingModel(MSM_Cloth)) {
      ShadingModelStr = TEXT("Cloth");
    } else if (ShadingModels.HasShadingModel(MSM_Eye)) {
      ShadingModelStr = TEXT("Eye");
    } else if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin)) {
      ShadingModelStr = TEXT("PreintegratedSkin");
    }
  }
  Stats->SetStringField(TEXT("shadingModel"), ShadingModelStr);

  // Get instruction count from material resource
  // Note: GetMaxNumInstructionsForShader takes FShaderType* in UE 5.6, EShaderFrequency in some earlier versions
  // Skip this in 5.6 as there's no clean way to get a FShaderType* for the pixel shader
  int32 InstructionCount = -1; // Not easily available in this UE version
  Stats->SetNumberField(TEXT("instructionCount"), InstructionCount);

  // Count texture samplers used in the material
  int32 SamplerCount = 0;
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(BaseMat)) {
      if (Expr && Expr->IsA<UMaterialExpressionTextureSample>()) {
        SamplerCount++;
      }
    }
  }
  Stats->SetNumberField(TEXT("samplerCount"), SamplerCount);

  // Advertise the root output node identity (BB-016): connect_nodes addresses the
  // material result node only as "Main"; stats is the always-reachable read that
  // must surface it along with its accepted inputs.
  Stats->SetStringField(TEXT("resultNode"), TEXT("Main"));
  TArray<TSharedPtr<FJsonValue>> ResultNodeInputs;
  for (const TCHAR* Input : { TEXT("BaseColor"), TEXT("EmissiveColor"), TEXT("Roughness"), TEXT("Metallic"),
                              TEXT("Specular"), TEXT("Normal"), TEXT("Opacity"), TEXT("OpacityMask"),
                              TEXT("AmbientOcclusion"), TEXT("SubsurfaceColor"), TEXT("WorldPositionOffset") })
  {
    ResultNodeInputs.Add(MakeShared<FJsonValueString>(Input));
  }
  Stats->SetArrayField(TEXT("resultNodeInputs"), ResultNodeInputs);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("stats"), Stats);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material stats retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
