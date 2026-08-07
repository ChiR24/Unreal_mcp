// Minimal Unreal Engine API shim for the Task-25 native discovery harness.
//
// This exists so the REAL native discovery sources compile and run under a
// host compiler, without UnrealBuildTool or an engine install. It implements
// only the surface those sources use, and it deliberately mirrors UE semantics
// that could otherwise mask a bug:
//   * FString operator== / operator< are case-INSENSITIVE, exactly like UE's.
//   * FJsonObject field lookup is case-insensitive, like UE's TMap<FString,...>.
//   * TArray::Sort is an unstable sort, like UE's introsort.
// A comparison that only works because the shim is stricter than UE would be
// worthless evidence, so the shim is never more forgiving than the engine.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// FString::ParseIntoArrayWS fills a TArray, but TArray is declared below it, so
// the method is declared inside FString and defined once TArray is complete.
template <typename T> class TArray;

using int32 = std::int32_t;
using int64 = std::int64_t;
using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
using TCHAR = wchar_t;

// UE's "not found" sentinel. FString::FindLastChar below reports it on failure,
// so it has to be declared before FString.
constexpr int32 INDEX_NONE = -1;

#define TEXT(x) L##x
#define check(expr) assert(expr)
#define UE_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define FORCEINLINE inline

enum class ESearchCase : uint8 { CaseSensitive, IgnoreCase };

template <typename T> using TFunctionRef = std::function<T>;
template <typename T> constexpr T&& MoveTemp(T& value) { return static_cast<T&&>(value); }
template <typename T> void Swap(T& a, T& b) { std::swap(a, b); }

// UE's TPair spells its members `.Key` / `.Value`, not std::pair's `.first` /
// `.second`, and the canonical-JSON writer carries key/value together in a
// TArray<TPair<...>> because FJsonObject's key type changed in UE 5.8.
//
// The two-argument constructor forwards rather than taking `const&`, so an
// rvalue key and an lvalue value bind in the same call. Being a TWO-parameter
// template it can never hijack the one-parameter copy or move constructor.
template <typename K, typename V>
struct TPair
{
	K Key{};
	V Value{};

	TPair() = default;

	template <typename InKeyType, typename InValueType>
	TPair(InKeyType&& InKey, InValueType&& InValue)
		: Key(std::forward<InKeyType>(InKey))
		, Value(std::forward<InValueType>(InValue))
	{
	}
};

class FString
{
public:
	FString() = default;
	FString(const TCHAR* Raw) : Data(Raw ? Raw : L"") {}
	explicit FString(std::wstring Raw) : Data(std::move(Raw)) {}

	int32 Len() const { return static_cast<int32>(Data.size()); }
	bool IsEmpty() const { return Data.empty(); }
	const TCHAR* operator*() const { return Data.c_str(); }
	TCHAR operator[](int32 Index) const { return Data[static_cast<size_t>(Index)]; }
	const std::wstring& Raw() const { return Data; }

	void Reset() { Data.clear(); }
	void Append(const FString& Other) { Data += Other.Data; }
	void Append(const TCHAR* Other) { if (Other) Data += Other; }
	void AppendChar(TCHAR Ch) { Data.push_back(Ch); }

	FString ToLower() const
	{
		std::wstring Lower = Data;
		for (wchar_t& Ch : Lower) Ch = static_cast<wchar_t>(std::towlower(Ch));
		return FString(Lower);
	}

	FString TrimStartAndEnd() const
	{
		size_t Begin = 0;
		size_t End = Data.size();
		while (Begin < End && std::iswspace(Data[Begin])) ++Begin;
		while (End > Begin && std::iswspace(Data[End - 1])) --End;
		return FString(Data.substr(Begin, End - Begin));
	}

	bool Contains(const FString& Needle, ESearchCase Case = ESearchCase::IgnoreCase) const
	{
		if (Case == ESearchCase::CaseSensitive) return Data.find(Needle.Data) != std::wstring::npos;
		return ToLower().Data.find(Needle.ToLower().Data) != std::wstring::npos;
	}

	bool Equals(const FString& Other, ESearchCase Case = ESearchCase::IgnoreCase) const
	{
		if (Case == ESearchCase::CaseSensitive) return Data == Other.Data;
		return ToLower().Data == Other.ToLower().Data;
	}

	// Splits on whitespace. UE culls empty entries by default and returns the
	// number of pieces; `ExtraDelim` adds further single-character separators.
	int32 ParseIntoArrayWS(TArray<FString>& OutArray, const TCHAR* ExtraDelim = nullptr, bool CullEmpty = true) const;

	// UE reports INDEX_NONE in the out-parameter when the character is absent, so
	// a caller that checks only the out-parameter behaves the same here.
	bool FindLastChar(TCHAR Ch, int32& OutIndex) const
	{
		const size_t Found = Data.rfind(Ch);
		if (Found == std::wstring::npos)
		{
			OutIndex = INDEX_NONE;
			return false;
		}
		OutIndex = static_cast<int32>(Found);
		return true;
	}

	// Drops the FIRST `Count` characters. UE clamps rather than throwing: a count
	// at or past the end yields an empty string, a non-positive count the whole one.
	FString RightChop(int32 Count) const
	{
		if (Count <= 0) return *this;
		if (Count >= Len()) return FString();
		return FString(Data.substr(static_cast<size_t>(Count)));
	}

	int32 Compare(const FString& Other, ESearchCase Case = ESearchCase::IgnoreCase) const
	{
		const std::wstring& L = Case == ESearchCase::CaseSensitive ? Data : ToLower().Data;
		const std::wstring R = Case == ESearchCase::CaseSensitive ? Other.Data : Other.ToLower().Data;
		if (L < R) return -1;
		return L == R ? 0 : 1;
	}

	// UE's FString::Printf treats %s as TCHAR* on every platform, while C's wide
	// printf reads %s as char* and would stop at the first embedded NUL byte of a
	// UTF-32 string. Rewriting %s to %ls reproduces the engine's contract.
	static FString Printf(const TCHAR* Format, ...)
	{
		std::wstring Widened;
		for (const wchar_t* Cursor = Format; *Cursor; ++Cursor)
		{
			Widened.push_back(*Cursor);
			if (*Cursor != L'%') continue;
			const wchar_t Next = *(Cursor + 1);
			if (Next == L'%') { Widened.push_back(Next); ++Cursor; }
			else if (Next == L's') { Widened.push_back(L'l'); }
		}

		std::vector<wchar_t> Buffer(1024);
		for (;;)
		{
			va_list Args;
			va_start(Args, Format);
			const int Written = std::vswprintf(Buffer.data(), Buffer.size(), Widened.c_str(), Args);
			va_end(Args);
			if (Written >= 0) return FString(std::wstring(Buffer.data(), static_cast<size_t>(Written)));
			if (Buffer.size() > (1u << 22)) return FString();
			Buffer.resize(Buffer.size() * 2);
		}
	}

	static FString FromInt(int32 Value) { return Printf(TEXT("%d"), Value); }

	// UE's FString relational operators are case-insensitive; reproduced so the
	// harness cannot pass on a comparison the engine would resolve differently.
	friend bool operator==(const FString& L, const FString& R) { return L.Equals(R, ESearchCase::IgnoreCase); }
	friend bool operator!=(const FString& L, const FString& R) { return !(L == R); }
	friend bool operator<(const FString& L, const FString& R) { return L.Compare(R, ESearchCase::IgnoreCase) < 0; }
	FString operator+(const FString& Other) const { return FString(Data + Other.Data); }
	FString& operator+=(const FString& Other) { Data += Other.Data; return *this; }

private:
	std::wstring Data;
};

// UE's FStringView is a NON-OWNING view over an existing buffer. The describe
// unit uses it to compare FJsonObject keys without allocating, which is the only
// surface needed here: construction from a null-terminated buffer, and an Equals
// that honours ESearchCase exactly like FString's does.
class FStringView
{
public:
	FStringView() = default;
	FStringView(const TCHAR* Raw) : View(Raw ? Raw : L"") {}

	int32 Len() const { return static_cast<int32>(View.size()); }
	bool IsEmpty() const { return View.empty(); }

	bool Equals(const FStringView& Other, ESearchCase Case = ESearchCase::IgnoreCase) const
	{
		if (View.size() != Other.View.size()) return false;
		if (Case == ESearchCase::CaseSensitive) return View == Other.View;
		for (size_t Index = 0; Index < View.size(); ++Index)
		{
			if (std::towlower(View[Index]) != std::towlower(Other.View[Index])) return false;
		}
		return true;
	}

private:
	std::wstring_view View;
};

template <typename T>
class TArray
{
public:
	using SizeType = int32;
	int32 Num() const { return static_cast<int32>(Items.size()); }
	void Reserve(int32 Count) { Items.reserve(static_cast<size_t>(Count)); }
	void Empty() { Items.clear(); }
	// UE's Reset() empties the array but KEEPS the allocation; Empty() releases it.
	// std::vector::clear has exactly that behaviour, so both map onto it here.
	void Reset() { Items.clear(); }
	void Add(const T& Value) { Items.push_back(Value); }
	void Add(T&& Value) { Items.push_back(std::move(Value)); }

	// UE's TArray::Emplace constructs in place and returns the new INDEX (it is
	// Emplace_GetRef that hands back a reference).
	template <typename... ArgTypes>
	int32 Emplace(ArgTypes&&... Args)
	{
		Items.emplace_back(std::forward<ArgTypes>(Args)...);
		return static_cast<int32>(Items.size()) - 1;
	}
	void AddUnique(const T& Value) { if (!Contains(Value)) Items.push_back(Value); }
	void SetNumUninitialized(int32 Count) { Items.resize(static_cast<size_t>(Count)); }
	T& operator[](int32 Index) { return Items[static_cast<size_t>(Index)]; }
	const T& operator[](int32 Index) const { return Items[static_cast<size_t>(Index)]; }
	bool Contains(const T& Value) const { return std::find(Items.begin(), Items.end(), Value) != Items.end(); }

	template <typename Predicate>
	bool ContainsByPredicate(Predicate Pred) const
	{
		return std::find_if(Items.begin(), Items.end(), Pred) != Items.end();
	}

	template <typename Predicate>
	const T* FindByPredicate(Predicate Pred) const
	{
		auto Found = std::find_if(Items.begin(), Items.end(), Pred);
		return Found == Items.end() ? nullptr : &*Found;
	}

	// std::sort is unstable, matching UE's introsort: a comparator that is not a
	// strict total order produces an arbitrary order here too.
	template <typename Predicate>
	void Sort(Predicate Pred) { std::sort(Items.begin(), Items.end(), Pred); }
	void Sort() { std::sort(Items.begin(), Items.end()); }

	auto begin() { return Items.begin(); }
	auto end() { return Items.end(); }
	auto begin() const { return Items.begin(); }
	auto end() const { return Items.end(); }

private:
	std::vector<T> Items;
};

// Defined here rather than inline in FString because it needs a complete TArray.
inline int32 FString::ParseIntoArrayWS(TArray<FString>& OutArray, const TCHAR* ExtraDelim, bool CullEmpty) const
{
	OutArray.Reset();
	const std::wstring_view Extra = ExtraDelim ? std::wstring_view(ExtraDelim) : std::wstring_view();
	std::wstring Current;
	const auto Flush = [&]()
	{
		if (!Current.empty() || !CullEmpty) OutArray.Add(FString(Current));
		Current.clear();
	};
	for (const wchar_t Ch : Data)
	{
		const bool IsDelimiter = std::iswspace(Ch) != 0
			|| (!Extra.empty() && Extra.find(Ch) != std::wstring_view::npos);
		if (IsDelimiter) Flush();
		else Current.push_back(Ch);
	}
	Flush();
	return OutArray.Num();
}

template <typename T>
class TSharedPtr : public std::shared_ptr<T>
{
public:
	using std::shared_ptr<T>::shared_ptr;
	TSharedPtr() = default;
	TSharedPtr(const std::shared_ptr<T>& Other) : std::shared_ptr<T>(Other) {}
	TSharedPtr(std::shared_ptr<T>&& Other) : std::shared_ptr<T>(std::move(Other)) {}
	template <typename U>
	TSharedPtr(const TSharedPtr<U>& Other) : std::shared_ptr<T>(Other) {}
	bool IsValid() const { return static_cast<bool>(*this); }
};

template <typename T> using TSharedRef = TSharedPtr<T>;

template <typename T, typename... Args> TSharedPtr<T> MakeShared(Args&&... args)
{
	return TSharedPtr<T>(std::make_shared<T>(std::forward<Args>(args)...));
}

// UE's TMap exposes .Key/.Value pairs and Num(); the ordering below is
// deterministic while UE's is hash order, which is safe only because every
// consumer sorts explicitly before emitting anything.
template <typename K, typename V, typename Less = std::less<K>>
class TMap
{
public:
	struct FPair { const K& Key; const V& Value; };

	class FIterator
	{
	public:
		explicit FIterator(typename std::map<K, V, Less>::const_iterator In) : It(In) {}
		FPair operator*() const { return FPair{ It->first, It->second }; }
		FIterator& operator++() { ++It; return *this; }
		bool operator!=(const FIterator& Other) const { return It != Other.It; }
	private:
		typename std::map<K, V, Less>::const_iterator It;
	};

	FIterator begin() const { return FIterator(Data.begin()); }
	FIterator end() const { return FIterator(Data.end()); }
	int32 Num() const { return static_cast<int32>(Data.size()); }
	V& operator[](const K& Key) { return Data[Key]; }
	const V& operator[](const K& Key) const { return Data.at(Key); }
	const V* Find(const K& Key) const
	{
		auto Found = Data.find(Key);
		return Found == Data.end() ? nullptr : &Found->second;
	}
	bool Contains(const K& Key) const { return Data.find(Key) != Data.end(); }

private:
	std::map<K, V, Less> Data;
};

struct FMath
{
	template <typename T> static T Min(T a, T b) { return a < b ? a : b; }
	template <typename T> static T Max(T a, T b) { return a > b ? a : b; }
	template <typename T> static T Min3(T a, T b, T c) { return Min(Min(a, b), c); }
	template <typename T> static T Clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
	static double TruncToDouble(double v) { return std::trunc(v); }
};
