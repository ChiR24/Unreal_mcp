// create_data_registry: a registry asset over data tables, one DataTable source per table, its item struct the tables'
// shared row struct, saved. Every refusal comes before anything is made. The game sets up only the registries under
// the DataRegistry settings' DirectoriesToScan, so the reply says whether this one is (scanned); the settings are the
// project's and stay as they are.

#include "Domains/AssetWorkflow/DataRegistries/McpDataRegistries.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetCreation.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Misc/PackageName.h"
#include "Safety/McpSafeOperations.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace McpDataRegistries
{
bool HandleCreate(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                  FRegistrySocket Socket)
{
	FString AssetPath, RegistryType, Folder, Name, PackageName, Refusal;
	Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
	Payload->TryGetStringField(TEXT("registryType"), RegistryType);
	const auto Refuse = [&](const FString& Message, const TCHAR* Code)
	{
		Self->SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was created."), Code);
		return true;
	};
	if (!AssetPath.Split(TEXT("/"), &Folder, &Name, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
		!ValidateAssetCreationPath(Folder, Name, PackageName, Refusal))
	{
		return Refuse(Refusal.IsEmpty() ? FString::Printf(TEXT("'%s' is no asset path (/Game/Folder/Name)."), *AssetPath) : Refusal,
			TEXT("INVALID_ARGUMENT"));
	}
	if (RegistryType.TrimStartAndEnd().IsEmpty())
	{
		return Refuse(TEXT("registryType names the registry the game looks items up by; it is empty."), TEXT("INVALID_ARGUMENT"));
	}
	if (FPackageName::DoesPackageExist(PackageName) || FindPackage(nullptr, *PackageName))
	{
		return Refuse(FString::Printf(TEXT("%s already exists."), *PackageName), TEXT("ASSET_EXISTS"));
	}
	for (const FAssetData& Asset : RegistryAssets())
	{
		if (RegistryTag(Asset, TEXT("RegistryType")).Equals(RegistryType, ESearchCase::IgnoreCase))
		{
			return Refuse(FString::Printf(TEXT("Registry type %s is %s's already; each registry has its own."), *RegistryType,
				*Asset.PackageName.ToString()), TEXT("DATA_REGISTRY_EXISTS"));
		}
	}
	TArray<UDataTable*> Tables;
	const UScriptStruct* RowStruct = nullptr;
	for (const FString& Path : McpHandlerUtils::GetStringArrayField(Payload, TEXT("dataTables")))
	{
		UDataTable* Table = Cast<UDataTable>(LoadExisting(Path));
		if (!Table || !Table->GetRowStruct())
		{
			return Refuse(FString::Printf(TEXT("%s is no data table with a row struct."), *Path), TEXT("DATA_TABLE_NOT_FOUND"));
		}
		if (RowStruct && Table->GetRowStruct() != RowStruct)
		{
			return Refuse(FString::Printf(TEXT("%s holds %s rows, %s holds %s: a registry's tables share one item struct."),
				*Tables[0]->GetPathName(), *RowStruct->GetName(), *Table->GetPathName(), *Table->GetRowStruct()->GetName()),
				TEXT("ROW_STRUCT_MISMATCH"));
		}
		RowStruct = Table->GetRowStruct();
		Tables.Add(Table);
	}
	UClass* Class = RegistryClass();
	UClass* SourceClass = DataTableSourceClass();
	FNameProperty* TypeProperty = FindFProperty<FNameProperty>(Class, TEXT("RegistryType"));
	FObjectPropertyBase* StructProperty = FindFProperty<FObjectPropertyBase>(Class, TEXT("ItemStruct"));
	FArrayProperty* SourcesProperty = FindFProperty<FArrayProperty>(Class, TEXT("DataSources"));
	FObjectPropertyBase* SourceElement = SourcesProperty ? CastField<FObjectPropertyBase>(SourcesProperty->Inner) : nullptr;
	FSoftObjectProperty* TableProperty = SourceClass ? FindFProperty<FSoftObjectProperty>(SourceClass, TEXT("SourceTable")) : nullptr;
	if (Tables.Num() == 0 || !TypeProperty || !StructProperty || !SourceElement || !TableProperty)
	{
		return Refuse(Tables.Num() == 0 ? FString(TEXT("dataTables lists no table.")) : FString(TEXT("This engine's DataRegistry "
			"classes lack the properties a registry is made from.")), TEXT("INVALID_ARGUMENT"));
	}
	UObject* Registry = NewObject<UObject>(CreatePackage(*PackageName), Class, FName(*FPackageName::GetShortName(PackageName)),
		RF_Public | RF_Standalone);
	TypeProperty->SetPropertyValue_InContainer(Registry, FName(*RegistryType));
	StructProperty->SetObjectPropertyValue_InContainer(Registry, const_cast<UScriptStruct*>(RowStruct));
	FScriptArrayHelper Sources(SourcesProperty, SourcesProperty->ContainerPtrToValuePtr<void>(Registry));
	TArray<FString> TableNames;
	for (UDataTable* Table : Tables)
	{
		UObject* Source = NewObject<UObject>(Registry, SourceClass);
		TableProperty->SetObjectPropertyValue_InContainer(Source, Table);
		SourceElement->SetObjectPropertyValue(Sources.GetRawPtr(Sources.AddValue()), Source);
		TableNames.Add(Table->GetPathName());
	}
	FAssetRegistryModule::AssetCreated(Registry);
	bool bAllLoaded = false;
	const bool bScanned = IsScanned(PackageName, ScanDirectories(bAllLoaded), false);
	// The editor's own refresh registers a scanned registry with the subsystem at once; for one outside the scan
	// directories it does nothing but log that the subsystem has not registered it.
	if (bScanned)
	{
		Registry->PostEditChange();
	}
	const bool bSaved = McpSafeAssetSave(Registry);
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetStringField(TEXT("assetPath"), PackageName);
	Result->SetStringField(TEXT("registryType"), RegistryType);
	Result->SetStringField(TEXT("itemStruct"), RowStruct->GetPathName());
	Result->SetArrayField(TEXT("dataTables"), McpHandlerUtils::ToJsonStringArray(TableNames));
	Result->SetBoolField(TEXT("saved"), bSaved);
	Result->SetBoolField(TEXT("scanned"), bScanned);
	Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Created %s over %d table(s).%s"), *PackageName,
		Tables.Num(), bScanned ? TEXT("") : TEXT(" The game sets up only registries under the DataRegistry settings' "
		"DirectoriesToScan, and its folder is not one: add it there (Project Settings, Data Registry) for the game to use it.")),
		Result);
	return true;
}
} // namespace McpDataRegistries
