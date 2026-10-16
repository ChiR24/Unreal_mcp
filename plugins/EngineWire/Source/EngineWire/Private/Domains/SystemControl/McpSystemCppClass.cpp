#include "Domains/SystemControl/McpSystemPrivate.h"

#include "AddToProjectConfig.h"
#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "GameProjectUtils.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "ModuleDescriptor.h"
#include "Modules/ModuleManager.h"

// create_cpp_class: the editor's New C++ Class, without its dialog. There was no way to add code to a project through
// the tool at all, so every C++ gameplay class meant leaving it for the editor's wizard or an IDE.
namespace McpSystemControlHandlers {
namespace {
bool IsCppIdentifier(const FString &Name) {
  if (Name.IsEmpty() || Name.Len() > 64 || FChar::IsDigit(Name[0])) {
    return false;
  }
  for (const TCHAR Char : Name) {
    if (!FChar::IsAlnum(Char) && Char != TEXT('_')) {
      return false;
    }
  }
  return true;
}

// A folder under the module, built of name characters: never absolute, never "..".
bool IsSafeSubfolder(const FString &Folder) {
  TArray<FString> Parts;
  Folder.ParseIntoArray(Parts, TEXT("/"));
  for (const FString &Part : Parts) {
    if (!IsCppIdentifier(Part.Replace(TEXT("-"), TEXT("_")))) {
      return false;
    }
  }
  return true;
}

const FModuleContextInfo *FindModule(const FString &Wanted) {
  const TArray<FModuleContextInfo> &Modules = GameProjectUtils::GetCurrentProjectModules();
  const FString Name = Wanted.IsEmpty() ? FString(FApp::GetProjectName()) : Wanted;
  for (const FModuleContextInfo &Module : Modules) {
    if (Module.ModuleName.Equals(Name, ESearchCase::IgnoreCase)) {
      return &Module;
    }
  }
  // No module named after the project: its first runtime module, when none was asked for.
  for (const FModuleContextInfo &Module : Modules) {
    if (Wanted.IsEmpty() && Module.ModuleType == EHostType::Runtime) {
      return &Module;
    }
  }
  return nullptr;
}

FString ModuleNames() {
  TArray<FString> Names;
  for (const FModuleContextInfo &Module : GameProjectUtils::GetCurrentProjectModules()) {
    Names.Add(Module.ModuleName);
  }
  return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : FString(TEXT("none"));
}
} // namespace

bool HandleCreateCppClass(UEngineWireSubsystem *Self, const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload, FSystemControlSocket Socket) {
  FString ClassName, ParentName, ModuleName, Subfolder, Location;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  Payload->TryGetStringField(TEXT("parentClass"), ParentName);
  Payload->TryGetStringField(TEXT("moduleName"), ModuleName);
  Payload->TryGetStringField(TEXT("subfolder"), Subfolder);
  Payload->TryGetStringField(TEXT("classLocation"), Location);
  Subfolder.ReplaceInline(TEXT("\\"), TEXT("/"));
  const auto Refuse = [Self, &Socket, &RequestId](const FString &Message, const TCHAR *Code) {
    Self->SendAutomationError(Socket, RequestId, Message, Code);
    return true;
  };
  if (!IsCppIdentifier(ClassName)) {
    return Refuse(FString::Printf(TEXT("className '%s' is not a C++ class name: letters, digits and _, not starting "
                                       "with a digit, without the prefix (MyActor makes AMyActor)."), *ClassName),
                  TEXT("INVALID_ARGUMENT"));
  }
  if (!IsSafeSubfolder(Subfolder)) {
    return Refuse(FString::Printf(TEXT("subfolder '%s' must be a relative folder of names (Gameplay/Pickups), with "
                                       "no '..' or drive."), *Subfolder), TEXT("INVALID_ARGUMENT"));
  }
  if (!Location.IsEmpty() && !Location.Equals(TEXT("public"), ESearchCase::IgnoreCase) &&
      !Location.Equals(TEXT("private"), ESearchCase::IgnoreCase)) {
    return Refuse(FString::Printf(TEXT("classLocation '%s' is public or private."), *Location), TEXT("INVALID_ARGUMENT"));
  }
  const FModuleContextInfo *Found = FindModule(ModuleName);
  if (!Found) {
    return Refuse(FString::Printf(TEXT("The project has no module '%s'. Its modules: %s."), *ModuleName, *ModuleNames()),
                  TEXT("MODULE_NOT_FOUND"));
  }
  // A copy: adding a class that needs another module dependency rewrites and reloads the project file, which empties
  // the engine's module list Found points into, and the call then read freed strings.
  const FModuleContextInfo Module = *Found;

  // "None" (or no parent) is a plain C++ class; anything else is a native class to derive from.
  FNewClassInfo Parent(FNewClassInfo::EClassType::EmptyCpp);
  const UClass *ParentClass = nullptr;
  if (!ParentName.IsEmpty() && !ParentName.Equals(TEXT("None"), ESearchCase::IgnoreCase)) {
    ParentClass = ResolveClassByName(ParentName);
    if (!ParentClass) {
      return Refuse(FString::Printf(TEXT("parentClass '%s' names no class (Actor, Character, ActorComponent, Object, "
                                         "/Script/Engine.Pawn ...; None for a plain C++ class)."), *ParentName),
                    TEXT("CLASS_NOT_FOUND"));
    }
    if (!ParentClass->HasAnyClassFlags(CLASS_Native)) {
      return Refuse(FString::Printf(TEXT("%s is a Blueprint class; a C++ class derives from a C++ class (its "
                                         "Blueprint's parent, such as %s)."), *ParentClass->GetName(),
                                    *GetNameSafe(ParentClass->GetSuperClass())), TEXT("INVALID_PARENT_CLASS"));
    }
    if (!GameProjectUtils::IsValidBaseClassForCreation(ParentClass, Module)) {
      return Refuse(FString::Printf(TEXT("%s cannot be a parent in module %s: its module does not export it, or it "
                                         "is not meant to be subclassed."), *ParentClass->GetName(), *Module.ModuleName),
                    TEXT("INVALID_PARENT_CLASS"));
    }
    Parent = FNewClassInfo(ParentClass);
  }

  // Public/<subfolder> puts the header in Public and the .cpp in Private, as the wizard does; private keeps both in
  // Private. A module laid out without those folders takes both files in <subfolder>.
  const FString Root = Module.ModuleSourcePath;
  const bool bSplit = IFileManager::Get().DirectoryExists(*(Root / TEXT("Public"))) ||
                      IFileManager::Get().DirectoryExists(*(Root / TEXT("Private")));
  // The engine writes the new .cpp's include relative to the module folder (#include "Sub/X.h"), and that folder is no
  // include path under current build settings: a class in a subfolder of such a module did not compile.
  if (!bSplit && !Subfolder.IsEmpty()) {
    return Refuse(FString::Printf(TEXT("Module %s keeps its sources in one folder (no Public or Private), which is not an "
                                       "include path, so a class in subfolder '%s' would not compile. Leave subfolder "
                                       "out, or give the module Public and Private folders."), *Module.ModuleName,
                                  *Subfolder), TEXT("INVALID_ARGUMENT"));
  }
  const FString Area = !bSplit ? FString() : Location.Equals(TEXT("private"), ESearchCase::IgnoreCase) ? TEXT("Private")
                                                                                                         : TEXT("Public");
  const FString ClassPath = Area.IsEmpty() && Subfolder.IsEmpty() ? Root : Root / Area / Subfolder / TEXT("");
  const bool bHadCode = GameProjectUtils::ProjectHasCodeFiles();

  FString HeaderPath, CppPath;
  FText FailReason;
  GameProjectUtils::EReloadStatus Reload = GameProjectUtils::EReloadStatus::NotReloaded;
  GameProjectUtils::EAddCodeToProjectResult Result;
  {
    // The wizard's message boxes (a first class under Live Coding asks for a restart) would block the game thread,
    // and with it every request after this one.
    TGuardValue<bool> Unattended(GIsRunningUnattendedScript, true);
    Result = GameProjectUtils::AddCodeToProject(ClassName, ClassPath, Module, Parent, TSet<FString>(), HeaderPath,
                                                CppPath, FailReason, Reload);
  }
  if (Result == GameProjectUtils::EAddCodeToProjectResult::InvalidInput ||
      Result == GameProjectUtils::EAddCodeToProjectResult::FailedToAddCode) {
    return Refuse(FailReason.ToString(), Result == GameProjectUtils::EAddCodeToProjectResult::InvalidInput
                                             ? TEXT("INVALID_ARGUMENT") : TEXT("CREATE_FAILED"));
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  const FString Prefix = ParentClass ? FString(ParentClass->GetPrefixCPP()) : FString();
  Data->SetStringField(TEXT("className"), Prefix + ClassName);
  Data->SetStringField(TEXT("headerPath"), FPaths::ConvertRelativePathToFull(HeaderPath));
  Data->SetStringField(TEXT("cppPath"), FPaths::ConvertRelativePathToFull(CppPath));
  Data->SetStringField(TEXT("moduleName"), Module.ModuleName);
  if (ParentClass) {
    Data->SetStringField(TEXT("classPath"), FString::Printf(TEXT("/Script/%s.%s"), *Module.ModuleName, *ClassName));
  }
  const bool bCompiled = Reload == GameProjectUtils::EReloadStatus::Reloaded;
  Data->SetBoolField(TEXT("compiled"), bCompiled);
  // Live Coding compiles in its own process after this answers, and may still fail.
  const bool bLiveCoding = FModuleManager::Get().IsModuleLoaded(TEXT("LiveCoding"));
  const FString Note =
      !bHadCode ? TEXT("The project had no C++ module: one was made for it. Close the editor and build the editor "
                       "target (run_ubt <Project>Editor) before the class exists.")
      : !bCompiled ? TEXT("Not compiled yet: build the module (LiveCoding.Compile, or run_ubt with the editor closed) "
                          "before the class can be used.")
      : bLiveCoding ? TEXT("Live Coding started compiling it and runs on after this reply: read_log category "
                           "LogLiveCoding says whether it succeeded. The patch lasts until the next full build.")
                    : TEXT("The editor compiled and reloaded the module.");
  Data->SetStringField(TEXT("note"), Note);
  if (Result == GameProjectUtils::EAddCodeToProjectResult::FailedToHotReload) {
    // The files are written; only the compile failed.
    Self->SendAutomationResponse(Socket, RequestId, false,
                                 FString::Printf(TEXT("Created %s, but it did not compile: %s"), *(Prefix + ClassName),
                                                 *FailReason.ToString()), Data, TEXT("COMPILE_FAILED"));
    return true;
  }
  Self->SendAutomationResponse(Socket, RequestId, true,
                               FString::Printf(TEXT("Created %s in %s"), *(Prefix + ClassName), *Module.ModuleName),
                               Data, FString());
  return true;
}
} // namespace McpSystemControlHandlers
