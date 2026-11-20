// The EngineWire sample extension: one capability, ext_enginewiresample.echo, added through
// IEngineWireCapabilityProvider. It shows the whole path a plugin takes: register the provider when the module starts,
// give records in the generated shard shape on the ext_<plugin> parent tool, and answer each call once.

#include "IEngineWireCapabilityProvider.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

namespace
{
const TCHAR* const SampleRecords = TEXT(R"json([{"record":{
  "id":"ext_enginewiresample.echo",
  "aliases":[],
  "legacyIds":[{"tool":"ext_enginewiresample","action":"echo"}],
  "discovery":{"domain":"extension","family":"sample","topics":["sample extension","echo text"],
    "summary":"Echo text back: the EngineWire sample extension's one capability, a call that passes the same gate as a built-in one and is answered by another plugin.",
    "whenToUse":["Checking that an extension provider joined and its calls run."],
    "whenNotToUse":["Any real editor work."]},
  "schemas":{
    "input":{"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{"text":{"type":"string","maxLength":200,"description":"Text to echo back."}},"required":["text"],"additionalProperties":false},
    "output":{"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{"success":{"type":"boolean"},"message":{"type":"string"},
      "text":{"type":"string","description":"The text sent."},"length":{"type":"number","description":"Its length in characters."}},"required":["success"],"additionalProperties":false}},
  "examples":[{"title":"Echo a word","input":{"action":"echo","text":"hello"},"output":{"success":true,"message":"Echoed 5 character(s).","text":"hello","length":5}}],
  "availability":{"requiredPlugins":[],"editorStates":["edit","pie","simulate"]},
  "behavior":{"effect":"read","idempotency":"idempotent","longRunning":false,"safeToRetry":true},
  "policy":{"requiredScope":"read","consent":"none","dataAccess":"project-read"},
  "cost":{"latency":"instant","resources":"low"},
  "routing":{"parentTool":"ext_enginewiresample","dispatchAction":"echo"},
  "hashes":{"algorithm":"sha256","schema":"sample","content":"sample"}
}}])json");

class FSampleProvider final : public IEngineWireCapabilityProvider
{
public:
    virtual FName ProviderName() const override { return TEXT("EngineWireSample"); }
    virtual int32 ApiVersion() const override { return CurrentApiVersion; }
    virtual FString RecordsJson() const override { return SampleRecords; }

    virtual void Execute(const FMcpExtensionCall& Call) override
    {
        FString Text;
        if (Call.Params.IsValid())
        {
            Call.Params->TryGetStringField(TEXT("text"), Text);
        }
        const TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("text"), Text);
        Data->SetNumberField(TEXT("length"), Text.Len());
        Call.Answer(true, FString::Printf(TEXT("Echoed %d character(s)."), Text.Len()), Data, FString());
    }
};

class FEngineWireSampleModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        IModularFeatures::Get().RegisterModularFeature(IEngineWireCapabilityProvider::FeatureName(), &Provider);
    }
    virtual void ShutdownModule() override
    {
        IModularFeatures::Get().UnregisterModularFeature(IEngineWireCapabilityProvider::FeatureName(), &Provider);
    }

private:
    FSampleProvider Provider;
};
} // namespace

IMPLEMENT_MODULE(FEngineWireSampleModule, EngineWireSample)
