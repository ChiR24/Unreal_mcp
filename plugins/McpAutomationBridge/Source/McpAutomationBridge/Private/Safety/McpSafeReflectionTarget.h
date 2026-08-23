// McpSafeReflectionTarget.h — which objects the generic reflection surface may address.
//
// `get_object_property`, `set_object_property` and the array/map/set element
// handlers take a caller-supplied `objectPath` and then read or write ANY
// reflected property on whatever it resolves to. That is the right shape for
// authoring content, and the wrong shape for the engine's own in-memory
// configuration: object resolution accepts `/Script/` paths, and a `/Script/`
// path names a native class object or its Class Default Object.
//
// The CDO of UMcpAutomationBridgeSettings lives at
// `/Script/McpAutomationBridge.Default__McpAutomationBridgeSettings` and carries
// `CapabilityToken`, `ScopedCapabilityTokens`, `bRequireCapabilityToken` and
// `bAllowNonLoopback` as reflected UPROPERTYs. The authorization path re-reads
// that CDO per connection, and PostEditChange() persists edits to DefaultGame.ini.
// Without this guard a `write`-scoped principal could turn the plugin's own token
// requirement off through `inspect.set_property` (policy: write / consent none)
// and a `read`-scoped principal could read the Admin token straight out through
// `inspect.get_property` — the plugin reconfiguring its own authentication from
// its own automation surface. The same primitive reaches every other engine and
// plugin CDO reachable at `/Script/...`.
//
// So: script packages are not addressable through the reflection surface, for
// every principal and every scope. Content packages (/Game, /Engine, mounted
// roots), spawned actors and Blueprint CDOs — whose outermost is the Blueprint's
// own content package — are unaffected. Class INTROSPECTION reaches its targets
// through dedicated capabilities: `inspect_class` and `get_project_settings`
// read fixed field sets rather than arbitrary reflection, and `set_project_setting`
// (an arbitrary ini write) is separately denied for the plugin's own settings
// section in McpAutomationBridge_UiHandlersProjectSettings, so nothing
// legitimate loses a route here.
//
// Deliberate boundary: the transient package (`/Engine/Transient`, the outer of
// live engine singletons and editor subsystems) remains addressable. No
// auth-relevant or ini-persisting target has been found there - UDeveloperSettings
// are reached as their /Script CDOs and are blocked - and /Engine *content*
// packages must stay addressable, so refusing the whole transient package is not
// attempted. If a reflected write to a live subsystem ever becomes a route to
// persisted configuration, revisit this line before adding any other one.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace McpSafeReflectionTarget
{
/** Shared refusal message, so every call site reports the boundary identically. */
inline const TCHAR* DenyMessage()
{
	return TEXT("Objects in /Script packages are not addressable through the property "
	            "reflection surface. Address project content (/Game/...), a spawned "
	            "actor, or a Blueprint CDO via blueprintPath instead.");
}

/** Stable error code for a refusal, distinct from OBJECT_NOT_FOUND so a caller can
 *  tell "you may not address that" from "there is nothing there". */
inline const TCHAR* DenyCode()
{
	return TEXT("OBJECT_NOT_ADDRESSABLE");
}

/**
 * True when Object may be read or written through the generic property handlers.
 *
 * FAIL CLOSED: a null object, or one with no outermost package, is not addressable.
 * The rule is the OUTERMOST package, not the class or the RF_ClassDefaultObject
 * flag — a Blueprint CDO is a legitimate authoring target and lives in a content
 * package, while every native CDO lives under /Script and none is.
 */
inline bool IsAddressable(const UObject* Object)
{
	if (Object == nullptr)
	{
		return false;
	}
	const UPackage* Outermost = Object->GetOutermost();
	if (Outermost == nullptr)
	{
		return false;
	}
	const FString PackageName = Outermost->GetName();
	return !PackageName.Equals(TEXT("/Script"), ESearchCase::IgnoreCase)
		&& !PackageName.StartsWith(TEXT("/Script/"), ESearchCase::IgnoreCase);
}

/**
 * FindObject narrowed to reflection-addressable targets. Returns nullptr both when
 * nothing resolves and when what resolved may not be addressed, so a call site that
 * only checks for null stays correct; OutDenied distinguishes the two for call
 * sites that report the boundary explicitly.
 */
inline UObject* FindAddressableObject(const FString& ObjectPath, bool* OutDenied = nullptr)
{
	if (OutDenied != nullptr)
	{
		*OutDenied = false;
	}
	UObject* Found = FindObject<UObject>(nullptr, *ObjectPath);
	if (Found == nullptr)
	{
		return nullptr;
	}
	if (!IsAddressable(Found))
	{
		if (OutDenied != nullptr)
		{
			*OutDenied = true;
		}
		return nullptr;
	}
	return Found;
}
}  // namespace McpSafeReflectionTarget
