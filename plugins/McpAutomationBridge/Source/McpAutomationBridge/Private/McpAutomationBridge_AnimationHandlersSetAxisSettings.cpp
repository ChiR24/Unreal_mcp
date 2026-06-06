#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridge_AnimationHandlersBlendSpaceAssets.h"
#include "McpSafeOperations.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationSetAxisSettingsAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Set blend space axis settings
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    int32 AxisIndex = 0;
    double AxisIndexDouble = 0.0;
    if (Payload->TryGetNumberField(TEXT("axisIndex"), AxisIndexDouble)) {
      AxisIndex = static_cast<int32>(AxisIndexDouble);
    }

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for set_axis_settings");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
#if MCP_HAS_BLENDSPACE_BASE
      PRAGMA_DISABLE_DEPRECATION_WARNINGS
      UBlendSpaceBase *BlendSpace = LoadObject<UBlendSpaceBase>(nullptr, *AssetPath);
      PRAGMA_ENABLE_DEPRECATION_WARNINGS
      if (!BlendSpace) {
        Message = FString::Printf(TEXT("Blend space not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        BlendSpace->Modify();

        double MinValue = 0.0, MaxValue = 100.0;
        int32 GridNum = 4;
        FString AxisName;

        Payload->TryGetNumberField(TEXT("minValue"), MinValue);
        Payload->TryGetNumberField(TEXT("maxValue"), MaxValue);
        double GridNumDouble = 4.0;
        if (Payload->TryGetNumberField(TEXT("gridNum"), GridNumDouble)) {
          GridNum = FMath::Max(1, static_cast<int32>(GridNumDouble));
        }
        Payload->TryGetStringField(TEXT("axisName"), AxisName);

        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        FBlendParameter& Axis = const_cast<FBlendParameter&>(BlendSpace->GetBlendParameter(AxisIndex));
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        Axis.Min = static_cast<float>(MinValue);
        Axis.Max = static_cast<float>(MaxValue);
        Axis.GridNum = GridNum;
        if (!AxisName.IsEmpty()) {
          Axis.DisplayName = AxisName;
        }

        BlendSpace->MarkPackageDirty();
        McpSafeOperations::McpSafeAssetSave(BlendSpace);

        bSuccess = true;
        Message = FString::Printf(TEXT("Axis %d configured: [%.2f, %.2f] grid=%d"), AxisIndex, MinValue, MaxValue, GridNum);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetNumberField(TEXT("axisIndex"), AxisIndex);
        Resp->SetNumberField(TEXT("minValue"), MinValue);
        Resp->SetNumberField(TEXT("maxValue"), MaxValue);
        Resp->SetNumberField(TEXT("gridNum"), GridNum);
      }
#else
      Message = TEXT("BlendSpaceBase not available");
      ErrorCode = TEXT("NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
#endif
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
