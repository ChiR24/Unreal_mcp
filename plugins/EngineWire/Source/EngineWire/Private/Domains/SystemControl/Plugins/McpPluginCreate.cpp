// create_plugin and remove_plugin. A content-only plugin is written into this project's Plugins folder, added to the
// plugin list, enabled in the project and mounted at once, as the Plugins browser's Content Only template does;
// remove_plugin takes one that no longer loads (disable_plugin, then a restart, then this) out of the project: its
// folder moves to Saved/EngineWire/RemovedPlugins and the .uproject drops it. Only a plugin in this project's Plugins
// folder without code is removed: an engine or marketplace plugin, or code, never moves.

#include "Domains/SystemControl/McpSystemPrivate.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "McpGameFeaturesProvider.h"
#include "Misc/Paths.h"
#include "PluginDescriptor.h"
#include "ProjectDescriptor.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Safety/McpSafeOperations.h"

namespace
{
FString PluginCreateRoot()
{
	FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
	FPaths::NormalizeDirectoryName(Root);
	return Root + TEXT("/");
}

// A folder as a reply names it: relative to the project.
FString PluginCreateShown(const FString& Folder)
{
	FString Shown = FPaths::ConvertRelativePathToFull(Folder);
	FPaths::MakePathRelativeTo(Shown, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	return Shown;
}

// The Plugins browser's rule for a plugin name: a letter, then letters, digits or underscores.
bool PluginCreateValidName(const FString& Name)
{
	if (Name.IsEmpty() || Name.Len() > 64 || !FChar::IsAlpha(Name[0]))
	{
		return false;
	}
	for (const TCHAR Character : Name)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			return false;
		}
	}
	return true;
}

// Copies the icon the template gives such a plugin (the Plugins browser's Content Only one, or Game Features' own), when
// this engine has it.
void PluginCreateCopyIcon(const FString& Folder, bool bGameFeature)
{
	const TSharedPtr<IPlugin> Owner = IPluginManager::Get().FindPlugin(bGameFeature ? TEXT("GameFeatures") : TEXT("PluginBrowser"));
	// Joined in parts: one literal holding a slash before Content reads as a path alias to the source contracts.
	const FString Icon = Owner.IsValid() ? Owner->GetBaseDir() / TEXT("Templates") / (bGameFeature ? TEXT("GameFeaturePluginContentOnly") : TEXT("ContentOnly")) / TEXT("Resources/Icon128.png") : FString();
	if (FPaths::FileExists(Icon))
	{
		IFileManager::Get().Copy(*(Folder / TEXT("Resources/Icon128.png")), *Icon);
	}
}

// Drops PluginName's reference from the project file; the editor keeps its own copy of the file, so it is reloaded.
bool PluginCreateDropReference(const FString& PluginName, bool& bOutDropped, FString& OutError)
{
	const FProjectDescriptor* Current = IProjectManager::Get().GetCurrentProject();
	bOutDropped = Current && Current->Plugins.ContainsByPredicate(
		[&PluginName](const FPluginReferenceDescriptor& Each) { return Each.Name == PluginName; });
	if (!bOutDropped)
	{
		return true;
	}
	FProjectDescriptor Descriptor = *Current;
	Descriptor.Plugins.RemoveAll([&PluginName](const FPluginReferenceDescriptor& Each) { return Each.Name == PluginName; });
	const FString ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FText FailReason;
	FProjectDescriptor Written;
	if (!Descriptor.Save(ProjectFile, FailReason) || !Written.Load(ProjectFile, FailReason))
	{
		OutError = FailReason.ToString();
		return false;
	}
	IProjectManager::Get().LoadProjectFile(ProjectFile);
	return true;
}
} // namespace

namespace McpSystemControlHandlers
{
bool HandleCreatePlugin(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                        FSystemControlSocket RequestingSocket)
{
	FString Name, FriendlyName, Description, Category, CreatedBy, Template;
	Payload->TryGetStringField(TEXT("pluginName"), Name);
	Payload->TryGetStringField(TEXT("friendlyName"), FriendlyName);
	Payload->TryGetStringField(TEXT("description"), Description);
	Payload->TryGetStringField(TEXT("category"), Category);
	Payload->TryGetStringField(TEXT("createdBy"), CreatedBy);
	Payload->TryGetStringField(TEXT("template"), Template);
	// A game feature plugin is the Game Feature (Content Only) template's: under Plugins/GameFeatures, loaded on demand
	// and active from the start, given its GameFeatureData asset by the game features module.
	const bool bGameFeature = Template == TEXT("game_feature");
	IMcpGameFeaturesProvider* GameFeatures = bGameFeature ? GameFeaturesProvider() : nullptr;
	if (bGameFeature && !GameFeatures)
	{
		SendGameFeaturesMissing(Self, RequestId, RequestingSocket, TEXT(" Nothing was created."));
		return true;
	}
	const FString Folder = PluginCreateRoot() + (bGameFeature ? TEXT("GameFeatures/") : TEXT("")) + Name;
	const FString Refusal = !PluginCreateValidName(Name)
		? FString::Printf(TEXT("'%s' is no plugin name: a letter, then letters, digits or underscores (64 at most)."), *Name)
		: IPluginManager::Get().FindPlugin(Name).IsValid() ? FString::Printf(TEXT("A plugin named %s already exists."), *Name)
		: IFileManager::Get().DirectoryExists(*Folder) ? FString::Printf(TEXT("%s already exists."), *PluginCreateShown(Folder))
		: FString();
	if (!Refusal.IsEmpty())
	{
		Self->SendAutomationError(RequestingSocket, RequestId, Refusal + TEXT(" Nothing was created."),
			PluginCreateValidName(Name) ? TEXT("PLUGIN_EXISTS") : TEXT("INVALID_ARGUMENT"));
		return true;
	}
	FPluginDescriptor Descriptor;
	Descriptor.FriendlyName = FriendlyName.IsEmpty() ? Name : FriendlyName;
	Descriptor.Description = Description;
	Descriptor.Category = !Category.IsEmpty() ? Category : bGameFeature ? FString(TEXT("Game Features")) : FString(TEXT("Other"));
	Descriptor.CreatedBy = CreatedBy;
	Descriptor.Version = 1;
	Descriptor.VersionName = TEXT("1.0");
	Descriptor.bCanContainContent = true;
	if (bGameFeature)
	{
		Descriptor.bExplicitlyLoaded = true;
		Descriptor.AdditionalFieldsToWrite.Add(TEXT("BuiltInInitialFeatureState"), MakeShared<FJsonValueString>(TEXT("Active")));
	}
	const FString File = Folder / (Name + TEXT(".uplugin"));
	FText Fail;
	const bool bWritten = IFileManager::Get().MakeDirectory(*(Folder / TEXT("Content")), /*Tree=*/true) && Descriptor.Save(File, &Fail);
	if (!bWritten || !IPluginManager::Get().AddToPluginsList(File, &Fail))
	{
		IFileManager::Get().DeleteDirectory(*Folder, /*RequireExists=*/false, /*Tree=*/true);
		Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was not created: %s Nothing is left "
			"on disk."), *Name, bWritten ? *Fail.ToString() : TEXT("its folder or descriptor could not be written.")), TEXT("CREATE_FAILED"));
		return true;
	}
	PluginCreateCopyIcon(Folder, bGameFeature);
	if (!IProjectManager::Get().SetPluginEnabled(Name, true, Fail) ||
		(IProjectManager::Get().IsCurrentProjectDirty() && !IProjectManager::Get().SaveCurrentProjectToDisk(Fail)))
	{
		Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s is written to %s but the project file "
			"could not enable it: %s"), *Name, *PluginCreateShown(Folder), *Fail.ToString()), TEXT("PROJECT_SAVE_FAILED"));
		return true;
	}
	IPluginManager::Get().MountNewlyCreatedPlugin(Name);
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Name);
	const FString Root = Plugin.IsValid() ? Plugin->GetMountedAssetPath() : FString();
	if (!Root.IsEmpty())
	{
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().ScanPathsSynchronous({Root}, /*bForceRescan=*/true);
	}
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetStringField(TEXT("pluginName"), Name);
	Result->SetStringField(TEXT("folder"), PluginCreateShown(Folder));
	Result->SetStringField(TEXT("mountedContentPath"), Root);
	Result->SetBoolField(TEXT("enabled"), Plugin.IsValid() && Plugin->IsEnabled());
	// Its GameFeatureData, saved first (the subsystem registers a plugin only when that package is on disk); then the
	// subsystem registers the plugin and activates it as its descriptor asks over the next frames.
	if (GameFeatures && !GameFeatures->AdoptNewPlugin(Name, [](UObject* Data) { return McpSafeAssetSave(Data); }))
	{
		Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s is created and enabled, but its "
			"GameFeatureData asset could not be made and saved, so no game feature runs from it."), *Name), TEXT("GAME_FEATURE_FAILED"));
		return true;
	}
	if (GameFeatures)
	{
		Result->SetStringField(TEXT("gameFeatureData"), Root + Name);
	}
	Self->SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(
		TEXT("Created %s and enabled it in the project; its assets go under %s.%s"), *Name, *Root,
		GameFeatures ? TEXT(" It is a game feature plugin, activating now (list_game_features shows its state).") : TEXT("")), Result);
	return true;
}

bool HandleRemovePlugin(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                        FSystemControlSocket RequestingSocket)
{
	FString Name;
	Payload->TryGetStringField(TEXT("pluginName"), Name);
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Name);
	if (!Plugin.IsValid())
	{
		Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(
			TEXT("No plugin named '%s' is installed (list_plugins lists them)."), *Name), TEXT("PLUGIN_NOT_FOUND"));
		return true;
	}
	FString Folder = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
	FPaths::NormalizeDirectoryName(Folder);
	const FString Root = PluginCreateRoot();
	const FString Refusal = !Folder.StartsWith(Root, ESearchCase::IgnoreCase) || Folder.Len() <= Root.Len()
		? FString::Printf(TEXT("%s is not in this project's Plugins folder; only a project's own plugins are removed."), *Name)
		: Plugin->GetDescriptor().Modules.Num() > 0 || IFileManager::Get().DirectoryExists(*(Folder / TEXT("Source")))
		? FString::Printf(TEXT("%s has code; it is not removed, as that code may exist nowhere else."), *Name)
		: FString();
	if (!Refusal.IsEmpty())
	{
		Self->SendAutomationError(RequestingSocket, RequestId, Refusal + TEXT(" Nothing was changed."), TEXT("PLUGIN_NOT_REMOVABLE"));
		return true;
	}
	// A loaded plugin's assets may be open or referenced, and only a restart unmounts a plugin that loads at startup.
	if (Plugin->IsEnabled())
	{
		Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s is loaded in this editor. Turn it off "
			"with disable_plugin, restart the editor, then remove it. Nothing was changed."), *Name), TEXT("PLUGIN_LOADED"));
		return true;
	}
	// Moved, not deleted: a rename never follows a link into another tree (a deletion would), and it can be moved back.
	// First, so a move that fails leaves the project as it was.
	const FString Trash = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) / TEXT("EngineWire/RemovedPlugins") / Name;
	FString MovedTo = Trash;
	for (int32 Copy = 2; IFileManager::Get().DirectoryExists(*MovedTo); ++Copy)
	{
		MovedTo = FString::Printf(TEXT("%s-%d"), *Trash, Copy);
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(MovedTo), /*Tree=*/true);
	if (!IFileManager::Get().Move(*MovedTo, *Folder, /*Replace=*/false, /*EvenIfReadOnly=*/true, /*Attributes=*/false, /*bDoNotRetryOrError=*/true))
	{
		Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s could not be moved out of the project "
			"(a file in it is open?). Nothing was changed."), *PluginCreateShown(Folder)), TEXT("MOVE_FAILED"));
		return true;
	}
	// The subfolder of Plugins the move left empty goes too (create_plugin made Plugins/GameFeatures for a game feature
	// plugin); without Tree, DeleteDirectory removes only an empty folder, so one holding anything stays.
	if (FPaths::GetPath(Folder).Len() >= Root.Len())
	{
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Folder), /*RequireExists=*/false, /*Tree=*/false);
	}
	bool bDropped = false;
	FString Error;
	if (!PluginCreateDropReference(Name, bDropped, Error))
	{
		Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s moved to %s, but the project file "
			"could not drop it: %s"), *Name, *PluginCreateShown(MovedTo), *Error), TEXT("PROJECT_SAVE_FAILED"));
		return true;
	}
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
	IPluginManager::Get().RemoveFromPluginsList(Plugin->GetDescriptorFileName());
#endif
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetStringField(TEXT("pluginName"), Name);
	Result->SetStringField(TEXT("movedTo"), PluginCreateShown(MovedTo));
	Result->SetBoolField(TEXT("referenceRemoved"), bDropped);
	Self->SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Removed %s from the project: its "
		"folder is now %s (delete it there for good, or move it back to restore it)."), *Name, *PluginCreateShown(MovedTo)), Result);
	return true;
}
} // namespace McpSystemControlHandlers
