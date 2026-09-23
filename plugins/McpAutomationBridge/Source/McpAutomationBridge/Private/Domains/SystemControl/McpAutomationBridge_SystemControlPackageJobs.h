#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

// Packaging runs for tens of minutes, far past any request timeout, so
// package_project cannot answer with the result the way run_ubt does. It starts
// the UAT task, hands back a jobId, and package_status reports on it later.
// Every handler runs on the game thread, so this registry needs no lock.
namespace McpPackageJobs
{
struct FJob
{
	FString CommandLine;
	FString ArchiveDirectory;
	FString Platform;
	FString Configuration;
	double StartedAtSeconds = 0.0;
	double DurationSeconds = 0.0;
	// Empty while the task is still running; UAT's own result word once it is
	// done ("Completed", "Failed", "Canceled", ...). A launch_build run ends
	// "Completed" when the game outlived its run, "ExitedEarly" when it did not.
	FString Result;

	// launch_build only: the packaged game this job runs and where it logs.
	bool bLaunch = false;
	FProcHandle Proc;
	FString GameLogPath;
	double RunSeconds = 0.0;
	int32 ExitCode = 0;
};

inline TMap<FString, FJob>& Registry()
{
	static TMap<FString, FJob> Jobs;
	return Jobs;
}

inline FString StatusOf(const FJob& Job)
{
	if (Job.Result.IsEmpty()) { return TEXT("running"); }
	return Job.Result.Equals(TEXT("Completed"), ESearchCase::IgnoreCase) ? TEXT("succeeded") : TEXT("failed");
}
}
