#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Consent through the client (MCP 2026-07-28 multi round-trip requests). A call whose capability needs a consent
// grant and carries none asks the user with a form elicitation instead of being refused, and the retry carrying the
// user's yes becomes the grant describe would have minted. Older clients, and clients that cannot elicit, keep the
// describe consentGrant path.
//
// The requestState the client echoes back is signed with a key that lives only in this editor process, over the
// asking principal, the capability and a digest of the call's arguments, and it expires; its nonce becomes the
// grant's nonce, so the consent ledger lets one yes run the call once.
namespace McpModernConsent
{
struct FOutcome
{
    TSharedPtr<FJsonObject> Grant;  // the user said yes: authorize with this grant
    TSharedPtr<FJsonObject> Ask;    // answer this InputRequiredResult instead of refusing
    FString Declined;               // the user said no: refuse with this message
    bool bInvalidState = false;     // an echoed requestState that does not verify: refuse the request (-32602)
};

// The JSON-RPC error a request with an unverifiable requestState answers, worded as the TypeScript SDK words it.
FString InvalidStateError(const TSharedPtr<FJsonValue>& Id);

// Nothing set when the request is not a modern one or the client cannot be asked: refuse as before. WindowFolder, when
// not empty (FMcpConsentWindows::OfferFor), adds a second answer to the form: also allow this capability under that
// folder for a few calls or minutes. The folder is signed into the requestState, so an answer cannot widen it.
FOutcome Resolve(const FString& Principal, const FString& CapabilityId, const FString& ConsentMode,
                 const FString& Target, const TSharedPtr<FJsonObject>& Arguments, const FString& WindowFolder = FString());
} // namespace McpModernConsent
