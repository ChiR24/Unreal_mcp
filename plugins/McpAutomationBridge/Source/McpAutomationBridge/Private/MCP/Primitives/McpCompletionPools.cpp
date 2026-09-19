#include "MCP/Primitives/McpCompletionPools.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Resources/McpResourceUri.h"

const TArray<FMcpCompletionCandidate>& McpCapabilityCompletionPool()
{
	static const TArray<FMcpCompletionCandidate> Pool = []()
	{
		TArray<FMcpCompletionCandidate> Out;
		TSet<FString> Seen;
		for (const FMcpCapabilityRecord& Record : FMcpCapabilityStore::Get().GetRecords())
		{
			if (!Record.Id.IsEmpty() && !Seen.Contains(Record.Id))
			{
				Seen.Add(Record.Id);
				Out.Add({ Record.Id, TEXT("capability"), Record.Id });
			}
			// Mirrors completion-sources.ts buildCapabilityPool: every declared
			// alias and every {tool}.{action} pair (a folded family's old names
			// included) completes as a legacy id tagged with the canonical id.
			for (const FString& Alias : Record.Aliases)
			{
				if (!Alias.IsEmpty() && !Seen.Contains(Alias))
				{
					Seen.Add(Alias);
					Out.Add({ Alias, TEXT("legacy-id"), Record.Id });
				}
			}
			for (const FMcpLegacyPair& Pair : Record.LegacyPairs)
			{
				const FString Legacy = Pair.Tool + TEXT(".") + Pair.Action;
				if (!Pair.Tool.IsEmpty() && !Pair.Action.IsEmpty() && !Seen.Contains(Legacy))
				{
					Seen.Add(Legacy);
					Out.Add({ Legacy, TEXT("legacy-id"), Record.Id });
				}
			}
		}
		return Out;
	}();
	return Pool;
}

// Mirrors the TS buildProjectHandlePool (completion-sources.ts): the UE content
// mount roots, sorted. This pool used to serve the ACTOR_CLASS_ALIASES keys on
// both surfaces, which meant every suggestion failed the mount-root rule the
// object/asset resource templates enforce and came back as an invalid URI.
const TArray<FMcpCompletionCandidate>& McpProjectHandleCompletionPool()
{
	static const TArray<FMcpCompletionCandidate> Pool = []()
	{
		TArray<FString> Roots = McpResourceUri::ContentRoots();
		Roots.Sort();
		TArray<FMcpCompletionCandidate> Out;
		for (const FString& Root : Roots)
		{
			Out.Add({ Root, TEXT("project-handle"), FString() });
		}
		return Out;
	}();
	return Pool;
}

TSet<FString> McpEnabledCapabilityIds(TFunctionRef<bool(const FString&)> IsParentEnabled)
{
	TSet<FString> Enabled;
	for (const FMcpCapabilityRecord& Record : FMcpCapabilityStore::Get().GetRecords())
	{
		if (IsParentEnabled(Record.Parent))
		{
			Enabled.Add(Record.Id);
		}
	}
	return Enabled;
}
