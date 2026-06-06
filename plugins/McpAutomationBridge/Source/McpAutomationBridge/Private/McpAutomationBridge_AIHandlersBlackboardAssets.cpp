#include "McpAutomationBridge_AIHandlerContext.h"

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
    // Sanitize and validate path first
    FString SanitizedPath;
    if (!SanitizeAIAssetPath(Path, SanitizedPath, OutError))
    {
        return nullptr;
    }

    FString FullPath = SanitizedPath / Name;

    // Check if asset already exists
    if (FindObject<UBlackboardData>(nullptr, *FullPath) != nullptr)
    {
        OutError = FString::Printf(TEXT("Asset already exists: %s"), *FullPath);
        return nullptr;
    }

    // Also check if the package exists
    if (FPackageName::DoesPackageExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Package already exists: %s"), *FullPath);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlackboardData* Blackboard = NewObject<UBlackboardData>(Package, UBlackboardData::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (!Blackboard)
    {
        OutError = TEXT("Failed to create Blackboard asset");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blackboard);
    McpSafeAssetSave(Blackboard);

    return Blackboard;
}

// Helper to create Behavior Tree asset

bool HandleCreateBlackboardAsset(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("create_blackboard_asset");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("create_blackboard_asset"))
    {
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/Blackboards"));

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
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Blackboard: %s"), *Name));
        McpHandlerUtils::AddVerification(Result, Blackboard);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard created"), Result);
        return true;
    }

    return true;
}

bool HandleCreateBlackboard(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("create_blackboard");
    if (SubAction == TEXT("create_blackboard"))
    {
        // Redirect to existing create_blackboard_asset handler
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        if (Name.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Path = GetStringFieldAI(Payload, TEXT("path"));
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
            ExistResult->SetStringField(TEXT("blackboardPath"), SanitizedPath);
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
        BBResult->SetStringField(TEXT("blackboardPath"), SanitizedPath);
        BBResult->SetBoolField(TEXT("alreadyExisted"), false);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard created"), BBResult);
        return true;
    }

    // Alias: setup_perception -> add_ai_perception_component (same logic)
    return true;
}
}
#endif
