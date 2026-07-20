// Task 25 native discovery harness.
//
// Compiles and runs the REAL native discovery sources (capability store,
// search, describe, canonical JSON, guidance) against the REAL generated
// capability shards, under a minimal UE shim. For each case in the shared
// fixture file it prints one canonical JSON line, which is diffed byte-for-byte
// against the TypeScript reference rendering of the same case.
//
// This is not a re-implementation: every discovery decision below is made by
// the same translation units UnrealBuildTool compiles into the plugin.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayDescribe.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
std::wstring Utf8ToWide(const std::string& Bytes)
{
	std::wstring Out;
	size_t Index = 0;
	while (Index < Bytes.size())
	{
		const unsigned char Lead = static_cast<unsigned char>(Bytes[Index]);
		uint32 Code = 0;
		size_t Extra = 0;
		if (Lead < 0x80) { Code = Lead; }
		else if ((Lead & 0xe0) == 0xc0) { Code = Lead & 0x1fu; Extra = 1; }
		else if ((Lead & 0xf0) == 0xe0) { Code = Lead & 0x0fu; Extra = 2; }
		else { Code = Lead & 0x07u; Extra = 3; }
		++Index;
		for (size_t I = 0; I < Extra && Index < Bytes.size(); ++I, ++Index)
		{
			Code = (Code << 6) | (static_cast<unsigned char>(Bytes[Index]) & 0x3fu);
		}
		Out.push_back(static_cast<wchar_t>(Code));
	}
	return Out;
}

std::string WideToUtf8(const std::wstring& Text)
{
	std::string Out;
	for (const wchar_t Ch : Text)
	{
		const uint32 Code = static_cast<uint32>(Ch);
		if (Code < 0x80) Out.push_back(static_cast<char>(Code));
		else if (Code < 0x800)
		{
			Out.push_back(static_cast<char>(0xc0 | (Code >> 6)));
			Out.push_back(static_cast<char>(0x80 | (Code & 0x3f)));
		}
		else if (Code < 0x10000)
		{
			Out.push_back(static_cast<char>(0xe0 | (Code >> 12)));
			Out.push_back(static_cast<char>(0x80 | ((Code >> 6) & 0x3f)));
			Out.push_back(static_cast<char>(0x80 | (Code & 0x3f)));
		}
		else
		{
			Out.push_back(static_cast<char>(0xf0 | (Code >> 18)));
			Out.push_back(static_cast<char>(0x80 | ((Code >> 12) & 0x3f)));
			Out.push_back(static_cast<char>(0x80 | ((Code >> 6) & 0x3f)));
			Out.push_back(static_cast<char>(0x80 | (Code & 0x3f)));
		}
	}
	return Out;
}

FString ReadFileAsString(const char* Path)
{
	std::ifstream Stream(Path, std::ios::binary);
	std::ostringstream Buffer;
	Buffer << Stream.rdbuf();
	return FString(Utf8ToWide(Buffer.str()));
}

FMcpDiscoveryQuery BuildQuery(const TSharedPtr<FJsonObject>& Case)
{
	FMcpDiscoveryQuery Query;
	Case->TryGetStringField(TEXT("query"), Query.Query);
	Case->TryGetStringField(TEXT("tool"), Query.Tool);
	Query.bHasDomain = Case->TryGetStringField(TEXT("domain"), Query.Domain);
	Query.bHasFamily = Case->TryGetStringField(TEXT("family"), Query.Family);
	Query.bHasAction = Case->TryGetStringField(TEXT("action"), Query.Action);
	Query.bHasParam = Case->TryGetStringField(TEXT("param"), Query.Param);

	FString Operation;
	Case->TryGetStringField(TEXT("operation"), Operation);
	Query.Limit = Operation.Equals(TEXT("search"), ESearchCase::CaseSensitive)
		? McpSearchDefaultLimit : McpDescribeDefaultLimit;
	const int32 MaxLimit = Operation.Equals(TEXT("search"), ESearchCase::CaseSensitive)
		? McpSearchMaxLimit : McpDescribeMaxLimit;

	int32 Limit = 0;
	if (Case->TryGetNumberField(TEXT("limit"), Limit)) Query.Limit = FMath::Clamp(Limit, 1, MaxLimit);
	int32 Offset = 0;
	if (Case->TryGetNumberField(TEXT("offset"), Offset)) Query.Offset = FMath::Max(0, Offset);
	return Query;
}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cerr << "usage: native-discovery-harness <cases.json>\n";
		return 2;
	}

	const FMcpCapabilityStore& Store = FMcpCapabilityStore::Get();
	if (!Store.IsReady())
	{
		std::cerr << "capability store failed to load: "
			<< WideToUtf8(Store.GetStatusDetail().Raw()) << "\n";
		return 3;
	}

	TArray<TSharedPtr<FJsonValue>> Cases;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReadFileAsString(argv[1]));
	if (!FJsonSerializer::Deserialize(Reader, Cases))
	{
		std::cerr << "fixture case file is not a JSON array\n";
		return 4;
	}

	// The fixture precondition is the plugin default (bLoadAllToolsOnStart), so
	// every canonical parent is exposed; availability then varies only by the
	// record's own deprecation status.
	auto AllToolsEnabled = [](const FString&) { return true; };

	for (const TSharedPtr<FJsonValue>& Entry : Cases)
	{
		if (!Entry || Entry->Type != EJson::Object)
		{
			std::cerr << "fixture case is not an object\n";
			return 5;
		}
		const TSharedPtr<FJsonObject> Case = Entry->AsObject();
		FString Operation;
		Case->TryGetStringField(TEXT("operation"), Operation);
		const FMcpDiscoveryQuery Query = BuildQuery(Case);

		const TSharedPtr<FJsonObject> Result =
			Operation.Equals(TEXT("search"), ESearchCase::CaseSensitive)
				? McpGatewaySearchCapabilities(Query, Store, AllToolsEnabled)
				: McpGatewayDescribeCapability(Query, Store, AllToolsEnabled);

		FString Rendered;
		if (!McpCanonicalJsonObject(Result, Rendered))
		{
			std::cerr << "case could not be rendered canonically\n";
			return 6;
		}
		std::cout << WideToUtf8(Rendered.Raw()) << "\n";
	}
	return 0;
}
