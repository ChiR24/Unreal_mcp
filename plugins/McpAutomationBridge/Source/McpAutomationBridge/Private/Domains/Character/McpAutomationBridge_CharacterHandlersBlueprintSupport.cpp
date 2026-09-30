#include "Domains/Character/McpAutomationBridge_CharacterHandlers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"

namespace McpCharacterHandlers
{
UBlueprint* CreateCharacterBlueprintAsset(const FString& Path, const FString& Name, UClass* ParentClass, FString& OutError)
{
    const FString FullPath = Path / Name;
    if (!IsValidAssetPath(FullPath))
    {
        OutError = FString::Printf(TEXT("Invalid asset path: '%s'. Path must start with '/', cannot contain '..' or '//'."), *FullPath);
        return nullptr;
    }
    if (McpAssetExists(FullPath))
    {
        OutError = FString::Printf(TEXT("Asset already exists at path: %s"), *FullPath);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ParentClass ? ParentClass : ACharacter::StaticClass();
    UBlueprint* Blueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
        UBlueprint::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
    if (!Blueprint)
    {
        OutError = TEXT("Failed to create character blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint;
}

UBlueprint* LoadCharacterBlueprint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& BlueprintPath, FCharacterSocket Socket)
{
    if (BlueprintPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
    }
    return Blueprint;
}

}
