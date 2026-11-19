#include "Core/Compatibility/McpCompiledFeatures.h"

namespace McpCompiledFeatures
{
namespace
{
// Filled while the module's statics construct, before any request; only read after that.
TSet<FString>& Registered()
{
	static TSet<FString> Names;
	return Names;
}
} // namespace

FRegistrar::FRegistrar(const TCHAR* Name)
{
	Registered().Add(Name);
}

bool Has(const FString& Name)
{
	return Registered().Contains(Name);
}
} // namespace McpCompiledFeatures
