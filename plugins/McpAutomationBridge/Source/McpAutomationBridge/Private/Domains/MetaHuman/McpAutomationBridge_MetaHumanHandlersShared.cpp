#include "Domains/MetaHuman/McpAutomationBridge_MetaHumanHandlers.h"

DEFINE_LOG_CATEGORY(LogMcpMetaHumanHandlers);

#if WITH_EDITOR
#include "Editor.h"
#include "EditorSubsystem.h"
#include "Foundation/Reflection/McpReflectedInvoke.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

namespace McpMetaHumanHandlers
{
const TCHAR* const MetaHumanCharacterClassPath = TEXT("/Script/MetaHumanCharacter.MetaHumanCharacter");
const TCHAR* const MetaHumanSubsystemClassPath = TEXT("/Script/MetaHumanCharacterEditor.MetaHumanCharacterEditorSubsystem");
const TCHAR* const MetaHumanFactoryClassPath = TEXT("/Script/MetaHumanCharacterEditor.MetaHumanCharacterFactoryNew");

UClass* FindMetaHumanCharacterClass()
{
    // Resolved by path rather than by C++ type so this compiles and links on
    // engines that ship no MetaHuman plugin at all.
    return FindObject<UClass>(nullptr, MetaHumanCharacterClassPath);
}

UObject* FindMetaHumanSubsystem()
{
    if (!GEditor)
    {
        return nullptr;
    }
    UClass* SubsystemClass = FindObject<UClass>(nullptr, MetaHumanSubsystemClassPath);
    if (!SubsystemClass || !SubsystemClass->IsChildOf(UEditorSubsystem::StaticClass()))
    {
        return nullptr;
    }
    return GEditor->GetEditorSubsystemBase(TSubclassOf<UEditorSubsystem>(SubsystemClass));
}

bool IsCoreDataInstalled(FString& OutDetail)
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("MetaHumanCharacter"));
    if (!Plugin.IsValid())
    {
        OutDetail = TEXT("MetaHumanCharacter plugin is not installed or not enabled.");
        return false;
    }

    // The engine module gates on these same three folders; a partial install is
    // as broken as no install, so name the missing one rather than answering a
    // bare boolean the caller cannot act on.
    const FString OptionalDir = Plugin->GetContentDir() / TEXT("Optional");
    TArray<FString> Missing;
    if (!FPaths::DirectoryExists(OptionalDir)) { Missing.Add(TEXT("Optional")); }
    if (!FPaths::DirectoryExists(OptionalDir / TEXT("TextureSynthesis"))) { Missing.Add(TEXT("Optional/TextureSynthesis")); }
    if (!FPaths::DirectoryExists(OptionalDir / TEXT("BodyTextures"))) { Missing.Add(TEXT("Optional/BodyTextures")); }

    if (Missing.Num() > 0)
    {
        OutDetail = FString::Printf(
            TEXT("MetaHuman Creator Core Data is not installed: missing %s under %s. ")
            TEXT("Install 'MetaHuman Creator Core Data' for this engine from the Epic Games Launcher; ")
            TEXT("without it texture synthesis and body textures are unavailable."),
            *FString::Join(Missing, TEXT(", ")), *Plugin->GetContentDir());
        return false;
    }

    OutDetail = FString::Printf(TEXT("Core Data present at %s"), *OptionalDir);
    return true;
}

bool InvokeMetaHumanFunction(UObject* Target, const TCHAR* FunctionName,
    const TSharedPtr<FJsonObject>& Args, UObject* CharacterArg,
    TSharedPtr<FJsonObject>& OutResults, FString& OutError)
{
    if (!Target)
    {
        OutError = TEXT("No MetaHuman object to call.");
        return false;
    }
    UFunction* Function = Target->FindFunction(FName(FunctionName));
    if (!Function)
    {
        // A missing UFUNCTION is exactly how an older MetaHuman version presents
        // itself, so name the function rather than reporting a generic failure.
        OutError = FString::Printf(TEXT("'%s' does not exist on this MetaHuman version (%s)."),
            FunctionName, *Target->GetClass()->GetName());
        return false;
    }

    FMcpScopedParamBlock ParamBlock(Function);
    TArray<TSharedPtr<FJsonValue>> Unset;
    if (!McpBindJsonArgsToParams(Function, Args, ParamBlock.Data(), Unset, OutError))
    {
        return false;
    }

    // Object parameters are bound here rather than through JSON: the character
    // is a live UObject the caller named by asset path, and every one of these
    // functions takes it as its object parameter.
    if (CharacterArg)
    {
        for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
        {
            FObjectProperty* ObjectParam = CastField<FObjectProperty>(*It);
            if (!ObjectParam || ObjectParam->HasAnyPropertyFlags(CPF_ReturnParm)) { continue; }
            if (!CharacterArg->IsA(ObjectParam->PropertyClass)) { continue; }
            ObjectParam->SetObjectPropertyValue(ObjectParam->ContainerPtrToValuePtr<void>(ParamBlock.Data()), CharacterArg);
            break;
        }
    }

    Target->ProcessEvent(Function, ParamBlock.Data());
    OutResults = McpReadParamOutputs(Function, ParamBlock.Data());
    return true;
}

UObject* LoadMetaHumanCharacter(const FString& AssetPath)
{
    UClass* CharacterClass = FindMetaHumanCharacterClass();
    if (!CharacterClass || AssetPath.IsEmpty())
    {
        return nullptr;
    }
    UObject* Loaded = StaticLoadObject(CharacterClass, nullptr, *AssetPath);
    return (Loaded && Loaded->IsA(CharacterClass)) ? Loaded : nullptr;
}

void SendMetaHumanUnavailable(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, FMetaHumanSocket Socket)
{
    Self->SendAutomationError(Socket, RequestId,
        TEXT("MetaHuman Creator is not available on this editor. It ships with UE 5.6 and later; ")
        TEXT("enable the 'MetaHumanCharacter' plugin in the project and restart the editor."),
        TEXT("FEATURE_UNAVAILABLE"));
}

UObject* RequireEditableCharacter(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket, UObject*& OutSubsystem)
{
    OutSubsystem = FindMetaHumanSubsystem();
    if (!OutSubsystem)
    {
        SendMetaHumanUnavailable(Self, RequestId, Socket);
        return nullptr;
    }

    const FString AssetPath = GetJsonStringField(Payload, TEXT("characterPath"));
    if (AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Missing 'characterPath'."), TEXT("MISSING_PARAMETER"));
        return nullptr;
    }

    UObject* Character = LoadMetaHumanCharacter(AssetPath);
    if (!Character)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("MetaHuman character not found: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        return nullptr;
    }

    // Registering an already-registered character is a no-op subsystem-side, so
    // running it on every call is safe and spares the caller an explicit open
    // step that the subsystem otherwise fails on with only a log line.
    TSharedPtr<FJsonObject> Results;
    FString Error;
    if (!InvokeMetaHumanFunction(OutSubsystem, TEXT("TryAddObjectToEdit"),
            MakeShared<FJsonObject>(), Character, Results, Error))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Could not open the character for editing: %s"), *Error), TEXT("OPERATION_FAILED"));
        return nullptr;
    }
    return Character;
}
}
#endif
