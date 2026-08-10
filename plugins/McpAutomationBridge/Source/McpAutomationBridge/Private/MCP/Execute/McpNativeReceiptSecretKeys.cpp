// McpNativeReceiptSecretKeys.cpp — key-NAME credential classification.
//
// Native mirror of isSecretKey() in
// src/tools/catalog/capabilities/semantic/receipt-redaction.ts.
//
// McpMaskSecrets needs a `keyword<sep>value` run WITHIN one string, so a
// credential that arrives as a real JSON value has no keyword context left to
// match — the key it hangs under is the only signal, and the invariant (a secret
// never reaches a receipt) makes that signal sufficient. Split out of
// McpNativeReceiptRedaction.cpp to keep that file inside the 250 pure-line
// ceiling; the declaration lives in McpNativeReceiptRedaction.h beside the
// masking entry points that consume it.

#include "MCP/Execute/McpNativeReceiptRedaction.h"

namespace
{
const TSet<FString>& SecretKeyWords()
{
	static const TSet<FString> Words = {
		TEXT("token"), TEXT("secret"), TEXT("password"), TEXT("passwd"), TEXT("pwd"),
		TEXT("apikey"), TEXT("authorization"), TEXT("credential"), TEXT("privatekey"),
		TEXT("accesskey"), TEXT("signingkey")};
	return Words;
}

// Heads carrying no meaning of their own: the real head sits one word left.
const TSet<FString>& TransparentHeads()
{
	static const TSet<FString> Words = {
		TEXT("value"), TEXT("data"), TEXT("string"), TEXT("text"), TEXT("raw"), TEXT("plain")};
	return Words;
}

// Heads that make a compound a MEASUREMENT or STATUS rather than a credential.
// This is the ONLY thing that suppresses masking once a secret word is present,
// so the default is to mask: `secretKey` and `passwordHash` name real
// credentials, and an unrecognised head must never be assumed harmless.
const TSet<FString>& MeasurementHeads()
{
	static const TSet<FString> Words = {
		TEXT("count"), TEXT("budget"), TEXT("length"), TEXT("size"), TEXT("limit"),
		TEXT("total"), TEXT("max"), TEXT("min"), TEXT("index"), TEXT("order"),
		TEXT("position"), TEXT("offset"), TEXT("depth"), TEXT("age"), TEXT("ttl"),
		TEXT("found"), TEXT("required"), TEXT("enabled"), TEXT("disabled"),
		TEXT("expired"), TEXT("valid"), TEXT("present"), TEXT("missing"),
		TEXT("used"), TEXT("remaining"), TEXT("supported"), TEXT("allowed"),
		TEXT("name"), TEXT("id"), TEXT("type"), TEXT("kind"), TEXT("label"),
		TEXT("status"), TEXT("state"), TEXT("mode"), TEXT("policy"), TEXT("rule"),
		TEXT("scheme"), TEXT("algorithm"), TEXT("format"), TEXT("source"),
		TEXT("reason"), TEXT("message"), TEXT("error"), TEXT("version"),
		TEXT("timestamp"), TEXT("time"), TEXT("date"), TEXT("duration"), TEXT("at")};
	return Words;
}

// Qualifiers that precede a credential noun. They name no secret alone, so they
// matter only when reuniting a compound that carried no separator.
const TSet<FString>& SecretQualifiers()
{
	static const TSet<FString> Words = {
		TEXT("api"), TEXT("access"), TEXT("auth"), TEXT("private"), TEXT("public"),
		TEXT("client"), TEXT("server"), TEXT("session"), TEXT("user"), TEXT("admin"),
		TEXT("root"), TEXT("master"), TEXT("signing"), TEXT("refresh"), TEXT("bearer"),
		TEXT("oauth"), TEXT("app"), TEXT("service"), TEXT("encryption"), TEXT("shared"),
		TEXT("secret")};
	return Words;
}

// Credential nouns that can close a compound. `secret` + `key` is a credential
// even though `key` alone is far too generic to live in SecretKeyWords.
const TSet<FString>& CredentialTails()
{
	static const TSet<FString> Words = {
		TEXT("key"), TEXT("token"), TEXT("secret"), TEXT("password"), TEXT("passwd"),
		TEXT("pwd"), TEXT("credential"), TEXT("authorization"), TEXT("signature"),
		TEXT("cert"), TEXT("certificate")};
	return Words;
}

bool IsSplittableWord(const FString& Word)
{
	return SecretKeyWords().Contains(Word) || SecretQualifiers().Contains(Word)
		|| CredentialTails().Contains(Word) || MeasurementHeads().Contains(Word)
		|| TransparentHeads().Contains(Word);
}

// Depluralisation is tried as an ALTERNATIVE rather than applied up front,
// because stripping unconditionally mangles words that merely end in `s`.
FString Singular(const FString& Word)
{
	return (Word.Len() > 1 && Word.EndsWith(TEXT("s"))) ? Word.LeftChop(1) : Word;
}

bool InSet(const TSet<FString>& Set, const FString& Word)
{
	return Set.Contains(Word) || Set.Contains(Singular(Word));
}

// `SECRETKEY` and `ACCESSTOKEN` carry no camelCase boundary and no separator, so
// they arrive as one unrecognised word and escape masking entirely — while
// `SECRET_KEY` and `secretKey` are both masked. Recover the boundary by
// splitting a single run into two KNOWN words, at least one naming a credential.
// Requiring both halves to be in the closed vocabulary is what stops `tokenizer`
// becoming token + izer and `passwordless` becoming password + less.
void AppendCompound(const FString& Word, TArray<FString>& Out)
{
	if (Word.Len() >= 6 && !IsSplittableWord(Word))
	{
		for (int32 Cut = 2; Cut <= Word.Len() - 2; ++Cut)
		{
			const FString Head = Word.Left(Cut);
			const FString Tail = Word.RightChop(Cut);
			if (!IsSplittableWord(Head) || !IsSplittableWord(Tail))
			{
				continue;
			}
			if (SecretKeyWords().Contains(Head) || SecretKeyWords().Contains(Tail)
				|| CredentialTails().Contains(Tail))
			{
				Out.Add(Head);
				Out.Add(Tail);
				return;
			}
		}
	}
	Out.Add(Word);
}

// Split on camelCase transitions and any non-alphanumeric run. Matching WHOLE
// words keeps `tokenizer`, `passwordless` and `unauthorized` out of the set.
TArray<FString> KeyWords(const FString& Key)
{
	TArray<FString> Words;
	FString Current;
	for (int32 Index = 0; Index < Key.Len(); ++Index)
	{
		const TCHAR Ch = Key[Index];
		if (!FChar::IsAlnum(Ch))
		{
			if (!Current.IsEmpty())
			{
				AppendCompound(Current.ToLower(), Words);
				Current.Reset();
			}
			continue;
		}
		const bool bBoundary = !Current.IsEmpty() && FChar::IsUpper(Ch)
			&& (FChar::IsLower(Key[Index - 1]) || FChar::IsDigit(Key[Index - 1]));
		if (bBoundary)
		{
			AppendCompound(Current.ToLower(), Words);
			Current.Reset();
		}
		Current.AppendChar(Ch);
	}
	if (!Current.IsEmpty())
	{
		AppendCompound(Current.ToLower(), Words);
	}
	return Words;
}

bool NamesCredential(const TArray<FString>& Words)
{
	for (int32 Index = 0; Index < Words.Num(); ++Index)
	{
		if (InSet(SecretKeyWords(), Words[Index]))
		{
			return true;
		}
		if (Index + 1 < Words.Num())
		{
			const FString Joined = Words[Index] + Words[Index + 1];
			const FString JoinedSingular = Singular(Words[Index]) + Singular(Words[Index + 1]);
			if (InSet(SecretKeyWords(), Joined) || InSet(SecretKeyWords(), JoinedSingular))
			{
				return true;
			}
		}
	}
	return false;
}
}  // namespace

bool McpIsSecretKey(const FString& Key)
{
	// FAIL-CLOSED: once any whole word names a credential the value is masked
	// unless the head word proves the compound merely measures or describes it.
	const TArray<FString> Words = KeyWords(Key);
	if (!NamesCredential(Words))
	{
		return false;
	}
	int32 End = Words.Num();
	while (End > 1 && InSet(TransparentHeads(), Words[End - 1]))
	{
		--End;
	}
	return End < 1 || !InSet(MeasurementHeads(), Words[End - 1]);
}
