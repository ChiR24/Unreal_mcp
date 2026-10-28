// Data registries (manage_asset list_data_registries, get_data_registry, create_data_registry), read and made by
// reflection: the DataRegistry plugin is Beta and a project may leave it off, so nothing here links it and the .uplugin
// does not name it (a module linking it would also leave the Fab build, which drops Beta and Experimental plugins). The
// registry asset's properties (RegistryType, IdFormat, ItemStruct, DataSources, RuntimeSources), the table sources'
// SourceTable and the settings' DirectoriesToScan carry the same names in every UE 5.x.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

namespace McpDataRegistries
{
using FRegistrySocket = TSharedPtr<FMcpBridgeWebSocket>;

// list_data_registries, get_data_registry and create_data_registry; false for any other action.
bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const FString& Action,
            const TSharedPtr<FJsonObject>& Payload, FRegistrySocket Socket);
bool HandleList(UEngineWireSubsystem* Self, const FString& RequestId, FRegistrySocket Socket);
bool HandleGet(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
               FRegistrySocket Socket);
bool HandleCreate(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                  FRegistrySocket Socket);

// Every registry asset the asset registry knows, by package name, and a tag of one (an object path for ItemStruct).
TArray<FAssetData> RegistryAssets();
FString RegistryTag(const FAssetData& Asset, const TCHAR* Tag);
// The asset at an object or package path, loaded; null without a load (and its warnings) when no package is there.
UObject* LoadExisting(const FString& Path);
// The DataRegistry plugin's classes by path, null where the plugin is off.
UClass* RegistryClass();
UClass* DataTableSourceClass();
// The settings' DirectoriesToScan, and whether every registry that loads is set up anyway.
TArray<FString> ScanDirectories(bool& bOutAllLoaded);
// Whether the game sets this registry up: its package under a scan directory, or every loaded registry is.
bool IsScanned(const FString& PackageName, const TArray<FString>& Directories, bool bAllLoaded);

// Reflected reads; none when the property is missing or of another kind.
FName ReadName(const UObject* Owner, const TCHAR* Property);
UObject* ReadObject(const UObject* Owner, const TCHAR* Property);
TArray<UObject*> ReadObjects(const UObject* Owner, const TCHAR* ArrayProperty);
FString ReadSoftPath(const UObject* Owner, const TCHAR* Property);
// The registry's id format: its base gameplay tag, empty when it has none.
FString ReadIdFormatTag(const UObject* Registry);
}
