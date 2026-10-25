#include "Domains/AI/McpAIContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"

#include "AIController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
static UBlueprint* CreateAIControllerBlueprint(const FString& Path, const FString& Name, FString& OutError)
{
    // Sanitize and validate path first
    FString SanitizedPath;
    if (!SanitizeAIAssetPath(Path, SanitizedPath, OutError))
    {
        return nullptr;
    }

    FString FullPath = SanitizedPath / Name;

    // CRITICAL: Use UEditorAssetLibrary for reliable existence check
    // This prevents the Kismet2.cpp assertion failure when blueprint already exists
    // The FindObject check alone is insufficient because:
    // 1. It checks for exact object path format
    // 2. The package may exist even if the blueprint object doesn't
    if (McpAssetExists(FullPath))
    {
        OutError = FString::Printf(TEXT("Asset already exists: %s"), *FullPath);
        return nullptr;
    }

    // Also check with .AssetName suffix (blueprint object path format)
    FString ObjectPath = FullPath + TEXT(".") + Name;
    if (McpAssetExists(ObjectPath))
    {
        OutError = FString::Printf(TEXT("Blueprint already exists: %s"), *ObjectPath);
        return nullptr;
    }

    // Check if package exists on disk (for assets that haven't been loaded yet)
    if (FPackageName::DoesPackageExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Package already exists on disk: %s"), *FullPath);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    if (!Factory)
    {
        OutError = TEXT("Failed to create BlueprintFactory");
        return nullptr;
    }

    Factory->ParentClass = AAIController::StaticClass();

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name,
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (!Blueprint)
    {
        OutError = TEXT("Failed to create AI Controller blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    if (!McpSafeAssetSave(Blueprint))
    {
        Blueprint->MarkPackageDirty(); // unsaved, so save_all saves it and the reply can tell
    }

    return Blueprint;
}

// Implements the "create_ai_controller" action.
bool HandleCreateAIController(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/Controllers"));

    if (Name.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing name parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    FString Error;
    UBlueprint* Blueprint = CreateAIControllerBlueprint(Path, Name, Error);
    if (!Blueprint)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    Result->SetStringField(TEXT("controllerPath"), Blueprint->GetPathName());
    // The save's result was dropped, so a controller that never reached disk answered like a saved one.
    const bool bSaved = !Blueprint->GetOutermost()->IsDirty();
    Result->SetBoolField(TEXT("saved"), bSaved);
    if (!bSaved)
    {
        TArray<TSharedPtr<FJsonValue>> Warnings;
        Warnings.Add(MakeShared<FJsonValueString>(TEXT("Created in memory only: the save failed, so it is lost when the editor closes unless control_editor save_all saves it.")));
        Result->SetArrayField(TEXT("warnings"), Warnings);
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI Controller created"), Result);
    return true;
}

// assign_behavior_tree lives with the other recipe-built controller actions (McpAIControllerFocus.cpp).
}
