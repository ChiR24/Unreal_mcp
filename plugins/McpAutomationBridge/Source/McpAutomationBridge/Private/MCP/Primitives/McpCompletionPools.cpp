#include "MCP/Primitives/McpCompletionPools.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

namespace
{
	// Mirror the ACTOR_CLASS_ALIASES keys (src/config/class-aliases.ts), the safe
	// cached project handles the TS projectHandleCandidates draws from.
	//
	// KNOWN GAP mirrored from TS: these bare class names do not satisfy the
	// mount-root rule the object/asset resource templates enforce, so every
	// suggestion is refused on read. Emitting the alias target paths would
	// resolve, but the TS safety test forbids a project handle that looks like a
	// path. Both surfaces stay wrong the SAME way rather than diverging.
	const TArray<FString>& ClassAliasHandles()
	{
		static const TArray<FString> Handles = {
			TEXT("Actor"), TEXT("BlockingVolume"), TEXT("Camera"), TEXT("CameraActor"),
			TEXT("Character"), TEXT("DirectionalLight"), TEXT("Pawn"), TEXT("PlayerStart"),
			TEXT("PointLight"), TEXT("RectLight"), TEXT("SkeletalMeshActor"), TEXT("Spline"),
			TEXT("SplineActor"), TEXT("SpotLight"), TEXT("StaticMeshActor"), TEXT("TriggerBox"),
			TEXT("TriggerSphere"),
		};
		return Handles;
	}

}  // namespace

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

const TArray<FMcpCompletionCandidate>& McpProjectHandleCompletionPool()
{
	static const TArray<FMcpCompletionCandidate> Pool = []()
	{
		TArray<FMcpCompletionCandidate> Out;
		for (const FString& Handle : ClassAliasHandles())
		{
			Out.Add({ Handle, TEXT("project-handle"), FString() });
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
