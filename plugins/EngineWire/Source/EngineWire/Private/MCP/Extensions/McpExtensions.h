#pragma once

#include "CoreMinimal.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

class IEngineWireCapabilityProvider;

// Item 19: other plugins' capabilities on the built-in path (IEngineWireCapabilityProvider.h). The capability store's
// one build collects them after the generated shards, so search, describe and execute see one catalog; each provider
// joins whole or not at all.
namespace McpExtensions
{
struct FProviderEntry
{
    FString Name;
    int32 ApiVersion = 0;
    int32 RecordCount = 0;
    bool bJoined = false;
    FString Refusal; // why it did not join
    TArray<FString> Parents;
    IEngineWireCapabilityProvider* Provider = nullptr;
};

struct FCollected
{
    TArray<FMcpCapabilityRecord> Records; // of the providers that joined
    TArray<FProviderEntry> Providers;
};

/** Each provider's records: trusted by plugin name, built for this API, every record parsing with an effect and a
 *  scope (and consent when destructive), on a parent tool named ext_<name in lower case>..., under an id no built-in
 *  or earlier provider has. A provider failing any of it adds nothing, and its entry says why. */
FCollected Collect(const TArray<IEngineWireCapabilityProvider*>& Providers, const TArray<FString>& Trusted,
                   const TSet<FString>& TakenIds);

/** Collect over the registered providers and the trust setting, once, on the store's build (BuiltinIds: its ids). */
const FCollected& Get(const TSet<FString>& BuiltinIds);

/** Every provider found, joined or not (builds the store first). */
const TArray<FProviderEntry>& Providers();

/** manage_tools list_extensions: each provider found, whether it joined, its parent tools and records, and why not. */
TSharedPtr<FJsonObject> ListReply();

/** Put each joined provider's parent tools in the tool registry, before the native transport starts reading it: the
 *  gateway refuses a call whose parent is not registered (UNKNOWN_TOOL), and only registered tools can be enabled. */
void RegisterParentTools();
} // namespace McpExtensions
