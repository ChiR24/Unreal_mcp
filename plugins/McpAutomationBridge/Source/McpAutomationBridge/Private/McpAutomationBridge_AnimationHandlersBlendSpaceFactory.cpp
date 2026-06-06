#include "McpAutomationBridge_AnimationHandlersBlendSpaceAssets.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"

#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR && MCP_HAS_BLENDSPACE_FACTORY
UObject *CreateBlendSpaceAsset(const FString &AssetName,
                                      const FString &PackagePath,
                                      USkeleton *TargetSkeleton,
                                      bool bTwoDimensional, FString &OutError) {
  OutError.Reset();

  UFactory *Factory = nullptr;
  UClass *DesiredClass = nullptr;

  if (bTwoDimensional) {
    UBlendSpaceFactoryNew *Factory2D = NewObject<UBlendSpaceFactoryNew>();
    if (!Factory2D) {
      OutError = TEXT("Failed to allocate BlendSpace factory");
      return nullptr;
    }
    Factory2D->TargetSkeleton = TargetSkeleton;
    Factory = Factory2D;
    DesiredClass = UBlendSpace::StaticClass();
  } else {
    UBlendSpaceFactory1D *Factory1D = NewObject<UBlendSpaceFactory1D>();
    if (!Factory1D) {
      OutError = TEXT("Failed to allocate BlendSpace1D factory");
      return nullptr;
    }
    Factory1D->TargetSkeleton = TargetSkeleton;
    Factory = Factory1D;
    DesiredClass = UBlendSpace1D::StaticClass();
  }

  if (!Factory || !DesiredClass) {
    OutError = TEXT("BlendSpace factory unavailable");
    return nullptr;
  }

  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
  return AssetToolsModule.Get().CreateAsset(AssetName, PackagePath,
                                            DesiredClass, Factory);
}

/**
 * @brief Applies axis range and grid configuration to a blend space asset.
 *
 * Reads numeric fields from the provided JSON payload and updates the blend
 * space's first axis (minX, maxX, gridX) and, if bTwoDimensional is true,
 * the second axis (minY, maxY, gridY). Marks the asset package dirty when
 * modifications are applied.
 *
 * @param BlendSpaceAsset Blend space or blend space base object to configure.
 *                       If null, the function is a no-op.
 * @param Payload JSON object containing axis configuration fields:
 *                - "minX", "maxX", "gridX" for axis 0 (required defaults:
 * 0,1,3)
 *                - "minY", "maxY", "gridY" for axis 1 when bTwoDimensional is
 * true
 * @param bTwoDimensional If true, the second axis is also configured.
 *
 * Notes:
 * - If the engine headers/types required to modify blend parameters are
 *   unavailable, the function logs and skips axis configuration.
 * - Grid values are clamped to a minimum of 1.
 */

void ApplyBlendSpaceConfiguration(UObject *BlendSpaceAsset,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         bool bTwoDimensional) {
  if (!BlendSpaceAsset || !Payload.IsValid()) {
    return;
  }

  double MinX = 0.0, MaxX = 1.0, GridX = 3.0;
  Payload->TryGetNumberField(TEXT("minX"), MinX);
  Payload->TryGetNumberField(TEXT("maxX"), MaxX);
  Payload->TryGetNumberField(TEXT("gridX"), GridX);

#if MCP_HAS_BLENDSPACE_BASE
  // UBlendSpaceBase is deprecated in UE 5.0+ but still needed for backward compatibility
  PRAGMA_DISABLE_DEPRECATION_WARNINGS
  if (UBlendSpaceBase *BlendBase = Cast<UBlendSpaceBase>(BlendSpaceAsset)) {
    BlendBase->Modify();

    FBlendParameter &Axis0 =
        const_cast<FBlendParameter &>(BlendBase->GetBlendParameter(0));
    Axis0.Min = static_cast<float>(MinX);
    Axis0.Max = static_cast<float>(MaxX);
    Axis0.GridNum = FMath::Max(1, static_cast<int32>(GridX));

    if (bTwoDimensional) {
      double MinY = 0.0, MaxY = 1.0, GridY = 3.0;
      Payload->TryGetNumberField(TEXT("minY"), MinY);
      Payload->TryGetNumberField(TEXT("maxY"), MaxY);
      Payload->TryGetNumberField(TEXT("gridY"), GridY);

      FBlendParameter &Axis1 =
          const_cast<FBlendParameter &>(BlendBase->GetBlendParameter(1));
      Axis1.Min = static_cast<float>(MinY);
      Axis1.Max = static_cast<float>(MaxY);
      Axis1.GridNum = FMath::Max(1, static_cast<int32>(GridY));
    }

    BlendBase->MarkPackageDirty();
  }
  PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("ApplyBlendSpaceConfiguration: BlendSpaceBase headers "
              "unavailable; skipping axis configuration."));
  if (bTwoDimensional) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Requested 2D blend space but BlendSpaceBase headers are "
                "missing; axis configuration skipped."));
  }
  if (!BlendSpaceAsset->IsA<UBlendSpace>() &&
      !BlendSpaceAsset->IsA<UBlendSpace1D>()) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("ApplyBlendSpaceConfiguration: Asset %s is not a BlendSpace type"),
        *BlendSpaceAsset->GetName());
  }
#endif
}
#endif
} // namespace McpAnimationHandlers
