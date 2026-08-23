// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "ProjectDescriptor.h"

namespace
{
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

bool HandleManagePlugins(UMcpAutomationBridgeSubsystem* Self,
                         const FString& RequestId, const FString& SubAction,
                         const TSharedPtr<FJsonObject>& Payload,
                         FSystemControlSocket RequestingSocket) {
  IPluginManager& PluginManager = IPluginManager::Get();

  if (SubAction == TEXT("list_plugins")) {
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
    bool bEnabledOnly = false;
    Payload->TryGetBoolField(TEXT("enabledOnly"), bEnabledOnly);

    TArray<TSharedPtr<FJsonValue>> Plugins;
    for (const TSharedRef<IPlugin>& Plugin : PluginManager.GetDiscoveredPlugins()) {
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
    Plugins.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) {
      FString NameA, NameB;
      A->AsObject()->TryGetStringField(TEXT("name"), NameA);
      B->AsObject()->TryGetStringField(TEXT("name"), NameB);
      return NameA < NameB;
    });

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
#else
namespace McpSystemControlHandlers {
bool HandleManagePlugins(UMcpAutomationBridgeSubsystem* Self,
                         const FString& RequestId, const FString& SubAction,
                         const TSharedPtr<FJsonObject>& Payload,
                         FSystemControlSocket RequestingSocket) {
  Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Plugin management requires the editor."),
                            TEXT("EDITOR_ONLY"));
  return true;
}
}
#endif
