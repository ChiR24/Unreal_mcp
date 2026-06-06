#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridge_AnimationHandlersBlendSpaceAssets.h"
#include "McpHandlerUtils.h"

#include "Animation/Skeleton.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateBlendSpaceAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Message = TEXT("name field required for blend space creation");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) ||
          SkeletonPath.IsEmpty()) {
        Message =
            TEXT("skeletonPath is required to bind blend space to a skeleton");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        USkeleton *TargetSkeleton =
            LoadObject<USkeleton>(nullptr, *SkeletonPath);
        if (!TargetSkeleton) {
          Message = TEXT("Failed to load skeleton for blend space");
          ErrorCode = TEXT("LOAD_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          int32 Dimensions = 1;
          double DimensionsNumber = 1.0;
          if (Payload->TryGetNumberField(TEXT("dimensions"),
                                         DimensionsNumber)) {
            Dimensions = static_cast<int32>(DimensionsNumber);
          }
          const bool bTwoDimensional = (Dimensions >= 2);

          // Validation for Issue #10
          double MinX = 0.0, MaxX = 1.0, GridX = 3.0;
          Payload->TryGetNumberField(TEXT("minX"), MinX);
          Payload->TryGetNumberField(TEXT("maxX"), MaxX);
          Payload->TryGetNumberField(TEXT("gridX"), GridX);

          if (MinX >= MaxX) {
            Message = TEXT("minX must be less than maxX");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
          } else if (GridX <= 0) {
            Message = TEXT("gridX must be greater than 0");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            if (bTwoDimensional) {
              double MinY = 0.0, MaxY = 1.0, GridY = 3.0;
              Payload->TryGetNumberField(TEXT("minY"), MinY);
              Payload->TryGetNumberField(TEXT("maxY"), MaxY);
              Payload->TryGetNumberField(TEXT("gridY"), GridY);

              if (MinY >= MaxY) {
                Message = TEXT("minY must be less than maxY");
                ErrorCode = TEXT("INVALID_ARGUMENT");
                Resp->SetStringField(TEXT("error"), Message);
                goto ValidationFailed;
              }
              if (GridY <= 0) {
                Message = TEXT("gridY must be greater than 0");
                ErrorCode = TEXT("INVALID_ARGUMENT");
                Resp->SetStringField(TEXT("error"), Message);
                goto ValidationFailed;
              }
            }

            FString FactoryError;
#if MCP_HAS_BLENDSPACE_FACTORY
            UObject *CreatedBlendAsset = CreateBlendSpaceAsset(
                Name, SavePath, TargetSkeleton, bTwoDimensional, FactoryError);
            if (CreatedBlendAsset) {
              ApplyBlendSpaceConfiguration(CreatedBlendAsset, Payload,
                                           bTwoDimensional);
#if MCP_HAS_BLENDSPACE_BASE
              // UBlendSpaceBase is deprecated in UE 5.0+ but still needed for backward compatibility
              PRAGMA_DISABLE_DEPRECATION_WARNINGS
              if (UBlendSpaceBase *BlendSpace =
                      Cast<UBlendSpaceBase>(CreatedBlendAsset)) {

                bSuccess = true;
                Message = TEXT("Blend space created successfully");
                Resp->SetStringField(TEXT("blendSpacePath"),
                                     BlendSpace->GetPathName());
                Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
                Resp->SetBoolField(TEXT("twoDimensional"), bTwoDimensional);
                McpHandlerUtils::AddVerification(Resp, BlendSpace);
              } else {
                Message =
                    TEXT("Created asset is not a BlendSpaceBase instance");
                ErrorCode = TEXT("TYPE_MISMATCH");
                Resp->SetStringField(TEXT("error"), Message);
              }
              PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else

              bSuccess = true;
              Message = TEXT("Blend space created (limited configuration)");
              Resp->SetStringField(TEXT("blendSpacePath"),
                                   CreatedBlendAsset->GetPathName());
              Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
              Resp->SetBoolField(TEXT("twoDimensional"), bTwoDimensional);
              Resp->SetStringField(TEXT("warning"),
                                   TEXT("BlendSpaceBase headers unavailable; "
                                        "axis configuration skipped."));
              McpHandlerUtils::AddVerification(Resp, CreatedBlendAsset);
#endif // MCP_HAS_BLENDSPACE_BASE
            } else {
              Message = FactoryError.IsEmpty()
                            ? TEXT("Failed to create blend space asset")
                            : FactoryError;
              ErrorCode = TEXT("ASSET_CREATION_FAILED");
              Resp->SetStringField(TEXT("error"), Message);
            }
#else
            Message = TEXT(
                "Blend space creation requires editor blend space factories");
            ErrorCode = TEXT("NOT_AVAILABLE");
            Resp->SetStringField(TEXT("error"), Message);
#endif
          } // End valid params

        ValidationFailed:;
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
