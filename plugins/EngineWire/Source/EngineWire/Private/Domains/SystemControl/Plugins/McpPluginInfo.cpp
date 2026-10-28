// get_plugin_info: one plugin read from its descriptor, with the plugins it needs and the plugins that need it; or the
// plugin whose content holds an asset path.

#include "Domains/SystemControl/McpSystemPrivate.h"

#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Interfaces/IPluginManager.h"
#include "ModuleDescriptor.h"
#include "PluginDescriptor.h"

namespace
{
const TCHAR* PluginInfoType(EPluginType Type)
{
	switch (Type)
	{
	case EPluginType::Engine: return TEXT("engine");
	case EPluginType::Enterprise: return TEXT("enterprise");
	case EPluginType::Project: return TEXT("project");
	case EPluginType::External: return TEXT("external");
	default: return TEXT("mod");
	}
}

const TCHAR* PluginInfoDefault(EPluginEnabledByDefault Default)
{
	return Default == EPluginEnabledByDefault::Enabled ? TEXT("enabled")
		: Default == EPluginEnabledByDefault::Disabled ? TEXT("disabled") : TEXT("unspecified");
}

// The plugin whose mounted content root ("/Name/") starts AssetPath; /Game, /Engine and /Script answer without one.
TSharedPtr<IPlugin> PluginInfoOwner(const FString& AssetPath, FString& OutRoot)
{
	FString Rest = AssetPath;
	Rest.RemoveFromStart(TEXT("/"));
	int32 Slash = INDEX_NONE;
	OutRoot = Rest.FindChar(TEXT('/'), Slash) ? Rest.Left(Slash) : Rest;
	const FString Mount = TEXT("/") + OutRoot + TEXT("/");
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		if (Plugin->CanContainContent() && Plugin->GetMountedAssetPath().Equals(Mount, ESearchCase::IgnoreCase))
		{
			return Plugin;
		}
	}
	return nullptr;
}

TSharedPtr<FJsonObject> PluginInfoRow(const IPlugin& Plugin)
{
	const FPluginDescriptor& D = Plugin.GetDescriptor();
	TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
	Row->SetStringField(TEXT("pluginName"), Plugin.GetName());
	Row->SetStringField(TEXT("friendlyName"), D.FriendlyName);
	Row->SetStringField(TEXT("description"), D.Description);
	Row->SetStringField(TEXT("category"), D.Category);
	Row->SetStringField(TEXT("versionName"), D.VersionName);
	Row->SetNumberField(TEXT("version"), D.Version);
	Row->SetStringField(TEXT("createdBy"), D.CreatedBy);
	Row->SetStringField(TEXT("docsURL"), D.DocsURL);
	Row->SetBoolField(TEXT("enabled"), Plugin.IsEnabled());
	Row->SetStringField(TEXT("enabledByDefault"), PluginInfoDefault(D.EnabledByDefault));
	Row->SetStringField(TEXT("type"), PluginInfoType(Plugin.GetType()));
	Row->SetBoolField(TEXT("beta"), D.bIsBetaVersion);
	Row->SetBoolField(TEXT("experimental"), D.bIsExperimentalVersion);
	Row->SetBoolField(TEXT("explicitlyLoaded"), D.bExplicitlyLoaded);
	Row->SetBoolField(TEXT("canContainContent"), Plugin.CanContainContent());
	Row->SetStringField(TEXT("mountedContentPath"), Plugin.CanContainContent() ? Plugin.GetMountedAssetPath() : FString());
	TArray<TSharedPtr<FJsonValue>> Modules;
	for (const FModuleDescriptor& Module : D.Modules)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Module.Name.ToString());
		Entry->SetStringField(TEXT("type"), EHostType::ToString(Module.Type));
		Entry->SetStringField(TEXT("loadingPhase"), ELoadingPhase::ToString(Module.LoadingPhase));
		Modules.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Row->SetArrayField(TEXT("modules"), Modules);
	TArray<TSharedPtr<FJsonValue>> Needs;
	for (const FPluginReferenceDescriptor& Reference : D.Plugins)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Reference.Name);
		Entry->SetBoolField(TEXT("enabled"), Reference.bEnabled);
		Entry->SetBoolField(TEXT("optional"), Reference.bOptional);
		Needs.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Row->SetArrayField(TEXT("dependencies"), Needs);
	// Which installed plugins list this one, enabled or not: turning it off breaks the enabled ones.
	TArray<FString> NeededBy;
	for (const TSharedRef<IPlugin>& Other : IPluginManager::Get().GetDiscoveredPlugins())
	{
		const bool bNeeds = Other->GetDescriptor().Plugins.ContainsByPredicate(
			[&Plugin](const FPluginReferenceDescriptor& Reference) { return Reference.Name == Plugin.GetName() && Reference.bEnabled; });
		if (bNeeds)
		{
			NeededBy.Add(Other->IsEnabled() ? Other->GetName() : Other->GetName() + TEXT(" (disabled)"));
		}
	}
	NeededBy.Sort();
	Row->SetArrayField(TEXT("dependents"), McpHandlerUtils::ToJsonStringArray(NeededBy));
	return Row;
}
} // namespace

namespace McpSystemControlHandlers
{
bool HandlePluginInfo(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                      FSystemControlSocket RequestingSocket)
{
	FString PluginName, AssetPath;
	Payload->TryGetStringField(TEXT("pluginName"), PluginName);
	Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
	TSharedPtr<IPlugin> Plugin;
	if (!AssetPath.IsEmpty())
	{
		FString Root;
		Plugin = PluginInfoOwner(AssetPath, Root);
		if (!Plugin.IsValid())
		{
			const bool bKnown = Root == TEXT("Game") || Root == TEXT("Engine") || Root == TEXT("Script");
			Self->SendAutomationError(RequestingSocket, RequestId, bKnown
				? FString::Printf(TEXT("'%s' is under /%s, the %s, not a plugin's."), *AssetPath, *Root,
					Root == TEXT("Game") ? TEXT("project's own content") : Root == TEXT("Engine") ? TEXT("engine's content") : TEXT("native classes"))
				: FString::Printf(TEXT("No enabled plugin mounts /%s/, where '%s' lies."), *Root, *AssetPath), TEXT("PLUGIN_NOT_FOUND"));
			return true;
		}
	}
	else
	{
		Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if (!Plugin.IsValid())
		{
			Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(
				TEXT("No plugin named '%s' is installed (list_plugins lists them)."), *PluginName), TEXT("PLUGIN_NOT_FOUND"));
			return true;
		}
	}
	const TSharedPtr<FJsonObject> Result = PluginInfoRow(*Plugin);
	Self->SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("%s (%s plugin, %s)."),
		*Plugin->GetName(), PluginInfoType(Plugin->GetType()), Plugin->IsEnabled() ? TEXT("enabled") : TEXT("disabled")), Result);
	return true;
}
} // namespace McpSystemControlHandlers
