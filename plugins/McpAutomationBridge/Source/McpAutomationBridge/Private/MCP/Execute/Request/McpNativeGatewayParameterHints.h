#pragma once

#include "CoreMinimal.h"

// Ranking behind the native gateway's "Undeclared parameter" text, mirrored
// byte-for-byte by the TypeScript describeUndeclaredParameter.
//
// A wrong parameter name is usually a synonym rather than a typo, so substring
// matching alone drew no "did you mean" for propertyValue when the caller sent
// defaultValue, and the alphabetical truncation then spent all 24 listed slots
// on names beginning with 'a' while dropping the one that would have worked.
// Affinity counts a shared camelCase token as well as a substring match, and
// the scoring names are listed first so truncation keeps the plausible ones.
namespace McpParameterHints
{
// camelCase -> lowercase word tokens: "propertyValue" -> { "property", "value" }.
// Splits on a lower/digit followed by an upper (the TypeScript /([a-z0-9])([A-Z])/
// boundary) and on any run of non-alphanumeric characters.
inline TArray<FString> ParameterNameTokens(const FString& Name)
{
	auto IsAsciiAlnum = [](TCHAR Ch) {
		return (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z')) ||
		       (Ch >= TEXT('0') && Ch <= TEXT('9'));
	};
	auto IsAsciiUpper = [](TCHAR Ch) { return Ch >= TEXT('A') && Ch <= TEXT('Z'); };
	auto IsAsciiLowerOrDigit = [](TCHAR Ch) {
		return (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('0') && Ch <= TEXT('9'));
	};
	TArray<FString> Tokens;
	FString Current;
	for (int32 Index = 0; Index < Name.Len(); ++Index)
	{
		const TCHAR Ch = Name[Index];
		if (!IsAsciiAlnum(Ch))
		{
			if (!Current.IsEmpty()) { Tokens.Add(Current); Current.Reset(); }
			continue;
		}
		if (Index > 0 && IsAsciiUpper(Ch) && IsAsciiLowerOrDigit(Name[Index - 1]) && !Current.IsEmpty())
		{
			Tokens.Add(Current);
			Current.Reset();
		}
		Current.AppendChar(FChar::ToLower(Ch));
	}
	if (!Current.IsEmpty()) { Tokens.Add(Current); }
	return Tokens;
}

// 2 for a substring match in either direction, +1 for any shared token. 0 means
// the name has nothing to do with what the caller sent.
inline int32 ParameterNameAffinity(
	const FString& Name, const FString& LowerKey, const TArray<FString>& KeyTokens)
{
	const FString Lower = Name.ToLower();
	int32 Score = (Lower.Contains(LowerKey) || LowerKey.Contains(Lower)) ? 2 : 0;
	for (const FString& Token : ParameterNameTokens(Name))
	{
		if (KeyTokens.Contains(Token)) { ++Score; break; }
	}
	return Score;
}

// Returns Declared reordered so related names come first, and fills OutHint with
// the "did you mean 'x'; " prefix when anything scored. Declared must already be
// case-sensitively sorted; ties fall back to shortest-then-byte-order.
inline TArray<FString> RankParameterNames(
	const FString& Key, const TArray<FString>& Declared, FString& OutHint)
{
	const FString LowerKey = Key.ToLower();
	const TArray<FString> KeyTokens = ParameterNameTokens(Key);
	TArray<FString> Near;
	TArray<FString> Rest;
	for (const FString& Name : Declared)
	{
		if (ParameterNameAffinity(Name, LowerKey, KeyTokens) > 0) { Near.Add(Name); }
		else { Rest.Add(Name); }
	}
	Near.Sort([&LowerKey, &KeyTokens](const FString& A, const FString& B) {
		const int32 ScoreA = ParameterNameAffinity(A, LowerKey, KeyTokens);
		const int32 ScoreB = ParameterNameAffinity(B, LowerKey, KeyTokens);
		if (ScoreA != ScoreB) { return ScoreA > ScoreB; }
		return A.Len() != B.Len() ? A.Len() < B.Len() : A.Compare(B, ESearchCase::CaseSensitive) < 0;
	});
	OutHint = Near.Num() > 0 ? FString::Printf(TEXT("did you mean '%s'; "), *Near[0]) : FString();
	TArray<FString> Ranked = MoveTemp(Near);
	Ranked.Append(Rest);
	return Ranked;
}
}
