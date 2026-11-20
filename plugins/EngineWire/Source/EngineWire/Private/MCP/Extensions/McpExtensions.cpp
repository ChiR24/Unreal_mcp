#include "MCP/Extensions/McpExtensions.h"

#include "EngineWireSettings.h"
#include "Features/IModularFeatures.h"
#include "IEngineWireCapabilityProvider.h"
#include "MCP/Registry/McpToolDefinition.h"
#include "MCP/Registry/McpToolRegistry.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace McpExtensions
{
namespace Detail
{
// Why Provider adds nothing, else empty with its records in Out.
FString Refusal(const IEngineWireCapabilityProvider& Provider, const FString& Prefix, const TArray<FString>& Trusted,
                const TSet<FString>& Taken, TArray<FMcpCapabilityRecord>& Out)
{
    if (!Trusted.Contains(Provider.ProviderName().ToString()))
    {
        return TEXT("not trusted: list its plugin name in Project Settings > EngineWire > Trusted Extension Providers");
    }
    if (Provider.ApiVersion() != IEngineWireCapabilityProvider::CurrentApiVersion)
    {
        return FString::Printf(TEXT("built for extension API %d; this EngineWire takes %d"), Provider.ApiVersion(),
                               IEngineWireCapabilityProvider::CurrentApiVersion);
    }
    TArray<TSharedPtr<FJsonValue>> Entries;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Provider.RecordsJson()), Entries))
    {
        return TEXT("its records are not a JSON array");
    }
    TSet<FString> Seen;
    for (int32 Index = 0; Index < Entries.Num(); ++Index)
    {
        FMcpCapabilityRecord Record;
        FString Error, Scope, Consent;
        if (!Entries[Index].IsValid() || Entries[Index]->Type != EJson::Object ||
            !McpParseCapabilityRecord(Entries[Index]->AsObject(), Record, Error))
        {
            return FString::Printf(TEXT("record %d does not parse: %s"), Index, *Error);
        }
        if (!Record.Parent.StartsWith(Prefix))
        {
            return FString::Printf(TEXT("record '%s' is on parent tool '%s'; its parent tools must start with %s"), *Record.Id,
                                   *Record.Parent, *Prefix);
        }
        if (Taken.Contains(Record.Id) || Seen.Contains(Record.Id))
        {
            return FString::Printf(TEXT("record id '%s' is already taken"), *Record.Id);
        }
        const bool bPolicy = Record.Policy.IsValid() && Record.Policy->TryGetStringField(TEXT("requiredScope"), Scope) && !Scope.IsEmpty();
        if (Record.Effect.IsEmpty() || !bPolicy)
        {
            return FString::Printf(TEXT("record '%s' declares no effect or no requiredScope"), *Record.Id);
        }
        Record.Policy->TryGetStringField(TEXT("consent"), Consent);
        if (Record.Effect == TEXT("destructive") && (Consent.IsEmpty() || Consent == TEXT("none")))
        {
            return FString::Printf(TEXT("record '%s' is destructive with no consent"), *Record.Id);
        }
        Seen.Add(Record.Id);
        Out.Add(MoveTemp(Record));
    }
    return FString();
}
} // namespace Detail

FCollected Collect(const TArray<IEngineWireCapabilityProvider*>& Providers, const TArray<FString>& Trusted,
                   const TSet<FString>& TakenIds)
{
    FCollected Result;
    TSet<FString> Taken = TakenIds;
    for (IEngineWireCapabilityProvider* Provider : Providers)
    {
        if (!Provider)
        {
            continue;
        }
        FProviderEntry& Entry = Result.Providers.AddDefaulted_GetRef();
        Entry.Name = Provider->ProviderName().ToString();
        Entry.ApiVersion = Provider->ApiVersion();
        Entry.Provider = Provider;
        TArray<FMcpCapabilityRecord> Records;
        Entry.Refusal = Detail::Refusal(*Provider, TEXT("ext_") + Entry.Name.ToLower(), Trusted, Taken, Records);
        Entry.bJoined = Entry.Refusal.IsEmpty();
        Entry.RecordCount = Entry.bJoined ? Records.Num() : 0;
        for (FMcpCapabilityRecord& Record : Records)
        {
            if (!Entry.bJoined)
            {
                break;
            }
            Taken.Add(Record.Id);
            Entry.Parents.AddUnique(Record.Parent);
            Result.Records.Add(MoveTemp(Record));
        }
    }
    return Result;
}

const FCollected& Get(const TSet<FString>& BuiltinIds)
{
    // ponytail: read once with the store; a provider whose plugin loads later joins at the next editor start.
    static const FCollected Collected = Collect(
        IModularFeatures::Get().GetModularFeatureImplementations<IEngineWireCapabilityProvider>(IEngineWireCapabilityProvider::FeatureName()),
        GetDefault<UEngineWireSettings>()->TrustedExtensionProviders, BuiltinIds);
    return Collected;
}

const TArray<FProviderEntry>& Providers()
{
    FMcpCapabilityStore::Get(); // its build collects the providers
    return Get(TSet<FString>()).Providers;
}

TSharedPtr<FJsonObject> ListReply()
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Joined = 0;
    for (const FProviderEntry& Entry : Providers())
    {
        const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Entry.Name);
        Row->SetNumberField(TEXT("apiVersion"), Entry.ApiVersion);
        Row->SetBoolField(TEXT("joined"), Entry.bJoined);
        Row->SetNumberField(TEXT("records"), Entry.RecordCount);
        TArray<TSharedPtr<FJsonValue>> Parents;
        for (const FString& Parent : Entry.Parents) Parents.Add(MakeShared<FJsonValueString>(Parent));
        Row->SetArrayField(TEXT("parentTools"), Parents);
        if (!Entry.bJoined) Row->SetStringField(TEXT("refusal"), Entry.Refusal);
        Joined += Entry.bJoined ? 1 : 0;
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), true);
    Out->SetArrayField(TEXT("extensions"), Rows);
    Out->SetNumberField(TEXT("apiVersion"), IEngineWireCapabilityProvider::CurrentApiVersion);
    Out->SetStringField(TEXT("message"), FString::Printf(TEXT("%d extension provider(s) found, %d joined."), Rows.Num(), Joined));
    return Out;
}

void RegisterParentTools()
{
    FMcpToolRegistry& Registry = FMcpToolRegistry::Get();
    for (const FProviderEntry& Entry : Providers())
    {
        for (const FString& Parent : Entry.bJoined ? Entry.Parents : TArray<FString>())
        {
            if (!Registry.FindTool(Parent))
            {
                // ponytail: never freed, as the generated parents' definitions live for the process.
                Registry.Register(new FMcpToolDefinition(*Parent,
                    *FString::Printf(TEXT("Capabilities of the %s extension."), *Entry.Name), TEXT("extension")));
            }
        }
    }
}
} // namespace McpExtensions
