// FJsonSerializer shim: recursive-descent parse into the FJsonValue hierarchy.
//
// Surrogate pairs are combined into one code point because TCHAR is UTF-32 on
// this host, mirroring what UE's reader produces on Linux/Mac. The canonical
// serializer re-splits them, so a payload round-trips byte-identically.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"

class FJsonSerializer
{
public:
	static bool Deserialize(const TSharedRef<TJsonReader<TCHAR>>& Reader, TArray<TSharedPtr<FJsonValue>>& OutArray)
	{
		FParser Parser(Reader->GetSource().Raw());
		TSharedPtr<FJsonValue> Value;
		if (!Parser.ParseValue(Value) || !Parser.AtEnd()) return false;
		if (!Value || Value->Type != EJson::Array) return false;
		OutArray = Value->AsArray();
		return true;
	}

	static bool Deserialize(const TSharedRef<TJsonReader<TCHAR>>& Reader, TSharedPtr<FJsonObject>& OutObject)
	{
		FParser Parser(Reader->GetSource().Raw());
		TSharedPtr<FJsonValue> Value;
		if (!Parser.ParseValue(Value) || !Parser.AtEnd()) return false;
		if (!Value || Value->Type != EJson::Object) return false;
		OutObject = Value->AsObject();
		return true;
	}

private:
	class FParser
	{
	public:
		explicit FParser(const std::wstring& InText) : Text(InText) {}

		bool AtEnd() { SkipWhitespace(); return Cursor >= Text.size(); }

		bool ParseValue(TSharedPtr<FJsonValue>& Out)
		{
			SkipWhitespace();
			if (Cursor >= Text.size()) return false;
			const wchar_t Ch = Text[Cursor];
			if (Ch == L'{') return ParseObject(Out);
			if (Ch == L'[') return ParseArray(Out);
			if (Ch == L'"')
			{
				std::wstring Value;
				if (!ParseString(Value)) return false;
				Out = MakeShared<FJsonValueString>(FString(Value));
				return true;
			}
			if (Match(L"true")) { Out = MakeShared<FJsonValueBoolean>(true); return true; }
			if (Match(L"false")) { Out = MakeShared<FJsonValueBoolean>(false); return true; }
			if (Match(L"null")) { Out = MakeShared<FJsonValueNull>(); return true; }
			return ParseNumber(Out);
		}

	private:
		void SkipWhitespace()
		{
			while (Cursor < Text.size() && (Text[Cursor] == L' ' || Text[Cursor] == L'\t' ||
				Text[Cursor] == L'\n' || Text[Cursor] == L'\r')) ++Cursor;
		}

		bool Match(const wchar_t* Literal)
		{
			const size_t Length = std::wcslen(Literal);
			if (Text.compare(Cursor, Length, Literal) != 0) return false;
			Cursor += Length;
			return true;
		}

		bool ParseString(std::wstring& Out)
		{
			if (Cursor >= Text.size() || Text[Cursor] != L'"') return false;
			++Cursor;
			Out.clear();
			while (Cursor < Text.size())
			{
				const wchar_t Ch = Text[Cursor++];
				if (Ch == L'"') return true;
				if (Ch != L'\\') { Out.push_back(Ch); continue; }
				if (Cursor >= Text.size()) return false;
				const wchar_t Escape = Text[Cursor++];
				switch (Escape)
				{
				case L'"': Out.push_back(L'"'); break;
				case L'\\': Out.push_back(L'\\'); break;
				case L'/': Out.push_back(L'/'); break;
				case L'b': Out.push_back(L'\b'); break;
				case L'f': Out.push_back(L'\f'); break;
				case L'n': Out.push_back(L'\n'); break;
				case L'r': Out.push_back(L'\r'); break;
				case L't': Out.push_back(L'\t'); break;
				case L'u':
				{
					uint32 Code = 0;
					if (!ParseHex4(Code)) return false;
					if (Code >= 0xd800 && Code <= 0xdbff && Text.compare(Cursor, 2, L"\\u") == 0)
					{
						Cursor += 2;
						uint32 Low = 0;
						if (!ParseHex4(Low)) return false;
						Code = 0x10000 + ((Code - 0xd800) << 10) + (Low - 0xdc00);
					}
					Out.push_back(static_cast<wchar_t>(Code));
					break;
				}
				default: return false;
				}
			}
			return false;
		}

		bool ParseHex4(uint32& Out)
		{
			if (Cursor + 4 > Text.size()) return false;
			Out = 0;
			for (int i = 0; i < 4; ++i)
			{
				const wchar_t Ch = Text[Cursor++];
				Out <<= 4;
				if (Ch >= L'0' && Ch <= L'9') Out |= static_cast<uint32>(Ch - L'0');
				else if (Ch >= L'a' && Ch <= L'f') Out |= static_cast<uint32>(Ch - L'a' + 10);
				else if (Ch >= L'A' && Ch <= L'F') Out |= static_cast<uint32>(Ch - L'A' + 10);
				else return false;
			}
			return true;
		}

		bool ParseNumber(TSharedPtr<FJsonValue>& Out)
		{
			const size_t Start = Cursor;
			if (Cursor < Text.size() && (Text[Cursor] == L'-' || Text[Cursor] == L'+')) ++Cursor;
			while (Cursor < Text.size() && (std::iswdigit(Text[Cursor]) || Text[Cursor] == L'.' ||
				Text[Cursor] == L'e' || Text[Cursor] == L'E' || Text[Cursor] == L'-' || Text[Cursor] == L'+')) ++Cursor;
			if (Cursor == Start) return false;
			Out = MakeShared<FJsonValueNumber>(std::wcstod(Text.substr(Start, Cursor - Start).c_str(), nullptr));
			return true;
		}

		bool ParseArray(TSharedPtr<FJsonValue>& Out)
		{
			++Cursor;
			TArray<TSharedPtr<FJsonValue>> Items;
			SkipWhitespace();
			if (Cursor < Text.size() && Text[Cursor] == L']') { ++Cursor; Out = MakeShared<FJsonValueArray>(Items); return true; }
			for (;;)
			{
				TSharedPtr<FJsonValue> Item;
				if (!ParseValue(Item)) return false;
				Items.Add(Item);
				SkipWhitespace();
				if (Cursor >= Text.size()) return false;
				if (Text[Cursor] == L',') { ++Cursor; continue; }
				if (Text[Cursor] == L']') { ++Cursor; break; }
				return false;
			}
			Out = MakeShared<FJsonValueArray>(Items);
			return true;
		}

		bool ParseObject(TSharedPtr<FJsonValue>& Out)
		{
			++Cursor;
			auto Object = MakeShared<FJsonObject>();
			SkipWhitespace();
			if (Cursor < Text.size() && Text[Cursor] == L'}') { ++Cursor; Out = MakeShared<FJsonValueObject>(Object); return true; }
			for (;;)
			{
				SkipWhitespace();
				std::wstring Key;
				if (!ParseString(Key)) return false;
				SkipWhitespace();
				if (Cursor >= Text.size() || Text[Cursor] != L':') return false;
				++Cursor;
				TSharedPtr<FJsonValue> Value;
				if (!ParseValue(Value)) return false;
				Object->Values[FString(Key)] = Value;
				SkipWhitespace();
				if (Cursor >= Text.size()) return false;
				if (Text[Cursor] == L',') { ++Cursor; continue; }
				if (Text[Cursor] == L'}') { ++Cursor; break; }
				return false;
			}
			Out = MakeShared<FJsonValueObject>(Object);
			return true;
		}

		const std::wstring& Text;
		size_t Cursor = 0;
	};
};
