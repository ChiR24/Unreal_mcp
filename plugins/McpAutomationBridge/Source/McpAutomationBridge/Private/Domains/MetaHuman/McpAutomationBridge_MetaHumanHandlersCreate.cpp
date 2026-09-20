#include "Domains/MetaHuman/McpAutomationBridge_MetaHumanHandlers.h"

#if WITH_EDITOR
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
bool HandleCreateMetaHuman(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
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

    McpSafeAssetSave(Created);

    FString CoreDataDetail;
    const bool bCoreData = IsCoreDataInstalled(CoreDataDetail);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("characterPath"), FullPath);
    Result->SetStringField(TEXT("name"), Name);
    Result->SetBoolField(TEXT("created"), true);
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
#endif
