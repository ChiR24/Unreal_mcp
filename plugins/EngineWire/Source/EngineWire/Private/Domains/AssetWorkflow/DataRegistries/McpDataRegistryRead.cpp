// list_data_registries and get_data_registry. A registry's items come from its table sources in order (the first
// source holding a name serves it, as the registry looks items up); an initialized registry's runtime sources are read
// instead of its authored ones, since they expand meta sources into the tables they found.

#include "Domains/AssetWorkflow/DataRegistries/McpDataRegistries.h"

#include "Domains/AssetWorkflow/DataTables/Shared.h"
#include "Dom/JsonObject.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr int32 RegistryItemNamesShown = 500;
constexpr int32 RegistryItemsRead = 50;

// A registry by asset path, or by its RegistryType through the asset registry's tags (no load to search).
UObject* FindRegistry(const FString& Wanted)
{
	if (Wanted.StartsWith(TEXT("/")))
	{
		UObject* Asset = McpDataRegistries::LoadExisting(Wanted);
		return Asset && Asset->IsA(McpDataRegistries::RegistryClass()) ? Asset : nullptr;
	}
	for (const FAssetData& Asset : McpDataRegistries::RegistryAssets())
	{
		if (McpDataRegistries::RegistryTag(Asset, TEXT("RegistryType")).Equals(Wanted, ESearchCase::IgnoreCase))
		{
			return Asset.GetAsset();
		}
	}
	return nullptr;
}

// What a source reads: a DataTable or CurveTable by its SourceTable; null for a source of another kind.
UObject* RegistrySourceTable(const UObject* Source)
{
	const FString Path = McpDataRegistries::ReadSoftPath(Source, TEXT("SourceTable"));
	return Path.IsEmpty() ? nullptr : FSoftObjectPath(Path).TryLoad();
}
} // namespace

namespace McpDataRegistries
{
bool HandleList(UEngineWireSubsystem* Self, const FString& RequestId, FRegistrySocket Socket)
{
	bool bAllLoaded = false;
	const TArray<FString> Directories = ScanDirectories(bAllLoaded);
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FAssetData& Asset : RegistryAssets())
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("assetPath"), Asset.PackageName.ToString());
		Row->SetStringField(TEXT("registryType"), RegistryTag(Asset, TEXT("RegistryType")));
		Row->SetStringField(TEXT("itemStruct"), RegistryTag(Asset, TEXT("ItemStruct")));
		Row->SetBoolField(TEXT("scanned"), IsScanned(Asset.PackageName.ToString(), Directories, false));
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetArrayField(TEXT("dataRegistries"), Rows);
	Result->SetArrayField(TEXT("directoriesToScan"), McpHandlerUtils::ToJsonStringArray(Directories));
	Result->SetBoolField(TEXT("initializeAllLoaded"), bAllLoaded);
	Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%d data registr%s."), Rows.Num(),
		Rows.Num() == 1 ? TEXT("y") : TEXT("ies")), Result);
	return true;
}

bool HandleGet(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FRegistrySocket Socket)
{
	FString Wanted;
	Payload->TryGetStringField(TEXT("registry"), Wanted);
	UObject* Registry = FindRegistry(Wanted);
	if (!Registry)
	{
		Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No data registry is at or named '%s' "
			"(list_data_registries lists them)."), *Wanted), TEXT("DATA_REGISTRY_NOT_FOUND"));
		return true;
	}
	const UScriptStruct* ItemStruct = Cast<UScriptStruct>(ReadObject(Registry, TEXT("ItemStruct")));
	const TArray<UObject*> Authored = ReadObjects(Registry, TEXT("DataSources"));
	const TArray<UObject*> Runtime = ReadObjects(Registry, TEXT("RuntimeSources"));
	TArray<TSharedPtr<FJsonValue>> Sources;
	for (const UObject* Source : Authored)
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("class"), Source->GetClass()->GetName());
		Row->SetStringField(TEXT("table"), ReadSoftPath(Source, TEXT("SourceTable")));
		Sources.Add(MakeShared<FJsonValueObject>(Row));
	}
	// Every item name in lookup order, with the table that serves it.
	TArray<FName> Names;
	TMap<FName, UObject*> ServedBy;
	for (const UObject* Source : Runtime.Num() > 0 ? Runtime : Authored)
	{
		UObject* Table = RegistrySourceTable(Source);
		TArray<FName> Rows;
		if (const UDataTable* Data = Cast<UDataTable>(Table))
		{
			Rows = Data->GetRowNames();
		}
		else if (const UCurveTable* Curves = Cast<UCurveTable>(Table))
		{
			Curves->GetRowMap().GetKeys(Rows);
		}
		for (const FName& Row : Rows)
		{
			if (!ServedBy.Contains(Row))
			{
				ServedBy.Add(Row, Table);
				Names.Add(Row);
			}
		}
	}
	TArray<TSharedPtr<FJsonValue>> Schema;
	if (ItemStruct)
	{
		for (TFieldIterator<FProperty> It(ItemStruct); It; ++It)
		{
			TSharedPtr<FJsonObject> Field = MakeShared<FJsonObject>();
			Field->SetStringField(TEXT("name"), It->GetAuthoredName());
			Field->SetStringField(TEXT("type"), It->GetCPPType());
			Schema.Add(MakeShared<FJsonValueObject>(Field));
		}
	}
	TSharedPtr<FJsonObject> Items = MakeShared<FJsonObject>();
	TArray<FString> NotFound;
	TArray<FString> Asked = McpHandlerUtils::GetStringArrayField(Payload, TEXT("items"));
	Asked.SetNum(FMath::Min(Asked.Num(), RegistryItemsRead));
	for (const FString& Name : Asked)
	{
		UObject* const* Table = ServedBy.Find(FName(*Name));
		const UDataTable* Data = Table ? Cast<UDataTable>(*Table) : nullptr;
		const uint8* Row = Data ? Data->FindRowUnchecked(FName(*Name)) : nullptr;
		if (Row && Data->GetRowStruct())
		{
			Items->SetObjectField(Name, McpExportDataTableRow(Data->GetRowStruct(), Row));
		}
		else if (Table)
		{
			Items->SetStringField(Name, FString::Printf(TEXT("a curve row of %s"), *(*Table)->GetPathName()));
		}
		else
		{
			NotFound.Add(Name);
		}
	}
	bool bAllLoaded = false;
	const TArray<FString> Directories = ScanDirectories(bAllLoaded);
	TArray<FString> Shown;
	for (int32 Index = 0; Index < FMath::Min(Names.Num(), RegistryItemNamesShown); ++Index)
	{
		Shown.Add(Names[Index].ToString());
	}
	const FString RegistryType = ReadName(Registry, TEXT("RegistryType")).ToString();
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetStringField(TEXT("assetPath"), Registry->GetOutermost()->GetName());
	Result->SetStringField(TEXT("registryType"), RegistryType);
	Result->SetStringField(TEXT("itemStruct"), ItemStruct ? ItemStruct->GetPathName() : FString());
	Result->SetStringField(TEXT("idFormatTag"), ReadIdFormatTag(Registry));
	Result->SetBoolField(TEXT("scanned"), IsScanned(Registry->GetOutermost()->GetName(), Directories, false));
	Result->SetBoolField(TEXT("initialized"), Runtime.Num() > 0);
	Result->SetArrayField(TEXT("schema"), Schema);
	Result->SetArrayField(TEXT("sources"), Sources);
	Result->SetArrayField(TEXT("itemNames"), McpHandlerUtils::ToJsonStringArray(Shown));
	Result->SetNumberField(TEXT("itemCount"), Names.Num());
	Result->SetObjectField(TEXT("items"), Items);
	if (NotFound.Num() > 0)
	{
		Result->SetArrayField(TEXT("itemsNotFound"), McpHandlerUtils::ToJsonStringArray(NotFound));
	}
	Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%s: %d item(s) from %d source(s)."),
		*RegistryType, Names.Num(), Authored.Num()), Result);
	return true;
}
} // namespace McpDataRegistries
