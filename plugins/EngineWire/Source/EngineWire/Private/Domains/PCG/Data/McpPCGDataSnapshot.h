#pragma once

#include "Domains/PCG/Execution/McpPCGExecution.h"

#if MCP_HAS_PCG_EXECUTION
namespace McpPCGHandlers
{
// An immutable, bounded projection, not a live native dataset or a world-input freshness token.
// Callers must prove source membership, bound projection construction and cap retained handles separately.
class FPCGDataSnapshot
{
public:
    static constexpr int32 MaxRows = 256, MaxPageRows = 64, MaxCharacters = 262144;
    static constexpr int32 MaxRowCharacters = 16000, MaxPageCharacters = 60000;
    static constexpr double LifetimeSeconds = 60.0;
    ~FPCGDataSnapshot();
    bool ReadPage(const FString& Principal, UPCGComponent* Component, int32 Offset, int32 Limit,
        TArray<TSharedPtr<FJsonValue>>& OutRows, FString& OutError) const;
    int32 GetRowCount() const { return EncodedRows.Num(); }
    int64 GetCharacterCount() const { return EncodedCharacterCount; }
    double GetExpiresAt() const { return ExpiresAt; }
private:
    friend TSharedPtr<FPCGDataSnapshot> CreatePCGDataSnapshot(UPCGComponent*, const UPCGData*, const FString&,
        const TArray<TSharedPtr<FJsonValue>>&, FString&);
    FString Owner, ComponentPath, DataPath, GraphRevision, InstanceRevision;
    TWeakObjectPtr<UPCGComponent> Target;
    TWeakObjectPtr<const UPCGData> Source;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UPCGGraph> Graph;
    TArray<FString> EncodedRows;
    int64 EncodedCharacterCount = 0;
    double ExpiresAt = 0;
    TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Invalidated = MakeShared<std::atomic<bool>, ESPMode::ThreadSafe>(false);
    FDelegateHandle Started, Generated, Cleaned, Cancelled;
};
TSharedPtr<FPCGDataSnapshot> CreatePCGDataSnapshot(UPCGComponent* Component, const UPCGData* Data,
    const FString& Principal, const TArray<TSharedPtr<FJsonValue>>& Rows, FString& OutError);
}
#endif
