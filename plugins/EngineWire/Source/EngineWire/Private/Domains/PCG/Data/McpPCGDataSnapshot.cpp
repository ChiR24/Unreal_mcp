#include "Domains/PCG/Data/McpPCGDataSnapshot.h"

#if MCP_HAS_PCG_EXECUTION
#include "PCGData.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpPCGHandlers
{
namespace
{
bool CheckProjectionValue(const TSharedPtr<FJsonValue>& Value, FPCGReadBudget& Budget, int32 Depth)
{
    if (!Value.IsValid() || Depth > 6 || ++Budget.Values > 4096 || !Budget.IsWithinTime()) return false;
    if (Value->Type == EJson::Number) return FMath::IsFinite(Value->AsNumber());
    if (Value->Type == EJson::String) Budget.Characters += Value->AsString().Len();
    else if (Value->Type == EJson::Array)
    {
        const auto& Array = Value->AsArray();
        if (Array.Num() > 4096 - Budget.Values) return false;
        for (const auto& Item : Array) if (!CheckProjectionValue(Item, Budget, Depth + 1)) return false;
    }
    else if (Value->Type == EJson::Object)
    {
        const auto Object = Value->AsObject();
        if (!Object.IsValid() || Object->Values.Num() > 4096 - Budget.Values) return false;
        for (const auto& Pair : Object->Values)
        {
            Budget.Characters += Pair.Key.Len();
            if (Budget.Characters > FPCGDataSnapshot::MaxCharacters || !CheckProjectionValue(Pair.Value, Budget, Depth + 1)) return false;
        }
    }
    else if (Value->Type != EJson::Null && Value->Type != EJson::Boolean) return false;
    return Budget.Characters <= FPCGDataSnapshot::MaxCharacters;
}

bool IsIdle(UPCGComponent* Component)
{
    return Component && !Component->IsPartitioned() && !Component->IsLocalComponent() && !Component->IsManagedByRuntimeGenSystem() &&
        !Component->IsGenerating() && !Component->IsCleaningUp() && !HasActivePCGOperation(Component);
}
}

FPCGDataSnapshot::~FPCGDataSnapshot()
{
    check(IsInGameThread());
    if (UPCGComponent* Component = Target.Get())
    {
        Component->OnPCGGraphStartGeneratingDelegate.Remove(Started);
        Component->OnPCGGraphGeneratedDelegate.Remove(Generated);
        Component->OnPCGGraphCleanedDelegate.Remove(Cleaned);
        Component->OnPCGGraphCancelledDelegate.Remove(Cancelled);
    }
}

TSharedPtr<FPCGDataSnapshot> CreatePCGDataSnapshot(UPCGComponent* Component, const UPCGData* Data,
    const FString& Principal, const TArray<TSharedPtr<FJsonValue>>& Rows, FString& OutError)
{
    OutError.Reset();
    if (!IsInGameThread() || !IsIdle(Component) || !IsValid(Data) || Principal.IsEmpty() ||
        !Component->GetWorld() || Rows.Num() > FPCGDataSnapshot::MaxRows)
    {
        OutError = TEXT("A complete bounded projection, trusted owner and idle world component are required.");
        return nullptr;
    }
    FPCGReadBudget Budget;
    auto Snapshot = MakeShared<FPCGDataSnapshot>();
    Snapshot->Owner = Principal;
    Snapshot->Target = Component;
    Snapshot->Source = Data;
    Snapshot->ComponentPath = Component->GetPathName();
    Snapshot->DataPath = Data->GetPathName();
    if (Snapshot->ComponentPath.Len() > 4096 || Snapshot->DataPath.Len() > 4096)
    {
        OutError = TEXT("Native component or dataset identity exceeds the bounded locator size.");
        return nullptr;
    }
    Snapshot->World = Component->GetWorld();
    Snapshot->Graph = Component->GetGraph();
    Snapshot->GraphRevision = Component->GetGraph() ? GetPCGGraphRevision(Component->GetGraph()) : TEXT("no_graph");
    Snapshot->InstanceRevision = GetPCGInstanceRevision(Component);
    if (Snapshot->GraphRevision.IsEmpty() || Snapshot->InstanceRevision.IsEmpty() || !Budget.IsWithinTime())
    {
        OutError = TEXT("A complete bounded graph and component revision is unavailable.");
        return nullptr;
    }
    int64 EncodedCharacters = 0;
    for (const auto& Row : Rows)
    {
        if (!CheckProjectionValue(Row, Budget, 0))
        {
            OutError = TEXT("Projected values exceed the finite-value, nesting, size or time budget.");
            return nullptr;
        }
        auto Envelope = MakeShared<FJsonObject>();
        Envelope->SetField(TEXT("row"), Row);
        FString Encoded;
        auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded);
        if (!FJsonSerializer::Serialize(Envelope, Writer))
        {
            OutError = TEXT("A projected row could not be encoded losslessly.");
            return nullptr;
        }
        EncodedCharacters += Encoded.Len();
        if (Encoded.Len() > FPCGDataSnapshot::MaxRowCharacters || EncodedCharacters > FPCGDataSnapshot::MaxCharacters || !Budget.IsWithinTime())
        {
            OutError = TEXT("Encoded projection exceeds the row, snapshot or time budget; no partial view was retained.");
            return nullptr;
        }
        Snapshot->EncodedRows.Add(MoveTemp(Encoded));
    }
    Snapshot->EncodedCharacterCount = EncodedCharacters;
    Snapshot->ExpiresAt = FPlatformTime::Seconds() + FPCGDataSnapshot::LifetimeSeconds;
    // Native callbacks retain only a thread-safe flag, never the GT-owned snapshot or a UObject.
    const auto Invalidated = Snapshot->Invalidated;
    const auto Invalidate = [Invalidated](UPCGComponent*) { Invalidated->store(true); };
    Snapshot->Started = Component->OnPCGGraphStartGeneratingDelegate.AddLambda(Invalidate);
    Snapshot->Generated = Component->OnPCGGraphGeneratedDelegate.AddLambda(Invalidate);
    Snapshot->Cleaned = Component->OnPCGGraphCleanedDelegate.AddLambda(Invalidate);
    Snapshot->Cancelled = Component->OnPCGGraphCancelledDelegate.AddLambda(Invalidate);
    return Snapshot;
}

bool FPCGDataSnapshot::ReadPage(const FString& Principal, UPCGComponent* Component, int32 Offset, int32 Limit,
    TArray<TSharedPtr<FJsonValue>>& OutRows, FString& OutError) const
{
    OutRows.Reset();
    OutError.Reset();
    if (!IsInGameThread())
    {
        OutError = TEXT("Native projection reads require the existing game-thread dispatch.");
        return false;
    }
    FPCGReadBudget Budget;
    const UPCGData* Data = Source.Get();
    if (Principal != Owner || !Component || Component != Target.Get() || !Data ||
        !World.IsValid() || Component->GetWorld() != World.Get() || Component->GetPathName() != ComponentPath ||
        Data->GetPathName() != DataPath || Invalidated->load() || FPlatformTime::Seconds() >= ExpiresAt || !IsIdle(Component) ||
        Component->GetGraph() != Graph.Get())
    {
        OutError = TEXT("The projection is unavailable for this owner and exact target.");
        return false;
    }
    const FString CurrentGraph = Component->GetGraph() ? GetPCGGraphRevision(Component->GetGraph()) : TEXT("no_graph");
    if (CurrentGraph.IsEmpty() || !Budget.IsWithinTime())
    {
        OutError = TEXT("The projection's complete graph revision is unavailable within the existing read bounds.");
        return false;
    }
    if (CurrentGraph != GraphRevision)
    {
        OutError = TEXT("The projection's authored graph revision changed.");
        return false;
    }
    const FString CurrentInstance = GetPCGInstanceRevision(Component);
    if (CurrentInstance.IsEmpty() || !Budget.IsWithinTime())
    {
        OutError = TEXT("The projection's complete component revision is unavailable within the existing read bounds.");
        return false;
    }
    if (CurrentInstance != InstanceRevision)
    {
        OutError = TEXT("The projection's authored component revision changed.");
        return false;
    }
    if (Offset < 0 || Limit < 1 || Limit > MaxPageRows)
    {
        OutError = TEXT("Projection page offset or limit is outside its explicit bounds.");
        return false;
    }
    TArray<TSharedPtr<FJsonValue>> Page;
    int64 Characters = 0;
    for (int32 Index = Offset; Index < EncodedRows.Num() && Page.Num() < Limit; ++Index)
    {
        Characters += EncodedRows[Index].Len();
        if (Characters > MaxPageCharacters || !Budget.IsWithinTime())
        {
            OutError = TEXT("Requested projection page exceeds its encoded output budget; request fewer rows.");
            return false;
        }
        TSharedPtr<FJsonObject> Row;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(EncodedRows[Index]), Row) || !Row.IsValid())
        {
            OutError = TEXT("The retained projection row could not be decoded.");
            return false;
        }
        const auto Value = Row->TryGetField(TEXT("row"));
        if (!Value.IsValid() || !Budget.IsWithinTime())
        {
            OutError = TEXT("The projection page could not be decoded completely within its read budget.");
            return false;
        }
        Page.Add(Value);
    }
    OutRows = MoveTemp(Page);
    return true;
}
}
#endif
