#include "Foundation/McpCapabilityAuthorization.h"

#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"

// Payload path collection for the pre-queue gate.
//
// Split out of McpCapabilityAuthorization.cpp so both files stay under the
// plugin's 250 pure-line ceiling; `CollectPayloadPaths` remains declared in
// McpCapabilityAuthorization.h and is the same namespace, so no caller changes.
//
// The rule this file exists to enforce: CANONICALIZE, THEN CHECK. Comparing a
// raw payload string against a literal `/Game/` prefix is what let `/Content/...`,
// `\Content\...` and bare-relative aliases walk past path confinement and get
// rooted afterwards by the handler.
//
// The scan is BOUNDED and therefore fallible. Every early return caused by a
// bound sets `bTruncated`, because a scan that stopped early proves nothing
// about the values it never reached; the gate turns that into a refusal rather
// than admitting a request on an incomplete answer.

namespace McpCapabilityAuthorization
{
namespace
{
// Bounds for the payload path scan. A hostile client cannot make the gate walk
// an unbounded structure before its request is even queued.
constexpr int32 MaxScanDepth = 8;
constexpr int32 MaxScanNodes = 4096;

struct FScanState
{
	FMcpPayloadPathScan* Out = nullptr;
	int32 NodeBudget = MaxScanNodes;
	// Set when the principal is path-restricted. Only then does the scan widen to
	// bare-relative values and to path-shaped values with no trustworthy
	// canonical form, so an unrestricted principal keeps its previous behaviour.
	bool bStrict = false;
};

void CollectString(const FString& Text, const FString& Key, FScanState& State)
{
	const bool bAssumeGameRoot = State.bStrict && IsPathParameterKey(Key);
	const FString Canonical = McpCanonicalizeContentPath(Text, bAssumeGameRoot);
	if (!Canonical.IsEmpty())
	{
		State.Out->Paths.AddUnique(Canonical);
		return;
	}
	if (!State.bStrict)
	{
		return;
	}
	// Canonicalization refused this value (traversal or colon) but the executor
	// could still root it. Carry the raw string so containment refuses it,
	// instead of dropping it and admitting the request by default.
	if (McpIsUnrealRootedCandidate(Text))
	{
		State.Out->Paths.AddUnique(Text);
		return;
	}
	// A value under a path key that is RELATIVE would be rooted at /Game by the
	// handler. One that is already absolute under a non-UE root (an OS import
	// source) never becomes a content path, so it stays outside confinement.
	if (bAssumeGameRoot && !Text.StartsWith(TEXT("/")) && !Text.StartsWith(TEXT("\\")))
	{
		State.Out->Paths.AddUnique(Text);
	}
}

void ScanValue(
	const TSharedPtr<FJsonValue>& Value, const FString& Key, int32 Depth, FScanState& State);

void ScanObject(const TSharedPtr<FJsonObject>& Object, int32 Depth, FScanState& State)
{
	if (!Object.IsValid())
	{
		return;
	}
	if (Depth > MaxScanDepth)
	{
		State.Out->bTruncated = true;
		return;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		if (State.NodeBudget <= 0)
		{
			State.Out->bTruncated = true;
			return;
		}
		ScanValue(Pair.Value, Pair.Key, Depth + 1, State);
	}
}

void ScanValue(
	const TSharedPtr<FJsonValue>& Value, const FString& Key, int32 Depth, FScanState& State)
{
	if (!Value.IsValid())
	{
		return;
	}
	if (Depth > MaxScanDepth || State.NodeBudget <= 0)
	{
		State.Out->bTruncated = true;
		return;
	}
	--State.NodeBudget;

	if (Value->Type == EJson::String)
	{
		CollectString(Value->AsString(), Key, State);
		return;
	}
	if (Value->Type == EJson::Object)
	{
		ScanObject(Value->AsObject(), Depth, State);
		return;
	}
	if (Value->Type == EJson::Array)
	{
		// Array elements inherit the array's key, so `paths: ["Secret"]` is
		// rooted exactly like the scalar `path: "Secret"` would be.
		for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
		{
			ScanValue(Element, Key, Depth + 1, State);
		}
	}
}
} // namespace

// Keys whose value a handler roots at `/Game` even with no separator, so
// `packagePath: "Secret"` becomes `/Game/Secret`. Value shape alone would miss
// those; a key allowlist alone would miss an unusual key. The gate does both.
//
// DELIBERATELY ABSENT: `propertyPath`, `objectPath`, `pathStartsWith` and
// `classPath` address a reflection member, a filter or a class — they are not
// content-write targets, and rooting them at /Game would deny ordinary traffic.
bool IsPathParameterKey(const FString& Key)
{
	static const TCHAR* const PathKeys[] = {
		TEXT("path"), TEXT("paths"), TEXT("folder"), TEXT("folderpath"), TEXT("packagepath"),
		TEXT("packagename"), TEXT("assetpath"), TEXT("assetpaths"), TEXT("assetfolder"),
		TEXT("sourcepath"), TEXT("sourceassetpath"), TEXT("sourcefolder"), TEXT("sourcedirectory"),
		TEXT("destinationpath"), TEXT("destinationassetpath"), TEXT("destinationfolder"),
		TEXT("destinationdirectory"), TEXT("destpath"), TEXT("destfolder"),
		TEXT("targetpath"), TEXT("targetfolder"), TEXT("targetdirectory"),
		TEXT("savepath"), TEXT("savedirectory"), TEXT("directory"), TEXT("directorypath"),
		TEXT("parentpath"), TEXT("contentpath"), TEXT("contentfolder"),
		TEXT("outputpath"), TEXT("outputfolder"), TEXT("outputdirectory"), TEXT("exportpath"),
		TEXT("importpath"), TEXT("basepath"), TEXT("rootpath"), TEXT("newpath"), TEXT("oldpath"),
		TEXT("frompath"), TEXT("topath"),
		TEXT("blueprintpath"), TEXT("blueprintassetpath"), TEXT("widgetpath"),
		TEXT("levelpath"), TEXT("levelassetpath"), TEXT("sublevelpath"), TEXT("worldpath"),
		TEXT("mappath"), TEXT("hlodlayerpath"), TEXT("datalayerassetpath"),
		TEXT("materialpath"), TEXT("meshpath"), TEXT("staticmeshpath"), TEXT("skeletalmeshpath"),
		TEXT("skeletonpath"), TEXT("physicsassetpath"), TEXT("animationpath"), TEXT("animpath"),
		TEXT("texturepath"), TEXT("rendertargetpath"), TEXT("sequencepath"), TEXT("audiopath"),
		TEXT("soundpath"), TEXT("niagarapath"), TEXT("curvepath"), TEXT("datatablepath"),
		TEXT("structpath"), TEXT("enumpath")
	};
	const FString Lower = Key.ToLower();
	for (const TCHAR* const Candidate : PathKeys)
	{
		if (Lower.Equals(Candidate))
		{
			return true;
		}
	}
	return false;
}

FMcpPayloadPathScan CollectPayloadPaths(const TSharedPtr<FJsonObject>& Payload, bool bStrict)
{
	FMcpPayloadPathScan Scan;
	FScanState State;
	State.Out = &Scan;
	State.bStrict = bStrict;
	ScanObject(Payload, 0, State);
	return Scan;
}
} // namespace McpCapabilityAuthorization
