// TJsonReader shim: carries the source text for the serializer to parse.
#pragma once

#include "CoreMinimal.h"

template <typename CharType = TCHAR>
class TJsonReader
{
public:
	explicit TJsonReader(FString InSource) : Source(std::move(InSource)) {}
	const FString& GetSource() const { return Source; }
private:
	FString Source;
};

template <typename CharType = TCHAR>
struct TJsonReaderFactory
{
	static TSharedRef<TJsonReader<CharType>> Create(const FString& Source)
	{
		return MakeShared<TJsonReader<CharType>>(Source);
	}
};
