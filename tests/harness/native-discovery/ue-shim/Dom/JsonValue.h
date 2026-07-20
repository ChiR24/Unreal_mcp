// FJsonValue shim. Mirrors the UE type hierarchy the discovery sources use.
#pragma once

#include "CoreMinimal.h"

enum class EJson : uint8 { None, Null, String, Number, Boolean, Array, Object };

class FJsonObject;

class FJsonValue
{
public:
	EJson Type = EJson::Null;
	virtual ~FJsonValue() = default;

	virtual FString AsString() const { return FString(); }
	virtual bool AsBool() const { return false; }
	virtual double AsNumber() const { return 0.0; }
	virtual const TArray<TSharedPtr<FJsonValue>>& AsArray() const { static TArray<TSharedPtr<FJsonValue>> Empty; return Empty; }
	virtual TSharedPtr<FJsonObject> AsObject() const { return nullptr; }

	bool TryGetString(FString& Out) const
	{
		if (Type != EJson::String) return false;
		Out = AsString();
		return true;
	}
};

class FJsonValueNull : public FJsonValue
{
public:
	FJsonValueNull() { Type = EJson::Null; }
};

class FJsonValueString : public FJsonValue
{
public:
	explicit FJsonValueString(FString In) : Value(std::move(In)) { Type = EJson::String; }
	FString AsString() const override { return Value; }
private:
	FString Value;
};

class FJsonValueNumber : public FJsonValue
{
public:
	explicit FJsonValueNumber(double In) : Value(In) { Type = EJson::Number; }
	double AsNumber() const override { return Value; }
	FString AsString() const override { return FString::Printf(TEXT("%g"), Value); }
private:
	double Value;
};

class FJsonValueBoolean : public FJsonValue
{
public:
	explicit FJsonValueBoolean(bool In) : Value(In) { Type = EJson::Boolean; }
	bool AsBool() const override { return Value; }
private:
	bool Value;
};

class FJsonValueArray : public FJsonValue
{
public:
	explicit FJsonValueArray(TArray<TSharedPtr<FJsonValue>> In) : Value(std::move(In)) { Type = EJson::Array; }
	const TArray<TSharedPtr<FJsonValue>>& AsArray() const override { return Value; }
private:
	TArray<TSharedPtr<FJsonValue>> Value;
};

class FJsonValueObject : public FJsonValue
{
public:
	explicit FJsonValueObject(TSharedPtr<FJsonObject> In) : Value(std::move(In)) { Type = EJson::Object; }
	TSharedPtr<FJsonObject> AsObject() const override { return Value; }
private:
	TSharedPtr<FJsonObject> Value;
};
