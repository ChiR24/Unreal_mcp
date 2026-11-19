// What this build of EngineWire compiled, by name. A capability record's availability.compiled lists the features its
// handler needs; a build that left one out (an engine or package without it) reads the capability unavailable and
// execute refuses it FEATURE_UNAVAILABLE. Each feature registers itself inside its own compile guard, so the names
// follow detection and no version number is repeated here.
#pragma once

#include "CoreMinimal.h"

namespace McpCompiledFeatures
{
/** True when a source of this build registered Name with MCP_COMPILED_FEATURE. */
bool Has(const FString& Name);

struct FRegistrar
{
	explicit FRegistrar(const TCHAR* Name);
};
} // namespace McpCompiledFeatures

// At namespace scope, inside the block of the feature's own compile guard (never inside a function: a function-local
// static registers only once that function runs).
#define MCP_COMPILED_FEATURE(Name) static const McpCompiledFeatures::FRegistrar GMcpCompiledFeature_##Name(TEXT(#Name));
