#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;
class UBlackboardData;
class UBlackboardKeyType;
class UClass;
struct FBlackboardEntry;

namespace McpAIHandlers
{
// The Blackboard at blackboardPath, or null with OutError and OutCode: INVALID_ARGUMENT, NOT_FOUND, and for an edit
// ENGINE_CONTENT (engine content is read, never changed).
UBlackboardData* LoadBlackboard(const TSharedPtr<FJsonObject>& Payload, bool bForEdit, FString& OutError, FString& OutCode);

// A key type's class as the Blackboard editor names the type: Bool, Int, Object.
FString BlackboardKeyTypeName(const UClass* KeyClass);

// The key named KeyName among Blackboard's own keys, or null with OutError naming the keys it has, or the parent that holds it.
FBlackboardEntry* FindOwnBlackboardKey(UBlackboardData* Blackboard, const FString& KeyName, FString& OutError, FString& OutCode);

// What the Blackboard editor runs after a key change: synced keys are recounted, Blackboards deriving from this
// one pick the change up and open editors refresh.
void NotifyBlackboardKeysChanged(UBlackboardData* Blackboard);

// Value as the key's default, the value it starts with in every Blackboard made from the asset: UE 5.4+ for Bool, Int,
// Float, Vector, Rotator, Name and String keys, 5.5+ for Enum, Object and Class keys. False, nothing changed, with
// OutError and OutCode for a value the key cannot take or a type (or engine version) without a default.
bool WriteBlackboardKeyDefault(UBlackboardKeyType* KeyType, const TSharedPtr<FJsonValue>& Value, FString& OutError, FString& OutCode);

// The key's default as text; false for a type (or an engine version) with none, or a vector or rotator left unset.
bool ReadBlackboardKeyDefault(const UBlackboardKeyType* KeyType, FString& OutText);
}
