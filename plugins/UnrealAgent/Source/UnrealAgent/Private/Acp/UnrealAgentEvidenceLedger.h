#pragma once

#include "CoreMinimal.h"

struct FUnrealAgentEvidenceSummary
{
    FString StatePath;
    FString DecisionsPath;
    FString EvidenceDirectory;
    FString LastSummary;
    int32 EvidenceCount = 0;
    bool bWritable = false;
};

class FUnrealAgentEvidenceLedger
{
public:
    static bool EnsureLedger(const FString& ProjectDirectory, FUnrealAgentEvidenceSummary* OutSummary = nullptr);
    static bool RecordEvent(const FString& ProjectDirectory, const FString& EventType, const FString& Status, const FString& Summary, const FString& Details, FString* OutEvidencePath = nullptr);
    static FUnrealAgentEvidenceSummary LoadSummary(const FString& ProjectDirectory);
};
