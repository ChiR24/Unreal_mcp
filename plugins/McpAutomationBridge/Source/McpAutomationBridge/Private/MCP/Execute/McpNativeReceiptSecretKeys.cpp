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
// even though `key` alone is far too generic to live in SecretKeyWords. Must stay
// byte-identical to CREDENTIAL_TAILS in receipt-redaction.ts.
const TSet<FString>& CredentialTails()
{
	static const TSet<FString> Words = {
		TEXT("key"), TEXT("token"), TEXT("secret"), TEXT("password"), TEXT("passwd"),
		TEXT("pwd"), TEXT("credential"), TEXT("authorization"), TEXT("signature"),
		TEXT("cert"), TEXT("certificate"), TEXT("hash"), TEXT("bytes"), TEXT("digest"),
		TEXT("header"), TEXT("blob")};
	return Words;
}

bool IsSplittableWord(const FString& Word)
{
	return SecretKeyWords().Contains(Word) || SecretQualifiers().Contains(Word)
		|| CredentialTails().Contains(Word) || MeasurementHeads().Contains(Word)
		|| TransparentHeads().Contains(Word);
}

// Enumerates every complete segmentation of Word into `Parts` splittable words,
// leftmost cut first, mirroring segmentations() in receipt-redaction.ts. The
// credential predicate is deliberately NOT applied here: TS applies
// splitNamesCredential() only to the COMPLETE part list (the `.find()` over the
// full enumeration), so filtering sub-segmentations during recursion would reject
// a split whose first part is the credential word while its tail is not itself
// credential-named, and leave the native surface masking less than the stdio
// surface.
void CollectSegmentations(const FString& Word, int32 Parts,
	TArray<TArray<FString>>& Out)
{
	if (Parts == 1)
	{
		if (IsSplittableWord(Word))
		{
			TArray<FString> Single;
			Single.Add(Word);
			Out.Add(MoveTemp(Single));
		}
		return;
	}
	for (int32 Cut = 2; Cut <= Word.Len() - 2; ++Cut)
	{
		const FString Head = Word.Left(Cut);
		if (!IsSplittableWord(Head))
		{
			continue;
		}
		TArray<TArray<FString>> Rests;
		CollectSegmentations(Word.RightChop(Cut), Parts - 1, Rests);
		for (TArray<FString>& Rest : Rests)
		{
			Rest.Insert(Head, 0);
			Out.Add(MoveTemp(Rest));
		}
	}
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

// Mirrors splitNamesCredential() in receipt-redaction.ts: ANY part naming a
// secret key word, or the LAST part being a credential tail, makes the split a
// credential.
bool PartsNameCredential(const TArray<FString>& Parts)
{
	for (const FString& Part : Parts)
	{
		if (SecretKeyWords().Contains(Part))
		{
			return true;
		}
	}
	return Parts.Num() > 0 && CredentialTails().Contains(Parts.Last());
}

// `SECRETKEY` and `ACCESSTOKEN` carry no camelCase boundary and no separator, so
// they arrive as one unrecognised word and escape masking entirely — while
// `SECRET_KEY` and `secretKey` are both masked. Recover the boundary by
// splitting a single run into KNOWN words, at least one naming a credential.
// Requiring both halves to be in the closed vocabulary is what stops `tokenizer`
// becoming token + izer and `passwordless` becoming password + less.
// Two parts cannot reach `APIACCESSTOKEN`, so the search runs to three - fewest
// parts first, mirroring segmentations() in receipt-redaction.ts.
void AppendCompound(const FString& Word, TArray<FString>& Out)
{
	if (Word.Len() < 6 || IsSplittableWord(Word))
	{
		Out.Add(Word);
		return;
	}
	for (int32 Parts = 2; Parts <= 3; ++Parts)
	{
		TArray<TArray<FString>> Segmentations;
		CollectSegmentations(Word, Parts, Segmentations);
		for (const TArray<FString>& Segmentation : Segmentations)
		{
			if (PartsNameCredential(Segmentation))
			{
				Out.Append(Segmentation);
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

// Keys that carry a value but name no subject of their own, and keys that name
// the subject a sibling carries. The reflection handlers answer in exactly this
// split shape — `{propertyName: "CapabilityToken", value: "<token>"}` — which
// defeats key-name classification twice over: `value` names nothing, and
// `propertyName` holds a name rather than a secret, so masking it would hide the
// question instead of the answer.
const TSet<FString>& GenericValueKeys()
{
	static const TSet<FString> Words = {
		TEXT("value"), TEXT("values"), TEXT("currentvalue"), TEXT("previousvalue"),
		TEXT("oldvalue"), TEXT("newvalue"), TEXT("defaultvalue"), TEXT("element"),
		TEXT("elements"), TEXT("entry"), TEXT("entries"), TEXT("result")};
	return Words;
}

const TSet<FString>& NameBearingKeys()
{
	static const TSet<FString> Words = {
		TEXT("propertyname"), TEXT("propertypath"), TEXT("property"), TEXT("field"),
		TEXT("key"), TEXT("settingname"), TEXT("setting"), TEXT("name")};
	return Words;
}
}  // namespace

// The compound splitter is O(n^2) on a separator-less run, and a receipt key is
// caller-influenced (echoed map keys and property names), so an over-long key is
// skipped rather than classified — the same bound the sibling-name path applies.
constexpr int32 McpMaxKeyNameLength = 128;

bool McpIsSecretKey(const FString& Key)
{
	if (Key.Len() > McpMaxKeyNameLength)
	{
		return false;
	}
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

bool McpIsGenericValueKey(const FString& Key)
{
	return GenericValueKeys().Contains(Key.ToLower());
}

// The NAME the caller asked for decides, judged by the same classifier a key
// would face. `{name:"Foo", value:"bar"}` is untouched; only a name that itself
// reads as a credential masks its sibling. Mirrors namesCredentialBySibling()
// in receipt-redaction.ts, including the length bound: the name is caller-
// supplied and the compound splitter is O(n^2) on a separator-less run, so an
// over-long name is skipped rather than classified.
constexpr int32 McpMaxSiblingNameLength = 128;

bool McpNamesCredentialBySibling(const TSharedPtr<FJsonObject>& Object)
{
	if (!Object.IsValid())
	{
		return false;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		if (!NameBearingKeys().Contains(Pair.Key.ToLower()))
		{
			continue;
		}
		FString Named;
		if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String
			|| !Pair.Value->TryGetString(Named))
		{
			continue;
		}
		if (Named.Len() > McpMaxSiblingNameLength)
		{
			continue;
		}
		// The NAME the caller asked for decides, judged by the same classifier a
		// key would face. `{name:"Foo", value:"bar"}` is untouched; only a name
		// that itself reads as a credential masks its sibling.
		if (McpIsSecretKey(Named))
		{
			return true;
		}
	}
	return false;
}
