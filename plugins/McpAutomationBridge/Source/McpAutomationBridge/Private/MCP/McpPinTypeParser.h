// McpPinTypeParser.h — Bidirectional conversion between type DSL strings and FEdGraphPinType.
//
// Supported DSL:
//   Scalars : Bool Byte Int Int64 Float Double String Name Text
//   Structs : Vector Vector2D Rotator Transform Color LinearColor
//   Objects : <ClassName>*  (e.g. Texture2D*, StaticMesh*)
//   Classes : TSubclassOf<X>
//   Enums   : E_<Name>      (UUserDefinedEnum via AssetRegistry)
//   Structs : S_<Name>      (UUserDefinedStruct via AssetRegistry)
//   BP refs : BP_<Name> / WBP_<Name> / ABP_<Name>  (GeneratedClass of the Blueprint)
//   Arrays  : Array<T> Set<T> Map<K,V>  (no nested containers)

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphPin.h"
#endif

class FMcpPinTypeParser
{
public:
#if WITH_EDITOR
	/**
	 * Parse a type DSL string into FEdGraphPinType.
	 * Returns true on success; on failure OutError describes the reason.
	 */
	static bool Parse(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutError);

	/**
	 * Serialize a FEdGraphPinType back to the canonical DSL string.
	 * Always produces a string; unknown shapes serialize to "Unknown<...>" with details
	 * in OutWarning so callers can detect lossy round-trips.
	 */
	static FString Serialize(const FEdGraphPinType& PinType, FString& OutWarning);
#endif
};
