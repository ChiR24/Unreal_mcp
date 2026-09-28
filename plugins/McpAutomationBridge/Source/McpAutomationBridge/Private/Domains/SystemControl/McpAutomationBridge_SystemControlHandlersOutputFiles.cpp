#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "HAL/FileManager.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"

// Screenshots and environment snapshots are files the MCP writes itself, and
// nothing in the tool could remove them again: cleaning up after a check meant
// reaching into the project from a shell. These two actions reach exactly the
// folders the MCP writes into and nothing else.
namespace McpSystemControlHandlers {
namespace {

constexpr int32 DefaultListedOutputFiles = 100;
constexpr int32 MaxListedOutputFiles = 500;

FString ProjectRootFull() {
  FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
  FPaths::NormalizeDirectoryName(Root);
  return Root;
}

// The screenshot folder plus the three snapshot roots the snapshot path check
// accepts (EnvironmentSnapshotPaths), project-relative.
const TCHAR* const OutputRoots[] = {TEXT("Saved/Screenshots"), TEXT("Saved/unreal-mcp"),
                                    TEXT("tmp/unreal-mcp"), TEXT("temp/unreal-mcp")};

FString RootsText() {
  TArray<FString> Roots;
  for (const TCHAR* Root : OutputRoots) {
    Roots.Add(Root);
  }
  return FString::Join(Roots, TEXT(", "));
}

struct FMcpOutputFileEntry {
  FString Full;
  FDateTime Modified;
};

int32 ReadClampedInt(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, int32 Default, int32 Min,
                     int32 Max) {
  double Value = Default;
  if (Payload.IsValid()) {
    Payload->TryGetNumberField(Field, Value);
  }
  return static_cast<int32>(FMath::Clamp(Value, static_cast<double>(Min), static_cast<double>(Max)));
}

// Deletes one project-relative output file and describes the outcome. An
// empty ErrorCode on the returned entry means the file is gone.
TSharedPtr<FJsonObject> DeleteOneOutputFile(const FString& Project, FString Path, FString& OutErrorCode,
                                            FString& OutError) {
  Path.TrimStartAndEndInline();
  Path.ReplaceInline(TEXT("\\"), TEXT("/"));
  TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
  Entry->SetStringField(TEXT("path"), Path);
  Entry->SetBoolField(TEXT("deleted"), false);
  OutErrorCode.Reset();
  OutError.Reset();
  FString Full;
  if (Path.IsEmpty() || Path.Contains(TEXT("..")) || FPaths::IsDrive(Path.Left(2)) || Path.StartsWith(TEXT("/"))) {
    OutErrorCode = TEXT("INVALID_ARGUMENT");
    OutError = FString::Printf(TEXT("`%s` must be a project-relative file under %s, as list_output_files shows it."),
                               *Path, *RootsText());
  } else {
    Full = FPaths::ConvertRelativePathToFull(Project / Path);
    FPaths::NormalizeFilename(Full);
    FPaths::CollapseRelativeDirectories(Full);
    bool bInsideRoot = false;
    for (const TCHAR* Root : OutputRoots) {
      bInsideRoot |= Full.StartsWith(Project / Root + TEXT("/"), ESearchCase::IgnoreCase);
    }
    if (!bInsideRoot) {
      OutErrorCode = TEXT("PATH_OUTSIDE_OUTPUT_ROOTS");
      OutError = FString::Printf(TEXT("Only files the MCP writes can be deleted here (%s); %s is outside them."),
                                 *RootsText(), *Path);
    } else if (!IFileManager::Get().FileExists(*Full)) {
      OutErrorCode = TEXT("FILE_NOT_FOUND");
      OutError = FString::Printf(TEXT("No file at %s; list_output_files shows what exists."), *Path);
    } else {
      const bool bDeleted = IFileManager::Get().Delete(*Full, false, false, true);
      const bool bExistsAfter = IFileManager::Get().FileExists(*Full);
      Entry->SetBoolField(TEXT("deleted"), bDeleted && !bExistsAfter);
      Entry->SetBoolField(TEXT("existsAfter"), bExistsAfter);
      if (!bDeleted || bExistsAfter) {
        OutErrorCode = TEXT("DELETE_FAILED");
        OutError = FString::Printf(TEXT("Could not delete %s (read-only or in use)."), *Path);
      }
    }
  }
  if (!OutErrorCode.IsEmpty()) {
    Entry->SetStringField(TEXT("errorCode"), OutErrorCode);
    Entry->SetStringField(TEXT("error"), OutError);
  }
  return Entry;
}

}  // namespace

bool HandleListOutputFiles(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload,
                           FSystemControlSocket RequestingSocket) {
  const int32 Limit = ReadClampedInt(Payload, TEXT("limit"), DefaultListedOutputFiles, 1, MaxListedOutputFiles);
  const int32 Offset = ReadClampedInt(Payload, TEXT("offset"), 0, 0, MAX_int32);
  FString RootFilter;
  FString Extension;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("root"), RootFilter);
    Payload->TryGetStringField(TEXT("extension"), Extension);
  }
  RootFilter.TrimStartAndEndInline();
  RootFilter.ReplaceInline(TEXT("\\"), TEXT("/"));
  while (RootFilter.EndsWith(TEXT("/"))) {
    RootFilter.LeftChopInline(1);
  }
  Extension.TrimStartAndEndInline();
  while (Extension.StartsWith(TEXT("."))) {
    Extension.RightChopInline(1);
  }

  const FString Project = ProjectRootFull();
  TArray<FMcpOutputFileEntry> Found;
  bool bRootKnown = RootFilter.IsEmpty();
  for (const TCHAR* Root : OutputRoots) {
    if (!RootFilter.IsEmpty() && !RootFilter.Equals(Root, ESearchCase::IgnoreCase)) {
      continue;
    }
    bRootKnown = true;
    TArray<FString> InRoot;
    IFileManager::Get().FindFilesRecursive(InRoot, *(Project / Root), TEXT("*"), true, false);
    for (const FString& File : InRoot) {
      if (Extension.IsEmpty() || FPaths::GetExtension(File).Equals(Extension, ESearchCase::IgnoreCase)) {
        Found.Add({File, IFileManager::Get().GetTimeStamp(*File)});
      }
    }
  }
  if (!bRootKnown) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              FString::Printf(TEXT("Unknown root `%s`; use one of %s."), *RootFilter, *RootsText()),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // Newest first; the path breaks ties so paging is stable between calls.
  Found.Sort([](const FMcpOutputFileEntry& A, const FMcpOutputFileEntry& B) {
    return A.Modified != B.Modified ? A.Modified > B.Modified : A.Full < B.Full;
  });

  TArray<TSharedPtr<FJsonValue>> Files;
  for (int32 Index = Offset; Index < Found.Num() && Files.Num() < Limit; ++Index) {
    const FString& File = Found[Index].Full;
    FString Relative = File;
    FPaths::MakePathRelativeTo(Relative, *(Project + TEXT("/")));
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("path"), Relative);
    Entry->SetNumberField(TEXT("sizeBytes"), static_cast<double>(IFileManager::Get().FileSize(*File)));
    Entry->SetStringField(TEXT("modified"), Found[Index].Modified.ToIso8601());
    Files.Add(MakeShared<FJsonValueObject>(Entry));
  }

  const bool bHasMore = Offset + Files.Num() < Found.Num();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("files"), Files);
  Resp->SetNumberField(TEXT("total"), Found.Num());
  Resp->SetNumberField(TEXT("returned"), Files.Num());
  Resp->SetNumberField(TEXT("offset"), Offset);
  Resp->SetBoolField(TEXT("hasMore"), bHasMore);
  if (bHasMore) {
    Resp->SetNumberField(TEXT("nextOffset"), Offset + Files.Num());
  }
  Resp->SetStringField(TEXT("roots"), RootFilter.IsEmpty() ? RootsText() : RootFilter);
  const FString Message = FString::Printf(TEXT("%d of %d MCP output file(s), newest first"), Files.Num(), Found.Num());
  Self->SendAutomationResponse(RequestingSocket, RequestId, true, Message, Resp);
  return true;
}

bool HandleDeleteOutputFile(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload,
                            FSystemControlSocket RequestingSocket) {
  const FString Project = ProjectRootFull();
  const TArray<TSharedPtr<FJsonValue>>* PathValues = nullptr;
  if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("paths"), PathValues) || PathValues == nullptr) {
    FString Path;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("path"), Path);
    }
    FString ErrorCode;
    FString Error;
    TSharedPtr<FJsonObject> Resp = DeleteOneOutputFile(Project, Path, ErrorCode, Error);
    if (!ErrorCode.IsEmpty()) {
      Self->SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
      return true;
    }
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                                 FString::Printf(TEXT("Deleted %s"), *Resp->GetStringField(TEXT("path"))), Resp);
    return true;
  }

  if (PathValues->Num() == 0) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("`paths` is empty; pass the files list_output_files shows."),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }
  TArray<TSharedPtr<FJsonValue>> Results;
  int32 DeletedCount = 0;
  for (const TSharedPtr<FJsonValue>& Value : *PathValues) {
    FString Path;
    if (Value.IsValid()) {
      Value->TryGetString(Path);
    }
    FString ErrorCode;
    FString Error;
    Results.Add(MakeShared<FJsonValueObject>(DeleteOneOutputFile(Project, Path, ErrorCode, Error)));
    DeletedCount += ErrorCode.IsEmpty() ? 1 : 0;
  }
  const int32 FailedCount = Results.Num() - DeletedCount;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("results"), Results);
  Resp->SetNumberField(TEXT("deletedCount"), DeletedCount);
  Resp->SetNumberField(TEXT("failedCount"), FailedCount);
  const FString Message = FString::Printf(TEXT("Deleted %d of %d file(s)"), DeletedCount, Results.Num());
  // Any path that was not deleted fails the call; results[] says which and why.
  Self->SendAutomationResponse(RequestingSocket, RequestId, FailedCount == 0, Message, Resp,
                               FailedCount == 0 ? FString() : TEXT("PARTIAL_DELETE"));
  return true;
}

}  // namespace McpSystemControlHandlers
