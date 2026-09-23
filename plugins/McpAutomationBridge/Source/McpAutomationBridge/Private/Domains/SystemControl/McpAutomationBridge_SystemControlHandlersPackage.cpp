#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"
#include "Domains/SystemControl/McpAutomationBridge_SystemControlPackageJobs.h"

#include "Dom/JsonObject.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"

#if WITH_EDITOR
#include "IUATHelperModule.h"
#include "Styling/AppStyle.h"

namespace McpSystemControlHandlers {
namespace {

// Packaging is the one build step that had no capability at all, so callers had
// to leave the tool and run RunUAT from a shell. This drives the same
// IUATHelperModule::CreateUatTask the editor's own Package Project menu item
// uses, so the progress notification and log routing are the engine's.
const TCHAR* const AllowedPlatforms[] = { TEXT("Win64"), TEXT("Mac"), TEXT("Linux"), TEXT("LinuxArm64"), TEXT("Android"), TEXT("IOS") };
const TCHAR* const AllowedConfigs[] = { TEXT("Debug"), TEXT("DebugGame"), TEXT("Development"), TEXT("Test"), TEXT("Shipping") };

bool IsOneOf(const FString& Value, const TCHAR* const* Allowed, int32 Count)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Value.Equals(Allowed[Index], ESearchCase::IgnoreCase)) { return true; }
	}
	return false;
}

FString JoinAllowed(const TCHAR* const* Allowed, int32 Count)
{
	TArray<FString> Names;
	for (int32 Index = 0; Index < Count; ++Index) { Names.Add(Allowed[Index]); }
	return FString::Join(Names, TEXT(", "));
}

// A /Game map path may arrive as a package path or an object path; UAT wants the
// package path only, and refuses a quoted list with spaces in it.
bool CollectMaps(const TSharedPtr<FJsonObject>& Payload, FString& OutMaps, FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Payload->TryGetArrayField(TEXT("maps"), Values) || Values == nullptr) { return true; }
	TArray<FString> Packages;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString Path;
		if (!Value.IsValid() || !Value->TryGetString(Path)) { continue; }
		Path.TrimStartAndEndInline();
		if (Path.IsEmpty()) { continue; }
		int32 Dot = INDEX_NONE;
		if (Path.FindChar(TEXT('.'), Dot)) { Path.LeftInline(Dot); }
		if (!Path.StartsWith(TEXT("/Game/")))
		{
			OutError = FString::Printf(TEXT("maps entry '%s' is not under /Game."), *Path);
			return false;
		}
		if (Path.Contains(TEXT(" ")) || Path.Contains(TEXT("\"")))
		{
			OutError = FString::Printf(TEXT("maps entry '%s' contains a space or quote, which UAT cannot take."), *Path);
			return false;
		}
		Packages.Add(Path);
	}
	OutMaps = FString::Join(Packages, TEXT("+"));
	return true;
}

FString BuildCommandLine(const FString& ProjectPath, const FString& Platform, const FString& Configuration,
                         const FString& ArchiveDir, const FString& Maps, bool bPak, bool bBuild)
{
	FString Line = FString::Printf(
		TEXT("-ScriptsForProject=\"%s\" BuildCookRun -project=\"%s\" -noP4 -utf8output -nocompileeditor "
		     "-cook -stage -archive -archivedirectory=\"%s\" -package -targetplatform=%s -clientconfig=%s"),
		*ProjectPath, *ProjectPath, *ArchiveDir, *Platform, *Configuration);
	if (bBuild) { Line += TEXT(" -build"); }
	if (bPak) { Line += TEXT(" -pak"); }
	if (!Maps.IsEmpty()) { Line += FString::Printf(TEXT(" -map=%s"), *Maps); }
	return Line;
}
}

bool HandlePackageProject(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FSystemControlSocket RequestingSocket)
{
	const FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	if (ProjectPath.IsEmpty() || !FPaths::FileExists(ProjectPath))
	{
		Self->SendAutomationError(RequestingSocket, RequestId,
			TEXT("No .uproject on disk to package."), TEXT("PROJECT_NOT_FOUND"));
		return true;
	}

	FString Platform = TEXT("Win64");
	Payload->TryGetStringField(TEXT("platform"), Platform);
	Platform.TrimStartAndEndInline();
	if (!IsOneOf(Platform, AllowedPlatforms, UE_ARRAY_COUNT(AllowedPlatforms)))
	{
		Self->SendAutomationError(RequestingSocket, RequestId,
			FString::Printf(TEXT("platform '%s' is not allowed. Allowed: %s."), *Platform,
				*JoinAllowed(AllowedPlatforms, UE_ARRAY_COUNT(AllowedPlatforms))),
			TEXT("INVALID_ARGUMENT"));
		return true;
	}

	FString Configuration = TEXT("Development");
	Payload->TryGetStringField(TEXT("configuration"), Configuration);
	Configuration.TrimStartAndEndInline();
	if (!IsOneOf(Configuration, AllowedConfigs, UE_ARRAY_COUNT(AllowedConfigs)))
	{
		Self->SendAutomationError(RequestingSocket, RequestId,
			FString::Printf(TEXT("configuration '%s' is not allowed. Allowed: %s."), *Configuration,
				*JoinAllowed(AllowedConfigs, UE_ARRAY_COUNT(AllowedConfigs))),
			TEXT("INVALID_ARGUMENT"));
		return true;
	}

	FString ArchiveDir;
	Payload->TryGetStringField(TEXT("archiveDirectory"), ArchiveDir);
	ArchiveDir.TrimStartAndEndInline();
	if (ArchiveDir.IsEmpty())
	{
		ArchiveDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Packaged"));
	}
	ArchiveDir = FPaths::ConvertRelativePathToFull(ArchiveDir);
	if (ArchiveDir.Contains(TEXT("\"")))
	{
		Self->SendAutomationError(RequestingSocket, RequestId,
			TEXT("archiveDirectory contains a quote."), TEXT("INVALID_ARGUMENT"));
		return true;
	}

	FString Maps;
	FString MapError;
	if (!CollectMaps(Payload, Maps, MapError))
	{
		Self->SendAutomationError(RequestingSocket, RequestId, MapError, TEXT("INVALID_ARGUMENT"));
		return true;
	}

	bool bPak = true;
	Payload->TryGetBoolField(TEXT("pak"), bPak);
	bool bBuild = true;
	Payload->TryGetBoolField(TEXT("build"), bBuild);

	const FString CommandLine = BuildCommandLine(ProjectPath, Platform, Configuration, ArchiveDir, Maps, bPak, bBuild);
	const FString JobId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

	McpPackageJobs::FJob Job;
	Job.CommandLine = CommandLine;
	Job.ArchiveDirectory = ArchiveDir;
	Job.Platform = Platform;
	Job.Configuration = Configuration;
	Job.StartedAtSeconds = FPlatformTime::Seconds();
	McpPackageJobs::Registry().Add(JobId, Job);

	IUATHelperModule::Get().CreateUatTask(
		CommandLine, FText::FromString(Platform), FText::FromString(TEXT("Packaging project")),
		FText::FromString(TEXT("Package")), FAppStyle::Get().GetBrush(TEXT("MainFrame.PackageProject")),
		nullptr,
		[JobId](FString ResultType, double RuntimeSeconds)
		{
			if (McpPackageJobs::FJob* Tracked = McpPackageJobs::Registry().Find(JobId))
			{
				Tracked->Result = ResultType;
				Tracked->DurationSeconds = RuntimeSeconds;
			}
		},
		ArchiveDir);

	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetStringField(TEXT("jobId"), JobId);
	Result->SetStringField(TEXT("status"), TEXT("running"));
	Result->SetStringField(TEXT("platform"), Platform);
	Result->SetStringField(TEXT("configuration"), Configuration);
	Result->SetStringField(TEXT("archiveDirectory"), ArchiveDir);
	Result->SetStringField(TEXT("commandLine"), CommandLine);
	Self->SendAutomationResponse(RequestingSocket, RequestId, true,
		TEXT("Packaging started. It runs for many minutes; poll package_status with this jobId."),
		Result);
	return true;
}

bool HandlePackageStatus(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, FSystemControlSocket RequestingSocket)
{
	FString JobId;
	Payload->TryGetStringField(TEXT("jobId"), JobId);
	JobId.TrimStartAndEndInline();

	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	if (JobId.IsEmpty())
	{
		TArray<TSharedPtr<FJsonValue>> Ids;
		for (const TPair<FString, McpPackageJobs::FJob>& Pair : McpPackageJobs::Registry())
		{
			Ids.Add(MakeShared<FJsonValueString>(Pair.Key));
		}
		Result->SetArrayField(TEXT("jobIds"), Ids);
		Self->SendAutomationResponse(RequestingSocket, RequestId, true,
			FString::Printf(TEXT("%d packaging job(s) this session. Pass jobId for one."), Ids.Num()), Result);
		return true;
	}

	const McpPackageJobs::FJob* Job = McpPackageJobs::Registry().Find(JobId);
	if (Job == nullptr)
	{
		Self->SendAutomationError(RequestingSocket, RequestId,
			FString::Printf(TEXT("No packaging job '%s' in this editor session. Jobs do not survive a restart."), *JobId),
			TEXT("JOB_NOT_FOUND"));
		return true;
	}

	const FString Status = McpPackageJobs::StatusOf(*Job);
	Result->SetStringField(TEXT("jobId"), JobId);
	Result->SetStringField(TEXT("status"), Status);
	Result->SetStringField(TEXT("platform"), Job->Platform);
	Result->SetStringField(TEXT("configuration"), Job->Configuration);
	Result->SetStringField(TEXT("archiveDirectory"), Job->ArchiveDirectory);
	Result->SetStringField(TEXT("commandLine"), Job->CommandLine);
	Result->SetNumberField(TEXT("elapsedSeconds"),
		Status == TEXT("running") ? FPlatformTime::Seconds() - Job->StartedAtSeconds : Job->DurationSeconds);
	if (!Job->Result.IsEmpty()) { Result->SetStringField(TEXT("uatResult"), Job->Result); }
	// A failed pack leaves nothing to point at, so say where the log is rather
	// than letting the caller assume the archive directory holds an answer.
	Result->SetStringField(TEXT("logDirectory"),
		FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Logs"))));
	if (Job->bLaunch)
	{
		AppendLaunchStatus(Job->GameLogPath, Job->ExitCode, Job->Result == TEXT("ExitedEarly"), Result);
	}
	Self->SendAutomationResponse(RequestingSocket, RequestId, true,
		FString::Printf(TEXT("%s %s %s."), Job->bLaunch ? TEXT("Build run") : TEXT("Packaging job"), *JobId,
			Status == TEXT("running") ? TEXT("is running") : *Status), Result);
	return true;
}
}
#endif
