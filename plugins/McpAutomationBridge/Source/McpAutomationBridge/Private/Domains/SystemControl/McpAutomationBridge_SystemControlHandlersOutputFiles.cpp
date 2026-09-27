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

}  // namespace

bool HandleListOutputFiles(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload,
                           FSystemControlSocket RequestingSocket) {
  const FString Project = ProjectRootFull();
  TArray<FString> Found;
  for (const TCHAR* Root : OutputRoots) {
    TArray<FString> InRoot;
    IFileManager::Get().FindFilesRecursive(InRoot, *(Project / Root), TEXT("*"), true, false);
    Found.Append(InRoot);
  }
  Found.Sort();

  TArray<TSharedPtr<FJsonValue>> Files;
  for (const FString& File : Found) {
    if (Files.Num() >= MaxListedOutputFiles) {
      break;
    }
    FString Relative = File;
    FPaths::MakePathRelativeTo(Relative, *(Project + TEXT("/")));
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("path"), Relative);
    Entry->SetNumberField(TEXT("sizeBytes"), static_cast<double>(IFileManager::Get().FileSize(*File)));
    Entry->SetStringField(TEXT("modified"), IFileManager::Get().GetTimeStamp(*File).ToIso8601());
    Files.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("files"), Files);
  Resp->SetNumberField(TEXT("count"), Found.Num());
  Resp->SetBoolField(TEXT("truncated"), Found.Num() > Files.Num());
  Resp->SetStringField(TEXT("roots"), RootsText());
  const FString Message = FString::Printf(TEXT("%d MCP output file(s)"), Found.Num());
  Self->SendAutomationResponse(RequestingSocket, RequestId, true, Message, Resp);
  return true;
}

bool HandleDeleteOutputFile(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload,
                            FSystemControlSocket RequestingSocket) {
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);
  Path.TrimStartAndEndInline();
  Path.ReplaceInline(TEXT("\\"), TEXT("/"));
  if (Path.IsEmpty() || Path.Contains(TEXT("..")) || FPaths::IsDrive(Path.Left(2)) ||
      Path.StartsWith(TEXT("/"))) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("`path` must be a project-relative file under %s, as list_output_files shows it."),
                        *RootsText()),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString Project = ProjectRootFull();
  FString Full = FPaths::ConvertRelativePathToFull(Project / Path);
  FPaths::NormalizeFilename(Full);
  FPaths::CollapseRelativeDirectories(Full);
  bool bInsideRoot = false;
  for (const TCHAR* Root : OutputRoots) {
    bInsideRoot |= Full.StartsWith(Project / Root + TEXT("/"), ESearchCase::IgnoreCase);
  }
  if (!bInsideRoot) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Only files the MCP writes can be deleted here (%s); %s is outside them."),
                        *RootsText(), *Path),
        TEXT("PATH_OUTSIDE_OUTPUT_ROOTS"));
    return true;
  }
  if (!IFileManager::Get().FileExists(*Full)) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              FString::Printf(TEXT("No file at %s; list_output_files shows what exists."), *Path),
                              TEXT("FILE_NOT_FOUND"));
    return true;
  }

  const bool bDeleted = IFileManager::Get().Delete(*Full, false, false, true);
  const bool bExistsAfter = IFileManager::Get().FileExists(*Full);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("path"), Path);
  Resp->SetBoolField(TEXT("deleted"), bDeleted && !bExistsAfter);
  Resp->SetBoolField(TEXT("existsAfter"), bExistsAfter);
  if (!bDeleted || bExistsAfter) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              FString::Printf(TEXT("Could not delete %s (read-only or in use)."), *Path),
                              TEXT("DELETE_FAILED"));
    return true;
  }
  Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               FString::Printf(TEXT("Deleted %s"), *Path), Resp);
  return true;
}

}  // namespace McpSystemControlHandlers
