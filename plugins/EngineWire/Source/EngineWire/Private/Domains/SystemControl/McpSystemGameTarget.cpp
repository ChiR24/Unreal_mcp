#include "Domains/SystemControl/McpSystemPrivate.h"

#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// A packaged game always took the project's name (its launcher was <Project>.exe): the launcher, the binaries and an
// Android package are named after the game target that was built, and a project has one, named after itself. A release
// wants its own name, so package_project executableName adds a game target of that name, a copy of the project's own.
namespace McpSystemControlHandlers {
namespace {
bool IsTargetName(const FString &Name) {
  if (Name.IsEmpty() || Name.Len() > 64 || !FChar::IsAlpha(Name[0])) {
    return false;
  }
  for (const TCHAR Char : Name) {
    if (!FChar::IsAlnum(Char) && Char != TEXT('_')) {
      return false;
    }
  }
  return true;
}

bool IsGameTarget(const FString &Text) { return Text.Contains(TEXT("TargetType.Game")); }

// The project's game target: <Project>.Target.cs, else the first target file that builds TargetType.Game.
FString FindTemplateTarget(const FString &SourceDir) {
  FString Text;
  const FString Own = FPaths::Combine(SourceDir, FString(FApp::GetProjectName()) + TEXT(".Target.cs"));
  if (FFileHelper::LoadFileToString(Text, *Own) && IsGameTarget(Text)) {
    return Own;
  }
  TArray<FString> Files;
  IFileManager::Get().FindFiles(Files, *FPaths::Combine(SourceDir, TEXT("*.Target.cs")), true, false);
  for (const FString &File : Files) {
    const FString Path = FPaths::Combine(SourceDir, File);
    if (FFileHelper::LoadFileToString(Text, *Path) && IsGameTarget(Text)) {
      return Path;
    }
  }
  return FString();
}
} // namespace

bool McpEnsureGameTarget(const FString &Name, FString &OutFile, bool &bOutCreated, FString &OutError,
                         FString &OutCode) {
  bOutCreated = false;
  OutCode = TEXT("TARGET_UNAVAILABLE");
  if (!IsTargetName(Name)) {
    OutCode = TEXT("INVALID_ARGUMENT");
    OutError = FString::Printf(
        TEXT("executableName '%s' must be letters, digits and underscores, starting with a letter (at most 64)."), *Name);
    return false;
  }
  const FString SourceDir = FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir());
  OutFile = FPaths::Combine(SourceDir, Name + TEXT(".Target.cs"));
  FString Text;
  if (FPaths::FileExists(OutFile)) {
    if (FFileHelper::LoadFileToString(Text, *OutFile) && IsGameTarget(Text)) {
      return true;
    }
    OutError = FString::Printf(TEXT("%s exists and is not a game target: an editor or server target is not the game."),
                               *OutFile);
    return false;
  }
  const FString Template = FindTemplateTarget(SourceDir);
  if (Template.IsEmpty() || !FFileHelper::LoadFileToString(Text, *Template)) {
    OutError = FString::Printf(TEXT("No game target under %s to copy: a project without C++ code is packaged under its "
                                    "own name."),
                               *SourceDir);
    return false;
  }
  // GetBaseFilename("X.Target.cs") is "X.Target", and that file declares class XTarget.
  const FString OldClass = FPaths::GetBaseFilename(Template).Replace(TEXT(".Target"), TEXT("")) + TEXT("Target");
  if (!Text.Contains(OldClass, ESearchCase::CaseSensitive)) {
    OutError = FString::Printf(TEXT("%s does not declare %s, so it cannot be copied under a new name."), *Template,
                               *OldClass);
    return false;
  }
  Text.ReplaceInline(*OldClass, *(Name + TEXT("Target")), ESearchCase::CaseSensitive);
  if (!FFileHelper::SaveStringToFile(Text, *OutFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
    OutError = FString::Printf(TEXT("Could not write %s."), *OutFile);
    return false;
  }
  bOutCreated = true;
  return true;
}
} // namespace McpSystemControlHandlers
