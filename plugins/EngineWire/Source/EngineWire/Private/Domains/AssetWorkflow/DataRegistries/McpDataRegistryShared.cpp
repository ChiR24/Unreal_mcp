#include "Domains/AssetWorkflow/DataRegistries/McpDataRegistries.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

namespace McpDataRegistries
{
bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const FString& Action,
            const TSharedPtr<FJsonObject>& Payload, FRegistrySocket Socket)
{
	if (Action != TEXT("list_data_registries") && Action != TEXT("get_data_registry") && Action != TEXT("create_data_registry"))
	{
		return false;
	}
	if (!RegistryClass())
	{
		Self->SendAutomationError(Socket, RequestId, TEXT("Data registries need the DataRegistry plugin, which this project "
			"leaves off: enable_plugin DataRegistry, then restart the editor."), TEXT("DATA_REGISTRY_DISABLED"));
		return true;
	}
	return Action == TEXT("list_data_registries") ? HandleList(Self, RequestId, Socket)
		: Action == TEXT("get_data_registry") ? HandleGet(Self, RequestId, Payload, Socket)
		: HandleCreate(Self, RequestId, Payload, Socket);
}

TArray<FAssetData> RegistryAssets()
{
	FARFilter Filter;
#if MCP_HAS_ASSET_CLASS_PATHS
	Filter.ClassPaths.Add(FTopLevelAssetPath(FName(TEXT("/Script/DataRegistry")), FName(TEXT("DataRegistry"))));
#else
	Filter.ClassNames.Add(FName(TEXT("DataRegistry")));
#endif
	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.ToString().Compare(B.PackageName.ToString(), ESearchCase::CaseSensitive) < 0; });
	return Assets;
}

FString RegistryTag(const FAssetData& Asset, const TCHAR* Tag)
{
	FString Value;
	Asset.GetTagValue(FName(Tag), Value);
	// An object property's tag is export text (Class'/Path.Name'); the path is what other calls take.
	return Value.Contains(TEXT("'")) ? FPackageName::ExportTextPathToObjectPath(Value) : Value;
}

UObject* LoadExisting(const FString& Path)
{
	const FString Package = FPackageName::ObjectPathToPackageName(Path);
	if (!FPackageName::IsValidLongPackageName(Package) || !FPackageName::DoesPackageExist(Package))
	{
		return nullptr;
	}
	return LoadObject<UObject>(nullptr, *(Path.Contains(TEXT(".")) ? Path : Package + TEXT(".") + FPackageName::GetShortName(Package)));
}

UClass* RegistryClass()
{
	return FindObject<UClass>(nullptr, TEXT("/Script/DataRegistry.DataRegistry"));
}

UClass* DataTableSourceClass()
{
	return FindObject<UClass>(nullptr, TEXT("/Script/DataRegistry.DataRegistrySource_DataTable"));
}

TArray<FString> ScanDirectories(bool& bOutAllLoaded)
{
	TArray<FString> Directories;
	bOutAllLoaded = false;
	const UClass* Settings = FindObject<UClass>(nullptr, TEXT("/Script/DataRegistry.DataRegistrySettings"));
	const UObject* Defaults = Settings ? Settings->GetDefaultObject() : nullptr;
	if (!Defaults)
	{
		return Directories;
	}
	if (const FBoolProperty* AllLoaded = FindFProperty<FBoolProperty>(Settings, TEXT("bInitializeAllLoadedRegistries")))
	{
		bOutAllLoaded = AllLoaded->GetPropertyValue_InContainer(Defaults);
	}
	const FArrayProperty* Array = FindFProperty<FArrayProperty>(Settings, TEXT("DirectoriesToScan"));
	const FStructProperty* Entry = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
	const FStrProperty* Path = Entry ? FindFProperty<FStrProperty>(Entry->Struct, TEXT("Path")) : nullptr;
	if (!Path)
	{
		return Directories;
	}
	FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Defaults));
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		// Stored as a long package name (/Game/Data); a bare folder is relative to the game content.
		FString Directory = Path->GetPropertyValue_InContainer(Helper.GetRawPtr(Index));
		Directory.RemoveFromEnd(TEXT("/"));
		if (!Directory.IsEmpty())
		{
			Directories.Add(Directory.StartsWith(TEXT("/")) ? Directory : TEXT("/Game/") + Directory);
		}
	}
	return Directories;
}

bool IsScanned(const FString& PackageName, const TArray<FString>& Directories, bool bAllLoaded)
{
	return bAllLoaded || Directories.ContainsByPredicate([&PackageName](const FString& Directory)
		{ return PackageName.StartsWith(Directory + TEXT("/"), ESearchCase::IgnoreCase); });
}

FName ReadName(const UObject* Owner, const TCHAR* Property)
{
	const FNameProperty* Name = Owner ? FindFProperty<FNameProperty>(Owner->GetClass(), Property) : nullptr;
	return Name ? Name->GetPropertyValue_InContainer(Owner) : NAME_None;
}

UObject* ReadObject(const UObject* Owner, const TCHAR* Property)
{
	const FObjectPropertyBase* Object = Owner ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Property) : nullptr;
	return Object ? Object->GetObjectPropertyValue_InContainer(Owner) : nullptr;
}

TArray<UObject*> ReadObjects(const UObject* Owner, const TCHAR* ArrayProperty)
{
	TArray<UObject*> Objects;
	const FArrayProperty* Array = Owner ? FindFProperty<FArrayProperty>(Owner->GetClass(), ArrayProperty) : nullptr;
	const FObjectPropertyBase* Element = Array ? CastField<FObjectPropertyBase>(Array->Inner) : nullptr;
	if (!Element)
	{
		return Objects;
	}
	FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Owner));
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		if (UObject* Object = Element->GetObjectPropertyValue(Helper.GetRawPtr(Index)))
		{
			Objects.Add(Object);
		}
	}
	return Objects;
}

FString ReadSoftPath(const UObject* Owner, const TCHAR* Property)
{
	const FSoftObjectProperty* Soft = Owner ? FindFProperty<FSoftObjectProperty>(Owner->GetClass(), Property) : nullptr;
	return Soft ? Soft->GetPropertyValue_InContainer(Owner).ToSoftObjectPath().ToString() : FString();
}

FString ReadIdFormatTag(const UObject* Registry)
{
	const FStructProperty* Format = Registry ? FindFProperty<FStructProperty>(Registry->GetClass(), TEXT("IdFormat")) : nullptr;
	const FStructProperty* Tag = Format ? FindFProperty<FStructProperty>(Format->Struct, TEXT("BaseGameplayTag")) : nullptr;
	const FNameProperty* TagName = Tag ? FindFProperty<FNameProperty>(Tag->Struct, TEXT("TagName")) : nullptr;
	if (!TagName)
	{
		return FString();
	}
	const FName Value = TagName->GetPropertyValue_InContainer(Tag->ContainerPtrToValuePtr<void>(Format->ContainerPtrToValuePtr<void>(Registry)));
	return Value.IsNone() ? FString() : Value.ToString();
}
} // namespace McpDataRegistries
