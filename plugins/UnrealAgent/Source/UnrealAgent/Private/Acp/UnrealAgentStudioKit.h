#pragma once

#include "CoreMinimal.h"

struct FUnrealAgentStudioKitResult
{
    int32 FilesWritten = 0;
    int32 FilesPreserved = 0;
    int32 FilesFailed = 0;
    TArray<FString> WrittenPaths;
    TArray<FString> PreservedPaths;
    TArray<FString> FailedPaths;
    FString Summary;

    bool WasSuccessful() const
    {
        return FilesFailed == 0;
    }
};

class FUnrealAgentStudioKit
{
public:
    static FString GetStudioKitVersionMarker();
    static FString GetPromptVersionMarker();
    static FString MakePrimaryAgentMarkdown();
    static FUnrealAgentStudioKitResult EnsureForProject(const FString& ProjectDirectory);
    static FString RedactSensitiveText(const FString& Text);
    static bool IsManagedFileText(const FString& Text);
    static FString BuildStatusSummary(const FUnrealAgentStudioKitResult& Result);
};
