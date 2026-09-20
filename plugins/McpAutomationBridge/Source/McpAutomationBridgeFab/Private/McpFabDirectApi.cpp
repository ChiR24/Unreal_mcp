// Copyright (c) 2024 MCP Automation Bridge Contributors
//
// Drives Fab through Unreal reflection instead of its web page.
//
// The rest of this module talks to Fab by executing JavaScript inside the Fab
// tab, on the premise that no C++ entry point exists. That premise is wrong for
// a large part of the surface: UFabBrowserApi is a UCLASS and the import
// entry points are UFUNCTIONs --
//
//     UFUNCTION() void AddToProject(const FString& DownloadUrl, const FFabAssetMetadata&);
//     UFUNCTION() void InstallToProject(const FString& DownloadUrl, const FFabAssetMetadata&);
//     UFUNCTION() void InstallToEngine(const FString& DownloadUrl, const FFabAssetMetadata&);
//
// UFUNCTIONs are reachable with FindFunctionByName + ProcessEvent, so importing
// needs no Fab tab, no click, and no link against Fab's private headers (the
// class carries no _API export, so linking is not an option anyway).
//
// Deliberately no credential handling here. UFabBrowserApi also exposes
// GetAuthToken/GetRefreshToken, and it would be possible to read the signed-in
// session token and mint download URLs against Fab's REST API directly -- but
// that would put a live account credential through this process and into
// whatever logs it touches. The token stays inside Fab: callers pass a download
// URL that Fab itself issued, and Fab's own importer does the authenticated
// work.

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabDirect, Log, All);

namespace McpFabDirectApi
{
/** The Fab API class, or nullptr when the Fab plugin is absent or not loaded. */
UClass* FindFabBrowserApiClass()
{
	// Resolved by path so this compiles and links on engines with no Fab at all
	// (Fab does not exist across the whole 5.0-5.8 range the plugin advertises).
	return FindObject<UClass>(nullptr, TEXT("/Script/Fab.FabBrowserApi"));
}

/**
 * Opens a Fab tab purely through reflection -- no menu, no click, no UI path.
 *
 * UFabBrowserApi::OpenInNewTab is a UFUNCTION that forwards to
 * FFabBrowser::OpenInNewTab, which is a one-line wrapper over the private
 * CreateNewFabTab. Calling the UFUNCTION therefore spawns exactly the tab the
 * menu item spawns, without touching ToolMenus or Slate input.
 *
 * 5.8 and later only: the UFUNCTION does not exist on Fab 0.0.10 (UE 5.7) or
 * earlier, so callers keep the menu-entry route as a fallback.
 */
bool TryOpenFabTabViaReflection(FString& OutDiagnostic)
{
	UClass* ApiClass = FindFabBrowserApiClass();
	if (!ApiClass)
	{
		OutDiagnostic = TEXT("Fab plugin not loaded.");
		return false;
	}
	UFunction* OpenFn = ApiClass->FindFunctionByName(FName(TEXT("OpenInNewTab")));
	if (!OpenFn)
	{
		OutDiagnostic = TEXT("No 'OpenInNewTab' UFUNCTION on this Fab version (pre-0.0.17).");
		return false;
	}
	UObject* Api = NewObject<UObject>(GetTransientPackage(), ApiClass);
	if (!Api)
	{
		OutDiagnostic = TEXT("Could not construct UFabBrowserApi.");
		return false;
	}
	// One FString parameter. Empty means Fab's own default landing page, which is
	// what the menu item passes.
	struct FOpenInNewTabParams { FString Url; } Params;
	Api->ProcessEvent(OpenFn, &Params);
	OutDiagnostic = TEXT("OpenInNewTab dispatched.");
	return true;
}

/** True when Fab's reflected import surface is present on this engine. */
bool IsDirectImportAvailable(FString& OutDiagnostic)
{
	UClass* ApiClass = FindFabBrowserApiClass();
	if (!ApiClass)
	{
		OutDiagnostic = TEXT("Fab plugin not loaded on this engine "
							 "(no /Script/Fab.FabBrowserApi class).");
		return false;
	}
	// The reflected surface moves between engine versions, so probe rather than
	// assume. Verified against shipped headers: Fab 0.0.10 (UE 5.7) has
	// AddToProject but NOT InstallToProject/InstallToEngine/RegisterCallbacks;
	// Fab 0.0.17 (UE 5.8) adds all three. Requiring the 5.8-only names would
	// wrongly report 5.7 as unsupported, so only AddToProject is mandatory.
	if (!ApiClass->FindFunctionByName(FName(TEXT("AddToProject"))))
	{
		OutDiagnostic = TEXT("UFabBrowserApi exists but has no 'AddToProject' UFUNCTION "
							 "on this engine version.");
		return false;
	}
	const bool bHasInstallToProject =
		ApiClass->FindFunctionByName(FName(TEXT("InstallToProject"))) != nullptr;
	OutDiagnostic = FString::Printf(
		TEXT("available (AddToProject yes, InstallToProject %s)"),
		bHasInstallToProject ? TEXT("yes") : TEXT("no - pre-0.0.17 Fab"));
	return true;
}
} // namespace McpFabDirectApi

/**
 * Reports whether Fab can be driven by reflection on THIS engine, with no tab.
 *
 * Fab's surface moves between engine versions, so this answers for the running
 * build rather than from an assumption.
 */
static FAutoConsoleCommand GMcpFabDirectProbe(
	TEXT("Mcp.Fab.DirectProbe"),
	TEXT("Reports whether Fab's import UFUNCTIONs are reachable by reflection, with no Fab tab."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		FString Diagnostic;
		const bool bAvailable = McpFabDirectApi::IsDirectImportAvailable(Diagnostic);
		UE_LOG(LogMcpFabDirect, Log, TEXT("Mcp.Fab.DirectProbe: %s (%s)"),
			bAvailable ? TEXT("AVAILABLE") : TEXT("unavailable"), *Diagnostic);

		if (UClass* ApiClass = McpFabDirectApi::FindFabBrowserApiClass())
		{
			int32 Count = 0;
			for (TFieldIterator<UFunction> It(ApiClass); It; ++It)
			{
				UE_LOG(LogMcpFabDirect, Log, TEXT("  UFUNCTION: %s"), *It->GetName());
				++Count;
			}
			UE_LOG(LogMcpFabDirect, Log, TEXT("  %d reflected function(s) total."), Count);
		}
	}));
