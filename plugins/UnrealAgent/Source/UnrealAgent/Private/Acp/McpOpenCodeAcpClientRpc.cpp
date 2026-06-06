#include "McpOpenCodeAcpClient.h"
#include "McpOpenCodeAcpClientPrivate.h"

#include "UnrealAgentEditorContext.h"
#include "UnrealAgentStudioKit.h"
#include "UnrealAgentValidationRunner.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

using namespace UnrealAgent::OpenCodeAcp;

bool FOpenCodeAcpClient::SendInitialize()
{
    auto Capabilities = MakeObject();

    auto Fs = MakeObject();
    Fs->SetBoolField(TEXT("readTextFile"), false);
    Fs->SetBoolField(TEXT("writeTextFile"), false);
    Capabilities->SetObjectField(TEXT("fs"), Fs);
    Capabilities->SetBoolField(TEXT("terminal"), false);

    auto ClientInfo = MakeObject();
    ClientInfo->SetStringField(TEXT("name"), TEXT("Unreal Agent"));
    ClientInfo->SetStringField(TEXT("version"), TEXT("0.1.0"));

    auto Params = MakeObject();
    Params->SetNumberField(TEXT("protocolVersion"), 1.0);
    Params->SetObjectField(TEXT("clientCapabilities"), Capabilities);
    Params->SetObjectField(TEXT("clientInfo"), ClientInfo);

    InitializeRequestId = SendRequest(TEXT("initialize"), Params);
    if (InitializeRequestId == INDEX_NONE)
    {
        return false;
    }

    InitializeRequestStartedAt = FPlatformTime::Seconds();
    return true;
}

bool FOpenCodeAcpClient::SendNewSession()
{
    TArray<TSharedPtr<FJsonValue>> McpServers;
    AddConfiguredMcpServers(McpServers);

    auto Params = MakeObject();
    Params->SetStringField(TEXT("cwd"), WorkingDirectory);
    Params->SetArrayField(TEXT("mcpServers"), McpServers);

    NewSessionRequestId = SendRequest(TEXT("session/new"), Params);
    if (NewSessionRequestId == INDEX_NONE)
    {
        return false;
    }

    NewSessionRequestStartedAt = FPlatformTime::Seconds();
    return true;
}

bool FOpenCodeAcpClient::EnsureProjectUnrealAgentConfig()
{
    const FUnrealAgentStudioKitResult Result = FUnrealAgentStudioKit::EnsureForProject(WorkingDirectory);
    LastStudioKitSummary = Result.Summary;
    return Result.WasSuccessful();
}

void FOpenCodeAcpClient::AddConfiguredMcpServers(TArray<TSharedPtr<FJsonValue>>& McpServers) const
{
    bool bEnableNativeMcp = false;
    if (GConfig == nullptr || !GConfig->GetBool(AutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), bEnableNativeMcp, GGameIni) || !bEnableNativeMcp)
    {
        return;
    }

    int32 NativeMcpPort = 3000;
    GConfig->GetInt(AutomationBridgeSettingsSection, TEXT("NativeMCPPort"), NativeMcpPort, GGameIni);
    if (NativeMcpPort <= 0 || NativeMcpPort > 65535)
    {
        NativeMcpPort = 3000;
    }

    FString ListenHost = TEXT("127.0.0.1");
    GConfig->GetString(AutomationBridgeSettingsSection, TEXT("ListenHost"), ListenHost, GGameIni);
    const FString Url = FString::Printf(TEXT("http://%s:%d/mcp"), *NormalizeMcpHostForUrl(ListenHost), NativeMcpPort);

    auto Server = MakeObject();
    Server->SetStringField(TEXT("type"), TEXT("http"));
    Server->SetStringField(TEXT("name"), UnrealMcpServerName);
    Server->SetStringField(TEXT("url"), Url);

    TArray<TSharedPtr<FJsonValue>> Headers;
    bool bRequireCapabilityToken = false;
    FString CapabilityToken;
    GConfig->GetBool(AutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), bRequireCapabilityToken, GGameIni);
    GConfig->GetString(AutomationBridgeSettingsSection, TEXT("CapabilityToken"), CapabilityToken, GGameIni);
    if (bRequireCapabilityToken && !CapabilityToken.IsEmpty())
    {
        auto Header = MakeObject();
        Header->SetStringField(TEXT("name"), TEXT("X-MCP-Capability-Token"));
        Header->SetStringField(TEXT("value"), CapabilityToken);
        Headers.Add(MakeShared<FJsonValueObject>(Header));
    }
    Server->SetArrayField(TEXT("headers"), Headers);

    McpServers.Add(MakeShared<FJsonValueObject>(Server));
}

int32 FOpenCodeAcpClient::SendRequest(const FString& Method, const TSharedPtr<FJsonObject>& Params)
{
    const int32 Id = NextRequestId++;

    auto Root = MakeObject();
    Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    Root->SetNumberField(TEXT("id"), Id);
    Root->SetStringField(TEXT("method"), Method);
    Root->SetObjectField(TEXT("params"), Params.IsValid() ? Params : MakeObject());
    return SendJsonObject(Root) ? Id : INDEX_NONE;
}

bool FOpenCodeAcpClient::SendNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params)
{
    auto Root = MakeObject();
    Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    Root->SetStringField(TEXT("method"), Method);
    Root->SetObjectField(TEXT("params"), Params.IsValid() ? Params : MakeObject());
    return SendJsonObject(Root);
}

bool FOpenCodeAcpClient::SendResponse(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result)
{
    auto Root = MakeObject();
    Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
    Root->SetObjectField(TEXT("result"), Result.IsValid() ? Result : MakeObject());
    return SendJsonObject(Root);
}

bool FOpenCodeAcpClient::SendError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message)
{
    auto Error = MakeObject();
    Error->SetNumberField(TEXT("code"), Code);
    Error->SetStringField(TEXT("message"), Message);

    auto Root = MakeObject();
    Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
    Root->SetObjectField(TEXT("error"), Error);
    return SendJsonObject(Root);
}

bool FOpenCodeAcpClient::SendJsonObject(const TSharedPtr<FJsonObject>& Root)
{
    if (!ProcessHandle.IsValid() || !FPlatformProcess::IsProcRunning(ProcessHandle) || InputWritePipe == nullptr || !Root.IsValid())
    {
        return false;
    }

    const FString Json = JsonToString(Root);
    if (Json.IsEmpty())
    {
        return false;
    }

    FTCHARToUTF8 Converted(*Json);
    TArray<uint8> Payload;
    Payload.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    Payload.Add(static_cast<uint8>('\n'));

    int32 BytesWritten = 0;
    return FPlatformProcess::WritePipe(InputWritePipe, Payload.GetData(), Payload.Num(), &BytesWritten)
        && BytesWritten == Payload.Num();
}
