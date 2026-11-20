#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FMcpCapabilityDemand;
struct FMcpCapabilityPrincipal;

// Consent windows (opt-in: Project Settings > EngineWire > Allow Consent Windows, off by default). A user who allows one
// gated call through the client's consent form may also allow that same capability under that call's folder for a few
// more calls or minutes, whichever ends first; inside the window those calls need no describe grant. Never offered for
// elevated consent or Admin scope, and never on the WebSocket door. In memory only, so an editor restart closes every
// window; manage_tools list_consent_windows / revoke_consent_window read and close them. Thread-safe.
struct FMcpConsentWindow
{
    FString Id;
    FString Identity;     // the caller it was opened for (FMcpCapabilityPrincipal::Identity)
    FString CapabilityId; // the one capability it covers
    FString Prefix;       // a folder ending in '/': every path a covered call names must be under it
    int32 UsesLeft = 0;
    double ExpiresAtSeconds = 0.0; // FPlatformTime::Seconds() clock
    bool bPending = false;         // waiting for the user's Allow in the editor (bConfirmConsentWindowsInEditor)
};

struct FMcpConsentWindowUse
{
    FString Id; // empty: no window covered the call
    int32 UsesLeft = 0;
};

class FMcpConsentWindows
{
public:
    static FMcpConsentWindows& Get();

    /** The narrowest folder ("/Game/Sandbox/") holding every one of Paths; empty when there are none, when one is not
     *  canonical, or when they share nothing below a content root (a window over /Game/ itself is never offered). */
    static FString FolderOf(const TArray<FString>& Paths);

    /** The folder a consent form may offer a window over for this call; empty when windows are off, the consent is not
     *  explicit, the call needs Admin, or its paths (a complete strict scan) share no folder below a content root. */
    static FString OfferFor(const FMcpCapabilityDemand& Demand, const TSharedPtr<FJsonObject>& Arguments);

    /** Opens a window; its id. Uses and Minutes at least 1; the caller clamps them to the settings' caps. */
    FString Open(const FString& Identity, const FString& CapabilityId, const FString& Prefix, int32 Uses, int32 Minutes, bool bPending);

    /** The user's yes on a consent form that offered Prefix: a window with the settings' caps, pending until the user
     *  presses Allow on an editor notification when bConfirmConsentWindowsInEditor is on. */
    void OpenFromForm(const FString& Identity, const FString& CapabilityId, const FString& Prefix);

    /** The user allowed (or refused) a pending window in the editor. */
    void Settle(const FString& Id, bool bAllow);

    /** One use of a live window for Identity covering Demand's capability whose folder holds every one of Paths (at least
     *  one; each canonical, as the gate's strict scan gives them). A relative path, or one with "..", ':' or '\', is
     *  covered by none. */
    FMcpConsentWindowUse Take(const FString& Identity, const FMcpCapabilityDemand& Demand, const TArray<FString>& Paths);

    /** The gate's question: a window use standing in for the grant of this call. Only on the native door, for explicit
     *  consent, never for Admin scope, and only when a strict scan of the payload ran to completion. */
    FMcpConsentWindowUse TakeFor(const FMcpCapabilityPrincipal& Principal, const FMcpCapabilityDemand& Demand,
                                 const TSharedPtr<FJsonObject>& Payload, bool bNativeDoor);

    /** Closes the window with this id, or every window when Id is empty; how many closed. */
    int32 Revoke(const FString& Id);

    /** The live windows, pending ones included. */
    TArray<FMcpConsentWindow> List();

private:
    void DropDeadLocked();
    FCriticalSection Mutex;
    TArray<FMcpConsentWindow> Windows;
};

/** manage_tools list_consent_windows and revoke_consent_window. */
TSharedPtr<FJsonObject> McpConsentWindowsReply(const FString& Action, const TSharedPtr<FJsonObject>& Args);
