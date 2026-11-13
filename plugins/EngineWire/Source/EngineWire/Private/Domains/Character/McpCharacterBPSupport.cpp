#include "Domains/Character/McpCharacterHandlers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPLoadOrReply.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Blueprints/McpNewBlueprint.h"

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

    UPackage* Package = McpCreatePackageOrNull(FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprint* Blueprint = McpNewBlueprint(Package, FName(*Name), ParentClass ? ParentClass : ACharacter::StaticClass());
    if (!Blueprint)
    {
        OutError = TEXT("Failed to create character blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint;
}

UBlueprint* LoadCharacterBlueprint(UEngineWireSubsystem* Self, const FString& RequestId, const FString& BlueprintPath, FCharacterSocket Socket)
{
    if (BlueprintPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }

    UBlueprint* Blueprint = McpLoadBlueprintOrReply(Self, Socket, RequestId, BlueprintPath);
    if (!Blueprint)
    {
        return nullptr;
    }
    // Every character action edits the Character defaults: on another Blueprint the movement, jump, rotation, mesh
    // and camera edits wrote nothing, then compiled, saved and answered "configured".
    const ACharacter* CharacterDefaults = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CharacterDefaults || !CharacterDefaults->GetCharacterMovement())
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s is not a Character Blueprint with a CharacterMovement "
            "component (its parent is %s); nothing was changed."), *BlueprintPath, Blueprint->ParentClass ? *Blueprint->ParentClass->GetName() : TEXT("none")),
            TEXT("NOT_A_CHARACTER"));
        return nullptr;
    }
    return Blueprint;
}

}
