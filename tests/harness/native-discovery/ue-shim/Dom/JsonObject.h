// FJsonObject shim.
//
// Field lookup is case-insensitive because UE's TMap<FString, ...> hashes and
// compares FString case-insensitively. Two sibling JSON keys differing only by
// case therefore COLLIDE in the engine; the harness reproduces that rather than
// hiding it, and a parity test asserts the canonical registry never relies on
// such a pair.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

struct FCaseInsensitiveLess
{
	bool operator()(const FString& L, const FString& R) const
	{
		return L.Compare(R, ESearchCase::IgnoreCase) < 0;
	}
};

class FJsonObject
{
public:
	TMap<FString, TSharedPtr<FJsonValue>, FCaseInsensitiveLess> Values;

	void SetStringField(const FString& Name, const FString& Value) { Values[Name] = MakeShared<FJsonValueString>(Value); }
	void SetBoolField(const FString& Name, bool Value) { Values[Name] = MakeShared<FJsonValueBoolean>(Value); }
	void SetNumberField(const FString& Name, double Value) { Values[Name] = MakeShared<FJsonValueNumber>(Value); }
	void SetObjectField(const FString& Name, const TSharedPtr<FJsonObject>& Value) { Values[Name] = MakeShared<FJsonValueObject>(Value); }
	void SetArrayField(const FString& Name, const TArray<TSharedPtr<FJsonValue>>& Value) { Values[Name] = MakeShared<FJsonValueArray>(Value); }

	bool HasField(const FString& Name) const { return Values.Contains(Name); }

	bool TryGetStringField(const FString& Name, FString& Out) const
	{
		const TSharedPtr<FJsonValue>* Found = Values.Find(Name);
		if (!Found || !*Found || (*Found)->Type != EJson::String) return false;
		Out = (*Found)->AsString();
		return true;
	}

	bool TryGetBoolField(const FString& Name, bool& Out) const
	{
		const TSharedPtr<FJsonValue>* Found = Values.Find(Name);
		if (!Found || !*Found || (*Found)->Type != EJson::Boolean) return false;
		Out = (*Found)->AsBool();
		return true;
	}

	bool TryGetNumberField(const FString& Name, int32& Out) const
	{
		const TSharedPtr<FJsonValue>* Found = Values.Find(Name);
		if (!Found || !*Found || (*Found)->Type != EJson::Number) return false;
		Out = static_cast<int32>((*Found)->AsNumber());
		return true;
	}

	bool TryGetObjectField(const FString& Name, const TSharedPtr<FJsonObject>*& Out) const
	{
		const TSharedPtr<FJsonValue>* Found = Values.Find(Name);
		if (!Found || !*Found || (*Found)->Type != EJson::Object) return false;
		Cache[Name] = (*Found)->AsObject();
		Out = &Cache[Name];
		return true;
	}

	bool TryGetArrayField(const FString& Name, const TArray<TSharedPtr<FJsonValue>>*& Out) const
	{
		const TSharedPtr<FJsonValue>* Found = Values.Find(Name);
		if (!Found || !*Found || (*Found)->Type != EJson::Array) return false;
		Out = &(*Found)->AsArray();
		return true;
	}

	FString GetStringField(const FString& Name) const
	{
		FString Value;
		TryGetStringField(Name, Value);
		return Value;
	}

	TSharedPtr<FJsonObject> GetObjectField(const FString& Name) const
	{
		const TSharedPtr<FJsonValue>* Found = Values.Find(Name);
		return !Found || !*Found ? nullptr : (*Found)->AsObject();
	}

private:
	mutable TMap<FString, TSharedPtr<FJsonObject>, FCaseInsensitiveLess> Cache;
};
