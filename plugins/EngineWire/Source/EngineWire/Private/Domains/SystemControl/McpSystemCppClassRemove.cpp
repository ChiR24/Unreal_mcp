#include "Domains/SystemControl/McpSystemPrivate.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EngineWireSubsystem.h"
#include "GameFramework/Actor.h"
#include "GameProjectUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ModuleDescriptor.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

// remove_cpp_class: the way back out for a class create_cpp_class added. Without it a class made to try the tool stayed
// in the project's C++ Classes until someone deleted its files by hand, outside the tool.
namespace McpSystemControlHandlers {
namespace {
constexpr int32 MaxUsersListed = 20;
constexpr int32 MaxPackagesSearched = 50;

// The class's files in a module: <Name>.h and <Name>.cpp anywhere under its source folder, as full paths.
void ClassFiles(const FModuleContextInfo& Module, const FString& Name, TArray<FString>& OutHeaders, TArray<FString>& OutCpps) {
  IFileManager::Get().FindFilesRecursive(OutHeaders, *Module.ModuleSourcePath, *(Name + TEXT(".h")), true, false);
  IFileManager::Get().FindFilesRecursive(OutCpps, *Module.ModuleSourcePath, *(Name + TEXT(".cpp")), true, false);
  for (FString& File : OutHeaders) File = FPaths::ConvertRelativePathToFull(File);
  for (FString& File : OutCpps) File = FPaths::ConvertRelativePathToFull(File);
}

int32 CountOf(const FString& Text, const TCHAR* Needle) {
  int32 Count = 0;
  for (int32 At = Text.Find(Needle); At != INDEX_NONE; At = Text.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1)) {
    ++Count;
  }
  return Count;
}

// Word stands in Text as a whole identifier (AMyActor, not AMyActorBase).
bool NamesWord(const FString& Text, const FString& Word) {
  for (int32 At = Text.Find(Word, ESearchCase::CaseSensitive); At != INDEX_NONE;
       At = Text.Find(Word, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1)) {
    const int32 End = At + Word.Len();
    const bool bStart = At == 0 || !(FChar::IsAlnum(Text[At - 1]) || Text[At - 1] == TEXT('_'));
    const bool bEnd = End >= Text.Len() || !(FChar::IsAlnum(Text[End]) || Text[End] == TEXT('_'));
    if (bStart && bEnd) return true;
  }
  return false;
}

FString Describe(const UObject* Object) {
  const AActor* Actor = Cast<AActor>(Object);
  return Actor ? FString::Printf(TEXT("actor %s in %s"), *Actor->GetActorLabel(), *GetNameSafe(Actor->GetOutermost()))
               : FString::Printf(TEXT("object %s"), *Object->GetPathName());
}

// What the engine can see using Class: objects of it (placed actors, components), classes derived from it (native or
// Blueprint, loaded), and Blueprints on disk built on it, read from their ParentClass and NativeParentClass tags.
void EngineUsers(UClass* Class, IAssetRegistry& Registry, TArray<FString>& Users) {
  TArray<UObject*> Objects;
  GetObjectsOfClass(Class, Objects, true, RF_ClassDefaultObject | RF_ArchetypeObject);
  for (const UObject* Object : Objects) {
    if (IsValid(Object)) Users.AddUnique(Describe(Object));
  }
  TArray<UClass*> Derived;
  GetDerivedClasses(Class, Derived, true);
  for (const UClass* Child : Derived) {
    const FString Name = Child->GetName();
    if (!Child->HasAnyClassFlags(CLASS_NewerVersionExists) && !Name.StartsWith(TEXT("SKEL_")) && !Name.StartsWith(TEXT("REINST_"))) {
      Users.AddUnique(FString::Printf(TEXT("class %s derives from it"), *Child->GetPathName()));
    }
  }
  const FString Needle = Class->GetPathName() + TEXT("'"); // a tag value reads /Script/CoreUObject.Class'/Script/M.Name'
  TArray<FAssetData> Assets;
  Registry.GetAllAssets(Assets, true);
  for (const FAssetData& Asset : Assets) {
    FString Parent, NativeParent;
    Asset.GetTagValue(FName(TEXT("ParentClass")), Parent);
    Asset.GetTagValue(FName(TEXT("NativeParentClass")), NativeParent);
    if (Parent.Contains(Needle) || NativeParent.Contains(Needle)) {
      Users.AddUnique(FString::Printf(TEXT("Blueprint %s is built on it"), *Asset.PackageName.ToString()));
    }
  }
}

// Packages on disk that use the class's module (the registry knows the module a package imports from, not which of
// its types), loaded and searched for anything that points at the class or its defaults: a placed copy, a component,
// a cast node, a variable of its type. False with OutError when there are too many to search in one call.
bool PackageUsers(UClass* Class, const FString& ModuleName, IAssetRegistry& Registry, TArray<FString>& Users, FString& OutError) {
  TArray<FName> Referencers;
  Registry.GetReferencers(FName(*(TEXT("/Script/") + ModuleName)), Referencers);
  Referencers.RemoveAll([](const FName& Name) { return Name.ToString().StartsWith(TEXT("/Script/")); });
  if (Referencers.Num() > MaxPackagesSearched) {
    OutError = FString::Printf(TEXT("%d packages use module %s; searching more than %d of them for this class in one call "
                                    "is refused. Remove what uses the class first, or give the class a module of its own."),
                               Referencers.Num(), *ModuleName, MaxPackagesSearched);
    return false;
  }
  const UObject* Defaults = Class->GetDefaultObject(false);
  for (const FName& Name : Referencers) {
    UPackage* Package = FindPackage(nullptr, *Name.ToString());
    Package = Package ? Package : LoadPackage(nullptr, *Name.ToString(), LOAD_NoWarn | LOAD_Quiet);
    if (!Package) {
      Users.AddUnique(FString::Printf(TEXT("%s uses module %s and did not load, so it could not be searched"), *Name.ToString(), *ModuleName));
      continue;
    }
    // Two arguments: nested objects are included by default in every 5.x, and 5.8 deprecates the bool form.
    bool bUses = false;
    ForEachObjectWithPackage(Package, [&](UObject* Object) {
      TArray<UObject*> Referenced;
      FReferenceFinder Finder(Referenced);
      Finder.FindReferences(Object);
      bUses = Object->IsA(Class) || Referenced.Contains(Class) || (Defaults && Referenced.Contains(Defaults));
      return !bUses;
    });
    if (bUses) Users.AddUnique(FString::Printf(TEXT("%s points at it"), *Name.ToString()));
  }
  return true;
}

// Source files of the project's modules, besides the class's own, that include its header or name it.
void SourceUsers(const FString& Name, const FString& Prefixed, const TArray<FString>& Own, TArray<FString>& Users) {
  for (const FModuleContextInfo& Module : GameProjectUtils::GetCurrentProjectModules()) {
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *Module.ModuleSourcePath, TEXT("*.h"), true, false);
    IFileManager::Get().FindFilesRecursive(Files, *Module.ModuleSourcePath, TEXT("*.cpp"), true, false, false);
    for (const FString& File : Files) {
      FString Text;
      if (!Own.Contains(FPaths::ConvertRelativePathToFull(File)) && FFileHelper::LoadFileToString(Text, *File) &&
          (Text.Contains(TEXT("/") + Name + TEXT(".h\"")) || Text.Contains(TEXT("\"") + Name + TEXT(".h\"")) || NamesWord(Text, Prefixed))) {
        Users.AddUnique(FString::Printf(TEXT("%s includes or names it"), *FPaths::ConvertRelativePathToFull(File)));
      }
    }
  }
}

TArray<TSharedPtr<FJsonValue>> ToJsonStrings(const TArray<FString>& Values) {
  TArray<TSharedPtr<FJsonValue>> Out;
  for (const FString& Value : Values) Out.Add(MakeShared<FJsonValueString>(Value));
  return Out;
}
} // namespace

bool HandleRemoveCppClass(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                          FSystemControlSocket Socket) {
  FString Name, ModuleName;
  Payload->TryGetStringField(TEXT("className"), Name);
  Payload->TryGetStringField(TEXT("moduleName"), ModuleName);
  const auto Refuse = [Self, &Socket, &RequestId](const FString& Message, const TCHAR* Code) {
    Self->SendAutomationError(Socket, RequestId, Message, Code);
    return true;
  };
  // /Script/<Module>.<Name> names both.
  FString PathModule, PathName;
  if (Name.StartsWith(TEXT("/Script/")) && Name.Mid(8).Split(TEXT("."), &PathModule, &PathName)) {
    ModuleName = ModuleName.IsEmpty() ? PathModule : ModuleName;
    Name = PathName;
  }
  if (!McpIsCppIdentifier(Name)) {
    return Refuse(FString::Printf(TEXT("className '%s' is not a C++ class name (MyActor, AMyActor or /Script/<Module>.MyActor)."), *Name),
                  TEXT("INVALID_ARGUMENT"));
  }
  TArray<FModuleContextInfo> Modules;
  if (!ModuleName.IsEmpty()) {
    const FModuleContextInfo* Found = McpFindProjectModule(ModuleName);
    if (!Found) {
      return Refuse(FString::Printf(TEXT("The project has no module '%s'. Its modules: %s."), *ModuleName, *McpProjectModuleNames()),
                    TEXT("MODULE_NOT_FOUND"));
    }
    Modules.Add(*Found);
  } else {
    Modules = GameProjectUtils::GetCurrentProjectModules();
  }
  // Its files are named without the prefix (AMyActor lives in MyActor.h): try the name as given, then without one.
  TArray<FString> Candidates = {Name};
  if (Name.Len() > 2 && FChar::IsUpper(Name[0]) && FChar::IsUpper(Name[1]) && FString(TEXT("AUFIES")).Contains(Name.Left(1))) {
    Candidates.Add(Name.Mid(1));
  }
  const FModuleContextInfo* Module = nullptr;
  FString BaseName;
  TArray<FString> Headers, Cpps;
  for (const FString& Candidate : Candidates) {
    for (const FModuleContextInfo& Each : Modules) {
      TArray<FString> FoundHeaders, FoundCpps;
      ClassFiles(Each, Candidate, FoundHeaders, FoundCpps);
      if (FoundHeaders.Num() + FoundCpps.Num() == 0) continue;
      if (Module) {
        return Refuse(FString::Printf(TEXT("Modules %s and %s both have files named %s: name the module with moduleName."),
                                      *Module->ModuleName, *Each.ModuleName, *Candidate), TEXT("AMBIGUOUS_CLASS"));
      }
      Module = &Each;
      BaseName = Candidate;
      Headers = FoundHeaders;
      Cpps = FoundCpps;
    }
    if (Module) break;
  }
  if (!Module) {
    return Refuse(FString::Printf(TEXT("No project module (%s) has a file named %s.h or .cpp. Only classes in the project's "
                                       "own Source can be removed here, not engine or plugin classes."),
                                  *McpProjectModuleNames(), *FString::Join(Candidates, TEXT(".h or .cpp, or "))), TEXT("CLASS_NOT_FOUND"));
  }
  if (Headers.Num() > 1 || Cpps.Num() > 1) {
    return Refuse(FString::Printf(TEXT("Module %s has more than one file named %s (%s): which belongs to the class is not "
                                       "clear, so nothing was removed."), *Module->ModuleName, *BaseName,
                                  *FString::Join(Headers.Num() > 1 ? Headers : Cpps, TEXT(", "))), TEXT("AMBIGUOUS_CLASS"));
  }
  // A header holding other types too would take them with it.
  FString HeaderText;
  if (Headers.Num() == 1 && FFileHelper::LoadFileToString(HeaderText, *Headers[0])) {
    const int32 Types = CountOf(HeaderText, TEXT("UCLASS(")) + CountOf(HeaderText, TEXT("USTRUCT(")) +
                        CountOf(HeaderText, TEXT("UENUM(")) + CountOf(HeaderText, TEXT("UINTERFACE("));
    if (Types > 1) {
      return Refuse(FString::Printf(TEXT("%s declares %d reflected types, not only %s: removing it would take the others "
                                         "with it. Move them out first."), *Headers[0], Types, *BaseName), TEXT("FILE_HOLDS_MORE"));
    }
  }
  TArray<FString> Own = Headers;
  Own.Append(Cpps);
  UClass* Class = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/%s.%s"), *Module->ModuleName, *BaseName));
  const FString Prefixed = Class ? FString(Class->GetPrefixCPP()) + BaseName : BaseName;
  TArray<FString> Users;
  if (Class) {
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    EngineUsers(Class, Registry, Users);
    FString TooMany;
    if (!PackageUsers(Class, Module->ModuleName, Registry, Users, TooMany)) {
      return Refuse(TooMany, TEXT("TOO_MANY_PACKAGES"));
    }
  }
  SourceUsers(BaseName, Prefixed, Own, Users);
  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("className"), Prefixed);
  Data->SetStringField(TEXT("moduleName"), Module->ModuleName);
  if (Class) Data->SetStringField(TEXT("classPath"), Class->GetPathName());
  if (Users.Num() > 0) {
    Data->SetArrayField(TEXT("users"), ToJsonStrings(TArray<FString>(Users.GetData(), FMath::Min(Users.Num(), MaxUsersListed))));
    const FString First = FString::Join(TArray<FString>(Users.GetData(), FMath::Min(Users.Num(), 3)), TEXT("; "));
    const FString More = Users.Num() > 3 ? FString::Printf(TEXT(" (and %d more under users)."), Users.Num() - 3) : FString(TEXT("."));
    Self->SendAutomationResponse(Socket, RequestId, false,
                                 FString::Printf(TEXT("%s is still used, so nothing was removed: %s%s"), *Prefixed, *First, *More),
                                 Data, TEXT("CLASS_IN_USE"));
    return true;
  }
  // Read-only files (checked in to source control) are left, never forced.
  TArray<FString> Deleted, Left;
  for (const FString& File : Own) {
    (IFileManager::Get().Delete(*File, true, false, true) ? Deleted : Left).Add(File);
  }
  Data->SetArrayField(TEXT("deleted"), ToJsonStrings(Deleted));
  Data->SetStringField(TEXT("note"), TEXT("The class stays loaded in this editor (Live Coding cannot remove a class) until "
                                          "the editor target is built with the editor closed and the editor opens again; "
                                          "until then it still shows under C++ Classes."));
  if (Left.Num() > 0) {
    Data->SetArrayField(TEXT("notDeleted"), ToJsonStrings(Left));
    Self->SendAutomationResponse(Socket, RequestId, false,
                                 FString::Printf(TEXT("Could not delete %s (read-only or in use)."), *FString::Join(Left, TEXT(", "))),
                                 Data, TEXT("DELETE_FAILED"));
    return true;
  }
  Self->SendAutomationResponse(Socket, RequestId, true,
                               FString::Printf(TEXT("Removed %s from %s: deleted %s."), *Prefixed, *Module->ModuleName,
                                               *FString::Join(Deleted, TEXT(", "))),
                               Data, FString());
  return true;
}
} // namespace McpSystemControlHandlers
