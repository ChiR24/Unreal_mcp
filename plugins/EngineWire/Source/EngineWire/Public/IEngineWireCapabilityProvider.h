#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Features/IModularFeature.h"

/**
 * One call of an extension's action. The provider answers it once, on the game thread, through Answer: success and a
 * message, the reply data, and on failure an error code (UPPER_SNAKE). The call has already passed the same gate as a
 * built-in one: the record's scope, consent, parameter schema and path rules.
 */
struct FMcpExtensionCall
{
    FString Action;
    TSharedPtr<FJsonObject> Params;
    TFunction<void(bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Data, const FString& ErrorCode)> Answer;
};

/**
 * A plugin that adds capabilities to EngineWire's `unreal` tool on the same path as the built-in ones: register an
 * implementation as a modular feature under FeatureName() before the editor finishes starting. Its records join
 * search, describe and execute only once its plugin name is listed in Project Settings > EngineWire > Trusted Extension
 * Providers; manage_tools list_extensions shows every provider found and, for one that did not join, why.
 */
class IEngineWireCapabilityProvider : public IModularFeature
{
public:
    static FName FeatureName() { return TEXT("EngineWireCapabilityProvider"); }

    /** The extension API this header describes; a provider built against another is refused. */
    static constexpr int32 CurrentApiVersion = 1;

    /** The provider's plugin name: the trust list names it, and its parent tools are ext_<name in lower case>. */
    virtual FName ProviderName() const = 0;

    /** CurrentApiVersion as the provider was built. */
    virtual int32 ApiVersion() const = 0;

    /** A JSON array of capability records in the generated shard shape ({"record": {...}}), as the built-ins carry. */
    virtual FString RecordsJson() const = 0;

    /** Runs one call of one of its records' actions (game thread). */
    virtual void Execute(const FMcpExtensionCall& Call) = 0;
};
