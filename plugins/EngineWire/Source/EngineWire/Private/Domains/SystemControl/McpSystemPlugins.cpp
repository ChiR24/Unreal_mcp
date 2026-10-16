// Copyright (c) 2024 EngineWire Contributors

#include "Domains/SystemControl/McpSystemPrivate.h"

#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"

#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/Paths.h"
#include "ProjectDescriptor.h"

namespace
{
// Writes the project's reference to PluginName as enabled for Targets (every target when empty), adding the reference
// when the project file has none, then reloads the project: the editor keeps its own copy of the file and writes that
// copy back on its next plugin change. The engine's SetPluginEnabled cannot do this: it drops a reference that matches
// the plugin's default state whatever targets it lists.
bool McpWritePluginReference(const FString& PluginName, const TArray<EBuildTargetType>& Targets, bool& bOutChanged,
                             FString& OutError)
{
	const FProjectDescriptor* Current = IProjectManager::Get().GetCurrentProject();
	if (!Current)
	{
		OutError = TEXT("No project is loaded.");
		return false;
	}
	FProjectDescriptor Descriptor = *Current;
	FPluginReferenceDescriptor* Ref = Descriptor.Plugins.FindByPredicate(
		[&PluginName](const FPluginReferenceDescriptor& Each) { return Each.Name == PluginName; });
	bOutChanged = !Ref || !Ref->bEnabled || Ref->TargetAllowList != Targets;
	if (!bOutChanged)
	{
		return true;
	}
	if (!Ref)
	{
		Ref = &Descriptor.Plugins.Add_GetRef(FPluginReferenceDescriptor(PluginName, true));
	}
	Ref->bEnabled = true;
	Ref->TargetAllowList = Targets;
	const FString ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FText FailReason;
	// Read back before the editor takes it: LoadProjectFile opens a modal dialog when a file does not load.
	FProjectDescriptor Written;
	if (!Descriptor.Save(ProjectFile, FailReason) || !Written.Load(ProjectFile, FailReason))
	{
		OutError = FailReason.ToString();
		return false;
	}
	IProjectManager::Get().LoadProjectFile(ProjectFile);
	return true;
}

// One of Project Settings > Packaging's folder lists (DirectoriesToAlwaysCook, DirectoriesToNeverCook), each folder
// ending in '/'. Read by reflection: the settings class lives in a module this plugin does not link.
TArray<FString> McpPackagingFolders(const TCHAR* ListName)
{
	TArray<FString> Folders;
	const UClass* Settings = McpFindTypeQuiet(TEXT("ProjectPackagingSettings"));
	const FArrayProperty* List = Settings ? FindFProperty<FArrayProperty>(Settings, ListName) : nullptr;
	const FStructProperty* Entry = List ? CastField<FStructProperty>(List->Inner) : nullptr;
	const FStrProperty* PathField = Entry ? FindFProperty<FStrProperty>(Entry->Struct, TEXT("Path")) : nullptr;
	if (PathField)
	{
		FScriptArrayHelper Paths(List, List->ContainerPtrToValuePtr<void>(Settings->GetDefaultObject()));
		for (int32 Index = 0; Index < Paths.Num(); ++Index)
		{
			Folders.Add(*PathField->ContainerPtrToValuePtr<FString>(Paths.GetRawPtr(Index)) + TEXT("/"));
		}
	}
	return Folders;
}

// Every cook takes the folders of "Additional Asset Directories to Cook" unless "Directories to Never Cook" covers them.
// One of a plugin the packaged game leaves out fails that cook: its assets import classes the game does not have.
bool McpAlwaysCooksContentOf(const IPlugin& Plugin)
{
	const FString Root = Plugin.GetMountedAssetPath();
	return Plugin.CanContainContent()
		&& McpPackagingFolders(TEXT("DirectoriesToAlwaysCook")).ContainsByPredicate(
			[&Root](const FString& Folder) { return Folder.StartsWith(Root, ESearchCase::IgnoreCase); })
		&& !McpPackagingFolders(TEXT("DirectoriesToNeverCook")).ContainsByPredicate(
			[&Root](const FString& Folder) { return Root.StartsWith(Folder, ESearchCase::IgnoreCase); });
}

bool McpProjectEnablesPlugin(const FString& PluginName)
{
	const FProjectDescriptor* Current = IProjectManager::Get().GetCurrentProject();
	return Current && Current->Plugins.ContainsByPredicate(
		[&PluginName](const FPluginReferenceDescriptor& Each) { return Each.Name == PluginName && Each.bEnabled; });
}

bool McpSendPluginReference(UEngineWireSubsystem* Self, const FString& RequestId,
                            McpSystemControlHandlers::FSystemControlSocket Socket, const IPlugin& Plugin,
                            const TArray<EBuildTargetType>& Targets)
{
	bool bChanged = false;
	FString Error;
	if (!McpWritePluginReference(Plugin.GetName(), Targets, bChanged, Error))
	{
		Self->SendAutomationError(Socket, RequestId, Error, TEXT("PROJECT_SAVE_FAILED"));
		return true;
	}
	TArray<FString> Names;
	for (const EBuildTargetType Target : Targets)
	{
		Names.Add(LexToString(Target));
	}
	const bool bInEditor = Targets.Num() == 0 || Targets.Contains(EBuildTargetType::Editor);
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetStringField(TEXT("pluginName"), Plugin.GetName());
	Result->SetBoolField(TEXT("enabled"), true);
	if (Names.Num() > 0)
	{
		Result->SetArrayField(TEXT("targets"), McpHandlerUtils::ToJsonStringArray(Names));
	}
	Result->SetBoolField(TEXT("changed"), bChanged);
	if (Names.Num() > 0 && !Targets.Contains(EBuildTargetType::Game) && McpAlwaysCooksContentOf(Plugin))
	{
		Result->SetArrayField(TEXT("warnings"), McpHandlerUtils::ToJsonStringArray({FString::Printf(
			TEXT("%s is in Project Settings > Packaging > Additional Asset Directories to Cook, so the next package fails "
				"on its assets: they need this plugin, which the game now leaves out. Add it to Directories to Never Cook "
				"(set_project_setting, section ProjectPackagingSettings, key DirectoriesToNeverCook): a plugin's own "
				"config can put it back on the first list."),
			*Plugin.GetMountedAssetPath())}));
	}
	// This editor loads plugins at startup only, so it changes only when its own target is added or dropped.
	Result->SetBoolField(TEXT("restartRequired"), bChanged && Plugin.IsEnabled() != bInEditor);
	const FString Message = Names.Num() > 0
		? FString::Printf(TEXT("Plugin '%s' is %s enabled for %s targets only; a build for any other target%s leaves "
			"it out."), *Plugin.GetName(), bChanged ? TEXT("now") : TEXT("already"), *FString::Join(Names, TEXT(", ")),
			Targets.Contains(EBuildTargetType::Game) ? TEXT("") : TEXT(", a packaged game among them,"))
		: FString::Printf(TEXT("Plugin '%s' was on only because another plugin needs it; the project file now enables "
			"it itself, so it stays on without that plugin."), *Plugin.GetName());
	Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
	return true;
}

TSharedPtr<FJsonObject> DescribePlugin(const TSharedRef<IPlugin>& Plugin)
{
	TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("name"), Plugin->GetName());
	Entry->SetBoolField(TEXT("enabled"), Plugin->IsEnabled());
	Entry->SetBoolField(TEXT("canContainContent"), Plugin->CanContainContent());
	const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
	Entry->SetStringField(TEXT("friendlyName"), Descriptor.FriendlyName);
	Entry->SetStringField(TEXT("category"), Descriptor.Category);
	Entry->SetStringField(TEXT("versionName"), Descriptor.VersionName);
	// Mounted content root, so a caller can immediately list assets the plugin
	// ships without guessing the /PluginName convention.
	Entry->SetStringField(TEXT("mountedContentPath"),
		Plugin->CanContainContent() ? Plugin->GetMountedAssetPath() : FString());
	return Entry;
}
} // namespace

namespace McpSystemControlHandlers {

bool HandleManagePlugins(UEngineWireSubsystem* Self,
                         const FString& RequestId, const FString& SubAction,
                         const TSharedPtr<FJsonObject>& Payload,
                         FSystemControlSocket RequestingSocket) {
  IPluginManager& PluginManager = IPluginManager::Get();

  if (SubAction == TEXT("list_plugins")) {
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
    bool bEnabledOnly = false;
    Payload->TryGetBoolField(TEXT("enabledOnly"), bEnabledOnly);

    TArray<TSharedRef<IPlugin>> Discovered = PluginManager.GetDiscoveredPlugins();
    Discovered.Sort([](const TSharedRef<IPlugin>& A, const TSharedRef<IPlugin>& B) {
      return A->GetName() < B->GetName();
    });
    TArray<TSharedPtr<FJsonValue>> Plugins;
    for (const TSharedRef<IPlugin>& Plugin : Discovered) {
      if (bEnabledOnly && !Plugin->IsEnabled()) {
        continue;
      }
      if (!Filter.IsEmpty() &&
          !Plugin->GetName().Contains(Filter) &&
          !Plugin->GetDescriptor().Category.Contains(Filter)) {
        continue;
      }
      Plugins.Add(MakeShared<FJsonValueObject>(DescribePlugin(Plugin)));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("plugins"), Plugins);
    Result->SetNumberField(TEXT("pluginCount"), Plugins.Num());
    Self->SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Found %d plugin(s)."), Plugins.Num()), Result);
    return true;
  }

  const bool bEnable = SubAction == TEXT("enable_plugin");
  FString PluginName;
  if (!Payload->TryGetStringField(TEXT("pluginName"), PluginName) || PluginName.IsEmpty()) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Missing 'pluginName'."), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
  if (!Plugin.IsValid()) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Plugin '%s' is not installed. Use list_plugins to see available names."),
                        *PluginName),
        TEXT("NOT_FOUND"));
    return true;
  }
  // targets: the build targets the plugin is for, so ["Editor"] keeps an editor-only plugin out of a packaged game.
  const TArray<TSharedPtr<FJsonValue>>* TargetValues = nullptr;
  if (bEnable && Payload->TryGetArrayField(TEXT("targets"), TargetValues) && TargetValues) {
    TArray<EBuildTargetType> Targets;
    for (const TSharedPtr<FJsonValue>& Value : *TargetValues) {
      const FString Name = Value.IsValid() ? Value->AsString() : FString();
      EBuildTargetType Target = EBuildTargetType::Unknown;
      if (!LexTryParseString(Target, *Name)) {
        Self->SendAutomationError(RequestingSocket, RequestId,
                                  FString::Printf(TEXT("'%s' is not a build target: use Editor, Game, Client, Server "
                                                       "or Program. Nothing was written."), *Name),
                                  TEXT("INVALID_ARGUMENT"));
        return true;
      }
      Targets.AddUnique(Target);
    }
    return McpSendPluginReference(Self, RequestId, RequestingSocket, *Plugin, Targets);
  }
  // On only because another plugin needs it: the project file is what keeps it on once that plugin goes or is
  // limited to the editor, and it had said nothing.
  if (bEnable && Plugin->IsEnabled() && !Plugin->IsEnabledByDefault(true) && !McpProjectEnablesPlugin(PluginName)) {
    return McpSendPluginReference(Self, RequestId, RequestingSocket, *Plugin, {});
  }
  if (Plugin->IsEnabled() == bEnable) {
    TSharedPtr<FJsonObject> NoOp = McpHandlerUtils::CreateResultObject();
    NoOp->SetStringField(TEXT("pluginName"), PluginName);
    NoOp->SetBoolField(TEXT("enabled"), bEnable);
    NoOp->SetBoolField(TEXT("changed"), false);
    NoOp->SetBoolField(TEXT("restartRequired"), false);
    Self->SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Plugin '%s' is already %s."), *PluginName,
                        bEnable ? TEXT("enabled") : TEXT("disabled")),
        NoOp);
    return true;
  }

  FText FailReason;
  if (!IProjectManager::Get().SetPluginEnabled(PluginName, bEnable, FailReason)) {
    Self->SendAutomationError(RequestingSocket, RequestId, FailReason.ToString(),
                              TEXT("PLUGIN_UPDATE_FAILED"));
    return true;
  }
  if (IProjectManager::Get().IsCurrentProjectDirty() &&
      !IProjectManager::Get().SaveCurrentProjectToDisk(FailReason)) {
    Self->SendAutomationError(RequestingSocket, RequestId, FailReason.ToString(),
                              TEXT("PROJECT_SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("pluginName"), PluginName);
  Result->SetBoolField(TEXT("enabled"), bEnable);
  Result->SetBoolField(TEXT("changed"), true);
  // The .uproject is updated now, but module load/unload and content mounting
  // only happen at startup. Saying otherwise would have callers immediately
  // query for classes the process cannot yet see.
  Result->SetBoolField(TEXT("restartRequired"), true);
  Self->SendAutomationResponse(
      RequestingSocket, RequestId, true,
      FString::Printf(TEXT("Plugin '%s' %s in the project file. Restart the editor for it to take effect."),
                      *PluginName, bEnable ? TEXT("enabled") : TEXT("disabled")),
      Result);
  return true;
}

}
