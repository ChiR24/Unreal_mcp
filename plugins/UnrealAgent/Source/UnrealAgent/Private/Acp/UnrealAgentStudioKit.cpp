#include "UnrealAgentStudioKit.h"

#include "UnrealAgentStudioKitPrivate.h"

FString FUnrealAgentStudioKit::GetStudioKitVersionMarker()
{
    return UnrealAgentStudioKit::StudioKitVersionMarker;
}

FString FUnrealAgentStudioKit::GetPromptVersionMarker()
{
    return UnrealAgentStudioKit::PromptVersionMarker;
}

FString FUnrealAgentStudioKit::BuildStatusSummary(const FUnrealAgentStudioKitResult& Result)
{
    return FString::Printf(
        TEXT("Studio Kit: %d written, %d preserved, %d failed"),
        Result.FilesWritten,
        Result.FilesPreserved,
        Result.FilesFailed);
}
