#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"
#include "McpVersionCompatibility.h"

#include "Animation/AnimSequence.h"
#if __has_include("AnimData/IAnimationDataController.h")
#include "AnimData/IAnimationDataController.h"
#endif
#if __has_include("Animation/AnimData/CurveIdentifier.h")
#include "Animation/AnimData/CurveIdentifier.h"
#endif

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationSetCurveKeyAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Set an animation curve key
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString CurveName;
    Payload->TryGetStringField(TEXT("curveName"), CurveName);

    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);

    double Value = 0.0;
    Payload->TryGetNumberField(TEXT("value"), Value);

    if (AssetPath.IsEmpty() || CurveName.IsEmpty()) {
      Message = TEXT("assetPath and curveName required for set_curve_key");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimSequence *AnimSeq = LoadObject<UAnimSequence>(nullptr, *AssetPath);
      if (!AnimSeq) {
        Message = FString::Printf(TEXT("Animation sequence not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        AnimSeq->Modify();

#if WITH_EDITOR
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        // UE 5.3+: FAnimationCurveIdentifier takes FName directly
        FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1-5.2: FAnimationCurveIdentifier takes FSmartName
        FSmartName SmartCurveName;
        SmartCurveName.DisplayName = FName(*CurveName);
        FAnimationCurveIdentifier CurveId(SmartCurveName, ERawCurveTrackTypes::RCT_Float);
#else
        // UE 5.0: Curve editing API not available in the same form
        Message = TEXT("set_curve_key requires UE 5.1+");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: GetController() returns IAnimationDataController& (reference)
        IAnimationDataController& Controller = AnimSeq->GetController();

        // Add curve if it doesn't exist
        Controller.AddCurve(CurveId, AACF_DefaultCurve);

        // Add key to curve
        Controller.SetCurveKey(CurveId, FRichCurveKey(static_cast<float>(Time), static_cast<float>(Value)));
        // Success - set the response
        bSuccess = true;
        Message = FString::Printf(TEXT("Curve key set for '%s' at %.2fs = %.2f"), *CurveName, Time, Value);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("curveName"), CurveName);
        Resp->SetNumberField(TEXT("time"), Time);
        Resp->SetNumberField(TEXT("value"), Value);
#endif

#else
        Message = TEXT("set_curve_key requires editor build");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif
        if (bSuccess) {
          AnimSeq->MarkPackageDirty();
          McpSafeOperations::McpSafeAssetSave(AnimSeq);
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
