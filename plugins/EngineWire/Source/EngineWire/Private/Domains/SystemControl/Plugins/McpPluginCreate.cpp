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
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/Paths.h"
#include "PluginDescriptor.h"
#include "ProjectDescriptor.h"
#include "Runtime/Launch/Resources/Version.h"

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

// Copies the icon the Plugins browser gives a content-only plugin, when this engine has it.
void PluginCreateCopyIcon(const FString& Folder)
{
	const TSharedPtr<IPlugin> Browser = IPluginManager::Get().FindPlugin(TEXT("PluginBrowser"));
	// Joined in parts: one literal holding a slash before Content reads as a path alias to the source contracts.
	const FString Icon = Browser.IsValid() ? Browser->GetBaseDir() / TEXT("Templates") / TEXT("ContentOnly") / TEXT("Resources/Icon128.png") : FString();
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
	FString Name, FriendlyName, Description, Category, CreatedBy;
	Payload->TryGetStringField(TEXT("pluginName"), Name);
	Payload->TryGetStringField(TEXT("friendlyName"), FriendlyName);
	Payload->TryGetStringField(TEXT("description"), Description);
	Payload->TryGetStringField(TEXT("category"), Category);
	Payload->TryGetStringField(TEXT("createdBy"), CreatedBy);
	const FString Folder = PluginCreateRoot() + Name;
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
	Descriptor.Category = Category.IsEmpty() ? FString(TEXT("Other")) : Category;
	Descriptor.CreatedBy = CreatedBy;
	Descriptor.Version = 1;
	Descriptor.VersionName = TEXT("1.0");
	Descriptor.bCanContainContent = true;
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
	PluginCreateCopyIcon(Folder);
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
	Self->SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(
		TEXT("Created %s and enabled it in the project; its assets go under %s."), *Name, *Root), Result);
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
