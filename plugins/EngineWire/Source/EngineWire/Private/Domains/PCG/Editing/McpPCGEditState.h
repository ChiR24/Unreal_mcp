#pragma once

#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "UObject/StrongObjectPtr.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
class FPCGEditState
{
public:
    explicit FPCGEditState(UPCGGraph* InGraph);
    ~FPCGEditState();
    bool Restore();
    TArray<UObject*> GetObjects() const;
    const FString& GetRevisionBefore() const { return Revision; }
private:
    struct FRecord
    {
        explicit FRecord(UObject* InObject);
        TStrongObjectPtr<UObject> Object;
        UObject* Outer;
        FName Name;
        TArray<uint8> Bytes;
    };
    UPCGGraph* Graph;
    FString Revision;
    bool bWasDirty;
    TStrongObjectPtr<UObject> EditorRepresentation;
    TArray<TStrongObjectPtr<UObject>> EditorNodes;
    FString EditorFingerprint;
    TSet<UObject*> OriginalObjects;
    TArray<FRecord> Records;
};

bool HasOwnedPCGNodeSettings(UPCGGraph* Graph, UPCGNode* Node);
UPCGSettings* PreparePCGSettings(UPCGSettings* Source, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, FString& OutError);
bool ValidatePCGNodeSettings(UPCGGraph* Graph, UPCGNode* Node, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, FString& OutError);
bool ApplyPCGSettingsPayload(UPCGSettings* Settings, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, FString& OutError, int32& OutCount);
}
#endif
