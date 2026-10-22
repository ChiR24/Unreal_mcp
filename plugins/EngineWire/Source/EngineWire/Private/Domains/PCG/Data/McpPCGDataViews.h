#pragma once

#include "Domains/PCG/Data/McpPCGRootData.h"

#if MCP_HAS_PCG_EXECUTION
namespace McpPCGHandlers
{
// Owned by the bridge subsystem, never a global dataset service or scheduler.
class FPCGDataViewStore
{
public:
    static constexpr int32 MaxHandles = 64, MaxOwnerHandles = 16;
    static constexpr int64 MaxCharacters = 1048576, MaxOwnerCharacters = 262144;
    static constexpr int32 MaxDescriptionCharacters = 16000;
    FString RetainRoot(UPCGComponent* Component, const FString& Principal, int32 Slot, FString& OutError);
    TSharedPtr<FPCGRootDataView> ResolveRoot(const FString& Id, UPCGComponent* Component, const FString& Principal, FString& OutError);
    // Rows must be synchronously constructed by the typed adapter for this exact root, never supplied by a client.
    FString RetainRows(const FString& RootId, UPCGComponent* Component, const FString& Principal,
        const TArray<TSharedPtr<FJsonValue>>& Rows, FString& OutError, const FString& EncodedDescription = FString());
    bool DescribeRows(const FString& Id, UPCGComponent* Component, const FString& Principal,
        TSharedPtr<FJsonObject>& OutDescription, double& OutRemainingSeconds, FString& OutError);
    bool ReadRows(const FString& Id, UPCGComponent* Component, const FString& Principal, int32 Offset, int32 Limit,
        TArray<TSharedPtr<FJsonValue>>& OutRows, int32& OutTotal, FString& OutError);
    bool Release(const FString& Id, UPCGComponent* Component, const FString& Principal);
    void Purge();
    int32 GetHandleCount() const { return Entries.Num(); }
    int64 GetCharacterCount() const;
private:
    struct FEntry
    {
        FString Owner, EncodedDescription;
        TWeakObjectPtr<UPCGComponent> Component;
        TSharedPtr<FPCGRootDataView> Root;
        TSharedPtr<FPCGDataSnapshot> Rows;
        double ExpiresAt = 0;
    };
    TMap<FString, FEntry> Entries;
    FEntry* Find(const FString& Id, UPCGComponent* Component, const FString& Principal, FString& OutError);
    bool CheckCapacity(const FString& Principal, int64 Characters, FString& OutError) const;
};
}
#endif
