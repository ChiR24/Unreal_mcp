#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"

#include "Foundation/Diagnostics/McpDiagnosticsSnapshotFileNames.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshotSchema.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool RotationHasRecordedEvents(const FMcpDiagnosticsSnapshotState& State)
{
	return State.Requests > 0 || State.Refusals > 0 || State.bHasRequest
		|| State.bHasHandshake || State.bHasDisconnect || State.bHasSession;
}
}

void FMcpDiagnosticsSnapshot::RotateOnStartup()
{
	FScopeLock Lock(&Mutex);
	if (DiagnosticsRoot().IsEmpty() || !EnsureDiagnosticsDirectory())
	{
		return;
	}
	RecoverTempFor(McpDiagnosticsSnapshotFileNames::CurrentFileName(), McpDiagnosticsSnapshotFileNames::CurrentTempName());
	RecoverTempFor(McpDiagnosticsSnapshotFileNames::PreviousFileName(), McpDiagnosticsSnapshotFileNames::PreviousTempName());
	RemoveSurvivingTemps();

	FString Content;
	FMcpDiagnosticsSnapshotState Loaded;
	const bool bCurrentValid = LoadAndValidateFile(McpDiagnosticsSnapshotFileNames::CurrentFileName(), Content, Loaded);
	if (bCurrentValid && RotationHasRecordedEvents(Loaded))
	{
		WriteFileAtomic(McpDiagnosticsSnapshotFileNames::PreviousFileName(), McpDiagnosticsSnapshotFileNames::PreviousTempName(), McpDiagnosticsSchema::SerializeState(Loaded, false));
	}

	FString PreviousContent;
	FMcpDiagnosticsSnapshotState PreviousFileState;
	bHasPrevious = LoadAndValidateFile(McpDiagnosticsSnapshotFileNames::PreviousFileName(), PreviousContent, PreviousFileState);
	if (bHasPrevious)
	{
		PreviousState = MoveTemp(PreviousFileState);
	}

	InitializeFreshCurrent();
}

void FMcpDiagnosticsSnapshot::RecoverTempFor(const FString& TargetName, const FString& TempName)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TempPath = FPaths::Combine(DiagnosticsRoot(), TempName);
	const FString TargetPath = FPaths::Combine(DiagnosticsRoot(), TargetName);
	if (!PlatformFile.FileExists(*TempPath))
	{
		return;
	}
	FString Content;
	FMcpDiagnosticsSnapshotState Loaded;
	const bool bTempValid = LoadAndValidateFile(TempName, Content, Loaded);
	const bool bTargetValid = PlatformFile.FileExists(*TargetPath)
		&& LoadAndValidateFile(TargetName, Content, Loaded);
	if (bTempValid && !bTargetValid)
	{
		PlatformFile.MoveFile(*TargetPath, *TempPath);
	}
	else
	{
		PlatformFile.DeleteFile(*TempPath);
	}
}

void FMcpDiagnosticsSnapshot::RemoveSurvivingTemps()
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteFile(*FPaths::Combine(DiagnosticsRoot(), TEXT("current-session.json.tmp")));
	PlatformFile.DeleteFile(*FPaths::Combine(DiagnosticsRoot(), TEXT("previous-session.json.tmp")));
}
