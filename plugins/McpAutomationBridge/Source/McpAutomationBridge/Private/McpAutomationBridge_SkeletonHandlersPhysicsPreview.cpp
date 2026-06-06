#include "McpAutomationBridge_SkeletonHandlersActions.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR

namespace McpSkeletonHandlers {

bool HandlePreviewPhysicsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
        // Also accept skeletonPath for backward compatibility
        if (SkeletalMeshPath.IsEmpty())
        {
            SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
        }
        bool bEnable = GetJsonBoolField(Payload, TEXT("enable"), true);

        if (SkeletalMeshPath.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath (or skeletonPath) is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        UObject* PreviewAsset = LoadObject<UObject>(nullptr, *SkeletalMeshPath);
        if (!PreviewAsset)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Skeletal mesh or skeleton not found: %s"), *SkeletalMeshPath),
                TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), PreviewAsset->GetPathName());
        Result->SetBoolField(TEXT("previewRequested"), bEnable);
        Result->SetBoolField(TEXT("assetVerified"), true);
        Result->SetStringField(TEXT("mode"), TEXT("editor_asset_verification"));
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            bEnable ? TEXT("Physics preview asset verified") : TEXT("Physics preview request disabled"),
            Result);
        return true;
}

} // namespace McpSkeletonHandlers

#endif // WITH_EDITOR
