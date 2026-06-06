#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleDamageTypes() const
{
    if (SubAction == TEXT("create_damage_type"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateActorBlueprint(UDamageType::StaticClass(), Path, Name, Error);
        if (!Blueprint)
        {
            // Try creating as UObject-based blueprint
            FString FullPath = Path / Name;

            // Validate path before fallback CreatePackage
            if (!IsValidAssetPath(FullPath))
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Invalid asset path: '%s'. Path must start with '/', cannot contain '..' or '//'."), *FullPath),
                    TEXT("INVALID_PATH"));
                return true;
            }

            // Check if asset already exists
            if (UEditorAssetLibrary::DoesAssetExist(FullPath))
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Asset already exists at path: %s"), *FullPath),
                    TEXT("ASSET_EXISTS"));
                return true;
            }

            UPackage* Package = CreatePackage(*FullPath);
            if (!Package)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create damage type package."), TEXT("CREATION_FAILED"));
                return true;
            }

            UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
            Factory->ParentClass = UDamageType::StaticClass();

            Blueprint = Cast<UBlueprint>(
                Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                          RF_Public | RF_Standalone, nullptr, GWarn));

            if (!Blueprint)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create damage type blueprint."), TEXT("CREATION_FAILED"));
                return true;
            }

            FAssetRegistryModule::AssetCreated(Blueprint);
            Blueprint->MarkPackageDirty();
        }

        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("damageTypePath"), Blueprint->GetPathName());

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage type created successfully."), Result);
        return true;
    }

    // configure_damage_execution

    if (SubAction == TEXT("setup_damage_type"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateActorBlueprint(UDamageType::StaticClass(), Path, Name, Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error.IsEmpty() ? TEXT("Failed to create damage type.") : Error, TEXT("CREATION_FAILED"));
            return true;
        }

        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("damageTypePath"), Blueprint->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage type created successfully."), Result);
        return true;
    }

    // configure_hit_detection -> alias for setup_hitbox_component

    return false;
}
#endif
}
