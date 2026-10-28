// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

class UObject;

/** One game feature plugin as the adapter reads it. */
struct FMcpGameFeatureRow
{
	FString PluginName;
	/** The subsystem's state for it, simplified as Epic's toolset does: installed, registered, loaded, active, or the
	 *  engine's own name for a transition or error state. */
	FString State;
	/** What the plugin asks for at startup (its descriptor's BuiltInInitialFeatureState), empty when it says nothing. */
	FString InitialState;
};

/**
 * The seam that keeps the GameFeatures plugin out of the core module, as IMcpFabProvider keeps Fab out: the adapter
 * module EngineWireGameFeatures links GameFeatures and registers this feature, and it is loaded only where the
 * project enables that plugin. Core refuses GAME_FEATURES_DISABLED when nobody answered. Game thread only.
 */
class IMcpGameFeaturesProvider : public IModularFeature
{
public:
	static FName FeatureName() { return FName(TEXT("McpGameFeaturesProvider")); }

	virtual ~IMcpGameFeaturesProvider() = default;

	/** Every game feature plugin the subsystem knows, name-sorted. */
	virtual TArray<FMcpGameFeatureRow> List() const = 0;

	/** Its row; false when PluginName is no game feature plugin the subsystem knows. */
	virtual bool Find(const FString& PluginName, FMcpGameFeatureRow& OutRow) const = 0;

	/**
	 * Starts activating (or deactivating) a game feature plugin. OnDone runs once on the game thread with the engine's
	 * error text, empty on success. False (and OnDone never runs) when the plugin is no game feature plugin.
	 */
	virtual bool SetActive(const FString& PluginName, bool bActivate, TFunction<void(const FString& Error)> OnDone) = 0;

	/**
	 * Gives a just-made content-only plugin what the Game Feature template gives one: its GameFeatureData asset, named
	 * after the plugin at its content root, written by Save (the subsystem looks for it on disk: an unsaved one fails
	 * registering), then hands the plugin to the subsystem, which registers it and takes it to the state its descriptor
	 * asks for over the next frames. False, with nothing handed over, when the asset could not be made or saved.
	 */
	virtual bool AdoptNewPlugin(const FString& PluginName, TFunctionRef<bool(UObject* GameFeatureData)> Save) = 0;
};
