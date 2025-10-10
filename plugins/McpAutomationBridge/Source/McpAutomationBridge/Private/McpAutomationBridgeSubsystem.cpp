#include "McpAutomationBridgeSubsystem.h"

#include "McpBridgeWebSocket.h"
#include "IPythonScriptPlugin.h"
#include "McpAutomationBridgeSettings.h"
#include "HAL/PlatformTime.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Misc/PackageName.h"
#include "Misc/CoreMisc.h"
#include "Async/Async.h"
#include "ScopedTransaction.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/SceneComponent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/SoftObjectPath.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"

DEFINE_LOG_CATEGORY(LogMcpAutomationBridgeSubsystem);

namespace
{
class FMcpPythonOutputCapture final : public FOutputDevice
{
public:
    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        if (!V)
        {
            return;
        }

        static const FName LogPythonName(TEXT("LogPython"));
        static const FName PythonName(TEXT("Python"));

        if (Category == LogPythonName || Category == PythonName || Category == NAME_None)
        {
            CapturedLines.Add(FString(V));
        }
    }

    TArray<FString> Consume()
    {
        return MoveTemp(CapturedLines);
    }

private:
    TArray<FString> CapturedLines;
};

bool ApplyJsonValueToProperty(UObject* Target, FProperty* Property, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
{
    if (!Target || !Property)
    {
        OutError = TEXT("Invalid target or property.");
        return false;
    }

    if (!JsonValue.IsValid())
    {
        OutError = TEXT("Property update requires a JSON value.");
        return false;
    }

    void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(Target);
    if (!PropertyAddress)
    {
        OutError = TEXT("Unable to resolve property storage.");
        return false;
    }

    const int64 CheckFlags = 0;
    const int64 SkipFlags = 0;
    if (!FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, PropertyAddress, CheckFlags, SkipFlags))
    {
        OutError = FString::Printf(TEXT("Failed to convert JSON into property '%s'."), *Property->GetName());
        return false;
    }

    return true;
}

TSharedPtr<FJsonValue> ExportPropertyToJsonValue(UObject* Target, FProperty* Property)
{
    if (!Target || !Property)
    {
        return nullptr;
    }

    const void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(Target);
    if (!PropertyAddress)
    {
        return nullptr;
    }

    return FJsonObjectConverter::UPropertyToJsonValue(Property, PropertyAddress);
}

bool ReadVectorField(const TSharedPtr<FJsonObject>& Source, const FString& FieldName, FVector& OutVector, const FVector& DefaultValue)
{
    if (!Source.IsValid())
    {
        OutVector = DefaultValue;
        return false;
    }

    if (!Source->HasField(FieldName))
    {
        OutVector = DefaultValue;
        return false;
    }

    const TSharedPtr<FJsonValue> FieldValue = Source->TryGetField(FieldName);
    if (!FieldValue.IsValid())
    {
        OutVector = DefaultValue;
        return false;
    }

    if (FieldValue->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Elements = FieldValue->AsArray();
        if (Elements.Num() == 3)
        {
            OutVector.X = static_cast<float>(Elements[0]->AsNumber());
            OutVector.Y = static_cast<float>(Elements[1]->AsNumber());
            OutVector.Z = static_cast<float>(Elements[2]->AsNumber());
            return true;
        }
    }

    if (FieldValue->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> ObjectValue = FieldValue->AsObject();
        if (ObjectValue.IsValid())
        {
            auto GetComponent = [&ObjectValue](const TCHAR* Name, float DefaultComponent) -> float
            {
                if (ObjectValue->HasField(Name))
                {
                    return static_cast<float>(ObjectValue->GetNumberField(Name));
                }
                return DefaultComponent;
            };

            OutVector.X = GetComponent(TEXT("x"), DefaultValue.X);
            OutVector.Y = GetComponent(TEXT("y"), DefaultValue.Y);
            OutVector.Z = GetComponent(TEXT("z"), DefaultValue.Z);
            return true;
        }
    }

    OutVector = DefaultValue;
    return false;
}

bool ReadRotatorField(const TSharedPtr<FJsonObject>& Source, const FString& FieldName, FRotator& OutRotator, const FRotator& DefaultValue)
{
    FVector AsVector;
    const bool bHadValue = ReadVectorField(Source, FieldName, AsVector, FVector(DefaultValue.Pitch, DefaultValue.Yaw, DefaultValue.Roll));
    if (bHadValue)
    {
        OutRotator = FRotator(AsVector.X, AsVector.Y, AsVector.Z);
        return true;
    }

    OutRotator = DefaultValue;
    return false;
}

void GatherScsNodesRecursive(USCS_Node* Node, TArray<USCS_Node*>& OutNodes)
{
    if (!Node)
    {
        return;
    }

    OutNodes.Add(Node);
    const TArray<USCS_Node*>& Children = Node->GetChildNodes();
    for (USCS_Node* Child : Children)
    {
        GatherScsNodesRecursive(Child, OutNodes);
    }
}

USCS_Node* FindScsNodeByName(USimpleConstructionScript* SCS, const FString& ComponentName)
{
    if (!SCS)
    {
        return nullptr;
    }

    TArray<USCS_Node*> AllNodes;
    AllNodes.Reserve(32);
    for (USCS_Node* Root : SCS->GetRootNodes())
    {
        GatherScsNodesRecursive(Root, AllNodes);
    }

    const FString Normalized = ComponentName.TrimStartAndEnd();
    const FName NameLookup(*Normalized);

    for (USCS_Node* Node : AllNodes)
    {
        if (!Node)
        {
            continue;
        }

        const FName VariableName = Node->GetVariableName();
        if (VariableName == NameLookup)
        {
            return Node;
        }

        const FString VariableString = VariableName.ToString();
        if (!VariableString.IsEmpty() && VariableString.Equals(Normalized, ESearchCase::IgnoreCase))
        {
            return Node;
        }

        if (Node->GetName().Equals(Normalized, ESearchCase::IgnoreCase))
        {
            return Node;
        }
    }

    return nullptr;
}

bool ApplyPropertyOverrides(UObject* Target, const TSharedPtr<FJsonObject>& Properties, TArray<FString>& OutWarnings, FString& OutError)
{
    if (!Target || !Properties.IsValid())
    {
        return true;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Properties->Values)
    {
        FProperty* Property = Target->GetClass()->FindPropertyByName(*Pair.Key);
        if (!Property)
        {
            OutWarnings.Add(FString::Printf(TEXT("Property %s not found on %s"), *Pair.Key, *Target->GetName()));
            continue;
        }

        FString ConversionError;
        if (!ApplyJsonValueToProperty(Target, Property, Pair.Value, ConversionError))
        {
            OutError = ConversionError;
            return false;
        }
    }

    return true;
}

UBlueprint* LoadBlueprintAsset(const FString& InputPath, FString& OutNormalizedPath, FString& OutError)
{
    FString RequestedPath = InputPath;
    RequestedPath.TrimStartAndEndInline();

    if (RequestedPath.IsEmpty())
    {
        OutError = TEXT("Blueprint path is empty.");
        return nullptr;
    }

    FString NormalizedPath = RequestedPath;
    if (!NormalizedPath.StartsWith(TEXT("/")))
    {
        NormalizedPath = FString::Printf(TEXT("/Game/%s"), *NormalizedPath);
    }

    FString ObjectPath = NormalizedPath;
    if (!ObjectPath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetLongPackageAssetName(NormalizedPath);
        if (AssetName.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Unable to determine asset name for %s"), *NormalizedPath);
            return nullptr;
        }
        ObjectPath = FString::Printf(TEXT("%s.%s"), *NormalizedPath, *AssetName);
    }

    FSoftObjectPath SoftPath(ObjectPath);
    if (!SoftPath.IsValid())
    {
        OutError = FString::Printf(TEXT("Invalid Blueprint object path: %s"), *ObjectPath);
        return nullptr;
    }

    UObject* Loaded = SoftPath.TryLoad();
    if (!Loaded)
    {
        OutError = FString::Printf(TEXT("Failed to load Blueprint asset %s"), *ObjectPath);
        return nullptr;
    }

    UBlueprint* Blueprint = Cast<UBlueprint>(Loaded);
    if (!Blueprint)
    {
        OutError = FString::Printf(TEXT("Asset %s is not a Blueprint."), *ObjectPath);
        return nullptr;
    }

    OutNormalizedPath = SoftPath.ToString();
    return Blueprint;
}
}

void UMcpAutomationBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
    // Apply logging preferences from Project Settings if configured
    if (Settings)
    {
        auto MapVerbosity = [](EMcpLogVerbosity In) -> ELogVerbosity::Type
        {
            switch (In)
            {
            case EMcpLogVerbosity::NoLogging: return ELogVerbosity::NoLogging;
            case EMcpLogVerbosity::Fatal: return ELogVerbosity::Fatal;
            case EMcpLogVerbosity::Error: return ELogVerbosity::Error;
            case EMcpLogVerbosity::Warning: return ELogVerbosity::Warning;
            case EMcpLogVerbosity::Display: return ELogVerbosity::Display;
            case EMcpLogVerbosity::Log: return ELogVerbosity::Log;
            case EMcpLogVerbosity::Verbose: return ELogVerbosity::Verbose;
            case EMcpLogVerbosity::VeryVerbose: return ELogVerbosity::VeryVerbose;
            default: return ELogVerbosity::Log;
            }
        };

        // Informational log about selected verbosity
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Configured log verbosity (Project Settings): %d"), static_cast<int32>(Settings->LogVerbosity));

        if (Settings->bApplyLogVerbosityToAll)
        {
            const ELogVerbosity::Type Mapped = MapVerbosity(Settings->LogVerbosity);
            // Apply to the plugin's primary log category
            LogMcpAutomationBridgeSubsystem.SetVerbosity(Mapped);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Applied selected log verbosity to McpAutomationBridge subsystem."));
        }
    }
    EndpointUrl = Settings->EndpointUrl.IsEmpty() ? EndpointUrl : Settings->EndpointUrl;
    CapabilityToken = Settings->CapabilityToken.IsEmpty() ? CapabilityToken : Settings->CapabilityToken;
    AutoReconnectDelaySeconds = FMath::Max(Settings->AutoReconnectDelay, 0.0f);
    bReconnectEnabled = AutoReconnectDelaySeconds > 0.0f;
    // Heartbeat tuning
    if (Settings->HeartbeatTimeoutSeconds > 0.0f)
    {
        HeartbeatTimeoutSeconds = Settings->HeartbeatTimeoutSeconds;
    }
    // ClientPort is optional; if unset, fall back to environment or safe default
    if (Settings->ClientPort > 0)
    {
        ClientPort = Settings->ClientPort;
    }
    else
    {
        const FString EnvClient = FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_AUTOMATION_CLIENT_PORT"));
        const int32 Parsed = EnvClient.IsEmpty() ? 0 : FCString::Atoi(*EnvClient);
        ClientPort = Parsed > 0 ? Parsed : 0;
    }
    bRequireCapabilityToken = Settings->bRequireCapabilityToken;
    TimeUntilReconnect = 0.0f;
    ResetHeartbeatTracking();
    ServerName.Reset();
    ServerVersion.Reset();
    // Allow environment override for listen ports (e.g., MCP_AUTOMATION_WS_PORTS="8090,8091")
    {
        // Respect settings when present; otherwise check environment variables.
        if (!Settings->ListenPorts.IsEmpty())
        {
            EnvListenPorts = Settings->ListenPorts;
            bEnvListenPortsSet = true;
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("ListenPorts set via Project Settings: %s"), *EnvListenPorts);
        }
        else
        {
            const FString EnvPorts = FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_AUTOMATION_WS_PORTS"));
            if (!EnvPorts.IsEmpty())
            {
                EnvListenPorts = EnvPorts;
                bEnvListenPortsSet = true;
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("MCP_AUTOMATION_WS_PORTS override detected: %s"), *EnvListenPorts);
            }
        }

        if (!Settings->ListenHost.IsEmpty())
        {
            EnvListenHost = Settings->ListenHost;
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("ListenHost set via Project Settings: %s"), *EnvListenHost);
        }
        else
        {
            const FString EnvHost = FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_AUTOMATION_LISTEN_HOST"));
            if (!EnvHost.IsEmpty())
            {
                EnvListenHost = EnvHost;
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("MCP_AUTOMATION_LISTEN_HOST override detected: %s"), *EnvListenHost);
            }
        }
    }

    // Prefer always-listen behavior so the plugin is always open like Remote Control API
    StartBridge();
}

void UMcpAutomationBridgeSubsystem::Deinitialize()
{
    StopBridge();
    Super::Deinitialize();
}

bool UMcpAutomationBridgeSubsystem::SendRawMessage(const FString& Message)
{
    // Send to all connected sockets
    bool bSentToAny = false;
    for (const TSharedPtr<FMcpBridgeWebSocket>& Socket : ActiveSockets)
    {
        if (Socket.IsValid() && Socket->IsConnected())
        {
            Socket->Send(Message);
            bSentToAny = true;
        }
    }
    
    if (!bSentToAny)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Bridge sockets not connected; message dropped."));
    }
    return bSentToAny;
}

bool UMcpAutomationBridgeSubsystem::Tick(const float DeltaTime)
{
    if (!bBridgeAvailable)
    {
        return true;
    }

    if (BridgeState == EMcpAutomationBridgeState::Disconnected && bReconnectEnabled)
    {
        TimeUntilReconnect -= DeltaTime;
        if (TimeUntilReconnect <= 0.0f)
        {
            TimeUntilReconnect = AutoReconnectDelaySeconds;
            AttemptConnection();
        }
    }

    if (!ActiveSockets.Num() && BridgeState == EMcpAutomationBridgeState::Connecting)
    {
        BridgeState = EMcpAutomationBridgeState::Disconnected;
    }

    if (BridgeState == EMcpAutomationBridgeState::Connected && bHeartbeatTrackingEnabled)
    {
        const double NowSeconds = FPlatformTime::Seconds();
        if (HeartbeatTimeoutSeconds > 0.0f && LastHeartbeatTimestamp > 0.0 && (NowSeconds - LastHeartbeatTimestamp) > static_cast<double>(HeartbeatTimeoutSeconds))
        {
            const float ElapsedSeconds = static_cast<float>(NowSeconds - LastHeartbeatTimestamp);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation bridge heartbeat timed out after %.1f seconds; forcing reconnect."), ElapsedSeconds);
            ForceReconnect(TEXT("Heartbeat timeout."), 0.1f);
        }
        else if (HeartbeatTimeoutSeconds > 0.0f && (NowSeconds - LastHeartbeatTimestamp) > static_cast<double>(HeartbeatTimeoutSeconds / 3.0f))
        {
            // Send heartbeat ping to all connected clients
            for (const TSharedPtr<FMcpBridgeWebSocket>& Socket : ActiveSockets)
            {
                if (Socket.IsValid() && Socket->IsConnected())
                {
                    Socket->SendHeartbeatPing();
                }
            }
        }
    }

    return true;
}

void UMcpAutomationBridgeSubsystem::AttemptConnection()
{
    if (!bBridgeAvailable)
    {
        return;
    }

    ResetHeartbeatTracking();

    // Parse host/port from EndpointUrl or use defaults
    int32 ListenPort = 8091;
    FString ListenHost = TEXT("127.0.0.1");
    if (!EndpointUrl.IsEmpty())
    {
        const FString TrimmedUrl = EndpointUrl.TrimStartAndEnd();
        FString HostPortString = TrimmedUrl;

        // Strip scheme if present (ws://, wss://, etc.)
        const int32 SchemeSeparatorIndex = TrimmedUrl.Find(TEXT("://"));
        if (SchemeSeparatorIndex != INDEX_NONE)
        {
            HostPortString = TrimmedUrl.Mid(SchemeSeparatorIndex + 3);
        }

        // Remove path/query portion after first '/'
        int32 PathSeparatorIndex = INDEX_NONE;
        if (HostPortString.FindChar('/', PathSeparatorIndex))
        {
            HostPortString = HostPortString.Left(PathSeparatorIndex);
        }

        HostPortString = HostPortString.TrimStartAndEnd();

        FString ParsedHost = HostPortString;
        FString ParsedPort;

        if (!HostPortString.IsEmpty())
        {
            if (HostPortString.StartsWith(TEXT("[")))
            {
                // IPv6 literal: [::1]:port
                int32 ClosingBracketIndex = INDEX_NONE;
                if (HostPortString.FindChar(']', ClosingBracketIndex))
                {
                    ParsedHost = HostPortString.Mid(1, ClosingBracketIndex - 1);
                    if (ClosingBracketIndex + 1 < HostPortString.Len() && HostPortString[ClosingBracketIndex + 1] == ':')
                    {
                        ParsedPort = HostPortString.Mid(ClosingBracketIndex + 2);
                    }
                }
            }
            else
            {
                HostPortString.Split(TEXT(":"), &ParsedHost, &ParsedPort, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
            }

            ParsedHost = ParsedHost.TrimStartAndEnd();
            if (!ParsedHost.IsEmpty())
            {
                ListenHost = ParsedHost;
            }

            ParsedPort = ParsedPort.TrimStartAndEnd();
            if (!ParsedPort.IsEmpty())
            {
                const int32 CandidatePort = FCString::Atoi(*ParsedPort);
                if (CandidatePort > 0)
                {
                    ListenPort = CandidatePort;
                }
            }
        }
    }

    // Decide whether to operate in listen (server) mode or client mode
    const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
    bool bShouldAlwaysListen = Settings ? Settings->bAlwaysListen : true;

    // If configured to connect to an endpoint (client mode) and the project is NOT set to always listen,
    // create an outgoing WebSocket and attempt to connect. Otherwise create server listening sockets.
    if (!EndpointUrl.IsEmpty() && !bShouldAlwaysListen)
    {
        // Build headers (include capability token if present)
        TMap<FString, FString> Headers;
        if (!CapabilityToken.IsEmpty())
        {
            Headers.Add(TEXT("X-MCP-Capability"), CapabilityToken);
        }

        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Attempting MCP automation bridge client connection to %s"), *EndpointUrl);

        auto ClientSocket = MakeShared<FMcpBridgeWebSocket>(EndpointUrl, TEXT("mcp-automation"), Headers);
        ClientSocket->InitializeWeakSelf(ClientSocket);
        ClientSocket->OnConnected().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleConnected);
        // OnConnectionError expects a single FString parameter. Use a lambda that captures a weak
        // pointer to the client socket and forwards the call to our handler which needs the socket
        // pointer as the first parameter. Capturing a weak pointer avoids creating a circular
        // shared_ptr reference between the socket and the delegate.
        {
            TWeakPtr<FMcpBridgeWebSocket> WeakClient = ClientSocket;
            ClientSocket->OnConnectionError().AddLambda([WeakClient, this](const FString& Error)
            {
                if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakClient.Pin())
                {
                    this->HandleConnectionError(Pinned, Error);
                }
            });
        }
        ClientSocket->OnClosed().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleClosed);
        ClientSocket->OnMessage().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleMessage);
        ClientSocket->OnHeartbeat().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleHeartbeat);

        ActiveSockets.Add(ClientSocket);
        ClientSocket->Connect();
        BridgeState = EMcpAutomationBridgeState::Connecting;
        return;
    }

    // Build list of ports to listen on (respect user-configured ListenPorts env or settings)
    TArray<int32> PortsToListen;
    if (Settings && !Settings->ListenPorts.IsEmpty())
    {
        TArray<FString> Parts;
        Settings->ListenPorts.ParseIntoArray(Parts, TEXT(","), true);
        for (const FString& Part : Parts)
        {
            const int32 Candidate = FCString::Atoi(*Part);
            if (Candidate > 0) PortsToListen.Add(Candidate);
        }
    }
    
    if (PortsToListen.Num() == 0)
    {
        PortsToListen.Add(ListenPort);
    }

    // Use configured listen host if provided; otherwise bind to 0.0.0.0 to accept remote connections
    FString BindHost = ListenHost;
    if (Settings && !Settings->ListenHost.IsEmpty())
    {
        BindHost = Settings->ListenHost;
    }
    if (BindHost.IsEmpty())
    {
        BindHost = TEXT("0.0.0.0");
    }

    // Determine listen backlog & accept sleep from settings or environment
    const int32 ConfigBacklog = Settings->ListenBacklog > 0 ? Settings->ListenBacklog : (bEnvListenPortsSet ? 10 : 10);
    const float ConfigAcceptSleep = Settings->AcceptSleepSeconds > 0.0f ? Settings->AcceptSleepSeconds : 0.01f;

    for (int32 Port : PortsToListen)
    {
        auto ServerSocket = MakeShared<FMcpBridgeWebSocket>(Port, BindHost, ConfigBacklog, ConfigAcceptSleep);
        ServerSocket->InitializeWeakSelf(ServerSocket);
        ServerSocket->OnClientConnected().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleClientConnected);
        ServerSocket->OnConnectionError().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleServerConnectionError);
        ActiveSockets.Add(ServerSocket);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Starting MCP automation server listening on %s:%d"), *BindHost, Port);
        ServerSocket->Listen();
    }
    BridgeState = EMcpAutomationBridgeState::Connecting;
}

void UMcpAutomationBridgeSubsystem::HandleConnected(TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    BridgeState = EMcpAutomationBridgeState::Connected;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("MCP automation bridge connected."));

    // Add the socket to active sockets if it's not already there
    if (!ActiveSockets.Contains(Socket))
    {
        ActiveSockets.Add(Socket);
        
        // Set up event handlers for this socket
        Socket->OnClosed().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleClosed);
        Socket->OnMessage().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleMessage);
        Socket->OnHeartbeat().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleHeartbeat);
    }

    ActiveSessionId.Reset();
    RecordHeartbeat();
    bHeartbeatTrackingEnabled = false;

    FString HelloPayload;
    {
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&HelloPayload);
        Writer->WriteObjectStart();
        Writer->WriteValue(TEXT("type"), TEXT("bridge_hello"));
        if (!CapabilityToken.IsEmpty())
        {
            Writer->WriteValue(TEXT("capabilityToken"), CapabilityToken);
        }
        Writer->WriteObjectEnd();
        Writer->Close();
    }

    if (Socket.IsValid())
    {
        Socket->Send(HelloPayload);
    }

    FMcpAutomationMessage Handshake;
    Handshake.Type = TEXT("bridge_started");
    Handshake.PayloadJson = TEXT("{}");
    OnMessageReceived.Broadcast(Handshake);
}

void UMcpAutomationBridgeSubsystem::HandleClientConnected(TSharedPtr<FMcpBridgeWebSocket> ClientSocket)
{
    BridgeState = EMcpAutomationBridgeState::Connected;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("MCP automation client connected."));

    // Add the client socket to active sockets if it's not already there
    if (!ActiveSockets.Contains(ClientSocket))
    {
        ActiveSockets.Add(ClientSocket);
        
        // Set up event handlers for this client socket
        ClientSocket->OnClosed().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleClosed);
        ClientSocket->OnMessage().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleMessage);
        ClientSocket->OnHeartbeat().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleHeartbeat);
    }

    ActiveSessionId.Reset();
    RecordHeartbeat();
    bHeartbeatTrackingEnabled = false;

    // Send server hello to the connected client
    FString HelloPayload;
    {
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&HelloPayload);
        Writer->WriteObjectStart();
        Writer->WriteValue(TEXT("type"), TEXT("bridge_ack"));
        Writer->WriteValue(TEXT("serverVersion"), TEXT("1.0.0"));
        Writer->WriteValue(TEXT("serverName"), TEXT("Unreal Engine MCP Automation Bridge"));
        Writer->WriteValue(TEXT("sessionId"), FGuid::NewGuid().ToString());
    const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
    const int32 HeartbeatMs = (Settings && Settings->HeartbeatIntervalMs > 0) ? Settings->HeartbeatIntervalMs : 30000;
    Writer->WriteValue(TEXT("heartbeatIntervalMs"), HeartbeatMs);
        Writer->WriteObjectEnd();
        Writer->Close();
    }

    if (ClientSocket.IsValid())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Sending bridge_ack to automation client."));
        ClientSocket->Send(HelloPayload);
    }

    FMcpAutomationMessage Handshake;
    Handshake.Type = TEXT("bridge_started");
    Handshake.PayloadJson = TEXT("{}");
    OnMessageReceived.Broadcast(Handshake);
}

void UMcpAutomationBridgeSubsystem::HandleHeartbeat(TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    RecordHeartbeat();
}

void UMcpAutomationBridgeSubsystem::HandleConnectionError(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Error)
{
    if (AutoReconnectDelaySeconds > 0.0f)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation bridge connection error: %s (retrying in %.1f seconds)"), *Error, AutoReconnectDelaySeconds);
    }
    else
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation bridge connection error: %s"), *Error);
    }
    BridgeState = EMcpAutomationBridgeState::Disconnected;
    TimeUntilReconnect = AutoReconnectDelaySeconds;
    ResetHeartbeatTracking();
    
    // Remove the failed socket
    ActiveSockets.Remove(Socket);
    
    if (Socket.IsValid())
    {
        Socket->OnConnected().RemoveAll(this);
        Socket->OnConnectionError().RemoveAll(this);
        Socket->OnClosed().RemoveAll(this);
        Socket->OnMessage().RemoveAll(this);
        Socket->OnHeartbeat().RemoveAll(this);
    }
}

void UMcpAutomationBridgeSubsystem::HandleServerConnectionError(const FString& Error)
{
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Automation bridge server connection error: %s"), *Error);
    BridgeState = EMcpAutomationBridgeState::Disconnected;
    TimeUntilReconnect = AutoReconnectDelaySeconds;
    ResetHeartbeatTracking();
    
    // For server errors, clean up any listening server sockets (support multiple listening ports)
    if (ActiveSockets.Num() > 0)
    {
        TArray<TSharedPtr<FMcpBridgeWebSocket>> ToRemove;
        for (const TSharedPtr<FMcpBridgeWebSocket>& Socket : ActiveSockets)
        {
            if (!Socket.IsValid()) continue;

            // If this socket is a listening/server socket, tear it down
            if (Socket->IsListening())
            {
                Socket->OnClientConnected().RemoveAll(this);
                Socket->OnConnectionError().RemoveAll(this);
                Socket->OnClosed().RemoveAll(this);
                Socket->OnMessage().RemoveAll(this);
                Socket->OnHeartbeat().RemoveAll(this);
                Socket->Close();
                ToRemove.Add(Socket);
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Closed listening server socket on %s:%d due to server error."), *Socket->GetListenHost(), Socket->GetPort());
            }
        }

        // Remove closed server sockets from active list
        for (const TSharedPtr<FMcpBridgeWebSocket>& R : ToRemove)
        {
            ActiveSockets.Remove(R);
        }

        // If we removed everything or only clients remain, but server state is invalid, clear pending requests to prevent leaks
        if (ActiveSockets.Num() == 0)
        {
            PendingRequestsToSockets.Empty();
        }
    }
}

void UMcpAutomationBridgeSubsystem::HandleClosed(TSharedPtr<FMcpBridgeWebSocket> Socket, const int32 StatusCode, const FString& Reason, const bool bWasClean)
{
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation bridge closed (code %d, clean=%s): %s"), StatusCode, bWasClean ? TEXT("true") : TEXT("false"), *Reason);
    BridgeState = EMcpAutomationBridgeState::Disconnected;
    TimeUntilReconnect = AutoReconnectDelaySeconds;
    ResetHeartbeatTracking();
    
    // Remove the closed socket
    ActiveSockets.Remove(Socket);
    
    if (Socket.IsValid())
    {
        Socket->OnConnected().RemoveAll(this);
        Socket->OnConnectionError().RemoveAll(this);
        Socket->OnClosed().RemoveAll(this);
        Socket->OnMessage().RemoveAll(this);
        Socket->OnHeartbeat().RemoveAll(this);
    }
}

void UMcpAutomationBridgeSubsystem::HandleMessage(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Message)
{
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Automation bridge inbound: %s"), *Message);

    FMcpAutomationMessage Parsed;
    Parsed.Type = TEXT("raw");
    Parsed.PayloadJson = Message;

    TSharedPtr<FJsonObject> JsonObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FString ParsedType;
        if (JsonObject->TryGetStringField(TEXT("type"), ParsedType))
        {
            Parsed.Type = ParsedType;
        }

        if (Parsed.Type.Equals(TEXT("automation_request")))
        {
            FString RequestId;
            if (!JsonObject->TryGetStringField(TEXT("requestId"), RequestId) || RequestId.IsEmpty())
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation request missing requestId."));
            }
            else
            {
                FString Action;
                if (!JsonObject->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty())
                {
                    SendAutomationError(Socket, RequestId, TEXT("Automation request missing action."), TEXT("INVALID_ACTION"));
                }
                else
                {
                    TSharedPtr<FJsonObject> Payload;
                    if (JsonObject->HasTypedField<EJson::Object>(TEXT("payload")))
                    {
                        Payload = JsonObject->GetObjectField(TEXT("payload"));
                    }
                    // Track which socket made this request
                    PendingRequestsToSockets.Add(RequestId, Socket);
                    ProcessAutomationRequest(RequestId, Action, Payload, Socket);
                }
            }
            return;
        }

        if (Parsed.Type.Equals(TEXT("bridge_ack")))
        {
            FString ReportedVersion;
            if (JsonObject->TryGetStringField(TEXT("serverVersion"), ReportedVersion) && !ReportedVersion.IsEmpty())
            {
                ServerVersion = ReportedVersion;
            }

            FString ReportedName;
            if (JsonObject->TryGetStringField(TEXT("serverName"), ReportedName) && !ReportedName.IsEmpty())
            {
                ServerName = ReportedName;
            }

            FString ReportedSessionId;
            if (JsonObject->TryGetStringField(TEXT("sessionId"), ReportedSessionId) && !ReportedSessionId.IsEmpty())
            {
                ActiveSessionId = ReportedSessionId;
            }

            double HeartbeatIntervalMs = 0.0;
            if (JsonObject->TryGetNumberField(TEXT("heartbeatIntervalMs"), HeartbeatIntervalMs) && HeartbeatIntervalMs > 0.0)
            {
                HeartbeatTimeoutSeconds = FMath::Max(5.0f, static_cast<float>((HeartbeatIntervalMs / 1000.0) * 3.0));
                bHeartbeatTrackingEnabled = true;
            }
            else
            {
                HeartbeatTimeoutSeconds = 0.0f;
                bHeartbeatTrackingEnabled = false;
            }

            RecordHeartbeat();

            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Automation bridge handshake acknowledged (server=%s, version=%s, session=%s, heartbeat=%.0f ms)"),
                ServerName.IsEmpty() ? TEXT("unknown") : *ServerName,
                ServerVersion.IsEmpty() ? TEXT("unknown") : *ServerVersion,
                ActiveSessionId.IsEmpty() ? TEXT("n/a") : *ActiveSessionId,
                HeartbeatTimeoutSeconds > 0.0f ? HeartbeatTimeoutSeconds * (1000.0f / 3.0f) : 0.0f);
        }
        else if (Parsed.Type.Equals(TEXT("bridge_error")))
        {
            FString ErrorCode;
            JsonObject->TryGetStringField(TEXT("error"), ErrorCode);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Automation bridge reported error: %s"), ErrorCode.IsEmpty() ? TEXT("UNKNOWN_ERROR") : *ErrorCode);
        }
        else if (Parsed.Type.Equals(TEXT("bridge_heartbeat")))
        {
            RecordHeartbeat();
        }
        else if (Parsed.Type.Equals(TEXT("bridge_ping")))
        {
            RecordHeartbeat();
            TSharedPtr<FJsonObject> Pong = MakeShared<FJsonObject>();
            Pong->SetStringField(TEXT("type"), TEXT("bridge_pong"));
            FString Nonce;
            if (JsonObject->TryGetStringField(TEXT("nonce"), Nonce) && !Nonce.IsEmpty())
            {
                Pong->SetStringField(TEXT("nonce"), Nonce);
            }
            SendControlMessage(Pong);
        }
        else if (Parsed.Type.Equals(TEXT("bridge_pong")))
        {
            RecordHeartbeat();
        }
        else if (Parsed.Type.Equals(TEXT("bridge_shutdown")))
        {
            FString ShutdownReason;
            JsonObject->TryGetStringField(TEXT("reason"), ShutdownReason);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation bridge server requested shutdown: %s"), ShutdownReason.IsEmpty() ? TEXT("unspecified") : *ShutdownReason);
            ForceReconnect(TEXT("Server requested shutdown."));
        }
        else if (Parsed.Type.Equals(TEXT("bridge_goodbye")))
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation bridge server sent goodbye; scheduling reconnect."));
            ForceReconnect(TEXT("Server sent goodbye."));
        }
    }

    OnMessageReceived.Broadcast(Parsed);
}

void UMcpAutomationBridgeSubsystem::ProcessAutomationRequest(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Ensure automation processing happens on the game thread
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<UMcpAutomationBridgeSubsystem>(this), RequestId, Action, Payload, RequestingSocket]()
        {
            if (UMcpAutomationBridgeSubsystem* Pinned = WeakThis.Get())
            {
                Pinned->ProcessAutomationRequest(RequestId, Action, Payload, RequestingSocket);
            }
        });
        return;
    }

    // Guard against reentrant automation request processing to prevent Task Graph recursion crashes
    if (bProcessingAutomationRequest)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Ignoring reentrant automation request %s for action %s"), *RequestId, *Action);
        SendAutomationError(RequestingSocket, RequestId, TEXT("Automation request processing is already in progress. Please wait for the current request to complete."), TEXT("REENTRANT_REQUEST"));
        return;
    }

    bProcessingAutomationRequest = true;

    // Ensure the flag is reset even if an exception occurs
    ON_SCOPE_EXIT
    {
        bProcessingAutomationRequest = false;
    };
    if (Action.Equals(TEXT("execute_editor_python"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("execute_editor_python payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        FString Script;
        if (!Payload->TryGetStringField(TEXT("script"), Script) || Script.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("execute_editor_python requires a non-empty script."), TEXT("INVALID_ARGUMENT"));
            return;
        }

        if (!FModuleManager::Get().IsModuleLoaded(TEXT("PythonScriptPlugin")))
        {
            FModuleManager::LoadModulePtr<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
        }

        IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
        if (!PythonPlugin)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("PythonScriptPlugin is not available. Enable the Python Editor Script Plugin."), TEXT("PYTHON_PLUGIN_DISABLED"));
            return;
        }

        FMcpPythonOutputCapture OutputCapture;
        const bool bCaptureLogs = (GLog != nullptr);

        if (bCaptureLogs)
        {
            GLog->AddOutputDevice(&OutputCapture);
        }

        ON_SCOPE_EXIT
        {
            if (bCaptureLogs && GLog)
            {
                GLog->RemoveOutputDevice(&OutputCapture);
            }
        };

        const bool bSuccess = PythonPlugin->ExecPythonCommand(*Script);
        const FString ResultMessage = bSuccess
            ? TEXT("Python script executed via MCP Automation Bridge.")
            : TEXT("Python script executed but returned false.");

        TArray<FString> Captured = bCaptureLogs ? OutputCapture.Consume() : TArray<FString>();
        TSharedPtr<FJsonObject> ResultPayload;

        if (Captured.Num() > 0)
        {
            ResultPayload = MakeShared<FJsonObject>();

            FString CombinedOutput = FString::Join(Captured, TEXT("\n"));
            ResultPayload->SetStringField(TEXT("Output"), CombinedOutput);

            TArray<TSharedPtr<FJsonValue>> LogOutputArray;
            LogOutputArray.Reserve(Captured.Num());
            for (const FString& Line : Captured)
            {
                if (Line.TrimStartAndEnd().IsEmpty())
                {
                    continue;
                }

                TSharedPtr<FJsonObject> LogEntry = MakeShared<FJsonObject>();
                LogEntry->SetStringField(TEXT("Output"), Line);
                LogOutputArray.Add(MakeShared<FJsonValueObject>(LogEntry));
            }

            if (LogOutputArray.Num() > 0)
            {
                ResultPayload->SetArrayField(TEXT("LogOutput"), LogOutputArray);
            }
        }

        SendAutomationResponse(RequestingSocket, RequestId, bSuccess, ResultMessage, ResultPayload, bSuccess ? FString() : TEXT("PYTHON_EXEC_FAILED"));
        return;
    }

    if (Action.Equals(TEXT("set_object_property"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("set_object_property payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        FString ObjectPath;
        if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) || ObjectPath.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("set_object_property requires a non-empty objectPath."), TEXT("INVALID_OBJECT"));
            return;
        }

        FString PropertyName;
        if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) || PropertyName.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("set_object_property requires a non-empty propertyName."), TEXT("INVALID_PROPERTY"));
            return;
        }

        const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
        if (!ValueField.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("set_object_property payload missing value field."), TEXT("INVALID_VALUE"));
            return;
        }

        UObject* TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
        if (!TargetObject)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath), TEXT("OBJECT_NOT_FOUND"));
            return;
        }

        FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
        if (!Property)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Property %s not found on object %s."), *PropertyName, *ObjectPath), TEXT("PROPERTY_NOT_FOUND"));
            return;
        }

        FString ConversionError;

#if WITH_EDITOR
        TargetObject->Modify();
#endif

        if (!ApplyJsonValueToProperty(TargetObject, Property, ValueField, ConversionError))
        {
            SendAutomationError(RequestingSocket, RequestId, ConversionError, TEXT("PROPERTY_CONVERSION_FAILED"));
            return;
        }

        bool bMarkDirty = true;
        if (Payload->HasField(TEXT("markDirty")))
        {
            if (!Payload->TryGetBoolField(TEXT("markDirty"), bMarkDirty))
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("markDirty must be a boolean."), TEXT("INVALID_MARK_DIRTY"));
                return;
            }
        }
        if (bMarkDirty)
        {
            TargetObject->MarkPackageDirty();
        }

#if WITH_EDITOR
        TargetObject->PostEditChange();
#endif

        TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
        ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
        ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);

        if (TSharedPtr<FJsonValue> CurrentValue = ExportPropertyToJsonValue(TargetObject, Property))
        {
            ResultPayload->SetField(TEXT("value"), CurrentValue);
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property value updated."), ResultPayload, FString());
        return;
    }

    if (Action.Equals(TEXT("get_object_property"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("get_object_property payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        FString ObjectPath;
        if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) || ObjectPath.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("get_object_property requires a non-empty objectPath."), TEXT("INVALID_OBJECT"));
            return;
        }

        FString PropertyName;
        if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) || PropertyName.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("get_object_property requires a non-empty propertyName."), TEXT("INVALID_PROPERTY"));
            return;
        }

        UObject* TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
        if (!TargetObject)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath), TEXT("OBJECT_NOT_FOUND"));
            return;
        }

        FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
        if (!Property)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Property %s not found on object %s."), *PropertyName, *ObjectPath), TEXT("PROPERTY_NOT_FOUND"));
            return;
        }

        const TSharedPtr<FJsonValue> CurrentValue = ExportPropertyToJsonValue(TargetObject, Property);
        if (!CurrentValue.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unable to export property %s."), *PropertyName), TEXT("PROPERTY_EXPORT_FAILED"));
            return;
        }

        TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
        ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
        ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
        ResultPayload->SetField(TEXT("value"), CurrentValue);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property value retrieved."), ResultPayload, FString());
        return;
    }

    if (Action.Equals(TEXT("blueprint_modify_scs"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        FString BlueprintPath;
        // Accept either a single blueprintPath string or an array of blueprintCandidates
        TArray<FString> CandidatePaths;
        // We will read the blueprintCandidates array below if needed; presence alone
        // is checked when the blueprintPath string is empty.
        if (!Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) || BlueprintPath.TrimStartAndEnd().IsEmpty())
        {
            // If no single blueprintPath was provided, accept array of candidates
            const TArray<TSharedPtr<FJsonValue>>* CandidateArray = nullptr;
            if (!Payload->TryGetArrayField(TEXT("blueprintCandidates"), CandidateArray) || CandidateArray == nullptr || CandidateArray->Num() == 0)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs requires a non-empty blueprintPath or blueprintCandidates."), TEXT("INVALID_BLUEPRINT"));
                return;
            }
            for (const TSharedPtr<FJsonValue>& Value : *CandidateArray)
            {
                if (!Value.IsValid()) continue;
                FString Candidate = Value->AsString();
                if (!Candidate.TrimStartAndEnd().IsEmpty())
                {
                    CandidatePaths.Add(Candidate);
                }
            }
            if (CandidatePaths.Num() == 0)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs blueprintCandidates array provided but contains no valid strings."), TEXT("INVALID_BLUEPRINT_CANDIDATES"));
                return;
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("operations"), OperationsArray) || OperationsArray == nullptr)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs requires an operations array."), TEXT("INVALID_OPERATIONS"));
            return;
        }

        FString NormalizedBlueprintPath;
        FString LoadError;
    UBlueprint* Blueprint = nullptr;
    TArray<FString> TriedCandidates;
        // Try explicit blueprintPath first
        if (!BlueprintPath.IsEmpty())
        {
            TriedCandidates.Add(BlueprintPath);
            Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedBlueprintPath, LoadError);
            if (Blueprint)
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Loaded blueprint from explicit path: %s -> %s"), *BlueprintPath, *NormalizedBlueprintPath);
            }
        }
        // If that failed and we have candidate paths, try them in order
        if (!Blueprint && CandidatePaths.Num() > 0)
        {
            for (const FString& Candidate : CandidatePaths)
            {
                TriedCandidates.Add(Candidate);
                FString CandidateNormalized;
                FString CandidateError;
                UBlueprint* TryBp = LoadBlueprintAsset(Candidate, CandidateNormalized, CandidateError);
                if (TryBp)
                {
                    Blueprint = TryBp;
                    NormalizedBlueprintPath = CandidateNormalized;
                    LoadError.Empty();
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Loaded blueprint candidate: %s -> %s"), *Candidate, *CandidateNormalized);
                    break;
                }
                // accumulate last error for fallback
                LoadError = CandidateError;
            }
        }
        if (!Blueprint)
        {
            // Provide diagnostics about the candidates we tried
            TSharedPtr<FJsonObject> ErrPayload = MakeShared<FJsonObject>();
            if (TriedCandidates.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> TriedValues;
                for (const FString& C : TriedCandidates)
                {
                    TriedValues.Add(MakeShared<FJsonValueString>(C));
                }
                ErrPayload->SetArrayField(TEXT("triedCandidates"), TriedValues);
            }
            SendAutomationResponse(RequestingSocket, RequestId, false, LoadError, ErrPayload, TEXT("BLUEPRINT_NOT_FOUND"));
            return;
        }

        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            TSharedPtr<FJsonObject> ErrPayload = MakeShared<FJsonObject>();
            if (TriedCandidates.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> TriedValues;
                for (const FString& C : TriedCandidates)
                {
                    TriedValues.Add(MakeShared<FJsonValueString>(C));
                }
                ErrPayload->SetArrayField(TEXT("triedCandidates"), TriedValues);
            }
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint does not expose a SimpleConstructionScript."), ErrPayload, TEXT("SCS_UNAVAILABLE"));
            return;
        }

        bool bCompile = false;
        if (Payload->HasField(TEXT("compile")))
        {
            if (!Payload->TryGetBoolField(TEXT("compile"), bCompile))
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("compile must be a boolean."), TEXT("INVALID_COMPILE_FLAG"));
                return;
            }
        }

        bool bSave = false;
        if (Payload->HasField(TEXT("save")))
        {
            if (!Payload->TryGetBoolField(TEXT("save"), bSave))
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("save must be a boolean."), TEXT("INVALID_SAVE_FLAG"));
                return;
            }
        }

        if (OperationsArray->Num() == 0)
        {
            TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
            ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
            ResultPayload->SetArrayField(TEXT("operations"), TArray<TSharedPtr<FJsonValue>>());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No SCS operations supplied."), ResultPayload, FString());
            return;
        }

        Blueprint->Modify();
        SCS->Modify();

        bool bAnyChanges = false;
        TArray<FString> AccumulatedWarnings;
        TArray<TSharedPtr<FJsonValue>> OperationSummaries;

        for (int32 Index = 0; Index < OperationsArray->Num(); ++Index)
        {
            const TSharedPtr<FJsonValue>& OperationValue = (*OperationsArray)[Index];
            if (!OperationValue.IsValid() || OperationValue->Type != EJson::Object)
            {
                SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Operation at index %d is not an object."), Index), TEXT("INVALID_OPERATION_PAYLOAD"));
                return;
            }

            const TSharedPtr<FJsonObject> OperationObject = OperationValue->AsObject();
            FString OperationType;
            if (!OperationObject->TryGetStringField(TEXT("type"), OperationType) || OperationType.TrimStartAndEnd().IsEmpty())
            {
                SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Operation at index %d missing type."), Index), TEXT("INVALID_OPERATION_TYPE"));
                return;
            }

            const FString NormalizedType = OperationType.ToLower();
            TSharedPtr<FJsonObject> OperationSummary = MakeShared<FJsonObject>();
            OperationSummary->SetNumberField(TEXT("index"), Index);
            OperationSummary->SetStringField(TEXT("type"), NormalizedType);

            if (NormalizedType == TEXT("add_component"))
            {
                FString ComponentName;
                if (!OperationObject->TryGetStringField(TEXT("componentName"), ComponentName) || ComponentName.TrimStartAndEnd().IsEmpty())
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("add_component operation at index %d missing componentName."), Index), TEXT("INVALID_COMPONENT_NAME"));
                    return;
                }

                FString ComponentClassPath;
                if (!OperationObject->TryGetStringField(TEXT("componentClass"), ComponentClassPath) || ComponentClassPath.TrimStartAndEnd().IsEmpty())
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("add_component operation at index %d missing componentClass."), Index), TEXT("INVALID_COMPONENT_CLASS"));
                    return;
                }

                FString AttachToName;
                OperationObject->TryGetStringField(TEXT("attachTo"), AttachToName);

                FSoftClassPath ComponentClassSoftPath(ComponentClassPath);
                UClass* ComponentClass = ComponentClassSoftPath.TryLoadClass<UActorComponent>();
                if (!ComponentClass)
                {
                    ComponentClass = FindObject<UClass>(nullptr, *ComponentClassPath);
                }
                // If still unresolved, try common /Script/ packages and static load as fallbacks
                if (!ComponentClass)
                {
                    const TArray<FString> Prefixes = { TEXT("/Script/Engine."), TEXT("/Script/UMG."), TEXT("/Script/Paper2D.") };
                    for (const FString& Prefix : Prefixes)
                    {
                        const FString Guess = Prefix + ComponentClassPath;
                        UClass* TryClass = FindObject<UClass>(nullptr, *Guess);
                        if (!TryClass)
                        {
                            TryClass = StaticLoadClass(UActorComponent::StaticClass(), nullptr, *Guess);
                        }
                        if (TryClass)
                        {
                            ComponentClass = TryClass;
                            break;
                        }
                    }
                }
                // As last resort, scan loaded classes by short name (fast in editor builds)
                if (!ComponentClass)
                {
                    for (TObjectIterator<UClass> It; It; ++It)
                    {
                        UClass* Candidate = *It;
                        if (!Candidate) continue;
                        const FString ShortName = Candidate->GetName();
                        if (ShortName.Equals(ComponentClassPath, ESearchCase::IgnoreCase) || Candidate->GetFName().ToString().Equals(ComponentClassPath, ESearchCase::IgnoreCase))
                        {
                            if (Candidate->IsChildOf(UActorComponent::StaticClass()))
                            {
                                ComponentClass = Candidate;
                                break;
                            }
                        }
                    }
                }
                if (!ComponentClass)
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unable to load component class %s."), *ComponentClassPath), TEXT("COMPONENT_CLASS_NOT_FOUND"));
                    return;
                }

                if (!ComponentClass->IsChildOf(UActorComponent::StaticClass()))
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Class %s is not a component."), *ComponentClassPath), TEXT("INVALID_COMPONENT_CLASS"));
                    return;
                }

                if (FindScsNodeByName(SCS, ComponentName))
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Component %s already exists on Blueprint."), *ComponentName), TEXT("COMPONENT_ALREADY_EXISTS"));
                    return;
                }

                USCS_Node* NewNode = SCS->CreateNode(ComponentClass, *ComponentName);
                if (!NewNode)
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to create SCS node for %s."), *ComponentName), TEXT("NODE_CREATION_FAILED"));
                    return;
                }

                bool bAttachedToParent = false;
                if (!AttachToName.TrimStartAndEnd().IsEmpty())
                {
                    if (USCS_Node* ParentNode = FindScsNodeByName(SCS, AttachToName))
                    {
                        ParentNode->AddChildNode(NewNode);
                        bAttachedToParent = true;
                        OperationSummary->SetStringField(TEXT("attachedTo"), AttachToName);
                    }
                    else
                    {
                        AccumulatedWarnings.Add(FString::Printf(TEXT("Parent component %s not found; %s added as root."), *AttachToName, *ComponentName));
                    }
                }

                if (!bAttachedToParent)
                {
                    SCS->AddNode(NewNode);
                }

                const TSharedPtr<FJsonObject>* TransformObject = nullptr;
                if (OperationObject->TryGetObjectField(TEXT("transform"), TransformObject) && TransformObject && TransformObject->IsValid())
                {
                    USceneComponent* SceneTemplate = Cast<USceneComponent>(NewNode->ComponentTemplate);
                    if (SceneTemplate)
                    {
                        FVector Location = SceneTemplate->GetRelativeLocation();
                        FRotator Rotation = SceneTemplate->GetRelativeRotation();
                        FVector Scale = SceneTemplate->GetRelativeScale3D();

                        ReadVectorField(*TransformObject, TEXT("location"), Location, Location);
                        ReadRotatorField(*TransformObject, TEXT("rotation"), Rotation, Rotation);
                        ReadVectorField(*TransformObject, TEXT("scale"), Scale, Scale);

                        SceneTemplate->SetRelativeLocation(Location);
                        SceneTemplate->SetRelativeRotation(Rotation);
                        SceneTemplate->SetRelativeScale3D(Scale);
                    }
                    else
                    {
                        AccumulatedWarnings.Add(FString::Printf(TEXT("Transform ignored for non-scene component %s."), *ComponentName));
                    }
                }

                const TSharedPtr<FJsonObject>* PropertyOverrides = nullptr;
                if (OperationObject->TryGetObjectField(TEXT("properties"), PropertyOverrides) && PropertyOverrides && PropertyOverrides->IsValid())
                {
                    UActorComponent* Template = NewNode->ComponentTemplate;
                    if (Template)
                    {
                        FString PropertyError;
                        if (!ApplyPropertyOverrides(Template, *PropertyOverrides, AccumulatedWarnings, PropertyError))
                        {
                            SendAutomationError(RequestingSocket, RequestId, PropertyError, TEXT("COMPONENT_PROPERTY_FAILED"));
                            return;
                        }
                    }
                }

                bAnyChanges = true;
                OperationSummary->SetBoolField(TEXT("success"), true);
                OperationSummary->SetStringField(TEXT("componentName"), ComponentName);
                OperationSummary->SetStringField(TEXT("componentClass"), ComponentClass->GetPathName());
            }
            else if (NormalizedType == TEXT("remove_component"))
            {
                FString ComponentName;
                if (!OperationObject->TryGetStringField(TEXT("componentName"), ComponentName) || ComponentName.TrimStartAndEnd().IsEmpty())
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("remove_component operation at index %d missing componentName."), Index), TEXT("INVALID_COMPONENT_NAME"));
                    return;
                }

                if (USCS_Node* TargetNode = FindScsNodeByName(SCS, ComponentName))
                {
                    SCS->RemoveNode(TargetNode);
                    bAnyChanges = true;
                    OperationSummary->SetBoolField(TEXT("success"), true);
                    OperationSummary->SetStringField(TEXT("componentName"), ComponentName);
                }
                else
                {
                    AccumulatedWarnings.Add(FString::Printf(TEXT("Component %s not found; remove skipped."), *ComponentName));
                    OperationSummary->SetBoolField(TEXT("success"), false);
                    OperationSummary->SetStringField(TEXT("componentName"), ComponentName);
                    OperationSummary->SetStringField(TEXT("warning"), TEXT("Component not found"));
                }
            }
            else if (NormalizedType == TEXT("set_component_properties") || NormalizedType == TEXT("modify_component"))
            {
                FString ComponentName;
                if (!OperationObject->TryGetStringField(TEXT("componentName"), ComponentName) || ComponentName.TrimStartAndEnd().IsEmpty())
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("set_component_properties operation at index %d missing componentName."), Index), TEXT("INVALID_COMPONENT_NAME"));
                    return;
                }

                const TSharedPtr<FJsonObject>* PropertyOverrides = nullptr;
                const TSharedPtr<FJsonObject>* TransformObject = nullptr;
                const bool hasProps = OperationObject->TryGetObjectField(TEXT("properties"), PropertyOverrides) && PropertyOverrides && PropertyOverrides->IsValid();
                const bool hasTransform = OperationObject->TryGetObjectField(TEXT("transform"), TransformObject) && TransformObject && TransformObject->IsValid();

                // For set_component_properties, a properties object is required.
                // For modify_component, allow transform-only updates (properties optional)
                if (NormalizedType == TEXT("set_component_properties") && !hasProps)
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("set_component_properties operation at index %d missing properties object."), Index), TEXT("INVALID_PROPERTIES"));
                    return;
                }

                // If this is modify_component and neither properties nor transform present, it's invalid.
                if (NormalizedType == TEXT("modify_component") && !hasProps && !hasTransform)
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("modify_component operation at index %d requires either properties or transform."), Index), TEXT("INVALID_OPERATION"));
                    return;
                }

                if (USCS_Node* TargetNode = FindScsNodeByName(SCS, ComponentName))
                {
                    UActorComponent* Template = TargetNode->ComponentTemplate;
                    if (!Template)
                    {
                        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Component %s has no template for property assignment."), *ComponentName), TEXT("COMPONENT_TEMPLATE_MISSING"));
                        return;
                    }

                    FString PropertyError;
                    if (!ApplyPropertyOverrides(Template, *PropertyOverrides, AccumulatedWarnings, PropertyError))
                    {
                        SendAutomationError(RequestingSocket, RequestId, PropertyError, TEXT("COMPONENT_PROPERTY_FAILED"));
                        return;
                    }

                    if (Template->IsA<USceneComponent>())
                    {
                        USceneComponent* SceneTemplate = Cast<USceneComponent>(Template);
                        const TSharedPtr<FJsonObject>* TransformObject = nullptr;
                        if (OperationObject->TryGetObjectField(TEXT("transform"), TransformObject) && TransformObject && TransformObject->IsValid())
                        {
                            FVector Location = SceneTemplate->GetRelativeLocation();
                            FRotator Rotation = SceneTemplate->GetRelativeRotation();
                            FVector Scale = SceneTemplate->GetRelativeScale3D();

                            ReadVectorField(*TransformObject, TEXT("location"), Location, Location);
                            ReadRotatorField(*TransformObject, TEXT("rotation"), Rotation, Rotation);
                            ReadVectorField(*TransformObject, TEXT("scale"), Scale, Scale);

                            SceneTemplate->SetRelativeLocation(Location);
                            SceneTemplate->SetRelativeRotation(Rotation);
                            SceneTemplate->SetRelativeScale3D(Scale);
                        }
                    }

                    bAnyChanges = true;
                    OperationSummary->SetBoolField(TEXT("success"), true);
                    OperationSummary->SetStringField(TEXT("componentName"), ComponentName);
                }
                else
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Component %s not found for property assignment."), *ComponentName), TEXT("COMPONENT_NOT_FOUND"));
                    return;
                }
            }
            else if (NormalizedType == TEXT("attach_component"))
                {
                    FString AttachComponentName;
                    if (!OperationObject->TryGetStringField(TEXT("componentName"), AttachComponentName) || AttachComponentName.TrimStartAndEnd().IsEmpty())
                    {
                        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("attach_component operation at index %d missing componentName."), Index), TEXT("INVALID_COMPONENT_NAME"));
                        return;
                    }

                    FString ParentName;
                    if (!OperationObject->TryGetStringField(TEXT("parentComponent"), ParentName))
                    {
                        OperationObject->TryGetStringField(TEXT("attachTo"), ParentName);
                    }
                    if (ParentName.TrimStartAndEnd().IsEmpty())
                    {
                        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("attach_component operation at index %d missing parentComponent."), Index), TEXT("INVALID_PARENT_COMPONENT"));
                        return;
                    }

                    USCS_Node* ChildNode = FindScsNodeByName(SCS, AttachComponentName);
                    if (!ChildNode)
                    {
                        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Child component %s not found for attach."), *AttachComponentName), TEXT("COMPONENT_NOT_FOUND"));
                        return;
                    }

                    USCS_Node* ParentNode = FindScsNodeByName(SCS, ParentName);
                    if (!ParentNode)
                    {
                        AccumulatedWarnings.Add(FString::Printf(TEXT("Parent component %s not found; attach skipped for %s."), *ParentName, *AttachComponentName));
                        OperationSummary->SetBoolField(TEXT("success"), false);
                        OperationSummary->SetStringField(TEXT("componentName"), AttachComponentName);
                        OperationSummary->SetStringField(TEXT("warning"), TEXT("Parent not found"));
                        OperationSummaries.Add(MakeShared<FJsonValueObject>(OperationSummary));
                        continue;
                    }

                    // Remove from previous parent if necessary, then attach to new parent
                    ParentNode->AddChildNode(ChildNode);
                    bAnyChanges = true;
                    OperationSummary->SetBoolField(TEXT("success"), true);
                    OperationSummary->SetStringField(TEXT("componentName"), AttachComponentName);
                    OperationSummary->SetStringField(TEXT("attachedTo"), ParentName);
                }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown SCS operation type: %s"), *OperationType), TEXT("UNKNOWN_OPERATION"));
                return;
            }

            OperationSummaries.Add(MakeShared<FJsonValueObject>(OperationSummary));
        }

        if (bAnyChanges)
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        }

        // Defer compile and save operations to avoid Task Graph recursion
        if (bCompile || bSave)
        {
            AsyncTask(ENamedThreads::GameThread, [this, RequestId, Blueprint, bCompile, bSave, NormalizedBlueprintPath, OperationSummaries, &AccumulatedWarnings, RequestingSocket, TriedCandidates]() {
                bool bSaveResult = false;
                if (bSave)
                {
                    bSaveResult = UEditorAssetLibrary::SaveLoadedAsset(Blueprint);
                    if (!bSaveResult)
                    {
                        AccumulatedWarnings.Add(TEXT("Blueprint failed to save; please check output log."));
                    }
                }

                if (bCompile)
                {
                    FKismetEditorUtilities::CompileBlueprint(Blueprint);
                }

                TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
                ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
                ResultPayload->SetStringField(TEXT("matchedCandidate"), NormalizedBlueprintPath);
                ResultPayload->SetArrayField(TEXT("operations"), OperationSummaries);
                ResultPayload->SetBoolField(TEXT("compiled"), bCompile);
                ResultPayload->SetBoolField(TEXT("saved"), bSave && bSaveResult);

                if (AccumulatedWarnings.Num() > 0)
                {
                    TArray<TSharedPtr<FJsonValue>> WarningValues;
                    WarningValues.Reserve(AccumulatedWarnings.Num());
                    for (const FString& Warning : AccumulatedWarnings)
                    {
                        WarningValues.Add(MakeShared<FJsonValueString>(Warning));
                    }
                    ResultPayload->SetArrayField(TEXT("warnings"), WarningValues);
                }

                if (TriedCandidates.Num() > 0)
                {
                    TArray<TSharedPtr<FJsonValue>> TriedValues;
                    for (const FString& C : TriedCandidates)
                    {
                        TriedValues.Add(MakeShared<FJsonValueString>(C));
                    }
                    ResultPayload->SetArrayField(TEXT("triedCandidates"), TriedValues);
                }

                const FString Message = FString::Printf(TEXT("Processed %d SCS operation(s)."), OperationSummaries.Num());
                SendAutomationResponse(RequestingSocket, RequestId, true, Message, ResultPayload, FString());
            });
        }
        else
        {
            TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
            ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
            ResultPayload->SetStringField(TEXT("matchedCandidate"), NormalizedBlueprintPath);
            ResultPayload->SetArrayField(TEXT("operations"), OperationSummaries);
            ResultPayload->SetBoolField(TEXT("compiled"), false);
            ResultPayload->SetBoolField(TEXT("saved"), false);

            if (AccumulatedWarnings.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> WarningValues;
                WarningValues.Reserve(AccumulatedWarnings.Num());
                for (const FString& Warning : AccumulatedWarnings)
                {
                    WarningValues.Add(MakeShared<FJsonValueString>(Warning));
                }
                ResultPayload->SetArrayField(TEXT("warnings"), WarningValues);
            }

            if (TriedCandidates.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> TriedValues;
                for (const FString& C : TriedCandidates)
                {
                    TriedValues.Add(MakeShared<FJsonValueString>(C));
                }
                ResultPayload->SetArrayField(TEXT("triedCandidates"), TriedValues);
            }

            const FString Message = FString::Printf(TEXT("Processed %d SCS operation(s)."), OperationSummaries.Num());
            SendAutomationResponse(RequestingSocket, RequestId, true, Message, ResultPayload, FString());
        }
        return;
    }

    if (Action.Equals(TEXT("import_asset_deferred"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("import_asset_deferred payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        FString SourcePath;
        if (!Payload->TryGetStringField(TEXT("sourcePath"), SourcePath) || SourcePath.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("import_asset_deferred requires a non-empty sourcePath."), TEXT("INVALID_SOURCE_PATH"));
            return;
        }

        FString DestinationPath;
        if (!Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath) || DestinationPath.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("import_asset_deferred requires a non-empty destinationPath."), TEXT("INVALID_DESTINATION_PATH"));
            return;
        }

        // Sanitize destination path (remove trailing slash) and normalize UE path
        DestinationPath = DestinationPath.Replace(TEXT("/"), TEXT("/")).Replace(TEXT("//"), TEXT("/"));
        if (DestinationPath.EndsWith(TEXT("/")))
        {
            DestinationPath = DestinationPath.LeftChop(1);
        }
        // Map /Content -> /Game for UE asset destinations
        if (DestinationPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase))
        {
            DestinationPath = FString::Printf(TEXT("/Game%s"), *DestinationPath.RightChop(7));
        }

        // Defer the asset import to avoid Task Graph recursion by scheduling it for the next frame
        FCoreDelegates::OnEndFrame.AddLambda([this, RequestId, SourcePath, DestinationPath, RequestingSocket]() {
            // Create the import task
            UAssetImportTask* Task = NewObject<UAssetImportTask>();
            Task->Filename = SourcePath;
            Task->DestinationPath = DestinationPath;

            // Execute the import with default settings
            FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
            TArray<UAssetImportTask*> Tasks = { Task };
            AssetToolsModule.Get().ImportAssetTasks(Tasks);

            // Prepare response
            TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
            ResultPayload->SetStringField(TEXT("sourcePath"), SourcePath);
            ResultPayload->SetStringField(TEXT("destinationPath"), DestinationPath);

            if (Task->ImportedObjectPaths.Num() > 0)
            {
                ResultPayload->SetNumberField(TEXT("imported"), Task->ImportedObjectPaths.Num());
                TArray<TSharedPtr<FJsonValue>> PathsArray;
                for (const FString& Path : Task->ImportedObjectPaths)
                {
                    PathsArray.Add(MakeShared<FJsonValueString>(Path));
                }
                ResultPayload->SetArrayField(TEXT("paths"), PathsArray);

                const FString Message = FString::Printf(TEXT("Imported %d assets to %s"), Task->ImportedObjectPaths.Num(), *DestinationPath);
                SendAutomationResponse(RequestingSocket, RequestId, true, Message, ResultPayload, FString());
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("No assets were imported"), TEXT("IMPORT_FAILED"));
            }
        });

        return;
    }

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown automation action: %s"), *Action), TEXT("UNKNOWN_ACTION"));
}

void UMcpAutomationBridgeSubsystem::SendAutomationResponse(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, const bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode)
{
    if (!TargetSocket.IsValid() || !TargetSocket->IsConnected())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Unable to send automation response (socket not connected)."));
        return;
    }

    TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("type"), TEXT("automation_response"));
    Response->SetStringField(TEXT("requestId"), RequestId);
    Response->SetBoolField(TEXT("success"), bSuccess);
    if (!Message.IsEmpty())
    {
        Response->SetStringField(TEXT("message"), Message);
    }
    if (!ErrorCode.IsEmpty())
    {
        Response->SetStringField(TEXT("error"), ErrorCode);
    }
    if (Result.IsValid())
    {
        Response->SetObjectField(TEXT("result"), Result.ToSharedRef());
    }

    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(Response, Writer);

    TargetSocket->Send(Serialized);
    
    // Clean up the request tracking
    PendingRequestsToSockets.Remove(RequestId);
}

void UMcpAutomationBridgeSubsystem::SendAutomationError(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, const FString& Message, const FString& ErrorCode)
{
    const FString ResolvedError = ErrorCode.IsEmpty() ? TEXT("AUTOMATION_ERROR") : ErrorCode;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation request failed (%s): %s"), *ResolvedError, *Message);
    SendAutomationResponse(TargetSocket, RequestId, false, Message, nullptr, ResolvedError);
}

void UMcpAutomationBridgeSubsystem::StartBridge()
{
    if (!TickerHandle.IsValid())
    {
        const FTickerDelegate TickDelegate = FTickerDelegate::CreateUObject(this, &UMcpAutomationBridgeSubsystem::Tick);
        const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
        const float Interval = (Settings && Settings->TickerIntervalSeconds > 0.0f) ? Settings->TickerIntervalSeconds : 0.25f;
        TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate, Interval);
    }
    // Mark the bridge as available so AttemptConnection() will run.
    bBridgeAvailable = true;
    bReconnectEnabled = AutoReconnectDelaySeconds > 0.0f;
    TimeUntilReconnect = 0.0f;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Starting MCP automation bridge."));
    AttemptConnection();
}

void UMcpAutomationBridgeSubsystem::StopBridge()
{
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle = FTSTicker::FDelegateHandle();
    }

    BridgeState = EMcpAutomationBridgeState::Disconnected;
    bBridgeAvailable = false;
    bReconnectEnabled = false;
    TimeUntilReconnect = 0.0f;

    // Close all active sockets
    for (TSharedPtr<FMcpBridgeWebSocket>& Socket : ActiveSockets)
    {
        if (Socket.IsValid())
        {
            Socket->OnConnected().RemoveAll(this);
            Socket->OnConnectionError().RemoveAll(this);
            Socket->OnClosed().RemoveAll(this);
            Socket->OnMessage().RemoveAll(this);
            Socket->OnHeartbeat().RemoveAll(this);
            Socket->Close();
        }
    }
    ActiveSockets.Empty();
    PendingRequestsToSockets.Empty();

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Automation bridge stopped."));
}

void UMcpAutomationBridgeSubsystem::RecordHeartbeat()
{
    LastHeartbeatTimestamp = FPlatformTime::Seconds();
}

void UMcpAutomationBridgeSubsystem::ResetHeartbeatTracking()
{
    LastHeartbeatTimestamp = 0.0;
    HeartbeatTimeoutSeconds = 0.0f;
    bHeartbeatTrackingEnabled = false;
}

void UMcpAutomationBridgeSubsystem::ForceReconnect(const FString& Reason, float ReconnectDelayOverride)
{
    const float EffectiveDelay = ReconnectDelayOverride >= 0.0f ? ReconnectDelayOverride : AutoReconnectDelaySeconds;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Forcing automation bridge reconnect (delay %.2f s): %s"), EffectiveDelay, Reason.IsEmpty() ? TEXT("no reason provided") : *Reason);

    // Close all active sockets
    for (TSharedPtr<FMcpBridgeWebSocket>& Socket : ActiveSockets)
    {
        if (Socket.IsValid())
        {
            Socket->OnConnected().RemoveAll(this);
            Socket->OnConnectionError().RemoveAll(this);
            Socket->OnClosed().RemoveAll(this);
            Socket->OnMessage().RemoveAll(this);
            Socket->OnHeartbeat().RemoveAll(this);
            Socket->Close();
        }
    }
    ActiveSockets.Empty();
    PendingRequestsToSockets.Empty();

    ResetHeartbeatTracking();
    BridgeState = EMcpAutomationBridgeState::Disconnected;
    bReconnectEnabled = true;
    TimeUntilReconnect = EffectiveDelay;

    if (EffectiveDelay <= 0.0f && bBridgeAvailable)
    {
        AttemptConnection();
    }
}

void UMcpAutomationBridgeSubsystem::SendControlMessage(const TSharedPtr<FJsonObject>& Message)
{
    if (!Message.IsValid())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Ignoring control message send; payload invalid."));
        return;
    }

    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(Message.ToSharedRef(), Writer);

    UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("Outbound control message: %s"), *Serialized);

    // Send control message to all connected sockets
    for (const TSharedPtr<FMcpBridgeWebSocket>& Socket : ActiveSockets)
    {
        if (Socket.IsValid() && Socket->IsConnected())
        {
            Socket->Send(Serialized);
        }
    }
}

