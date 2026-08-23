// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabAddScript.h"

#include "McpFabBridgeDispatch.h"

#include "HAL/IConsoleManager.h"
#include "Misc/EngineVersion.h"
#include "Misc/Guid.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabAdd, Log, All);

namespace
{
/** "5.7" from the running engine, used to pick the matching listing version. */
FString ShortEngineVersion()
{
	const FEngineVersion& Version = FEngineVersion::Current();
	return FString::Printf(TEXT("%u.%u"), Version.GetMajor(), Version.GetMinor());
}
} // namespace

namespace McpFabAddOperation
{
FString CurrentEngineVersion() { return ShortEngineVersion(); }
} // namespace McpFabAddOperation

/**
 * Starts the chain for one listing. Completion here means Fab accepted the
 * workflow, not that assets exist -- Unreal-side verification is the next step.
 *
 * Console-command bring-up only: the same path is the MCP capability
 * add_fab_asset_to_project, and the generated console-command policy blocks
 * Mcp.Fab.AddToProject on both transports so a write-scoped principal cannot
 * reach this without the capability's explicit consent.
 */
static FAutoConsoleCommand GMcpFabAddToProject(
	TEXT("Mcp.Fab.AddToProject"),
	TEXT("Resolves a listing's download inside Fab's page and hands it to Fab's own importer. Usage: Mcp.Fab.AddToProject <listingUid>"),
	FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
	{
		if (Args.Num() != 1 || !McpFabAddOperation::IsSafeListingIdShared(Args[0]))
		{
			UE_LOG(LogMcpFabAdd, Warning,
				TEXT("Mcp.Fab.AddToProject: expected one listing uid of [A-Za-z0-9_-], 64 chars max."));
			return;
		}
		FString Error;
		FString ErrorCode;
		if (!McpFabBridgeDispatch::Dispatch(
				[&Args](const FString& RequestId)
				{
					return McpFabAddOperation::BuildAddScript(
						RequestId, Args[0], McpFabAddOperation::CurrentEngineVersion());
				},
				[](bool bSuccess, const FString& Payload)
				{
					UE_LOG(LogMcpFabAdd, Log, TEXT("add result (%s): %s"),
						bSuccess ? TEXT("ok") : TEXT("failed"), *Payload);
				},
				Error, ErrorCode))
		{
			UE_LOG(LogMcpFabAdd, Warning,
				TEXT("Mcp.Fab.AddToProject: %s (%s)"), *Error, *ErrorCode);
		}
	}));
