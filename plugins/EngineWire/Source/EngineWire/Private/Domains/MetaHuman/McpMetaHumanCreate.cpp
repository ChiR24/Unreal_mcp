#include "Domains/MetaHuman/McpMetaHumanHandlers.h"

#include "AssetToolsModule.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "Safety/McpSafeOperations.h"

namespace McpMetaHumanHandlers
{
// create_metahuman -- author a new UMetaHumanCharacter asset.
//
// The factory is instantiated reflectively for the same reason the rest of this
// domain is: UMetaHumanCharacterFactoryNew lives in an editor module we do not
// link, and it only exists on UE 5.6+.
bool HandleCreateMetaHuman(UEngineWireSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket)
{
    UClass* CharacterClass = FindMetaHumanCharacterClass();
    UClass* FactoryClass = FindObject<UClass>(nullptr, MetaHumanFactoryClassPath);
    if (!CharacterClass || !FactoryClass)
    {
        SendMetaHumanUnavailable(Self, RequestId, Socket);
        return true;
    }

    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    const FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/MetaHumans"));
    if (Name.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."), TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString NameError;
    if (!McpAssetToolsCanCreate(Path, Name, NameError))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid character name '%s' in %s: %s"), *Name, *Path, *NameError), TEXT("INVALID_NAME"));
        return true;
    }
    const FString FullPath = Path / Name;
    if (UObject* Existing = LoadMetaHumanCharacter(FullPath))
    {
        // Answering the existing asset keeps the action idempotent: re-running a
        // scene build must not fail or silently create MH_Neo1 beside MH_Neo.
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("characterPath"), FullPath);
        Result->SetBoolField(TEXT("created"), false);
        McpHandlerUtils::AddVerification(Result, Existing);
        Self->SendAutomationResponse(Socket, RequestId, true, TEXT("MetaHuman character already exists"), Result);
        return true;
    }
    // CreateAsset asks the user whether to replace an asset of another class there, and Yes deletes it.
    if (McpAssetExists(FullPath))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("%s already holds an asset that is no MetaHuman character; pick another name or path. Nothing was created."),
                *FullPath),
            TEXT("ASSET_ALREADY_EXISTS"));
        return true;
    }

    UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
    if (!Factory)
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("Could not construct the MetaHuman character factory."), TEXT("OPERATION_FAILED"));
        return true;
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UObject* Created = AssetTools.CreateAsset(Name, Path, CharacterClass, Factory);
    if (!Created)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Could not create a MetaHuman character at %s."), *FullPath),
            TEXT("CREATION_FAILED"));
        return true;
    }

    const bool bSaved = McpSafeAssetSave(Created);
    if (!bSaved)
    {
        Created->MarkPackageDirty(); // unsaved, so save_all saves it
    }

    FString CoreDataDetail;
    const bool bCoreData = IsCoreDataInstalled(CoreDataDetail);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("characterPath"), FullPath);
    Result->SetStringField(TEXT("name"), Name);
    Result->SetBoolField(TEXT("created"), true);
    Result->SetBoolField(TEXT("saved"), bSaved);
    if (!bSaved)
    {
        TArray<TSharedPtr<FJsonValue>> Warnings;
        Warnings.Add(MakeShared<FJsonValueString>(TEXT("Created in memory only: the save failed, so it is lost when the editor closes unless control_editor save_all saves it.")));
        Result->SetArrayField(TEXT("warnings"), Warnings);
    }
    Result->SetBoolField(TEXT("coreDataInstalled"), bCoreData);
    if (!bCoreData)
    {
        // A character created without Core Data is real but untexturable, and
        // the failure would otherwise only appear much later at build time.
        Result->SetStringField(TEXT("coreDataDetail"), CoreDataDetail);
    }
    McpHandlerUtils::AddVerification(Result, Created);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("MetaHuman character created"), Result);
    return true;
}
}
