#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"

#include "Domains/Combat/McpCombatPrivate.h"

namespace McpCombatHandlers
{
// create_damage_type and setup_damage_type are one action: a DamageType Blueprint at path/name, compiled and saved.
// create_damage_type used to repeat the creation inline when CreateActorBlueprint failed; the copy failed for the same
// reasons, and only re-derived the refusal's code, which setup_damage_type never gave.
bool FCombatActionContext::HandleDamageTypes() const
{
    if (SubAction != TEXT("create_damage_type") && SubAction != TEXT("setup_damage_type"))
    {
        return false;
    }
    if (Name.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // CreateActorBlueprint refuses these too, under one code for every reason.
    const FString FullPath = Path / Name;
    if (!IsValidAssetPath(FullPath))
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid asset path: '%s'. Path must start with '/', cannot contain '..' or '//'."), *FullPath),
            TEXT("INVALID_PATH"));
        return true;
    }
    if (McpAssetExists(FullPath))
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Asset already exists at path: %s"), *FullPath),
            TEXT("ASSET_EXISTS"));
        return true;
    }

    FString Error;
    UBlueprint* Blueprint = CreateActorBlueprint(UDamageType::StaticClass(), Path, Name, Error);
    if (!Blueprint)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    McpSafeCompileBlueprint(Blueprint);
    if (!SaveOrReply(Blueprint))
    {
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("damageTypePath"), Blueprint->GetPathName());
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage type created successfully."), Result);
    return true;
}
}
