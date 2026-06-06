#include "McpAutomationBridge_GameFrameworkHandlersContext.h"

DEFINE_LOG_CATEGORY(LogMcpGameFrameworkHandlers);

namespace McpGameFrameworkHandlers
{
void FActionContext::SendError(const FString& Message, const FString& ErrorCode) const
{
    Subsystem->SendAutomationError(RequestingSocket, RequestId, Message, ErrorCode);
}

void FActionContext::SendSuccess(const TSharedPtr<FJsonObject>& Response) const
{
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Success"), Response);
}

FActionContext MakeActionContext(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FActionContext Context;
    Context.Subsystem = Subsystem;
    Context.RequestId = RequestId;
    Context.Payload = Payload;
    Context.RequestingSocket = RequestingSocket;
    return Context;
}

#if WITH_EDITOR
bool ValidateCommonFields(FActionContext& Context)
{
    if (!Context.Payload.IsValid())
    {
        Context.SendError(TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return false;
    }

    Context.SubAction = GetStringField(Context.Payload, TEXT("subAction"));
    if (Context.SubAction.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return false;
    }

    UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("HandleManageGameFrameworkAction: subAction=%s"), *Context.SubAction);

    Context.Name = GetStringField(Context.Payload, TEXT("name"));
    Context.Path = GetStringField(Context.Payload, TEXT("path"), TEXT("/Game"));
    Context.bSave = GetBoolField(Context.Payload, TEXT("save"), false);

    FString SanitizedPath = SanitizeProjectRelativePath(Context.Path);
    if (SanitizedPath.IsEmpty() && !Context.Path.IsEmpty())
    {
        Context.SendError(
            TEXT("Invalid path: path traversal or invalid characters detected. Path must start with /Game/, /Engine/, or /Script/"),
            TEXT("SECURITY_VIOLATION"));
        return false;
    }
    if (!SanitizedPath.IsEmpty())
    {
        Context.Path = SanitizedPath;
    }

    Context.GameModeBlueprint = GetStringField(Context.Payload, TEXT("gameModeBlueprint"));
    if (Context.GameModeBlueprint.IsEmpty())
    {
        Context.GameModeBlueprint = GetStringField(Context.Payload, TEXT("blueprintPath"));
    }

    if (!Context.GameModeBlueprint.IsEmpty())
    {
        FString SanitizedBPPath = SanitizeProjectRelativePath(Context.GameModeBlueprint);
        if (SanitizedBPPath.IsEmpty())
        {
            Context.SendError(
                TEXT("Invalid gameModeBlueprint path: path traversal or invalid characters detected"),
                TEXT("SECURITY_VIOLATION"));
            return false;
        }
        Context.GameModeBlueprint = SanitizedBPPath;
    }

    Context.BlueprintPath = Context.GameModeBlueprint;
    return true;
}

bool RequireGameModePath(FActionContext& Context)
{
    if (!Context.GameModeBlueprint.IsEmpty()) return true;
    Context.SendError(TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
    return false;
}

FString GetStringField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, const FString& Default)
{
    return Payload.IsValid() && Payload->HasField(FieldName) ? GetJsonStringField(Payload, FieldName) : Default;
}

double GetNumberField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, double Default)
{
    return Payload.IsValid() && Payload->HasField(FieldName) ? GetJsonNumberField(Payload, FieldName) : Default;
}

bool GetBoolField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, bool Default)
{
    return Payload.IsValid() && Payload->HasField(FieldName) ? GetJsonBoolField(Payload, FieldName) : Default;
}

TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
{
    return Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName) ? Payload->GetObjectField(FieldName) : nullptr;
}

const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
{
    return Payload.IsValid() && Payload->HasTypedField<EJson::Array>(FieldName) ? &Payload->GetArrayField(FieldName) : nullptr;
}

static void SetBPVarDefaultValueGF(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue)
{
    if (!Blueprint) return;
    McpSafeCompileBlueprint(Blueprint);
    if (!Blueprint->GeneratedClass) return;

    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    if (!CDO || !Property) return;

    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CDO);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Property->ImportText_Direct(*DefaultValue, ValuePtr, CDO, 0);
#else
    Property->ImportText(*DefaultValue, ValuePtr, PPF_None, CDO);
#endif
    Blueprint->MarkPackageDirty();
}

void SetVariableDefaultValue(UBlueprint* Blueprint, const FString& VarName, const FString& DefaultValue)
{
    SetBPVarDefaultValueGF(Blueprint, FName(*VarName), DefaultValue);
}

UBlueprint* LoadBlueprintFromPath(const FString& BlueprintPath)
{
    FString CleanPath = BlueprintPath;
    if (CleanPath.EndsWith(TEXT("_C"))) return nullptr;

    UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CleanPath));
    if (Blueprint) return Blueprint;

    if (CleanPath.EndsWith(TEXT(".uasset")))
    {
        CleanPath = CleanPath.LeftChop(7);
        return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CleanPath));
    }
    return nullptr;
}

UBlueprint* LoadRequiredGameMode(FActionContext& Context)
{
    UBlueprint* Blueprint = LoadBlueprintFromPath(Context.GameModeBlueprint);
    if (!Blueprint)
    {
        Context.SendError(
            FString::Printf(TEXT("Failed to load GameMode: %s"), *Context.GameModeBlueprint),
            TEXT("NOT_FOUND"));
    }
    return Blueprint;
}

UBlueprint* CreateGameFrameworkBlueprint(const FString& Path, const FString& Name, UClass* ParentClass, FString& OutError)
{
    if (!ParentClass)
    {
        OutError = TEXT("Invalid parent class");
        return nullptr;
    }

    FString FullPath = Path;
    if (!FullPath.StartsWith(TEXT("/Game/")))
    {
        if (FullPath.StartsWith(TEXT("/Content/"))) FullPath = FullPath.Replace(TEXT("/Content/"), TEXT("/Game/"));
        else if (!FullPath.StartsWith(TEXT("/"))) FullPath = TEXT("/Game/") + FullPath;
    }
    if (FullPath.EndsWith(TEXT("/"))) FullPath = FullPath.LeftChop(1);

    const FString AssetPath = FullPath / Name;
    if (FindObject<UBlueprint>(nullptr, *AssetPath))
    {
        OutError = FString::Printf(TEXT("Blueprint already exists: %s"), *AssetPath);
        return nullptr;
    }
    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        OutError = FString::Printf(TEXT("Asset already exists at path: %s"), *AssetPath);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *AssetPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ParentClass;
    UBlueprint* Blueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
        UBlueprint::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
    if (!Blueprint)
    {
        OutError = FString::Printf(TEXT("Failed to create %s blueprint"), *ParentClass->GetName());
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    Blueprint->MarkPackageDirty();
    McpSafeCompileBlueprint(Blueprint);
    return Blueprint;
}
#endif
}
