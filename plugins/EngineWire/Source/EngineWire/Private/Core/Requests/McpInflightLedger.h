// McpInflightLedger.h — the calls this editor is running, kept in Saved/MCP/inflight.json so that when the editor dies
// the stdio proxy can say which call it was running (src/proxy/endpoint.ts reads the file by this editor's pid).
//
// An entry is written before its handler runs (phase dispatch), turns latent when the handler returns without having
// replied (the call answers later: a playtest, a sample run), and leaves the file when its reply is sent. Entries hold
// the call's tool and action only, never its parameters.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

class FMcpInflightLedger
{
public:
    /** The editor's own ledger, in Saved/MCP. */
    static FMcpInflightLedger& Get();

    /** A ledger whose files go to Dir (tests use their own). */
    explicit FMcpInflightLedger(const FString& InDir) : Dir(InDir) {}

    /** Before a request's handler runs; on disk before it returns. Capability is "tool.action". */
    void Dispatch(const FString& RequestId, const FString& Capability);

    /** After the handler returned: a call still listed has not replied, so it waits for a later reply. */
    void Wait(const FString& RequestId);

    /** The call answered (any outcome): it leaves the file. */
    void Done(const FString& RequestId);

    /**
     * At startup, before the first call: entries another editor process left in inflight.json (it ended while running
     * them) move into last-crash.json with that process's id and, when the crash reporter wrote one, its report; then
     * inflight.json starts empty. Returns how many moved.
     */
    int32 Start();

    /** At a clean shutdown: no entries, so nothing reads as running. */
    void Stop();

    /**
     * Inside a request's error capture: when the last editor of this project ended while running this capability, a
     * warning says so on this call's reply (once per capability per editor run); the call still runs.
     */
    void WarnIfEndedIn(const FString& Capability);

private:
    struct FEntry
    {
        FString RequestId;
        FString Capability;
        FString StartedAtUtc;
        bool bLatent = false;
    };

    void WriteLocked() const;

    const FString Dir;
    FCriticalSection Mutex;
    TArray<FEntry> Entries; // oldest first: the call dispatched last is the one the game thread was in
    TSet<FString> EndedIn;  // what the last editor was running when it ended (Start), each warned about once
    FString EndedHow;       // its crash report's type and message, or that it left none
};
