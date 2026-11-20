// McpEngineRoles.cpp — see header: the role tables, the per-role cache, and the snapshot other threads read. The
// stages for one role are in McpEngineRolesResolve.cpp.
#include "Foundation/Reflect/McpEngineRoles.h"

#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/Reflect/McpEngineRolesMatch.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeRWLock.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include <atomic>

using McpEngineRoles::FRoleHit;

namespace
{
struct FRoleState
{
    TMap<FName, TSharedPtr<FJsonObject>> Rows;
    TMap<FName, FString> DomainOf;
    TMap<FName, FString> PluginOf;
    TMap<FName, FRoleHit> Hits;
    TSet<FName> InProgress;
    bool bLoaded = false;
};
FRoleState& State()
{
    static FRoleState Instance;
    return Instance;
}

FRWLock SnapshotLock;
TMap<FName, FString> Snapshot; // id -> empty when resolved, else why not
bool bSnapshotReady = false;
std::atomic<bool> bRefreshQueued{false};

void AddTable(const FString& File, const TSharedPtr<FJsonObject>& Table)
{
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!Table.IsValid() || !Table->TryGetArrayField(TEXT("roles"), Rows) || !Rows)
    {
        return;
    }
    const FString Domain = GetJsonStringField(Table, TEXT("domain"), FPaths::GetBaseFilename(File));
    const FString Plugin = GetJsonStringField(Table, TEXT("plugin"), TEXT(""));
    for (const TSharedPtr<FJsonValue>& Value : *Rows)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
        const FString Id = Row.IsValid() ? GetJsonStringField(Row, TEXT("id"), TEXT("")) : FString();
        if (!Id.IsEmpty())
        {
            State().Rows.Add(FName(*Id), Row);
            State().DomainOf.Add(FName(*Id), Domain);
            State().PluginOf.Add(FName(*Id), Plugin);
        }
    }
}

void LoadTables()
{
    FRoleState& S = State();
    if (S.bLoaded)
    {
        return;
    }
    S.bLoaded = true;
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("EngineWire"));
    const FString Dir = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("EngineRoles")) : FString();
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);
    for (const FString& File : Files)
    {
        FString Text;
        TSharedPtr<FJsonObject> Table;
        if (!File.Contains(TEXT(".generated.")) && FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, File)) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Table))
        {
            AddTable(File, Table);
        }
    }
    // A module that loads or unloads can add or take away what plays a role: resolve again on the next tick.
    FModuleManager::Get().OnModulesChanged().AddLambda([](FName, EModuleChangeReason)
    {
        State().Hits.Reset();
        McpEngineRoles::ScheduleRefresh();
    });
}

FString FoundName(const FRoleHit& Hit)
{
    return Hit.Type ? Hit.Type->GetPathName() : Hit.Property ? Hit.Property->GetName() : Hit.Function ? Hit.Function->GetName() : FString();
}
}

namespace McpEngineRoles
{
FRoleHit Resolve(FName RoleId)
{
    check(IsInGameThread());
    LoadTables();
    FRoleState& S = State();
    if (const FRoleHit* Done = S.Hits.Find(RoleId))
    {
        return *Done;
    }
    FRoleHit Hit;
    const TSharedPtr<FJsonObject>* Row = S.Rows.Find(RoleId);
    if (!Row || S.InProgress.Contains(RoleId))
    {
        Hit.Evidence = Row ? FString::Printf(TEXT("%s is its own owner"), *RoleId.ToString())
                           : FString::Printf(TEXT("no role %s in Resources/EngineRoles"), *RoleId.ToString());
        return Hit;
    }
    const TSharedPtr<FJsonObject> Found = *Row; // a copy: resolving the owner can grow the maps
    S.InProgress.Add(RoleId);
    const FString Kind = GetJsonStringField(Found, TEXT("kind"), TEXT(""));
    if (Kind == TEXT("class") || Kind == TEXT("struct"))
    {
        Hit = McpEngineRolesMatch::ResolveType(Found, Kind == TEXT("class"));
    }
    else if (Kind == TEXT("property") || Kind == TEXT("function"))
    {
        Hit = McpEngineRolesMatch::ResolveMember(Found, Kind);
    }
    else
    {
        Hit.Evidence = FString::Printf(TEXT("unknown kind '%s'"), *Kind);
    }
    S.InProgress.Remove(RoleId);
    S.Hits.Add(RoleId, Hit);
    return Hit;
}

UStruct* TypeOf(FName RoleId) { return Resolve(RoleId).Type; }
FProperty* PropertyOf(FName RoleId) { return Resolve(RoleId).Property; }
UFunction* FunctionOf(FName RoleId) { return Resolve(RoleId).Function; }

// The role whose owner Owner is (or derives from) and whose first-guess name is Name, resolved here; empty when none.
static FRoleHit HitByName(const UStruct* Owner, FName Name, const TCHAR* Kind)
{
    LoadTables();
    for (const TPair<FName, TSharedPtr<FJsonObject>>& Pair : State().Rows)
    {
        if (!Owner || GetJsonStringField(Pair.Value, TEXT("kind"), TEXT("")) != Kind ||
            FName(*GetJsonStringField(Pair.Value, TEXT("name"), TEXT(""))) != Name)
        {
            continue;
        }
        const UStruct* RoleOwner = TypeOf(FName(*GetJsonStringField(Pair.Value, TEXT("owner"), TEXT(""))));
        if (RoleOwner && Owner->IsChildOf(RoleOwner))
        {
            return Resolve(Pair.Key);
        }
    }
    return FRoleHit();
}

FProperty* PropertyByName(const UStruct* Owner, FName Name)
{
    FProperty* Played = HitByName(Owner, Name, TEXT("property")).Property;
    return Played ? Played : (Owner ? Owner->FindPropertyByName(Name) : nullptr);
}

UFunction* FunctionByName(const UClass* Owner, FName Name)
{
    UFunction* Played = HitByName(Owner, Name, TEXT("function")).Function;
    return Played ? Played : (Owner ? Owner->FindFunctionByName(Name) : nullptr);
}

bool IsKnownResolved(FName RoleId, FString* OutWhy)
{
    FReadScopeLock Lock(SnapshotLock);
    const FString* Why = Snapshot.Find(RoleId);
    if (Why && Why->IsEmpty())
    {
        return true;
    }
    if (OutWhy)
    {
        *OutWhy = !bSnapshotReady ? FString(TEXT("not resolved yet: the editor is starting"))
                : Why             ? *Why
                                  : FString::Printf(TEXT("no role %s"), *RoleId.ToString());
    }
    return false;
}

void RefreshAll()
{
    LoadTables();
    TArray<FName> Ids;
    State().Rows.GetKeys(Ids);
    Ids.Sort(FNameLexicalLess());
    TMap<FName, FString> Next;
    for (const FName Id : Ids)
    {
        const FRoleHit Hit = Resolve(Id);
        Next.Add(Id, Hit.IsResolved() ? FString() : Hit.Evidence);
        // A domain (a record's "MVVM") stands for every role of its table that is not optional: the first one missing.
        const FName Domain(*State().DomainOf.FindRef(Id));
        FString& Verdict = Next.FindOrAdd(Domain);
        bool bOptional = false;
        State().Rows.FindRef(Id)->TryGetBoolField(TEXT("optional"), bOptional);
        if (!Hit.IsResolved() && !bOptional && Verdict.IsEmpty())
        {
            Verdict = FString::Printf(TEXT("%s: %s"), *Id.ToString(), *Hit.Evidence);
        }
    }
    FWriteScopeLock Lock(SnapshotLock);
    Snapshot = MoveTemp(Next);
    bSnapshotReady = true;
}

void ScheduleRefresh()
{
    if (bRefreshQueued.exchange(true))
    {
        return;
    }
    // The core ticker is added to on the game thread only, so a task posts it there.
    AsyncTask(ENamedThreads::GameThread, []
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
            bRefreshQueued = false;
            RefreshAll();
            return false;
        }));
    });
}

TSharedRef<FJsonObject> Describe(const FString& Domain)
{
    LoadTables();
    TArray<FName> Ids;
    State().Rows.GetKeys(Ids);
    Ids.Sort(FNameLexicalLess());
    TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Resolved = 0;
    for (const FName Id : Ids)
    {
        const FString Of = State().DomainOf.FindRef(Id);
        if (!Domain.IsEmpty() && !Of.Equals(Domain, ESearchCase::IgnoreCase))
        {
            continue;
        }
        const FRoleHit Hit = Resolve(Id);
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("role"), Id.ToString());
        Row->SetStringField(TEXT("domain"), Of);
        Row->SetStringField(TEXT("plugin"), State().PluginOf.FindRef(Id));
        Row->SetBoolField(TEXT("resolved"), Hit.IsResolved());
        if (Hit.IsResolved())
        {
            Row->SetStringField(TEXT("stage"), Hit.Stage);
            Row->SetStringField(TEXT("found"), FoundName(Hit));
        }
        Row->SetStringField(TEXT("evidence"), Hit.Evidence);
        Resolved += Hit.IsResolved() ? 1 : 0;
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    const TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetArrayField(TEXT("roles"), Rows);
    Out->SetNumberField(TEXT("count"), Rows.Num());
    Out->SetNumberField(TEXT("resolved"), Resolved);
    return Out;
}
}
