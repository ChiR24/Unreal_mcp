#include "IMcpDebugProbeProvider.h"

#include "Common/TcpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Features/IModularFeatures.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SocketSubsystem.h"

namespace
{
constexpr double CaptureIntervalSeconds = 0.1;
constexpr int32 MaxSnapshotBytes = 1024 * 1024;
}

class FMcpDebugRuntimeModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
#if MCP_DEBUG_RUNTIME_ENABLED
        if (!FParse::Value(FCommandLine::Get(), TEXT("McpDebugPort="), Port) ||
            !FParse::Value(FCommandLine::Get(), TEXT("McpDebugToken="), Token) ||
            !FParse::Value(FCommandLine::Get(), TEXT("McpDebugSession="), SessionId) ||
            Port <= 0 || Token.IsEmpty() || SessionId.IsEmpty())
        {
            return;
        }
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FMcpDebugRuntimeModule::Tick));
#endif
    }

    virtual void ShutdownModule() override
    {
#if MCP_DEBUG_RUNTIME_ENABLED
        if (TickHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
            TickHandle.Reset();
        }
        CloseSocket();
#endif
    }

private:
#if MCP_DEBUG_RUNTIME_ENABLED
    bool Tick(float)
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - LastCaptureSeconds < CaptureIntervalSeconds)
        {
            return true;
        }
        LastCaptureSeconds = Now;
        if (!EnsureConnected())
        {
            return true;
        }

        TArray<IMcpDebugProbeProvider*> Providers =
            IModularFeatures::Get().GetModularFeatureImplementations<IMcpDebugProbeProvider>(
                IMcpDebugProbeProvider::GetModularFeatureName());
        for (const IMcpDebugProbeProvider* Provider : Providers)
        {
            if (Provider)
            {
                SendSnapshot(*Provider, Now);
            }
        }
        return true;
    }

    bool EnsureConnected()
    {
        if (Socket)
        {
            return true;
        }
        Socket = FTcpSocketBuilder(TEXT("McpDebugRuntime"))
            .AsReusable()
            .WithSendBufferSize(MaxSnapshotBytes + 16384);
        if (!Socket)
        {
            return false;
        }
        TSharedRef<FInternetAddr> Address =
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
        bool bValidAddress = false;
        Address->SetIp(TEXT("127.0.0.1"), bValidAddress);
        Address->SetPort(Port);
        if (!bValidAddress || !Socket->Connect(*Address))
        {
            CloseSocket();
            return false;
        }
        TSharedRef<FJsonObject> Hello = MakeShared<FJsonObject>();
        Hello->SetStringField(TEXT("type"), TEXT("probe_hello"));
        Hello->SetStringField(TEXT("token"), Token);
        Hello->SetStringField(TEXT("sessionId"), SessionId);
        return SendJson(Hello);
    }

    void SendSnapshot(const IMcpDebugProbeProvider& Provider, double Now)
    {
        TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
        if (!Provider.CaptureSnapshot(Snapshot))
        {
            return;
        }
        TSharedRef<FJsonObject> Envelope = MakeShared<FJsonObject>();
        Envelope->SetStringField(TEXT("type"), TEXT("probe_snapshot"));
        Envelope->SetStringField(TEXT("provider"), Provider.GetProbeName().ToString());
        Envelope->SetNumberField(TEXT("schemaVersion"), Provider.GetSchemaVersion());
        Envelope->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
        Envelope->SetNumberField(
            TEXT("simulationTime"), Provider.GetSimulationTimeSeconds());
        Envelope->SetNumberField(TEXT("monotonicTimestamp"), Now);
        Envelope->SetObjectField(TEXT("snapshot"), Snapshot);
        SendJson(Envelope);
    }

    bool SendJson(const TSharedRef<FJsonObject>& Object)
    {
        FString Text;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
        if (!FJsonSerializer::Serialize(Object, Writer))
        {
            return false;
        }
        Text.AppendChar(TEXT('\n'));
        FTCHARToUTF8 Encoded(*Text);
        if (Encoded.Length() > MaxSnapshotBytes + 16384)
        {
            return false;
        }
        int32 TotalSent = 0;
        while (TotalSent < Encoded.Length())
        {
            int32 Sent = 0;
            if (!Socket || !Socket->Send(
                    reinterpret_cast<const uint8*>(Encoded.Get()) + TotalSent,
                    Encoded.Length() - TotalSent,
                    Sent) || Sent <= 0)
            {
                CloseSocket();
                return false;
            }
            TotalSent += Sent;
        }
        return true;
    }

    void CloseSocket()
    {
        if (Socket)
        {
            Socket->Close();
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
            Socket = nullptr;
        }
    }

    FSocket* Socket = nullptr;
    int32 Port = 0;
    FString Token;
    FString SessionId;
    double LastCaptureSeconds = 0.0;
    FTSTicker::FDelegateHandle TickHandle;
#endif
};

IMPLEMENT_MODULE(FMcpDebugRuntimeModule, McpDebugRuntime)
