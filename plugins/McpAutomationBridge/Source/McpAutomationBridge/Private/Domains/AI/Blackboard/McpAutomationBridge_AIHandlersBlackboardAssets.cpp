#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BlackboardData.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace McpAIHandlers
{
static UBlackboardData* CreateBlackboardAsset(const FString& Path, const FString& Name, FString& OutError)
{
    UBlackboardData* Blackboard = CreateAIAssetInPackage<UBlackboardData>(
        Path, Name, TEXT("Blackboard"), OutError);
    if (Blackboard)
    {
        McpSafeAssetSave(Blackboard);
    }
    return Blackboard;
}

bool HandleCreateBlackboardAsset(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/Blackboards"));

    if (Name.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing name parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    FString Error;
    UBlackboardData* Blackboard = CreateBlackboardAsset(Path, Name, Error);
    if (!Blackboard)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    Result->SetStringField(TEXT("blackboardPath"), Blackboard->GetPathName());
    Result->SetStringField(TEXT("packagePath"), Blackboard->GetOutermost()->GetName()); // dogfood #66
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Blackboard: %s"), *Name));
    McpHandlerUtils::AddVerification(Result, Blackboard);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard created"), Result);
    return true;
}

bool HandleCreateBlackboard(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Redirect to existing create_blackboard_asset handler
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty())
    {
        Path = TEXT("/Game/AI/Blackboards");
    }

    FString AssetPath = Path / Name;
    FString SanitizedPath, SanitizeError;
    if (!SanitizeAIAssetPath(AssetPath, SanitizedPath, SanitizeError))
    {
        Self->SendAutomationError(RequestingSocket, RequestId, SanitizeError, TEXT("INVALID_PATH"));
        return true;
    }

    if (UEditorAssetLibrary::DoesAssetExist(SanitizedPath))
    {
        TSharedPtr<FJsonObject> ExistResult = McpHandlerUtils::CreateResultObject();
        // dogfood #66: object path like create_behavior_tree, plus the package path
        ExistResult->SetStringField(TEXT("blackboardPath"), SanitizedPath + TEXT(".") + FPaths::GetBaseFilename(SanitizedPath));
        ExistResult->SetStringField(TEXT("packagePath"), SanitizedPath);
        ExistResult->SetBoolField(TEXT("alreadyExisted"), true);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard already exists"), ExistResult);
        return true;
    }

    UBlackboardData* NewBB = NewObject<UBlackboardData>(CreatePackage(*SanitizedPath), *FPaths::GetBaseFilename(SanitizedPath), RF_Public | RF_Standalone);
    if (!NewBB)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create blackboard data asset"), TEXT("CREATION_FAILED"));
        return true;
    }

    McpSafeAssetSave(NewBB);

    TSharedPtr<FJsonObject> BBResult = McpHandlerUtils::CreateResultObject();
    BBResult->SetStringField(TEXT("blackboardPath"), NewBB->GetPathName()); // dogfood #66: object path
    BBResult->SetStringField(TEXT("packagePath"), SanitizedPath);
    BBResult->SetBoolField(TEXT("alreadyExisted"), false);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard created"), BBResult);
    return true;
}
}
#endif
