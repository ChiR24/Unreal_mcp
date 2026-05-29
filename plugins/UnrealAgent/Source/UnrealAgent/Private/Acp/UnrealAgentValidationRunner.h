#pragma once

#include "CoreMinimal.h"

struct FUnrealAgentValidationResult
{
    bool bPassed = true;
    FString Summary;
    FString EvidencePath;
    TArray<FString> Checks;
    TArray<FString> Warnings;
    TArray<FString> Errors;
};

class FUnrealAgentValidationRunner
{
public:
    static FUnrealAgentValidationResult RunFastValidation(const FString& ProjectDirectory);
    static FString FormatForTranscript(const FUnrealAgentValidationResult& Result);
};
