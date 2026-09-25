#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"
#include "Domains/SystemControl/McpAutomationBridge_SystemControlPackageJobs.h"

#include "Containers/Ticker.h"
#include "Core/Subsystem/McpAutomationBridgeSubsystemResponseSanitization.h"
#include "Dom/JsonObject.h"
#include "Domains/Log/McpAutomationBridge_LogHistory.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
namespace McpSystemControlHandlers {
namespace {

// A packaged build was only ever checked inside the editor: nothing in the tool
// could start the game package_project produced, so "it packaged" stood in for
// "it runs". This starts that game for a short smoke run and closes it again.

// The real game executable, not the bootstrap one at the archive root: killing
// the bootstrap would orphan the game it had started.
FString FindGameExecutable(const FString& ArchiveDir)
{
	const FString Project = FApp::GetProjectName();
	const FString BinDir = FPaths::Combine(ArchiveDir, TEXT("Windows"), Project, TEXT("Binaries"), TEXT("Win64"));
	const FString Exact = FPaths::Combine(BinDir, Project + TEXT(".exe"));
	if (FPaths::FileExists(Exact)) { return Exact; }
	// DebugGame and Shipping builds name it <Project>-Win64-<Configuration>.exe.
	TArray<FString> Found;
	IFileManager::Get().FindFiles(Found, *FPaths::Combine(BinDir, Project + TEXT("-Win64-*.exe")), true, false);
	return Found.Num() > 0 ? FPaths::Combine(BinDir, Found[0]) : FString();
}

// Closes the game at its deadline and records how the run ended. A game that
// quits or crashes before the deadline fails the smoke run.
bool TickLaunch(const FString& JobId)
{
	McpPackageJobs::FJob* Job = McpPackageJobs::Registry().Find(JobId);
	if (Job == nullptr || !Job->Result.IsEmpty()) { return false; }
	const double Elapsed = FPlatformTime::Seconds() - Job->StartedAtSeconds;
	if (FPlatformProcess::IsProcRunning(Job->Proc))
	{
		if (Elapsed < Job->RunSeconds) { return true; }
		FPlatformProcess::TerminateProc(Job->Proc, true);
		Job->Result = TEXT("Completed");
	}
	else
	{
		FPlatformProcess::GetProcReturnCode(Job->Proc, &Job->ExitCode);
		Job->Result = TEXT("ExitedEarly");
	}
	Job->DurationSeconds = Elapsed;
	FPlatformProcess::CloseProc(Job->Proc);
	return false;
}
}

bool HandleLaunchBuild(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, FSystemControlSocket RequestingSocket)
{
#if PLATFORM_WINDOWS
	FString ArchiveDir;
	Payload->TryGetStringField(TEXT("archiveDirectory"), ArchiveDir);
	ArchiveDir.TrimStartAndEndInline();
	if (ArchiveDir.IsEmpty()) { ArchiveDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Packaged")); }
	ArchiveDir = FPaths::ConvertRelativePathToFull(ArchiveDir);
	FPaths::NormalizeDirectoryName(ArchiveDir);
	// Only a build inside this project may run: an archive directory anywhere else
	// would let a caller start any executable that happens to carry its name.
	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	if (!FPaths::IsUnderDirectory(ArchiveDir, ProjectDir))
	{
		Self->SendAutomationError(RequestingSocket, RequestId,
			FString::Printf(TEXT("archiveDirectory must be inside the project (%s)."), *ProjectDir),
			TEXT("INVALID_ARGUMENT"));
		return true;
	}
	const FString Exe = FindGameExecutable(ArchiveDir);
	if (Exe.IsEmpty())
	{
		Self->SendAutomationError(RequestingSocket, RequestId,
			FString::Printf(TEXT("No packaged Win64 game under %s. Run package_project first."), *ArchiveDir),
			TEXT("BUILD_NOT_FOUND"));
		return true;
	}

	double Seconds = 20.0;
	Payload->TryGetNumberField(TEXT("seconds"), Seconds);
	Seconds = FMath::Clamp(Seconds, 5.0, 120.0);
	bool bWindowed = false;
	Payload->TryGetBoolField(TEXT("windowed"), bWindowed);

	const FString GameLog = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs"), TEXT("McpBuildRun.log")));
	IFileManager::Get().Delete(*GameLog, false, true, true);
	// Offscreen by default: no window appears and nobody's focus is taken.
	// No -FORCELOGFLUSH: a flush per line slowed startup several-fold, and an idle
	// title screen simply logs nothing more, which is not a truncated log.
	const FString Args = FString::Printf(TEXT("%s -unattended -nosplash -abslog=\"%s\""),
		bWindowed ? TEXT("-windowed -ResX=1280 -ResY=720") : TEXT("-RenderOffscreen"), *GameLog);

	McpPackageJobs::FJob Job;
	Job.bLaunch = true;
	Job.CommandLine = FString::Printf(TEXT("\"%s\" %s"), *Exe, *Args);
	Job.ArchiveDirectory = ArchiveDir;
	Job.Platform = TEXT("Win64");
	Job.GameLogPath = GameLog;
	Job.RunSeconds = Seconds;
	Job.StartedAtSeconds = FPlatformTime::Seconds();
	Job.Proc = FPlatformProcess::CreateProc(*Exe, *Args, true, false, false, nullptr, 0, nullptr, nullptr);
	if (!Job.Proc.IsValid())
	{
		Self->SendAutomationError(RequestingSocket, RequestId,
			FString::Printf(TEXT("Could not start %s."), *Exe), TEXT("LAUNCH_FAILED"));
		return true;
	}
	const FString JobId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	McpPackageJobs::Registry().Add(JobId, Job);
	// ponytail: a run still going when the editor closes keeps running; it is capped at 120 s.
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([JobId](float) { return TickLaunch(JobId); }), 0.5f);

	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetStringField(TEXT("jobId"), JobId);
	Result->SetStringField(TEXT("status"), TEXT("running"));
	Result->SetStringField(TEXT("executable"), Exe);
	Result->SetStringField(TEXT("gameLogPath"), GameLog);
	Result->SetNumberField(TEXT("seconds"), Seconds);
	Self->SendAutomationResponse(RequestingSocket, RequestId, true,
		FString::Printf(TEXT("Packaged game started for a %.0f s smoke run; poll package_status with this jobId."), Seconds),
		Result);
	return true;
#else
	Self->SendAutomationError(RequestingSocket, RequestId,
		TEXT("launch_build starts a Win64 build and needs a Windows editor."), TEXT("NOT_SUPPORTED"));
	return true;
#endif
}

void AppendLaunchStatus(const FString& GameLogPath, int32 ExitCode, bool bExitedEarly,
                        const TSharedPtr<FJsonObject>& Result)
{
	Result->SetStringField(TEXT("gameLogPath"), GameLogPath);
	if (bExitedEarly) { Result->SetNumberField(TEXT("exitCode"), ExitCode); }
	// The game may still hold its log open for writing.
	FString Text;
	FFileHelper::LoadFileToString(Text, *GameLogPath, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite);
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	int32 Errors = 0;
	// "LogLoad: Took 0.12 seconds to LoadMap(/Game/Maps/L_Main)" is the proof the
	// game reached a level rather than just initialising the engine.
	TArray<TSharedPtr<FJsonValue>> Maps;
	for (const FString& Line : Lines)
	{
		if (Line.Contains(TEXT(": Error: ")) || Line.Contains(TEXT("Fatal error"))) { ++Errors; }
		FString Map;
		if (Line.Split(TEXT("to LoadMap("), nullptr, &Map) && Map.Split(TEXT(")"), &Map, nullptr))
		{
			Maps.Add(MakeShared<FJsonValueString>(Map));
		}
	}
	Result->SetArrayField(TEXT("mapsLoaded"), Maps);
	TArray<TSharedPtr<FJsonValue>> Tail;
	for (int32 Index = FMath::Max(0, Lines.Num() - 30); Index < Lines.Num(); ++Index)
	{
		// Same redaction as read_log: the game's log carries host paths and account ids too.
		Tail.Add(MakeShared<FJsonValueString>(McpAutomationBridgeSubsystemResponse::SanitizeEngineErrorForResponse(
			FMcpLogHistory::KeepDiagnosticFileName(Lines[Index]))));
	}
	Result->SetNumberField(TEXT("errorCount"), Errors);
	Result->SetArrayField(TEXT("logTail"), Tail);
}
}
#endif
