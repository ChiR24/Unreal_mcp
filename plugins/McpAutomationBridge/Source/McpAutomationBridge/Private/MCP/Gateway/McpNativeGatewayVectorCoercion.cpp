// McpNativeGatewayVectorCoercion.cpp — vector-shaped argument coercion before schema validation.
//
// Some parents declare location/rotation/scale/size/color as arrays, others as
// {x,y,z} / {width,height} objects, and every handler accepts both spellings. The
// gateway converts between the two shapes instead of refusing with
// INVALID_PARAMETER_TYPE (dogfood #226). Mirrors coerceVectorShapes in
// src/server/gateway/gateway-schema-validate.ts.
//
// Matching is strict: a candidate key set only wins when the caller's keys are a
// subset of the set and every set member that would be emitted is a finite number.
// Anything else (a typo'd key, a short or over-long array, a non-numeric component)
// falls through to the schema's guided type error instead of being silently truncated
// or having keys dropped. In a set of four keys the last one (w / a) is optional, so
// three of four are required. The key sets and their order are pinned against the
// TypeScript VECTOR_KEY_SETS by tests/unit/tools/vector-shape-coercion-parity.test.ts.
#include "MCP/Execute/McpNativeGatewaySchemaValidation.h"

#include "Math/UnrealMathUtility.h"

namespace
{
	struct FVectorKeySet { const TCHAR*const* Keys; int32 Count; int32 RequiredCount; };

	const TCHAR* const XyzwKeys[] = { TEXT("x"), TEXT("y"), TEXT("z"), TEXT("w") };
	const TCHAR* const XyzKeys[] = { TEXT("x"), TEXT("y"), TEXT("z") };
	const TCHAR* const PyrKeys[] = { TEXT("pitch"), TEXT("yaw"), TEXT("roll") };
	const TCHAR* const RgbaKeys[] = { TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a") };
	const TCHAR* const WhKeys[] = { TEXT("width"), TEXT("height") };
	const TCHAR* const XyKeys[] = { TEXT("x"), TEXT("y") };

	const FVectorKeySet GVectorKeySets[] = {
		{ XyzwKeys, 4, 3 },
		{ XyzKeys, 3, 3 },
		{ PyrKeys, 3, 3 },
		{ RgbaKeys, 4, 3 },
		{ WhKeys, 2, 2 },
		{ XyKeys, 2, 2 },
	};

	bool DeclaresType(const TSharedPtr<FJsonObject>& PropertySchema, const TCHAR* TypeName)
	{
		FString Single;
		if (PropertySchema->TryGetStringField(TEXT("type"), Single))
		{
			return Single == TypeName;
		}
		const TArray<TSharedPtr<FJsonValue>>* Many = nullptr;
		if (PropertySchema->TryGetArrayField(TEXT("type"), Many) && Many)
		{
			for (const TSharedPtr<FJsonValue>& Entry : *Many)
			{
				if (Entry.IsValid() && Entry->Type == EJson::String && Entry->AsString() == TypeName) return true;
			}
		}
		return false;
	}

	bool IsFiniteNumber(const TSharedPtr<FJsonValue>& Value)
	{
		double Number = 0.0;
		return Value.IsValid() && Value->Type == EJson::Number
			&& Value->TryGetNumber(Number) && FMath::IsFinite(Number);
	}

	bool ObjectKeysAreSubset(const TSharedPtr<FJsonObject>& Object, const FVectorKeySet& Set)
	{
		for (const auto& Pair : Object->Values)
		{
			bool bInSet = false;
			for (int32 Index = 0; Index < Set.Count && !bInSet; ++Index)
			{
				bInSet = Pair.Key == Set.Keys[Index];
			}
			if (!bInSet) return false;
		}
		return true;
	}

	TSharedPtr<FJsonValue> ObjectToVector(const TSharedPtr<FJsonObject>& Object)
	{
		if (!Object.IsValid()) return nullptr;
		for (const FVectorKeySet& Set : GVectorKeySets)
		{
			// Per set, as in the TypeScript twin: a key outside THIS set disqualifies the object for this set only.
			if (!ObjectKeysAreSubset(Object, Set)) continue;
			TArray<TSharedPtr<FJsonValue>> Values;
			bool bOk = true;
			for (int32 Index = 0; Index < Set.Count && bOk; ++Index)
			{
				const TSharedPtr<FJsonValue> Field = Object->TryGetField(Set.Keys[Index]);
				if (IsFiniteNumber(Field)) { Values.Add(MakeShared<FJsonValueNumber>(Field->AsNumber())); }
				else if (Index < Set.RequiredCount || Field.IsValid()) { bOk = false; }
			}
			if (bOk && Values.Num() >= Set.RequiredCount) return MakeShared<FJsonValueArray>(Values);
		}
		return nullptr;
	}

	TSharedPtr<FJsonValue> VectorToObject(const TArray<TSharedPtr<FJsonValue>>& Values, const TSharedPtr<FJsonObject>& PropertySchema)
	{
		const TSharedPtr<FJsonObject>* Declared = nullptr;
		if (!PropertySchema->TryGetObjectField(TEXT("properties"), Declared) || !Declared
			|| (*Declared)->Values.Num() == 0)
		{
			return nullptr;
		}
		for (const FVectorKeySet& Set : GVectorKeySets)
		{
			bool bDeclared = Values.Num() >= Set.RequiredCount;
			for (int32 Index = 0; Index < Set.RequiredCount && bDeclared; ++Index)
			{
				bDeclared = (*Declared)->HasField(Set.Keys[Index]);
			}
			if (!bDeclared) continue;
			int32 Consumed = 0;
			for (int32 Index = 0; Index < Set.Count; ++Index)
			{
				if ((*Declared)->HasField(Set.Keys[Index])) ++Consumed;
			}
			// Over-long arrays would silently drop their tail; short arrays below the
			// required prefix would fabricate handler defaults. Both refuse instead.
			if (Values.Num() > Consumed) continue;
			TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
			for (int32 Index = 0; Index < Set.Count && Index < Values.Num(); ++Index)
			{
				if ((*Declared)->HasField(Set.Keys[Index])) Out->SetNumberField(Set.Keys[Index], Values[Index]->AsNumber());
			}
			return MakeShared<FJsonValueObject>(Out);
		}
		return nullptr;
	}
}

TSharedPtr<FJsonObject> McpCoerceCanonicalVectorShapes(const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonObject>& Schema)
{
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (!Params.IsValid() || !Schema.IsValid() || !Schema->TryGetObjectField(TEXT("properties"), Properties) || !Properties)
	{
		return Params;
	}
	TSharedPtr<FJsonObject> Out;
	for (const auto& Pair : (*Properties)->Values)
	{
		const TSharedPtr<FJsonObject>* PropertySchema = nullptr;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(PropertySchema) || !PropertySchema) continue;
		const TSharedPtr<FJsonValue> Value = Params->TryGetField(Pair.Key);
		if (!Value.IsValid()) continue;
		TSharedPtr<FJsonValue> Replacement;
		const bool bWantsArray = DeclaresType(*PropertySchema, TEXT("array")) && !DeclaresType(*PropertySchema, TEXT("object"));
		const bool bWantsObject = DeclaresType(*PropertySchema, TEXT("object")) && !DeclaresType(*PropertySchema, TEXT("array"));
		if (bWantsArray && Value->Type == EJson::Object)
		{
			Replacement = ObjectToVector(Value->AsObject());
		}
		else if (bWantsObject && Value->Type == EJson::Array)
		{
			Replacement = VectorToObject(Value->AsArray(), *PropertySchema);
		}
		if (Replacement.IsValid())
		{
			if (!Out.IsValid()) { Out = MakeShared<FJsonObject>(); Out->Values = Params->Values; }
			Out->SetField(Pair.Key, Replacement);
		}
	}
	return Out.IsValid() ? Out : Params;
}
