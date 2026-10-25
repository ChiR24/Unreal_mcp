#include "Domains/AI/McpAIContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BlackboardData.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace McpAIHandlers
{
// create_blackboard and create_blackboard_asset. A Blackboard already at path/name is kept as it is
// (alreadyExisted); any other asset there is refused, where create_blackboard used to answer "already
// exists" for it and create_blackboard_asset rebuilt an asset that was only in memory.
static bool CreateBlackboardAndReply(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty())
    {
        Path = TEXT("/Game/AI/Blackboards");
    }
    if (Name.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("name is required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SanitizedPath;
    FString Error;
    if (!SanitizeAIAssetPath(Path / Name, SanitizedPath, Error))
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_PATH"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (UBlackboardData* Existing = Cast<UBlackboardData>(McpLoadAsset(SanitizedPath)))
    {
        Result->SetStringField(TEXT("blackboardPath"), Existing->GetPathName());
        Result->SetStringField(TEXT("packagePath"), SanitizedPath);
        Result->SetBoolField(TEXT("alreadyExisted"), true);
        McpHandlerUtils::MarkNoAssetsChanged(Result);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard already exists; nothing was changed"), Result);
        return true;
    }

    FString ErrorCode;
    UBlackboardData* Blackboard = CreateAIAssetInPackage<UBlackboardData>(Path, Name, TEXT("Blackboard"), Error, ErrorCode);
    if (!Blackboard)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
        return true;
    }

    Result->SetStringField(TEXT("blackboardPath"), Blackboard->GetPathName()); // dogfood #66: object path
    Result->SetStringField(TEXT("packagePath"), Blackboard->GetOutermost()->GetName());
    Result->SetBoolField(TEXT("alreadyExisted"), false);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blackboard));
    McpHandlerUtils::AddVerification(Result, Blackboard);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard created"), Result);
    return true;
}

bool HandleCreateBlackboardAsset(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return CreateBlackboardAndReply(Self, RequestId, Payload, RequestingSocket);
}

bool HandleCreateBlackboard(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return CreateBlackboardAndReply(Self, RequestId, Payload, RequestingSocket);
}
}
