// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabProvider.h"

#include "Features/IModularFeatures.h"

/**
 * Resolves the Fab adapter, or nothing.
 *
 * Looked up per call rather than cached: the adapter registers during its own
 * module startup, which can land after a core handler has already run once, and
 * a plugin disabled mid-session unregisters.
 */
IMcpFabProvider* GetMcpFabProvider()
{
	IModularFeatures& Features = IModularFeatures::Get();
	const FName Name = IMcpFabProvider::FeatureName();
	if (Features.GetModularFeatureImplementationCount(Name) == 0)
	{
		return nullptr;
	}
	return &Features.GetModularFeature<IMcpFabProvider>(Name);
}
