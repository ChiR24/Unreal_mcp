#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace McpNetworkingHandlers
{
namespace
{
// The class the engine instantiates for game traffic (NetDriverDefinitions, GameNetDriver entry).
UClass* ResolveGameNetDriverClassForMcp()
{
    if (!GEngine) return nullptr;
    for (const FNetDriverDefinition& Definition : GEngine->NetDriverDefinitions)
    {
        if (Definition.DefName == NAME_GameNetDriver)
        {
            return LoadClass<UNetDriver>(nullptr, *Definition.DriverClassName.ToString());
        }
    }
    return nullptr;
}
}

bool HandleConfigureNetDriver(FNetworkingActionContext& Context)
{
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    struct FNetDriverSetting { const TCHAR* Field; const TCHAR* Property; };
    static const FNetDriverSetting Settings[] = {
        {TEXT("maxClientRate"), TEXT("MaxClientRate")},
        {TEXT("maxInternetClientRate"), TEXT("MaxInternetClientRate")},
        {TEXT("netServerMaxTickRate"), TEXT("NetServerMaxTickRate")},
    };
    UClass* DriverClass = ResolveGameNetDriverClassForMcp();
    UNetDriver* DriverCDO = DriverClass ? Cast<UNetDriver>(DriverClass->GetDefaultObject()) : nullptr;
    if (!DriverCDO)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Could not resolve the project's game net driver class (Engine NetDriverDefinitions)."), TEXT("NOT_FOUND"));
        return true;
    }

    // The editor world normally has no net driver, so the old reply ("Net driver configured") changed
    // nothing there. The values are the driver class's config properties: write them to its defaults
    // and the project's DefaultEngine.ini, and to a running driver when there is one.
    UWorld* World = GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
    UNetDriver* ActiveDriver = World ? World->GetNetDriver() : nullptr;
    // Every value is checked before any is written: a bad second value used to leave the first on disk.
    TArray<TPair<FIntProperty*, int32>> Pending;
    for (const FNetDriverSetting& Setting : Settings)
    {
        if (!Context.Payload->HasField(Setting.Field)) continue;
        const int32 Value = static_cast<int32>(GetJsonNumberField(Context.Payload, Setting.Field, 0.0));
        FIntProperty* Property = FindFProperty<FIntProperty>(DriverClass, Setting.Property);
        if (!Property || Value <= 0)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                Property ? FString::Printf(TEXT("%s must be a positive whole number."), Setting.Field)
                         : FString::Printf(TEXT("%s has no %s config property."), *DriverClass->GetName(), Setting.Property),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Pending.Emplace(Property, Value);
        ResultJson->SetNumberField(Setting.Field, Value);
    }
    const int32 Written = Pending.Num();
    if (Written == 0)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Nothing to configure: pass maxClientRate, maxInternetClientRate or netServerMaxTickRate."), TEXT("INVALID_PARAMS"));
        return true;
    }
    const FString ConfigFile = DriverCDO->GetDefaultConfigFilename();
    for (const TPair<FIntProperty*, int32>& Entry : Pending)
    {
        Entry.Key->SetPropertyValue_InContainer(DriverCDO, Entry.Value);
        DriverCDO->UpdateSinglePropertyInConfigFile(Entry.Key, ConfigFile);
        if (ActiveDriver && ActiveDriver->IsA(DriverClass))
        {
            Entry.Key->SetPropertyValue_InContainer(ActiveDriver, Entry.Value);
        }
    }
    // UpdateSinglePropertyInConfigFile returns nothing: a read-only or locked DefaultEngine.ini
    // kept the old values while the reply said they were written. Read the file back.
    FConfigFile OnDisk;
    OnDisk.Read(ConfigFile);
    for (const TPair<FIntProperty*, int32>& Entry : Pending)
    {
        int32 Stored = 0;
        if (!OnDisk.GetInt(*DriverClass->GetPathName(), *Entry.Key->GetName(), Stored) || Stored != Entry.Value)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(
                TEXT("%s was set for this session but could not be written to %s (read-only or locked?)."),
                *Entry.Key->GetName(), *FPaths::GetCleanFilename(ConfigFile)), TEXT("PERSIST_FAILED"));
            return true;
        }
    }

    const bool bAppliedToActive = ActiveDriver && ActiveDriver->IsA(DriverClass);
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("appliedToActiveDriver"), bAppliedToActive);
    ResultJson->SetStringField(TEXT("driverClass"), DriverClass->GetPathName());
    ResultJson->SetStringField(TEXT("configFile"), DriverCDO->GetDefaultConfigFilename());
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Wrote %d net driver setting(s) to %s for %s%s"), Written,
        *FPaths::GetCleanFilename(DriverCDO->GetDefaultConfigFilename()), *DriverClass->GetName(),
        bAppliedToActive ? TEXT(" and the running net driver") : TEXT("; they apply from the next session")));
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Net driver configured"), ResultJson);
    return true;
}

bool HandleSetNetRole(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString Role = GetJsonStringField(Payload, TEXT("role"));

    if (BlueprintPath.IsEmpty() || Role.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
        return true;
    }
    ENetRole NetRole = ROLE_None;
    FString ValidRoles;
    if (!TryParseNetEnum(Role, NetRole, ValidRoles))
    {
        ReplyInvalidEnum(Context, TEXT("role"), Role, ValidRoles);
        return true;
    }
    // A Blueprint stores no role: the server always holds authority, and a spawned actor's remote
    // role is derived from bReplicates (autonomous once a player controller possesses it). The one
    // thing the choice sets is whether the actor replicates, so a role that implies nothing is refused.
    if (NetRole == ROLE_Authority)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Every spawned actor has ROLE_Authority on the server; choose how clients see it instead: ROLE_None (not replicated), ROLE_SimulatedProxy, or ROLE_AutonomousProxy (replicated)."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }
    AActor* CDO = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CDO)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Network roles apply to Actors; this Blueprint is not an Actor."), TEXT("NOT_SUPPORTED"));
        return true;
    }
    CDO->SetReplicates(NetRole != ROLE_None);

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    const bool bReplicates = CDO->GetIsReplicated();
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("role"), NetRoleToString(NetRole));
    ResultJson->SetBoolField(TEXT("replicates"), bReplicates);
    ResultJson->SetStringField(TEXT("message"), NetRole == ROLE_None
        ? FString(TEXT("Replication turned off: clients never receive this actor (ROLE_None)."))
        : FString::Printf(TEXT("Replication turned on (replicates=%s): clients see it as ROLE_SimulatedProxy, and as ROLE_AutonomousProxy once a player controller possesses it."), bReplicates ? TEXT("true") : TEXT("false")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Net role configured"), ResultJson);
    return true;
}
bool HandleConfigureReplicatedMovement(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    bool bReplicateMovement = GetJsonBoolField(Payload, TEXT("replicateMovement"), true);

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
        return true;
    }

    // A Blueprint whose class never compiled has no defaults to write; the edit used to be skipped and reported.
    AActor* CDO = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CDO)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("The Blueprint has no compiled Actor class to configure; compile it first."), TEXT("NOT_SUPPORTED"));
        return true;
    }
    if (CDO)
    {
        CDO->SetReplicatingMovement(bReplicateMovement);
    }

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Replicate movement set to %s"), bReplicateMovement ? TEXT("true") : TEXT("false")), TEXT("Replicated movement configured"));
}
}
