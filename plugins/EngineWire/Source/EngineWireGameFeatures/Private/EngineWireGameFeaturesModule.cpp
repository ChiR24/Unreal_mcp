// Copyright (c) 2024 EngineWire Contributors
//
// The GameFeatures side of IMcpGameFeaturesProvider: game feature plugins listed with their subsystem state, activated
// and deactivated, and a just-made content-only plugin given what the Game Feature template gives one. Loaded by
// EngineWire's StartupModule only where the GameFeatures module exists (the project enables that plugin); the state
// enum is public from 5.1 (GameFeatureTypes.h), so on 5.0 the module registers nothing.

#include "McpGameFeaturesProvider.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
#include "AssetToolsModule.h"
#include "GameFeatureData.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureTypes.h"
#include "Interfaces/IPluginManager.h"

namespace
{
// The four states a plugin rests in, lower case as Epic's toolset reports them; any other (a step on the way, an error)
// by the engine's own name, taken from the engine's own state list so every version names its own states.
FString McpGameFeatureState(EGameFeaturePluginState State)
{
	const bool bRests = State == EGameFeaturePluginState::Installed || State == EGameFeaturePluginState::Registered ||
		State == EGameFeaturePluginState::Loaded || State == EGameFeaturePluginState::Active;
	switch (State)
	{
#define MCP_GAME_FEATURE_STATE(Name, Text) case EGameFeaturePluginState::Name: return bRests ? FString(TEXT(#Name)).ToLower() : FString(TEXT(#Name));
	GAME_FEATURE_PLUGIN_STATE_LIST(MCP_GAME_FEATURE_STATE)
#undef MCP_GAME_FEATURE_STATE
	default: return TEXT("Unknown");
	}
}

class FMcpGameFeaturesProvider final : public IMcpGameFeaturesProvider
{
public:
	virtual TArray<FMcpGameFeatureRow> List() const override
	{
		TArray<FMcpGameFeatureRow> Rows;
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetDiscoveredPlugins())
		{
			FMcpGameFeatureRow Row;
			if (Find(Plugin->GetName(), Row))
			{
				Rows.Add(MoveTemp(Row));
			}
		}
		Rows.Sort([](const FMcpGameFeatureRow& A, const FMcpGameFeatureRow& B) { return A.PluginName.Compare(B.PluginName, ESearchCase::CaseSensitive) < 0; });
		return Rows;
	}

	virtual bool Find(const FString& PluginName, FMcpGameFeatureRow& OutRow) const override
	{
		FString Url;
		UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();
		if (!Subsystem.GetPluginURLByName(PluginName, Url))
		{
			return false;
		}
		OutRow.PluginName = PluginName;
		OutRow.State = McpGameFeatureState(Subsystem.GetPluginState(Url));
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		const TSharedPtr<FJsonObject> Json = Plugin.IsValid() ? Plugin->GetDescriptor().CachedJson : nullptr;
		if (Json.IsValid())
		{
			Json->TryGetStringField(TEXT("BuiltInInitialFeatureState"), OutRow.InitialState);
		}
		return true;
	}

	virtual bool SetActive(const FString& PluginName, bool bActivate, TFunction<void(const FString& Error)> OnDone) override
	{
		FString Url;
		UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();
		if (!Subsystem.GetPluginURLByName(PluginName, Url))
		{
			return false;
		}
		const auto Done = [OnDone = MoveTemp(OnDone)](const UE::GameFeatures::FResult& Result) { OnDone(Result.HasError() ? Result.GetError() : FString()); };
		if (bActivate)
		{
			Subsystem.LoadAndActivateGameFeaturePlugin(Url, FGameFeaturePluginLoadComplete::CreateLambda(Done));
		}
		else
		{
			Subsystem.DeactivateGameFeaturePlugin(Url, FGameFeaturePluginDeactivateComplete::CreateLambda(Done));
		}
		return true;
	}

	// The engine's own Game Feature template takes the same steps (GameFeaturePluginTemplate.cpp OnPluginCreated), but
	// leaves the asset unsaved, and registering looks for its package on disk (FPackageName::DoesPackageExist).
	virtual bool AdoptNewPlugin(const FString& PluginName, TFunctionRef<bool(UObject* GameFeatureData)> Save) override
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		UObject* Data = Plugin.IsValid() ? FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(
			PluginName, Plugin->GetMountedAssetPath(), UGameFeatureData::StaticClass(), nullptr) : nullptr;
		if (!Data || !Save(Data))
		{
			return false;
		}
		UGameFeaturesSubsystem::Get().LoadBuiltInGameFeaturePlugin(Plugin.ToSharedRef(),
			[](const FString&, const FGameFeaturePluginDetails&, FBuiltInGameFeaturePluginBehaviorOptions&) { return true; });
		return true;
	}
};

FMcpGameFeaturesProvider GProvider;
} // namespace
#endif

class FEngineWireGameFeaturesModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
		IModularFeatures::Get().RegisterModularFeature(IMcpGameFeaturesProvider::FeatureName(), &GProvider);
#endif
	}

	virtual void ShutdownModule() override
	{
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
		IModularFeatures::Get().UnregisterModularFeature(IMcpGameFeaturesProvider::FeatureName(), &GProvider);
#endif
	}
};

IMPLEMENT_MODULE(FEngineWireGameFeaturesModule, EngineWireGameFeatures)
