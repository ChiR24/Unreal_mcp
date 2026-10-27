#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FMcpBridgeWebSocket;
class UBlueprint;
class UDataTable;
class UEngineWireSubsystem;

// asset diff_asset: how an asset differs from its saved file or from another asset. Read-only: the other side is
// loaded for diffing (a copy of the saved file in a /Temp package) and released afterwards.
struct FMcpAssetDiff
{
	static constexpr int32 MaxRows = 50;

	FString LivePackage, OtherPackage, LiveName, OtherName;
	TArray<TSharedPtr<FJsonValue>> Properties, Components, Graphs, Rows;
	bool bTruncated = false;
	/** Properties DiffContainers leaves out: bookkeeping, or what another list reports. */
	TSet<FName> Skip;

	/** The other side's text with its package and asset name read as the asset's own, so equal references compare equal. */
	FString Normalize(const FString& OtherText) const;
	/** Adds a row unless the list holds MaxRows already. */
	void Add(TArray<TSharedPtr<FJsonValue>>& List, const TSharedPtr<FJsonObject>& Row);
	/** Every saved property of two containers of possibly different types (a class and its saved copy), matched by name. */
	void DiffContainers(const FString& Object, const UStruct* LiveType, const void* Live, const UStruct* OtherType, const void* Other);
};

namespace McpAssetDiff
{
	/** Class defaults, components and graphs (McpAssetDiffBlueprint.cpp). */
	void DiffBlueprint(FMcpAssetDiff& Diff, UBlueprint* Live, UBlueprint* Other);
	/** Rows added, removed or changed (McpAssetDiffBlueprint.cpp). */
	void DiffDataTable(FMcpAssetDiff& Diff, UDataTable* Live, UDataTable* Other);

	bool HandleDiffAsset(UEngineWireSubsystem* Bridge, const FString& RequestId,
		const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
}
