#include "McpRuntimeReads.h"
#include "McpRuntimeServer.h"

#include "Async/Async.h"
#include "Dom/JsonValue.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpRuntimeRpcDetail
{
// One read handed to the game thread, shared with it, so a reply that timed out leaves nothing dangling.
struct FGameThreadRead
{
    FString Capability, Error;
    TSharedPtr<FJsonObject> Params, Data;
    FEvent* Done = FPlatformProcess::GetSynchEventFromPool(true);
    ~FGameThreadRead() { FPlatformProcess::ReturnSynchEventToPool(Done); }
};

TSharedPtr<FJsonObject> ReadOnGameThread(const FString& Capability, const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
    if (IsInGameThread())
    {
        return McpRuntimeRead(Capability, Params, OutError);
    }
    const TSharedRef<FGameThreadRead, ESPMode::ThreadSafe> Read = MakeShared<FGameThreadRead, ESPMode::ThreadSafe>();
    Read->Capability = Capability;
    Read->Params = Params;
    AsyncTask(ENamedThreads::GameThread, [Read]()
    {
        Read->Data = McpRuntimeRead(Read->Capability, Read->Params, Read->Error);
        Read->Done->Trigger();
    });
    if (!Read->Done->Wait(FTimespan::FromSeconds(10)))
    {
        OutError = TEXT("GAME_THREAD_BUSY: the game thread did not take the read within 10 s.");
        return nullptr;
    }
    OutError = Read->Error;
    return Read->Data;
}

FString Serialize(const TSharedRef<FJsonObject>& Object)
{
    FString Out;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));
    return Out;
}

TSharedRef<FJsonObject> ToolSchema()
{
    static const FString Text = TEXT(R"json({"name":"unreal","description":"EngineWire Runtime: reads of the running game (Development and Test builds). execute one capability: runtime.list_actors {classFilter?, limit?} lists actors with class and location; runtime.get_transform {actorName} gives location, rotation, scale and velocity; runtime.frame_cost {} gives frame, game thread, render thread and GPU milliseconds; runtime.screenshot {} saves the next frame as a PNG and gives its path.","inputSchema":{"type":"object","properties":{"operation":{"type":"string","enum":["execute"]},"capability":{"type":"string","enum":["runtime.list_actors","runtime.get_transform","runtime.frame_cost","runtime.screenshot"]},"params":{"type":"object"}},"required":["operation","capability"]}})json");
    TSharedPtr<FJsonObject> Tool;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Tool);
    return Tool.ToSharedRef();
}

TSharedRef<FJsonObject> Call(const TSharedPtr<FJsonObject>& Params)
{
    const TSharedPtr<FJsonObject>* Arguments = nullptr;
    const TSharedPtr<FJsonObject>* ReadParams = nullptr;
    FString Name, Capability, Error;
    Params->TryGetStringField(TEXT("name"), Name);
    if (Params->TryGetObjectField(TEXT("arguments"), Arguments))
    {
        (*Arguments)->TryGetStringField(TEXT("capability"), Capability);
        (*Arguments)->TryGetObjectField(TEXT("params"), ReadParams);
    }
    const TSharedPtr<FJsonObject> Data = Name == TEXT("unreal")
        ? ReadOnGameThread(Capability, ReadParams ? *ReadParams : TSharedPtr<FJsonObject>(), Error) : TSharedPtr<FJsonObject>();
    if (Name != TEXT("unreal"))
    {
        Error = FString::Printf(TEXT("UNKNOWN_TOOL: '%s'; the only tool is unreal."), *Name);
    }
    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    const TSharedRef<FJsonObject> Text = MakeShared<FJsonObject>();
    Text->SetStringField(TEXT("type"), TEXT("text"));
    Text->SetStringField(TEXT("text"), Data.IsValid() ? Serialize(Data.ToSharedRef()) : Error);
    Result->SetArrayField(TEXT("content"), {MakeShared<FJsonValueObject>(Text)});
    if (Data.IsValid())
    {
        Result->SetObjectField(TEXT("structuredContent"), Data);
    }
    Result->SetBoolField(TEXT("isError"), !Data.IsValid());
    return Result;
}
}

FString McpRuntimeAnswer(const FString& Body)
{
    using namespace McpRuntimeRpcDetail;
    TSharedPtr<FJsonObject> Request;
    const TSharedRef<FJsonObject> Reply = MakeShared<FJsonObject>();
    Reply->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Request) || !Request.IsValid())
    {
        Reply->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
        const TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetNumberField(TEXT("code"), -32700);
        Error->SetStringField(TEXT("message"), TEXT("Parse error: the body is not one JSON-RPC object."));
        Reply->SetObjectField(TEXT("error"), Error);
        return Serialize(Reply);
    }
    const TSharedPtr<FJsonValue> Id = Request->TryGetField(TEXT("id"));
    if (!Id.IsValid())
    {
        return FString(); // a notification (notifications/initialized): no answer
    }
    Reply->SetField(TEXT("id"), Id);
    FString Method;
    Request->TryGetStringField(TEXT("method"), Method);
    const TSharedPtr<FJsonObject>* Params = nullptr;
    Request->TryGetObjectField(TEXT("params"), Params);
    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    if (Method == TEXT("initialize"))
    {
        FString Version = TEXT("2025-11-25");
        if (Params)
        {
            (*Params)->TryGetStringField(TEXT("protocolVersion"), Version);
        }
        Result->SetStringField(TEXT("protocolVersion"), Version);
        const TSharedRef<FJsonObject> Capabilities = MakeShared<FJsonObject>();
        Capabilities->SetObjectField(TEXT("tools"), MakeShared<FJsonObject>());
        Result->SetObjectField(TEXT("capabilities"), Capabilities);
        const TSharedRef<FJsonObject> Info = MakeShared<FJsonObject>();
        Info->SetStringField(TEXT("name"), TEXT("enginewire-runtime"));
        Info->SetStringField(TEXT("version"), TEXT("1"));
        Result->SetObjectField(TEXT("serverInfo"), Info);
    }
    else if (Method == TEXT("tools/list"))
    {
        Result->SetArrayField(TEXT("tools"), {MakeShared<FJsonValueObject>(ToolSchema())});
    }
    else if (Method == TEXT("tools/call"))
    {
        Reply->SetObjectField(TEXT("result"), Call(Params ? *Params : TSharedPtr<FJsonObject>(MakeShared<FJsonObject>())));
        return Serialize(Reply);
    }
    else if (Method != TEXT("ping"))
    {
        const TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetNumberField(TEXT("code"), -32601);
        Error->SetStringField(TEXT("message"), FString::Printf(TEXT("Method not found: %s"), *Method));
        Reply->SetObjectField(TEXT("error"), Error);
        return Serialize(Reply);
    }
    Reply->SetObjectField(TEXT("result"), Result);
    return Serialize(Reply);
}
