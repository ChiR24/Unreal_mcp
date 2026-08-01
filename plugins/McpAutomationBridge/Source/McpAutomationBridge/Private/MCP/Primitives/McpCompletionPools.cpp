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

	// The TS legacy id is parentTool + the canonical id's action segment (the part
	// after the first dot), matching legacyIds [{tool: parentTool, action}].
	FString LegacyCompletionId(const FMcpCapabilityRecord& Record)
	{
		int32 Dot = INDEX_NONE;
		if (!Record.Id.FindChar(TEXT('.'), Dot))
		{
			return FString();
		}
		return Record.Parent + TEXT(".") + Record.Id.RightChop(Dot + 1);
	}
}  // namespace

const TArray<FString>& McpCompletionClassAliasHandles()
{
	return ClassAliasHandles();
}

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
			const FString Legacy = LegacyCompletionId(Record);
			if (!Legacy.IsEmpty() && !Seen.Contains(Legacy))
			{
				Seen.Add(Legacy);
				Out.Add({ Legacy, TEXT("legacy-id"), Record.Id });
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
