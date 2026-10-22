#pragma once

#include "Domains/PCG/Data/McpPCGDataSnapshot.h"

#if MCP_HAS_PCG_EXECUTION
namespace McpPCGHandlers
{
// A weak root-slot locator. It proves current membership, not native content immutability or input freshness.
class FPCGRootDataView
{
public:
    static constexpr int32 MaxDatasets = 256, MaxTags = 32, MaxTagCharacters = 1024, MaxTagTotalCharacters = 2048;
    const UPCGData* Resolve(const FString& Principal, UPCGComponent* Component, FString& OutError) const;
    TSharedPtr<FJsonObject> Describe() const;
    double GetExpiresAt() const { return Guard.IsValid() ? Guard->GetExpiresAt() : 0; }
    int32 GetSlot() const { return Slot; }
private:
    friend TSharedPtr<FPCGRootDataView> CreatePCGRootDataView(UPCGComponent*, const FString&, int32, FString&);
    TWeakObjectPtr<const UPCGData> Source;
    TSharedPtr<FPCGDataSnapshot> Guard;
    int32 Slot = INDEX_NONE;
    FName Pin;
    TSet<FString> Tags;
    FString DataClass, Kind;
    int32 PointCount = 0;
};
TSharedPtr<FPCGRootDataView> CreatePCGRootDataView(UPCGComponent* Component, const FString& Principal, int32 Slot, FString& OutError);
}
#endif
