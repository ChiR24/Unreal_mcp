#include "MCP/Extensions/McpExtensions.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "IEngineWireCapabilityProvider.h"
#include "MCP/Generated/McpGeneratedCapabilityShards.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpExtensionsTestDetail
{
// A provider as a plugin writes one: its records are real generated records, renamed onto its own parent tool.
class FTestProvider final : public IEngineWireCapabilityProvider
{
public:
    FString Records;
    int32 Api = CurrentApiVersion;
    virtual FName ProviderName() const override { return TEXT("McpTestTools"); }
    virtual int32 ApiVersion() const override { return Api; }
    virtual FString RecordsJson() const override { return Records; }
    virtual void Execute(const FMcpExtensionCall& Call) override { Call.Answer(true, TEXT("ran"), nullptr, FString()); }
};

// The first generated record, with its id and parent tool set (an array of one entry, as RecordsJson returns).
FString RenamedRecord(const FString& Id, const FString& Parent)
{
    const McpGeneratedCapabilityShards::FMcpCapabilityShard& Shard = McpGeneratedCapabilityShards::At(0);
    FString Payload;
    for (int32 Index = 0; Index < Shard.ChunkCount; ++Index) Payload.Append(Shard.Chunks[Index]);
    TArray<TSharedPtr<FJsonValue>> Entries;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Payload), Entries);
    const TSharedPtr<FJsonObject> Record = Entries.Num() ? Entries[0]->AsObject()->GetObjectField(TEXT("record")) : nullptr;
    if (!Record.IsValid()) return FString();
    Record->SetStringField(TEXT("id"), Id);
    Record->GetObjectField(TEXT("routing"))->SetStringField(TEXT("parentTool"), Parent);
    FString Out;
    FJsonSerializer::Serialize(TArray<TSharedPtr<FJsonValue>>{Entries[0]}, TJsonWriterFactory<>::Create(&Out));
    return Out;
}
} // namespace McpExtensionsTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpExtensionsCollectTest, "EngineWire.Gateway.Extensions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A provider joins only trusted, on its API, with every record on its ext_ parent under a free id; else it adds
// nothing and says why.
bool FMcpExtensionsCollectTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpExtensionsTestDetail;
    FTestProvider Provider;
    Provider.Records = RenamedRecord(TEXT("ext_mcptesttools.probe"), TEXT("ext_mcptesttools"));
    TestFalse(TEXT("a real record to rename"), Provider.Records.IsEmpty());
    const TArray<IEngineWireCapabilityProvider*> One{&Provider};
    const TArray<FString> Trusted{TEXT("McpTestTools")};

    McpExtensions::FCollected Untrusted = McpExtensions::Collect(One, {}, {});
    TestTrue(TEXT("untrusted adds nothing"), Untrusted.Records.Num() == 0 && Untrusted.Providers.Num() == 1 &&
             !Untrusted.Providers[0].bJoined && Untrusted.Providers[0].Refusal.Contains(TEXT("not trusted")));

    McpExtensions::FCollected Joined = McpExtensions::Collect(One, Trusted, {});
    TestTrue(TEXT("trusted joins with its record and parent tool"), Joined.Records.Num() == 1 && Joined.Providers[0].bJoined &&
             Joined.Providers[0].Parents == TArray<FString>{TEXT("ext_mcptesttools")});

    McpExtensions::FCollected Taken = McpExtensions::Collect(One, Trusted, {TEXT("ext_mcptesttools.probe")});
    TestTrue(TEXT("an id already taken refuses the provider"), Taken.Records.Num() == 0 && Taken.Providers[0].Refusal.Contains(TEXT("taken")));

    Provider.Records = RenamedRecord(TEXT("ext_mcptesttools.probe"), TEXT("control_actor"));
    McpExtensions::FCollected Foreign = McpExtensions::Collect(One, Trusted, {});
    TestTrue(TEXT("a built-in parent tool refuses it"), Foreign.Records.Num() == 0 && Foreign.Providers[0].Refusal.Contains(TEXT("must start with ext_mcptesttools")));

    Provider.Api = IEngineWireCapabilityProvider::CurrentApiVersion + 1;
    TestTrue(TEXT("another API version refuses it"), !McpExtensions::Collect(One, Trusted, {}).Providers[0].bJoined);
    return true;
}
#endif
