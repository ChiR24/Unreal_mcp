#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Field helpers of the reply compaction (McpJsonRpcReplyCompaction.cpp: execute; McpJsonRpcReplyDiscovery.cpp:
// describe and search). Every helper reads; a change is always made on a Copy, never on the reply.
namespace McpJsonRpcReplyFields
{
using FJsonArray = TArray<TSharedPtr<FJsonValue>>;

inline TSharedPtr<FJsonObject> Copy(const TSharedPtr<FJsonObject>& Source)
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->Values = Source->Values;
	return Out;
}

inline TSharedPtr<FJsonObject> Without(const TSharedPtr<FJsonObject>& Source, const FString& Field)
{
	TSharedPtr<FJsonObject> Out = Copy(Source);
	Out->RemoveField(Field);
	return Out;
}

inline TSharedPtr<FJsonObject> AsObject(const TSharedPtr<FJsonValue>& Value)
{
	const TSharedPtr<FJsonObject>* Object = nullptr;
	return Value.IsValid() && Value->TryGetObject(Object) && Object && Object->IsValid() ? *Object : nullptr;
}

inline TSharedPtr<FJsonObject> ObjectField(const TSharedPtr<FJsonObject>& Source, const FString& Field)
{
	return Source.IsValid() ? AsObject(Source->TryGetField(Field)) : nullptr;
}

inline const FJsonArray* ArrayField(const TSharedPtr<FJsonObject>& Source, const FString& Field)
{
	const FJsonArray* Value = nullptr;
	return Source.IsValid() && Source->TryGetArrayField(Field, Value) ? Value : nullptr;
}

// Both present and equal. Strings compare case-sensitively (FString == does not), as the TypeScript twin does.
inline bool Same(const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
{
	if (!Left.IsValid() || !Right.IsValid()) return false;
	if (Left->Type == EJson::String && Right->Type == EJson::String)
	{
		return Left->AsString().Equals(Right->AsString(), ESearchCase::CaseSensitive);
	}
	return FJsonValue::CompareEqual(*Left, *Right);
}

inline bool SameField(const TSharedPtr<FJsonObject>& Left, const FString& LeftKey,
	const TSharedPtr<FJsonObject>& Right, const FString& RightKey)
{
	return Left.IsValid() && Right.IsValid() && Same(Left->TryGetField(LeftKey), Right->TryGetField(RightKey));
}

inline bool IsTrue(const TSharedPtr<FJsonObject>& Source, const FString& Field)
{
	bool bValue = false;
	return Source->TryGetBoolField(Field, bValue) && bValue;
}

TSharedPtr<FJsonObject> CompactExecute(const TSharedPtr<FJsonObject>& Reply);
TSharedPtr<FJsonObject> CompactDescribe(const TSharedPtr<FJsonObject>& Reply);
TSharedPtr<FJsonObject> CompactSearch(const TSharedPtr<FJsonObject>& Reply);
}
