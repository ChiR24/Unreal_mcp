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
#include "Factories/BlueprintFactory.h"
// Level Sequence support
#include "Misc/Guid.h"
#include "Containers/Set.h"

DEFINE_LOG_CATEGORY(LogMcpAutomationBridgeSubsystem);

namespace
{
class FMcpPythonOutputCapture final : public FOutputDevice
{
public:
    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        if (!V) return;
        // Simple capture: append each incoming log line for later consumption by callers.
        Lines.Add(FString(V));
    }

    // Return captured lines and clear the internal buffer.
    TArray<FString> Consume()
    {
        TArray<FString> Copy = Lines;
        Lines.Empty();
        return Copy;
    }

private:
    TArray<FString> Lines;
};
} // anonymous namespace

// Local helper state and small utility functions for the McpAutomationBridge
// subsystem. Kept in an anonymous namespace to avoid polluting global scope.
namespace
{
// Lightweight in-memory registries used by sequence/blueprint stub handlers
static TSet<FString> GBlueprintBusySet;
static TMap<FString, TSharedPtr<FJsonObject>> GSequenceRegistry;
static TMap<FString, TSharedPtr<FJsonObject>> GBlueprintRegistry;
static FString GCurrentSequencePath;

// Read a vector field from a JSON object. Supports array [x,y,z], object
// {x:..,y:..,z:..}, or comma-separated string "x,y,z". Falls back to
// the provided Default when parsing fails.
static void ReadVectorField(const FJsonObject& Parent, const TCHAR* FieldName, FVector& OutValue, const FVector& Default)
{
    OutValue = Default;
    if (!Parent.HasField(FieldName)) return;

    const TSharedPtr<FJsonValue> Val = Parent.TryGetField(FieldName);
    if (!Val.IsValid()) return;

    if (Val->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Val->AsArray();
        if (Arr.Num() >= 3)
        {
            OutValue.X = Arr[0]->AsNumber();
            OutValue.Y = Arr[1]->AsNumber();
            OutValue.Z = Arr[2]->AsNumber();
        }
        return;
    }

    if (Val->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Obj = Val->AsObject();
        double Tmp = 0.0;
        if (Obj->TryGetNumberField(TEXT("x"), Tmp)) OutValue.X = Tmp; else OutValue.X = Default.X;
        if (Obj->TryGetNumberField(TEXT("y"), Tmp)) OutValue.Y = Tmp; else OutValue.Y = Default.Y;
        if (Obj->TryGetNumberField(TEXT("z"), Tmp)) OutValue.Z = Tmp; else OutValue.Z = Default.Z;
        return;
    }

    if (Val->Type == EJson::String)
    {
        FString S = Val->AsString();
        TArray<FString> Parts;
        S.ParseIntoArray(Parts, TEXT(","), true);
        if (Parts.Num() >= 3)
        {
            OutValue.X = FCString::Atod(*Parts[0]);
            OutValue.Y = FCString::Atod(*Parts[1]);
            OutValue.Z = FCString::Atod(*Parts[2]);
        }
        return;
    }
}

// Read a rotator field from JSON. Same formats as ReadVectorField.
static void ReadRotatorField(const FJsonObject& Parent, const TCHAR* FieldName, FRotator& OutValue, const FRotator& Default)
{
    OutValue = Default;
    if (!Parent.HasField(FieldName)) return;

    const TSharedPtr<FJsonValue> Val = Parent.TryGetField(FieldName);
    if (!Val.IsValid()) return;

    if (Val->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Val->AsArray();
        if (Arr.Num() >= 3)
        {
            OutValue.Roll = Arr[0]->AsNumber();
            OutValue.Pitch = Arr[1]->AsNumber();
            OutValue.Yaw = Arr[2]->AsNumber();
        }
        return;
    }

    if (Val->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Obj = Val->AsObject();
        double Tmp = 0.0;
        if (Obj->TryGetNumberField(TEXT("roll"), Tmp)) OutValue.Roll = Tmp; else OutValue.Roll = Default.Roll;
        if (Obj->TryGetNumberField(TEXT("pitch"), Tmp)) OutValue.Pitch = Tmp; else OutValue.Pitch = Default.Pitch;
        if (Obj->TryGetNumberField(TEXT("yaw"), Tmp)) OutValue.Yaw = Tmp; else OutValue.Yaw = Default.Yaw;
        return;
    }

    if (Val->Type == EJson::String)
    {
        FString S = Val->AsString();
        TArray<FString> Parts;
        S.ParseIntoArray(Parts, TEXT(","), true);
        if (Parts.Num() >= 3)
        {
            OutValue.Roll = FCString::Atod(*Parts[0]);
            OutValue.Pitch = FCString::Atod(*Parts[1]);
            OutValue.Yaw = FCString::Atod(*Parts[2]);
        }
        return;
    }
}

// Convenience overloads that accept a shared pointer to a JSON object so
// callers that already hold a TSharedPtr<FJsonObject> can pass it directly.
static void ReadVectorField(const TSharedPtr<FJsonObject>& ParentPtr, const TCHAR* FieldName, FVector& OutValue, const FVector& Default)
{
    if (!ParentPtr.IsValid()) { OutValue = Default; return; }
    ReadVectorField(*ParentPtr, FieldName, OutValue, Default);
}

static void ReadRotatorField(const TSharedPtr<FJsonObject>& ParentPtr, const TCHAR* FieldName, FRotator& OutValue, const FRotator& Default)
{
    if (!ParentPtr.IsValid()) { OutValue = Default; return; }
    ReadRotatorField(*ParentPtr, FieldName, OutValue, Default);
}

// Find an SCS node by name while traversing root + child nodes. We check
// both the node variable name and the node object name as a fallback.
static USCS_Node* FindScsNodeByName(USimpleConstructionScript* SCS, const FString& Name)
{
    if (!SCS || Name.IsEmpty()) return nullptr;

    const TArray<USCS_Node*>& Roots = SCS->GetRootNodes();
    TArray<USCS_Node*> Work;
    Work.Append(Roots);

    while (Work.Num() > 0)
    {
        USCS_Node* Node = Work.Pop();
        if (!Node) continue;

        // Check variable name (the usual identifier) and the node object name
        const FString VarName = Node->GetVariableName().ToString();
        if (VarName.Equals(Name, ESearchCase::IgnoreCase) || Node->GetName().Equals(Name, ESearchCase::IgnoreCase))
        {
            return Node;
        }

        // Add children for breadth-first traversal
        const TArray<USCS_Node*>& Children = Node->GetChildNodes();
        for (USCS_Node* Child : Children) if (Child) Work.Add(Child);
    }

    return nullptr;
}

// Load a Blueprint asset from a flexible spec. Attempts the provided string
// verbatim, then the left-of-dot form if present, and a few heuristics.
static UBlueprint* LoadBlueprintAsset(const FString& Spec, FString& OutNormalized, FString& OutError)
{
    OutNormalized.Empty();
    OutError.Empty();
    if (Spec.IsEmpty()) { OutError = TEXT("Empty blueprint path"); return nullptr; }

    TArray<FString> Candidates;
    Candidates.Add(Spec);
    FString Left, Right;
    if (Spec.Split(TEXT("."), &Left, &Right)) Candidates.Add(Left);

    // Try a few guesses (allow short names like "Blueprints/MyBp")
    if (!Spec.StartsWith(TEXT("/Game")) && !Spec.StartsWith(TEXT("/Engine")) && !Spec.StartsWith(TEXT("/Script")))
    {
        Candidates.Add(FString::Printf(TEXT("/Game/%s"), *Spec));
    }

    for (const FString& C : Candidates)
    {
        if (C.IsEmpty()) continue;
        UObject* Loaded = UEditorAssetLibrary::LoadAsset(C);
        if (!Loaded) continue;
        if (UBlueprint* BP = Cast<UBlueprint>(Loaded))
        {
            OutNormalized = C;
            return BP;
        }
        // If asset is a UClass (generated blueprint class), attempt to discover the blueprint
        if (UClass* Cls = Cast<UClass>(Loaded))
        {
            // Best-effort: try to find a blueprint asset with the same name in the same package
            FString AssetPath = Loaded->GetPathName();
            UBlueprint* FoundBp = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(AssetPath));
            if (FoundBp)
            {
                OutNormalized = AssetPath;
                return FoundBp;
            }
        }
    }

    OutError = FString::Printf(TEXT("Failed to load Blueprint asset %s"), *Spec);
    return nullptr;
}

// Export a property value into a FJsonValue. Supports most simple property
// kinds (string, name, bool, numeric, object refs, FVector/FRotator).
static TSharedPtr<FJsonValue> ExportPropertyToJsonValue(UObject* TargetObject, FProperty* Property)
{
    if (!TargetObject || !Property) return nullptr;

    void* PropAddr = Property->ContainerPtrToValuePtr<void>(TargetObject);

    if (FStrProperty* SP = CastField<FStrProperty>(Property))
    {
        return MakeShared<FJsonValueString>(SP->GetPropertyValue(PropAddr));
    }
    if (FNameProperty* NP = CastField<FNameProperty>(Property))
    {
        return MakeShared<FJsonValueString>(NP->GetPropertyValue(PropAddr).ToString());
    }
    if (FBoolProperty* BP = CastField<FBoolProperty>(Property))
    {
        return MakeShared<FJsonValueBoolean>(BP->GetPropertyValue(PropAddr));
    }
    if (FIntProperty* IP = CastField<FIntProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(IP->GetPropertyValue(PropAddr)));
    }
    if (FFloatProperty* FP = CastField<FFloatProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(FP->GetPropertyValue(PropAddr)));
    }
    if (FDoubleProperty* DP = CastField<FDoubleProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(DP->GetPropertyValue(PropAddr));
    }
    if (FObjectPropertyBase* OP = CastField<FObjectPropertyBase>(Property))
    {
        UObject* Obj = OP->GetObjectPropertyValue(PropAddr);
        if (Obj) return MakeShared<FJsonValueString>(Obj->GetPathName());
        return MakeShared<FJsonValueNull>();
    }
    if (FStructProperty* SPProp = CastField<FStructProperty>(Property))
    {
        const FName StructName = SPProp->Struct->GetFName();
        if (StructName == NAME_Vector)
        {
            const FVector* V = SPProp->ContainerPtrToValuePtr<FVector>(TargetObject);
            if (V)
            {
                TArray<TSharedPtr<FJsonValue>> Arr;
                Arr.Add(MakeShared<FJsonValueNumber>(V->X));
                Arr.Add(MakeShared<FJsonValueNumber>(V->Y));
                Arr.Add(MakeShared<FJsonValueNumber>(V->Z));
                return MakeShared<FJsonValueArray>(Arr);
            }
            return nullptr;
        }
        if (StructName == NAME_Rotator)
        {
            const FRotator* R = SPProp->ContainerPtrToValuePtr<FRotator>(TargetObject);
            if (R)
            {
                TArray<TSharedPtr<FJsonValue>> Arr;
                Arr.Add(MakeShared<FJsonValueNumber>(R->Roll));
                Arr.Add(MakeShared<FJsonValueNumber>(R->Pitch));
                Arr.Add(MakeShared<FJsonValueNumber>(R->Yaw));
                return MakeShared<FJsonValueArray>(Arr);
            }
            return nullptr;
        }
    }

    // Unknown/unsupported property type for export — return null to signal
    // callers that this property could not be converted into JSON.
    return nullptr;
}

// Apply a JSON value to a reflected UProperty on an object. Uses explicit
// assignment for common property kinds and falls back to ImportText for
// others. Returns true on success and sets OutError on failure.
static bool ApplyJsonValueToProperty(UObject* TargetObject, FProperty* Property, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
    OutError.Empty();
    if (!TargetObject || !Property || !Value.IsValid())
    {
        OutError = TEXT("Invalid arguments");
        return false;
    }

    void* PropAddr = Property->ContainerPtrToValuePtr<void>(TargetObject);

    if (FStrProperty* SP = CastField<FStrProperty>(Property))
    {
        if (Value->Type == EJson::String) { SP->SetPropertyValue(PropAddr, Value->AsString()); return true; }
        SP->SetPropertyValue(PropAddr, Value->AsString());
        return true;
    }

    if (FBoolProperty* BP = CastField<FBoolProperty>(Property))
    {
        if (Value->Type == EJson::Boolean) { BP->SetPropertyValue(PropAddr, Value->AsBool()); return true; }
        // Accept textual booleans
        if (Value->Type == EJson::String) { BP->SetPropertyValue(PropAddr, Value->AsString().ToBool()); return true; }
        OutError = TEXT("Cannot convert JSON value to bool");
        return false;
    }

    if (FIntProperty* IP = CastField<FIntProperty>(Property))
    {
        if (Value->Type == EJson::Number) { IP->SetPropertyValue(PropAddr, static_cast<int32>(Value->AsNumber())); return true; }
        if (Value->Type == EJson::String) { IP->SetPropertyValue(PropAddr, FCString::Atoi(*Value->AsString())); return true; }
        OutError = TEXT("Cannot convert JSON value to int");
        return false;
    }

    if (FFloatProperty* FP = CastField<FFloatProperty>(Property))
    {
        if (Value->Type == EJson::Number) { FP->SetPropertyValue(PropAddr, static_cast<float>(Value->AsNumber())); return true; }
        if (Value->Type == EJson::String) { FP->SetPropertyValue(PropAddr, FCString::Atof(*Value->AsString())); return true; }
        OutError = TEXT("Cannot convert JSON value to float");
        return false;
    }

    if (FDoubleProperty* DP = CastField<FDoubleProperty>(Property))
    {
        if (Value->Type == EJson::Number) { DP->SetPropertyValue(PropAddr, Value->AsNumber()); return true; }
        if (Value->Type == EJson::String) { DP->SetPropertyValue(PropAddr, FCString::Atod(*Value->AsString())); return true; }
        OutError = TEXT("Cannot convert JSON value to double");
        return false;
    }

    if (FObjectPropertyBase* OP = CastField<FObjectPropertyBase>(Property))
    {
        if (Value->Type == EJson::String)
        {
            const FString Path = Value->AsString();
            if (Path.IsEmpty()) { OP->SetObjectPropertyValue(PropAddr, nullptr); return true; }
            UObject* Loaded = StaticLoadObject(OP->PropertyClass, nullptr, *Path);
            if (!Loaded)
            {
                OutError = FString::Printf(TEXT("Failed to load object at path: %s"), *Path);
                return false;
            }
            OP->SetObjectPropertyValue(PropAddr, Loaded);
            return true;
        }
        OutError = TEXT("Object property requires string path in JSON");
        return false;
    }

    if (FStructProperty* SPProp = CastField<FStructProperty>(Property))
    {
        const FName StructName = SPProp->Struct->GetFName();
        if (StructName == NAME_Vector)
        {
            FVector* VPtr = SPProp->ContainerPtrToValuePtr<FVector>(TargetObject);
            const FVector Default = *VPtr;
            if (Value->Type == EJson::Array || Value->Type == EJson::Object || Value->Type == EJson::String)
            {
                // Build a temporary object wrapper to reuse ReadVectorField
                TSharedPtr<FJsonObject> Tmp = MakeShared<FJsonObject>();
                Tmp->SetField(TEXT("v"), Value);
                ReadVectorField(*Tmp, TEXT("v"), *VPtr, Default);
                return true;
            }
            OutError = TEXT("Unsupported JSON format for FVector");
            return false;
        }

        if (StructName == NAME_Rotator)
        {
            FRotator* RPtr = SPProp->ContainerPtrToValuePtr<FRotator>(TargetObject);
            const FRotator Default = *RPtr;
            if (Value->Type == EJson::Array || Value->Type == EJson::Object || Value->Type == EJson::String)
            {
                TSharedPtr<FJsonObject> Tmp = MakeShared<FJsonObject>();
                Tmp->SetField(TEXT("r"), Value);
                ReadRotatorField(*Tmp, TEXT("r"), *RPtr, Default);
                return true;
            }
            OutError = TEXT("Unsupported JSON format for FRotator");
            return false;
        }
    }

    // Fallback: not supported for arbitrary property kinds. Provide a
    // descriptive error so callers can handle unsupported properties.
    OutError = TEXT("Unsupported property type for JSON-to-property conversion. Implement additional cases if needed.");
    return false;
}

} // namespace

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
    // Clean up any pending request mappings for this socket to avoid stale state
    for (auto It = PendingRequestsToSockets.CreateIterator(); It; ++It)
    {
        if (It.Value() == Socket)
        {
            const FString PendingId = It.Key();
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Removing pending mapping for RequestId=%s due to socket close."), *PendingId);
            It.RemoveCurrent();
        }
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
// (ProcessPendingAutomationRequests implementation defined below)

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

    void UMcpAutomationBridgeSubsystem::ProcessPendingAutomationRequests()
    {
        TArray<FPendingAutomationRequest> LocalQueue;
        {
            FScopeLock Lock(&PendingAutomationRequestsMutex);
            LocalQueue = PendingAutomationRequests;
            PendingAutomationRequests.Empty();
            bPendingRequestsScheduled = false;
        }

        for (const FPendingAutomationRequest& Req : LocalQueue)
        {
            // Guard against reentrancy inside the sequential processing
            if (bProcessingAutomationRequest)
            {
                // Shouldn't happen since we process sequentially, but be defensive
                UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Reentrant detection while draining automation queue; skipping %s"), *Req.RequestId);
                continue;
            }
            ProcessAutomationRequest(Req.RequestId, Req.Action, Req.Payload, Req.RequestingSocket);
        }
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

    // Wrap the implementation in a lambda and catch any unhandled exceptions so
    // we always send an automation error back to the caller instead of
    // letting the request hang until the client's timeout.
    auto ProcessBody = [&]() -> void {
        // Ensure the flag is reset even if an exception occurs inside the body
        ON_SCOPE_EXIT
        {
            bProcessingAutomationRequest = false;
        };
    try {
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

    // Quick existence probe for Blueprints — plugin-side implementation to
    // avoid falling back to Editor Python for frequent existence checks.
    if (Action.Equals(TEXT("blueprint_exists"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_exists payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        TArray<FString> CandidatePaths;
        const TArray<TSharedPtr<FJsonValue>>* CandidateArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("candidates"), CandidateArray) && CandidateArray != nullptr && CandidateArray->Num() > 0)
        {
            for (const TSharedPtr<FJsonValue>& V : *CandidateArray)
            {
                if (!V.IsValid()) continue;
                const FString S = V->AsString();
                if (!S.TrimStartAndEnd().IsEmpty()) CandidatePaths.Add(S);
            }
        }
        else
        {
            FString Single;
            if (Payload->TryGetStringField(TEXT("requestedPath"), Single) && !Single.TrimStartAndEnd().IsEmpty())
            {
                CandidatePaths.Add(Single);
            }
            else if (Payload->TryGetStringField(TEXT("blueprintPath"), Single) && !Single.TrimStartAndEnd().IsEmpty())
            {
                CandidatePaths.Add(Single);
            }
        }

        if (CandidatePaths.Num() == 0)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_exists requires candidates or requestedPath."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        for (const FString& Candidate : CandidatePaths)
        {
            FString Normalized;
            FString LoadError;
            UBlueprint* Found = LoadBlueprintAsset(Candidate, Normalized, LoadError);
            if (Found)
            {
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetBoolField(TEXT("exists"), true);
                Result->SetStringField(TEXT("found"), Normalized);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint exists."), Result, FString());
                return;
            }
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("exists"), false);
        TArray<TSharedPtr<FJsonValue>> TriedValues;
        TriedValues.Reserve(CandidatePaths.Num());
        for (const FString& C : CandidatePaths) TriedValues.Add(MakeShared<FJsonValueString>(C));
        Result->SetArrayField(TEXT("triedCandidates"), TriedValues);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint not found."), Result, FString());
        return;
    }

    // Create a Blueprint asset using AssetTools and BlueprintFactory. This
    // keeps creation logic on the plugin side and prevents repeated Python
    // fallbacks for simple create operations.
    if (Action.Equals(TEXT("blueprint_create"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_create payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        FString Name;
        if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_create requires a non-empty name."), TEXT("INVALID_NAME"));
            return;
        }

        FString SavePath = TEXT("/Game/Blueprints");
        Payload->TryGetStringField(TEXT("savePath"), SavePath);
        // Normalize the save path
        SavePath = SavePath.Replace(TEXT("\\"), TEXT("/"));
        SavePath = SavePath.Replace(TEXT("//"), TEXT("/"));
        if (SavePath.EndsWith(TEXT("/"))) SavePath = SavePath.LeftChop(1);
        if (SavePath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase))
        {
            SavePath = FString::Printf(TEXT("/Game%s"), *SavePath.RightChop(8));
        }
        if (!SavePath.StartsWith(TEXT("/Game")))
        {
            // Accept short forms like 'Blueprints'
            SavePath = FString::Printf(TEXT("/Game/%s"), *SavePath.Replace(TEXT("/"), TEXT("")));
        }

        FString ParentClassSpec;
        Payload->TryGetStringField(TEXT("parentClass"), ParentClassSpec);

        // Attempt creation on the game thread synchronously — creating assets
        // must happen in the editor thread context.
        // First check whether an asset already exists at the target location
        // and treat create as idempotent: return success if the Blueprint
        // already exists so repeated test runs are deterministic.
        {
            FString CandidatePath = SavePath;
            if (!CandidatePath.EndsWith(TEXT("/"))) CandidatePath += TEXT("/");
            CandidatePath += Name;
            FString ExistingNormalized;
            FString ExistingErr;
            UBlueprint* ExistingBp = LoadBlueprintAsset(CandidatePath, ExistingNormalized, ExistingErr);
            if (ExistingBp)
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_create: asset already exists: %s -> %s"), *CandidatePath, *ExistingNormalized);
                TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
                ResultPayload->SetStringField(TEXT("path"), ExistingNormalized);
                ResultPayload->SetStringField(TEXT("assetPath"), ExistingBp->GetPathName());
                ResultPayload->SetBoolField(TEXT("alreadyExisted"), true);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint already exists"), ResultPayload, FString());
                return;
            }
        }
        UBlueprint* CreatedBlueprint = nullptr;
        FString CreatedNormalizedPath;
        FString CreationError;
        {
            // Factory and class resolution must run in editor context
            UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
            if (!ParentClassSpec.IsEmpty())
            {
                // Resolve simple parent hints (try /Script/ loads, blueprint assets, or short class names)
                UClass* ResolvedParent = nullptr;
                if (ParentClassSpec.StartsWith(TEXT("/Script/")))
                {
                    ResolvedParent = FindObject<UClass>(nullptr, *ParentClassSpec);
                    if (!ResolvedParent) ResolvedParent = StaticLoadClass(UObject::StaticClass(), nullptr, *ParentClassSpec);
                }
                else if (ParentClassSpec.StartsWith(TEXT("/Game/")))
                {
                    FString TmpNormalized; FString TmpErr;
                    UBlueprint* ParentBp = LoadBlueprintAsset(ParentClassSpec, TmpNormalized, TmpErr);
                    if (ParentBp && ParentBp->GeneratedClass) ResolvedParent = ParentBp->GeneratedClass;
                }
                else
                {
                    for (TObjectIterator<UClass> It; It; ++It)
                    {
                        UClass* C = *It;
                        if (!C) continue;
                        if (C->GetName().Equals(ParentClassSpec, ESearchCase::IgnoreCase))
                        {
                            ResolvedParent = C;
                            break;
                        }
                    }
                }
                if (ResolvedParent)
                {
                    Factory->ParentClass = ResolvedParent;
                }
            }

            FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
            UObject* NewObj = AssetToolsModule.Get().CreateAsset(Name, SavePath, UBlueprint::StaticClass(), Factory);
            if (!NewObj)
            {
                CreationError = FString::Printf(TEXT("Failed to create blueprint asset %s in %s"), *Name, *SavePath);
            }
            else
            {
                CreatedBlueprint = Cast<UBlueprint>(NewObj);
                if (!CreatedBlueprint)
                {
                    CreationError = FString::Printf(TEXT("Created asset is not a Blueprint: %s"), *NewObj->GetPathName());
                }
                else
                {
                    // Attempt to persist the created asset
                    const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(CreatedBlueprint);
                    CreatedNormalizedPath = CreatedBlueprint->GetPathName();
                    if (CreatedNormalizedPath.Contains(TEXT(".")))
                    {
                        // convert '/Game/path/Name.Name' -> '/Game/path/Name'
                        CreatedNormalizedPath = CreatedNormalizedPath.Left(CreatedNormalizedPath.Find(TEXT(".")));
                    }
                    if (!bSaved)
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Blueprint created but failed to save: %s"), *CreatedBlueprint->GetPathName());
                    }
                }
            }
        }

        if (!CreatedBlueprint)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, CreationError.IsEmpty() ? TEXT("Blueprint creation failed") : CreationError, nullptr, TEXT("CREATE_FAILED"));
            return;
        }

        TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
        ResultPayload->SetStringField(TEXT("path"), CreatedNormalizedPath);
        ResultPayload->SetStringField(TEXT("assetPath"), CreatedBlueprint->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint created"), ResultPayload, FString());
        return;
    }

    if (Action.Equals(TEXT("blueprint_modify_scs"), ESearchCase::IgnoreCase))
    {
        const double HandlerStartTimeSec = FPlatformTime::Seconds();
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_modify_scs handler start (RequestId=%s)"), *RequestId);

        if (!Payload.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        // Resolve blueprint path or candidate list
        FString BlueprintPath;
        TArray<FString> CandidatePaths;
        if (!Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) || BlueprintPath.TrimStartAndEnd().IsEmpty())
        {
            const TArray<TSharedPtr<FJsonValue>>* CandidateArray = nullptr;
            if (!Payload->TryGetArrayField(TEXT("blueprintCandidates"), CandidateArray) || CandidateArray == nullptr || CandidateArray->Num() == 0)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs requires a non-empty blueprintPath or blueprintCandidates."), TEXT("INVALID_BLUEPRINT"));
                return;
            }
            for (const TSharedPtr<FJsonValue>& Val : *CandidateArray)
            {
                if (!Val.IsValid()) continue;
                const FString Candidate = Val->AsString();
                if (!Candidate.TrimStartAndEnd().IsEmpty()) CandidatePaths.Add(Candidate);
            }
            if (CandidatePaths.Num() == 0)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs blueprintCandidates array provided but contains no valid strings."), TEXT("INVALID_BLUEPRINT_CANDIDATES"));
                return;
            }
        }

        // Operations are required
        const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("operations"), OperationsArray) || OperationsArray == nullptr)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs requires an operations array."), TEXT("INVALID_OPERATIONS"));
            return;
        }

        // Flags
        bool bCompile = false;
        if (Payload->HasField(TEXT("compile")) && !Payload->TryGetBoolField(TEXT("compile"), bCompile))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("compile must be a boolean."), TEXT("INVALID_COMPILE_FLAG"));
            return;
        }
        bool bSave = false;
        if (Payload->HasField(TEXT("save")) && !Payload->TryGetBoolField(TEXT("save"), bSave))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("save must be a boolean."), TEXT("INVALID_SAVE_FLAG"));
            return;
        }

        // Resolve the blueprint asset (explicit path preferred, then candidates)
        FString NormalizedBlueprintPath;
        FString LoadError;
        UBlueprint* Blueprint = nullptr;
        TArray<FString> TriedCandidates;

        if (!BlueprintPath.IsEmpty())
        {
            TriedCandidates.Add(BlueprintPath);
            Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedBlueprintPath, LoadError);
            if (Blueprint)
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Loaded blueprint from explicit path: %s -> %s"), *BlueprintPath, *NormalizedBlueprintPath);
            }
        }

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
                LoadError = CandidateError;
            }
        }

        if (!Blueprint)
        {
            TSharedPtr<FJsonObject> ErrPayload = MakeShared<FJsonObject>();
            if (TriedCandidates.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> TriedValues;
                for (const FString& C : TriedCandidates) TriedValues.Add(MakeShared<FJsonValueString>(C));
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
                for (const FString& C : TriedCandidates) TriedValues.Add(MakeShared<FJsonValueString>(C));
                ErrPayload->SetArrayField(TEXT("triedCandidates"), TriedValues);
            }
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint does not expose a SimpleConstructionScript."), ErrPayload, TEXT("SCS_UNAVAILABLE"));
            return;
        }

        if (OperationsArray->Num() == 0)
        {
            TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
            ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
            ResultPayload->SetArrayField(TEXT("operations"), TArray<TSharedPtr<FJsonValue>>());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No SCS operations supplied."), ResultPayload, FString());
            return;
        }

        // Prevent concurrent SCS modifications against the same blueprint.
        const FString BusyKey = NormalizedBlueprintPath;
        if (!BusyKey.IsEmpty())
        {
            if (GBlueprintBusySet.Contains(BusyKey))
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Blueprint %s is busy with another modification."), *BusyKey), nullptr, TEXT("BLUEPRINT_BUSY"));
                return;
            }

            GBlueprintBusySet.Add(BusyKey);
            this->CurrentBusyBlueprintKey = BusyKey;
            this->bCurrentBlueprintBusyMarked = true;
            this->bCurrentBlueprintBusyScheduled = false;

            // If we exit before scheduling the deferred work, clear the busy flag
            ON_SCOPE_EXIT
            {
                if (this->bCurrentBlueprintBusyMarked && !this->bCurrentBlueprintBusyScheduled)
                {
                    GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey);
                    this->bCurrentBlueprintBusyMarked = false;
                    this->CurrentBusyBlueprintKey.Empty();
                }
            };
        }

        // Make a shallow copy of the operations array so the deferred lambda
        // can safely reference them after this function returns.
        TArray<TSharedPtr<FJsonValue>> DeferredOps = *OperationsArray;

        // Lightweight validation of operations
        for (int32 Index = 0; Index < DeferredOps.Num(); ++Index)
        {
            const TSharedPtr<FJsonValue>& OperationValue = DeferredOps[Index];
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
        }

        // Mark busy as scheduled (the deferred worker will clear it)
        this->bCurrentBlueprintBusyScheduled = true;

        // Build immediate acknowledgement payload summarizing scheduled ops
        TArray<TSharedPtr<FJsonValue>> ImmediateSummaries;
        ImmediateSummaries.Reserve(DeferredOps.Num());
        for (int32 Index = 0; Index < DeferredOps.Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> OpObj = DeferredOps[Index]->AsObject();
            TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
            FString Type; OpObj->TryGetStringField(TEXT("type"), Type);
            Summary->SetNumberField(TEXT("index"), Index);
            Summary->SetStringField(TEXT("type"), Type.IsEmpty() ? TEXT("unknown") : Type);
            Summary->SetBoolField(TEXT("scheduled"), true);
            ImmediateSummaries.Add(MakeShared<FJsonValueObject>(Summary));
        }

        TSharedPtr<FJsonObject> AckPayload = MakeShared<FJsonObject>();
        AckPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
        AckPayload->SetStringField(TEXT("matchedCandidate"), NormalizedBlueprintPath);
        AckPayload->SetArrayField(TEXT("operations"), ImmediateSummaries);
        AckPayload->SetBoolField(TEXT("scheduled"), true);
        AckPayload->SetBoolField(TEXT("compiled"), false);
        AckPayload->SetBoolField(TEXT("saved"), false);

        const FString AckMessage = FString::Printf(TEXT("Scheduled %d SCS operation(s) for application."), DeferredOps.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, AckMessage, AckPayload, FString());
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_modify_scs: RequestId=%s scheduled %d ops and returned ack."), *RequestId, DeferredOps.Num());

        // Defer actual SCS application to the game thread
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, DeferredOps, NormalizedBlueprintPath, bCompile, bSave, TriedCandidates, HandlerStartTimeSec, RequestingSocket]() mutable {
            TSharedPtr<FJsonObject> CompletionResult = MakeShared<FJsonObject>();
            TArray<FString> LocalWarnings;
            TArray<TSharedPtr<FJsonValue>> FinalSummaries;
            bool bOk = false;

            // (Re)load the blueprint on the game thread
            FString LocalNormalized; FString LocalLoadError;
            UBlueprint* LocalBP = LoadBlueprintAsset(NormalizedBlueprintPath, LocalNormalized, LocalLoadError);

            if (!LocalBP)
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Deferred SCS application failed to load blueprint %s: %s"), *NormalizedBlueprintPath, *LocalLoadError);
                CompletionResult->SetStringField(TEXT("error"), LocalLoadError);
            }
            else
            {
                USimpleConstructionScript* LocalSCS = LocalBP->SimpleConstructionScript;
                if (!LocalSCS)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Deferred SCS application: SCS unavailable for %s"), *NormalizedBlueprintPath);
                    CompletionResult->SetStringField(TEXT("error"), TEXT("SCS_UNAVAILABLE"));
                }
                else
                {
                    LocalBP->Modify();
                    LocalSCS->Modify();

                    for (int32 Index = 0; Index < DeferredOps.Num(); ++Index)
                    {
                        const double OpStart = FPlatformTime::Seconds();
                        const TSharedPtr<FJsonValue>& V = DeferredOps[Index];
                        if (!V.IsValid() || V->Type != EJson::Object) continue;
                        const TSharedPtr<FJsonObject> Op = V->AsObject();
                        FString OpType; Op->TryGetStringField(TEXT("type"), OpType);
                        const FString NormalizedType = OpType.ToLower();
                        TSharedPtr<FJsonObject> OpSummary = MakeShared<FJsonObject>();
                        OpSummary->SetNumberField(TEXT("index"), Index);
                        OpSummary->SetStringField(TEXT("type"), NormalizedType);

                        if (NormalizedType == TEXT("modify_component"))
                        {
                            FString ComponentName; Op->TryGetStringField(TEXT("componentName"), ComponentName);
                            const TSharedPtr<FJsonObject>* TransformObj = nullptr;
                            Op->TryGetObjectField(TEXT("transform"), TransformObj);
                            if (!ComponentName.IsEmpty() && TransformObj && (*TransformObj).IsValid())
                            {
                                USCS_Node* Node = FindScsNodeByName(LocalSCS, ComponentName);
                                if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USceneComponent>())
                                {
                                    USceneComponent* SceneTemplate = Cast<USceneComponent>(Node->ComponentTemplate);
                                    FVector Location = SceneTemplate->GetRelativeLocation();
                                    FRotator Rotation = SceneTemplate->GetRelativeRotation();
                                    FVector Scale = SceneTemplate->GetRelativeScale3D();
                                    // TransformObj is a pointer to a TSharedPtr<FJsonObject>.
                                    // Pass the TSharedPtr<FJsonObject> itself to the helper
                                    // which expects a const TSharedPtr<FJsonObject>&.
                                                            // TransformObj is a pointer to a TSharedPtr<FJsonObject>.
                                                            // Dereference the TSharedPtr to obtain a FJsonObject&.
                                                            ReadVectorField(*TransformObj, TEXT("location"), Location, Location);
                                                            ReadRotatorField(*TransformObj, TEXT("rotation"), Rotation, Rotation);
                                                            ReadVectorField(*TransformObj, TEXT("scale"), Scale, Scale);
                                    SceneTemplate->SetRelativeLocation(Location);
                                    SceneTemplate->SetRelativeRotation(Rotation);
                                    SceneTemplate->SetRelativeScale3D(Scale);
                                    OpSummary->SetBoolField(TEXT("success"), true);
                                    OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                }
                                else
                                {
                                    OpSummary->SetBoolField(TEXT("success"), false);
                                    OpSummary->SetStringField(TEXT("warning"), TEXT("Component not found or template missing"));
                                }
                            }
                        }
                        else if (NormalizedType == TEXT("add_component"))
                        {
                            FString ComponentName; Op->TryGetStringField(TEXT("componentName"), ComponentName);
                            FString ComponentClassPath; Op->TryGetStringField(TEXT("componentClass"), ComponentClassPath);
                            FString AttachToName; Op->TryGetStringField(TEXT("attachTo"), AttachToName);
                            FSoftClassPath ComponentClassSoftPath(ComponentClassPath);
                            UClass* ComponentClass = ComponentClassSoftPath.TryLoadClass<UActorComponent>();
                            if (!ComponentClass) ComponentClass = FindObject<UClass>(nullptr, *ComponentClassPath);
                            if (!ComponentClass)
                            {
                                const TArray<FString> Prefixes = { TEXT("/Script/Engine."), TEXT("/Script/UMG."), TEXT("/Script/Paper2D.") };
                                for (const FString& Prefix : Prefixes)
                                {
                                    const FString Guess = Prefix + ComponentClassPath;
                                    UClass* TryClass = FindObject<UClass>(nullptr, *Guess);
                                    if (!TryClass) TryClass = StaticLoadClass(UActorComponent::StaticClass(), nullptr, *Guess);
                                    if (TryClass) { ComponentClass = TryClass; break; }
                                }
                            }
                            if (!ComponentClass)
                            {
                                OpSummary->SetBoolField(TEXT("success"), false);
                                OpSummary->SetStringField(TEXT("warning"), TEXT("Component class not found"));
                            }
                            else
                            {
                                USCS_Node* ExistingNode = FindScsNodeByName(LocalSCS, ComponentName);
                                if (ExistingNode)
                                {
                                    OpSummary->SetBoolField(TEXT("success"), true);
                                    OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                    OpSummary->SetStringField(TEXT("warning"), TEXT("Component already exists"));
                                }
                                else
                                {
                                    USCS_Node* NewNode = LocalSCS->CreateNode(ComponentClass, *ComponentName);
                                    if (NewNode)
                                    {
                                        if (!AttachToName.TrimStartAndEnd().IsEmpty())
                                        {
                                            if (USCS_Node* ParentNode = FindScsNodeByName(LocalSCS, AttachToName))
                                            {
                                                                                               ParentNode->AddChildNode(NewNode);
                                            }
                                            else
                                            {
                                                LocalSCS->AddNode(NewNode);
                                            }
                                        }
                                        else
                                        {
                                            LocalSCS->AddNode(NewNode);
                                        }
                                        OpSummary->SetBoolField(TEXT("success"), true);
                                        OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                    }
                                    else
                                    {
                                        OpSummary->SetBoolField(TEXT("success"), false);
                                        OpSummary->SetStringField(TEXT("warning"), TEXT("Failed to create SCS node"));
                                    }
                                }
                            }
                        }
                        else if (NormalizedType == TEXT("remove_component"))
                        {
                            FString ComponentName; Op->TryGetStringField(TEXT("componentName"), ComponentName);
                            if (USCS_Node* TargetNode = FindScsNodeByName(LocalSCS, ComponentName))
                            {
                                LocalSCS->RemoveNode(TargetNode);
                                OpSummary->SetBoolField(TEXT("success"), true);
                                OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                            }
                            else
                            {
                                OpSummary->SetBoolField(TEXT("success"), false);
                                OpSummary->SetStringField(TEXT("warning"), TEXT("Component not found; remove skipped"));
                            }
                        }
                        else if (NormalizedType == TEXT("attach_component"))
                        {
                            FString AttachComponentName; Op->TryGetStringField(TEXT("componentName"), AttachComponentName);
                            FString ParentName; Op->TryGetStringField(TEXT("parentComponent"), ParentName); if (ParentName.IsEmpty()) Op->TryGetStringField(TEXT("attachTo"), ParentName);
                            USCS_Node* ChildNode = FindScsNodeByName(LocalSCS, AttachComponentName);
                            USCS_Node* ParentNode = FindScsNodeByName(LocalSCS, ParentName);
                            if (ChildNode && ParentNode)
                            {
                                ParentNode->AddChildNode(ChildNode);
                                OpSummary->SetBoolField(TEXT("success"), true);
                                OpSummary->SetStringField(TEXT("componentName"), AttachComponentName);
                                OpSummary->SetStringField(TEXT("attachedTo"), ParentName);
                            }
                            else
                            {
                                OpSummary->SetBoolField(TEXT("success"), false);
                                OpSummary->SetStringField(TEXT("warning"), TEXT("Attach failed: child or parent not found"));
                            }
                        }
                        else
                        {
                            OpSummary->SetBoolField(TEXT("success"), false);
                            OpSummary->SetStringField(TEXT("warning"), TEXT("Unknown operation type"));
                        }

                        const double OpElapsedMs = (FPlatformTime::Seconds() - OpStart) * 1000.0;
                        OpSummary->SetNumberField(TEXT("durationMs"), OpElapsedMs);
                        FinalSummaries.Add(MakeShared<FJsonValueObject>(OpSummary));
                    }

                    bOk = FinalSummaries.Num() > 0;
                    CompletionResult->SetArrayField(TEXT("operations"), FinalSummaries);
                }
            }

                // Compile/save as requested
                bool bSaveResult = false;
                if (bSave && LocalBP)
                {
                    bSaveResult = UEditorAssetLibrary::SaveLoadedAsset(LocalBP);
                    if (!bSaveResult) LocalWarnings.Add(TEXT("Blueprint failed to save during deferred apply; check output log."));
                }
                if (bCompile && LocalBP)
                {
                    FKismetEditorUtilities::CompileBlueprint(LocalBP);
                }

                CompletionResult->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
                CompletionResult->SetBoolField(TEXT("compiled"), bCompile);
                CompletionResult->SetBoolField(TEXT("saved"), bSave && bSaveResult);
                if (LocalWarnings.Num() > 0)
                {
                    TArray<TSharedPtr<FJsonValue>> WVals;
                    for (const FString& W : LocalWarnings) WVals.Add(MakeShared<FJsonValueString>(W));
                    CompletionResult->SetArrayField(TEXT("warnings"), WVals);
                }

                // Broadcast completion and attempt to deliver final response
                TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>();
                Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
                Notify->SetStringField(TEXT("event"), TEXT("modify_scs_completed"));
                Notify->SetStringField(TEXT("requestId"), RequestId);
                Notify->SetObjectField(TEXT("result"), CompletionResult);
                SendControlMessage(Notify);

                // Try to send final automation_response to the original requester
                TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
                ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
                ResultPayload->SetArrayField(TEXT("operations"), FinalSummaries);
                ResultPayload->SetBoolField(TEXT("compiled"), bCompile);
                ResultPayload->SetBoolField(TEXT("saved"), bSave && bSaveResult);
                if (LocalWarnings.Num() > 0)
                {
                    TArray<TSharedPtr<FJsonValue>> WVals2;
                    WVals2.Reserve(LocalWarnings.Num());
                    for (const FString& W : LocalWarnings) WVals2.Add(MakeShared<FJsonValueString>(W));
                    ResultPayload->SetArrayField(TEXT("warnings"), WVals2);
                }

                const FString Message = FString::Printf(TEXT("Processed %d SCS operation(s)."), FinalSummaries.Num());
                SendAutomationResponse(RequestingSocket, RequestId, true, Message, ResultPayload, FString());

                // Release busy flag
                if (!this->CurrentBusyBlueprintKey.IsEmpty() && GBlueprintBusySet.Contains(this->CurrentBusyBlueprintKey))
                {
                    GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey);
                }
                this->bCurrentBlueprintBusyMarked = false;
                this->bCurrentBlueprintBusyScheduled = false;
                this->CurrentBusyBlueprintKey.Empty();
            });

            return;
}

// NOTE: A duplicate SendAutomationResponse() declaration was accidentally
// inserted here in a prior patch; it has been removed so the handler code
// that follows remains inside ProcessAutomationRequest's scope as intended.
    // Sequencer / LevelSequence actions: the plugin handles all sequence_*
    // actions internally so the server does not fallback to other plugins.
    if (Action.StartsWith(TEXT("sequence_"), ESearchCase::IgnoreCase))
    {
        // Ensure we have a payload object to read from without crashing
        TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();

        auto SerializeResponseAndSend = [&](bool bOk, const FString& Msg, const TSharedPtr<FJsonObject>& ResObj, const FString& ErrCode = FString()) {
            SendAutomationResponse(RequestingSocket, RequestId, bOk, Msg, ResObj, ErrCode);
        };

        const FString Lower = Action.ToLower();

        // Helper: resolve a sequence path provided in payload or use current
        auto ResolveSequencePath = [&]() -> FString {
            FString P;
            if (LocalPayload->TryGetStringField(TEXT("path"), P) && !P.IsEmpty()) return P;
            if (!GCurrentSequencePath.IsEmpty()) return GCurrentSequencePath;
            return FString();
        };

        // Ensure there is an entry for a sequence in the lightweight registry
        auto EnsureSequenceEntry = [&](const FString& SeqPath) -> TSharedPtr<FJsonObject> {
            if (SeqPath.IsEmpty()) return nullptr;
            TSharedPtr<FJsonObject>* Found = GSequenceRegistry.Find(SeqPath);
            if (Found && Found->IsValid()) return *Found;
            TSharedPtr<FJsonObject> NewObj = MakeShared<FJsonObject>();
            NewObj->SetStringField(TEXT("sequencePath"), SeqPath);
            NewObj->SetBoolField(TEXT("created"), false);
            NewObj->SetBoolField(TEXT("playing"), false);
            NewObj->SetNumberField(TEXT("playbackSpeed"), 1.0);
            NewObj->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());
            NewObj->SetArrayField(TEXT("bindings"), TArray<TSharedPtr<FJsonValue>>());
            GSequenceRegistry.Add(SeqPath, NewObj);
            return NewObj;
        };

        if (Lower.Equals(TEXT("sequence_create")))
        {
            FString Name; LocalPayload->TryGetStringField(TEXT("name"), Name);
            FString Path; LocalPayload->TryGetStringField(TEXT("path"), Path);
            if (Name.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_create requires name"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            if (Path.IsEmpty()) Path = TEXT("/Game/Cinematics");
            const FString FullPath = Path.EndsWith(TEXT("/")) ? FString::Printf(TEXT("%s%s"), *Path, *Name) : FString::Printf(TEXT("%s/%s"), *Path, *Name);

            // Create registry entry (in-memory only). This avoids falling back
            // to other plugins while still signalling to clients the sequence
            // exists for automation workflows. Actual on-disk creation may be
            // added later by plugin authors.
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(FullPath);
            if (!Entry.IsValid()) { SerializeResponseAndSend(false, TEXT("Failed to allocate sequence registry entry"), nullptr, TEXT("CREATE_FAILED")); return; }
            Entry->SetBoolField(TEXT("created"), true);
            Entry->SetStringField(TEXT("message"), FString::Printf(TEXT("Sequence created in registry: %s"), *FullPath));
            GCurrentSequencePath = FullPath;
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetStringField(TEXT("sequencePath"), FullPath);
            SerializeResponseAndSend(true, TEXT("Sequence created (in-memory)."), Resp, FString());
            return;
        }

        if (Lower.Equals(TEXT("sequence_open")))
        {
            FString Path; LocalPayload->TryGetStringField(TEXT("path"), Path);
            if (Path.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_open requires path"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            // Ensure entry exists in the registry
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(Path);
            if (!Entry.IsValid()) { SerializeResponseAndSend(false, TEXT("Failed to open sequence (registry)"), nullptr, TEXT("OPEN_FAILED")); return; }
            GCurrentSequencePath = Path;
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetStringField(TEXT("sequencePath"), Path);
            SerializeResponseAndSend(true, TEXT("Sequence opened (registry)."), Resp, FString());
            return;
        }

        if (Lower.Equals(TEXT("sequence_add_camera")))
        {
            const bool bSpawnable = LocalPayload->HasField(TEXT("spawnable")) ? LocalPayload->GetBoolField(TEXT("spawnable")) : true;
            FString SeqPath = ResolveSequencePath();
            if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            const FString Id = FGuid::NewGuid().ToString();
            TSharedPtr<FJsonObject> Bind = MakeShared<FJsonObject>();
            Bind->SetStringField(TEXT("id"), Id);
            Bind->SetStringField(TEXT("type"), TEXT("camera"));
            Bind->SetBoolField(TEXT("spawnable"), bSpawnable);
            TArray<TSharedPtr<FJsonValue>> Arr = Entry->HasField(TEXT("bindings")) ? Entry->GetArrayField(TEXT("bindings")) : TArray<TSharedPtr<FJsonValue>>();
            Arr.Add(MakeShared<FJsonValueObject>(Bind));
            Entry->SetArrayField(TEXT("bindings"), Arr);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetStringField(TEXT("cameraBindingId"), Id);
            Resp->SetStringField(TEXT("sequencePath"), SeqPath);
            SerializeResponseAndSend(true, TEXT("Camera binding added (registry)."), Resp, FString());
            return;
        }

        if (Lower.Equals(TEXT("sequence_add_actor")))
        {
            FString ActorName; LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);
            if (ActorName.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_add_actor requires actorName"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            FString SeqPath = ResolveSequencePath();
            if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            const FString Id = FGuid::NewGuid().ToString();
            TSharedPtr<FJsonObject> Bind = MakeShared<FJsonObject>();
            Bind->SetStringField(TEXT("id"), Id);
            Bind->SetStringField(TEXT("type"), TEXT("actor"));
            Bind->SetStringField(TEXT("actorName"), ActorName);
            TArray<TSharedPtr<FJsonValue>> Arr = Entry->HasField(TEXT("bindings")) ? Entry->GetArrayField(TEXT("bindings")) : TArray<TSharedPtr<FJsonValue>>();
            Arr.Add(MakeShared<FJsonValueObject>(Bind));
            Entry->SetArrayField(TEXT("bindings"), Arr);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetStringField(TEXT("bindingId"), Id);
            Resp->SetStringField(TEXT("sequencePath"), SeqPath);
            SerializeResponseAndSend(true, TEXT("Actor binding added (registry)."), Resp, FString());
            return;
        }

        if (Lower.Equals(TEXT("sequence_add_actors")))
        {
            const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
            if (!LocalPayload->TryGetArrayField(TEXT("actorNames"), Names) || !Names || Names->Num() == 0) { SerializeResponseAndSend(false, TEXT("sequence_add_actors requires actorNames array"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            FString SeqPath = ResolveSequencePath();
            if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            TArray<TSharedPtr<FJsonValue>> Arr = Entry->HasField(TEXT("bindings")) ? Entry->GetArrayField(TEXT("bindings")) : TArray<TSharedPtr<FJsonValue>>();
            TArray<FString> Added;
            for (const TSharedPtr<FJsonValue>& V : *Names) {
                if (!V.IsValid()) continue;
                FString Actor = V->AsString();
                if (Actor.IsEmpty()) continue;
                const FString Id = FGuid::NewGuid().ToString();
                TSharedPtr<FJsonObject> Bind = MakeShared<FJsonObject>();
                Bind->SetStringField(TEXT("id"), Id);
                Bind->SetStringField(TEXT("type"), TEXT("actor"));
                Bind->SetStringField(TEXT("actorName"), Actor);
                Arr.Add(MakeShared<FJsonValueObject>(Bind));
                Added.Add(Actor);
            }
            Entry->SetArrayField(TEXT("bindings"), Arr);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            TArray<TSharedPtr<FJsonValue>> AddedVals;
            for (const FString& A : Added) { AddedVals.Add(MakeShared<FJsonValueString>(A)); }
            Resp->SetArrayField(TEXT("actorsAdded"), AddedVals);
            SerializeResponseAndSend(true, TEXT("Actors added to sequence (registry)."), Resp, FString());
            return;
        }

        if (Lower.Equals(TEXT("sequence_remove_actors")))
        {
            const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
            if (!LocalPayload->TryGetArrayField(TEXT("actorNames"), Names) || !Names || Names->Num() == 0) { SerializeResponseAndSend(false, TEXT("sequence_remove_actors requires actorNames array"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            FString SeqPath = ResolveSequencePath();
            if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            TArray<TSharedPtr<FJsonValue>> Bindings = Entry->HasField(TEXT("bindings")) ? Entry->GetArrayField(TEXT("bindings")) : TArray<TSharedPtr<FJsonValue>>();
            TArray<TSharedPtr<FJsonValue>> NewBindings;
            TArray<FString> Removed;
            for (const TSharedPtr<FJsonValue>& V : Bindings) {
                if (!V.IsValid() || V->Type != EJson::Object) { NewBindings.Add(V); continue; }
                TSharedPtr<FJsonObject> Obj = V->AsObject();
                const FString ActorName = Obj->HasField(TEXT("actorName")) ? Obj->GetStringField(TEXT("actorName")) : FString();
                bool bShouldRemove = false;
                for (const TSharedPtr<FJsonValue>& R : *Names) { if (R.IsValid() && R->AsString().Equals(ActorName, ESearchCase::IgnoreCase)) { bShouldRemove = true; break; } }
                if (bShouldRemove) { Removed.Add(ActorName); continue; }
                NewBindings.Add(V);
            }
            Entry->SetArrayField(TEXT("bindings"), NewBindings);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            TArray<TSharedPtr<FJsonValue>> RemovedVals;
            for (const FString& S : Removed) RemovedVals.Add(MakeShared<FJsonValueString>(S));
            Resp->SetArrayField(TEXT("removedActors"), RemovedVals);
            SerializeResponseAndSend(true, TEXT("Actors removed (registry)."), Resp, FString());
            return;
        }

        if (Lower.Equals(TEXT("sequence_get_bindings")))
        {
            FString SeqPath = ResolveSequencePath();
            if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            TArray<TSharedPtr<FJsonValue>> Bindings = Entry->HasField(TEXT("bindings")) ? Entry->GetArrayField(TEXT("bindings")) : TArray<TSharedPtr<FJsonValue>>();
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetArrayField(TEXT("bindings"), Bindings);
            SerializeResponseAndSend(true, TEXT("Bindings retrieved (registry)."), Resp, FString());
            return;
        }

        if (Lower.Equals(TEXT("sequence_add_spawnable_from_class")))
        {
            FString ClassName; LocalPayload->TryGetStringField(TEXT("className"), ClassName);
            FString SeqPath = ResolveSequencePath();
            if (ClassName.IsEmpty()) { SerializeResponseAndSend(false, TEXT("className is required"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            const FString Id = FGuid::NewGuid().ToString();
            TSharedPtr<FJsonObject> Spawn = MakeShared<FJsonObject>();
            Spawn->SetStringField(TEXT("id"), Id);
            Spawn->SetStringField(TEXT("className"), ClassName);
            TArray<TSharedPtr<FJsonValue>> Spawnables = Entry->HasField(TEXT("spawnables")) ? Entry->GetArrayField(TEXT("spawnables")) : TArray<TSharedPtr<FJsonValue>>();
            Spawnables.Add(MakeShared<FJsonValueObject>(Spawn));
            Entry->SetArrayField(TEXT("spawnables"), Spawnables);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("spawnableId"), Id); Resp->SetStringField(TEXT("sequencePath"), SeqPath);
            SerializeResponseAndSend(true, TEXT("Spawnable created (registry)."), Resp, FString());
            return;
        }

        if (Lower.Equals(TEXT("sequence_play")))
        {
            FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            Entry->SetBoolField(TEXT("playing"), true);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("sequencePath"), SeqPath);
            SerializeResponseAndSend(true, TEXT("Sequence play requested (registry)."), Resp, FString()); return;
        }

        if (Lower.Equals(TEXT("sequence_pause")) || Lower.Equals(TEXT("sequence_stop")))
        {
            FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            Entry->SetBoolField(TEXT("playing"), false);
            if (Lower.Equals(TEXT("sequence_stop"))) Entry->SetNumberField(TEXT("position"), 0);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("sequencePath"), SeqPath);
            SerializeResponseAndSend(true, Lower.Equals(TEXT("sequence_pause")) ? TEXT("Sequence paused (registry).") : TEXT("Sequence stopped (registry)."), Resp, FString()); return;
        }

        if (Lower.Equals(TEXT("sequence_set_properties")))
        {
            FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            double frameRate = 0; double lengthInFrames = 0; double playbackStart = 0; double playbackEnd = 0;
            if (LocalPayload->HasField(TEXT("frameRate"))) { frameRate = LocalPayload->GetNumberField(TEXT("frameRate")); Entry->GetObjectField(TEXT("properties"))->SetNumberField(TEXT("frameRate"), frameRate); }
            if (LocalPayload->HasField(TEXT("lengthInFrames"))) { lengthInFrames = LocalPayload->GetNumberField(TEXT("lengthInFrames")); Entry->GetObjectField(TEXT("properties"))->SetNumberField(TEXT("lengthInFrames"), lengthInFrames); }
            if (LocalPayload->HasField(TEXT("playbackStart"))) { playbackStart = LocalPayload->GetNumberField(TEXT("playbackStart")); Entry->GetObjectField(TEXT("properties"))->SetNumberField(TEXT("playbackStart"), playbackStart); }
            if (LocalPayload->HasField(TEXT("playbackEnd"))) { playbackEnd = LocalPayload->GetNumberField(TEXT("playbackEnd")); Entry->GetObjectField(TEXT("properties"))->SetNumberField(TEXT("playbackEnd"), playbackEnd); }
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("sequencePath"), SeqPath); Resp->SetObjectField(TEXT("properties"), Entry->GetObjectField(TEXT("properties")));
            SerializeResponseAndSend(true, TEXT("Sequence properties updated (registry)."), Resp, FString()); return;
        }

        if (Lower.Equals(TEXT("sequence_get_properties")))
        {
            FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            TSharedPtr<FJsonObject> Props = Entry->HasField(TEXT("properties")) ? Entry->GetObjectField(TEXT("properties")) : MakeShared<FJsonObject>();
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetObjectField(TEXT("properties"), Props); Resp->SetStringField(TEXT("sequencePath"), SeqPath);
            SerializeResponseAndSend(true, TEXT("Sequence properties retrieved (registry)."), Resp, FString()); return;
        }

        if (Lower.Equals(TEXT("sequence_set_playback_speed")))
        {
            FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            double Speed = LocalPayload->HasField(TEXT("speed")) ? LocalPayload->GetNumberField(TEXT("speed")) : 1.0;
            TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
            Entry->SetNumberField(TEXT("playbackSpeed"), Speed);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetNumberField(TEXT("playbackSpeed"), Speed); Resp->SetStringField(TEXT("sequencePath"), SeqPath);
            SerializeResponseAndSend(true, TEXT("Playback speed updated (registry)."), Resp, FString()); return;
        }

        // Unknown sequence_* action - respond explicitly so server does not treat
        // this as an absent plugin feature or fallback candidate.
        SerializeResponseAndSend(false, FString::Printf(TEXT("Sequence action not implemented by plugin: %s"), *Action), nullptr, TEXT("NOT_IMPLEMENTED"));
        return;
    }

    // Blueprint-specific automation actions. These are implemented at the
    // plugin layer and will return explicit responses so the server never
    // silently falls back to other engine plugins.
    if (Action.StartsWith(TEXT("blueprint_"), ESearchCase::IgnoreCase))
    {
        TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
        const FString Lower = Action.ToLower();

        auto ResolveBlueprintRequestedPath = [&]() -> FString {
            FString Single;
            if (LocalPayload->TryGetStringField(TEXT("requestedPath"), Single) && !Single.IsEmpty()) return Single;
            const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
            if (LocalPayload->TryGetArrayField(TEXT("blueprintCandidates"), Arr) && Arr && Arr->Num() > 0) {
                for (const TSharedPtr<FJsonValue>& V : *Arr) { if (V.IsValid()) { FString Cand = V->AsString(); if (!Cand.IsEmpty()) return Cand; } }
            }
            if (LocalPayload->TryGetStringField(TEXT("blueprintPath"), Single) && !Single.IsEmpty()) return Single;
            if (LocalPayload->TryGetStringField(TEXT("name"), Single) && !Single.IsEmpty()) return Single;
            return FString();
        };

        // Lightweight registry for blueprint changes
        auto EnsureBlueprintEntry = [&](const FString& P) -> TSharedPtr<FJsonObject> {
            if (P.IsEmpty()) return nullptr;
            TSharedPtr<FJsonObject>* Found = GBlueprintRegistry.Find(P);
            if (Found && Found->IsValid()) return *Found;
            TSharedPtr<FJsonObject> NewObj = MakeShared<FJsonObject>();
            NewObj->SetStringField(TEXT("blueprintPath"), P);
            NewObj->SetArrayField(TEXT("variables"), TArray<TSharedPtr<FJsonValue>>() );
            NewObj->SetArrayField(TEXT("constructionScripts"), TArray<TSharedPtr<FJsonValue>>() );
            NewObj->SetObjectField(TEXT("defaults"), MakeShared<FJsonObject>());
            NewObj->SetObjectField(TEXT("metadata"), MakeShared<FJsonObject>());
            GBlueprintRegistry.Add(P, NewObj);
            return NewObj;
        };

        if (Lower.Equals(TEXT("blueprint_add_variable")))
        {
            FString Path = ResolveBlueprintRequestedPath();
            if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_variable requires a blueprint path (requestedPath or blueprintCandidates)."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return; }
            FString VarName; LocalPayload->TryGetStringField(TEXT("variableName"), VarName);
            FString VarType; LocalPayload->TryGetStringField(TEXT("variableType"), VarType);
            if (VarName.IsEmpty() || VarType.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("variableName and variableType are required."), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
            TArray<TSharedPtr<FJsonValue>> Vars = Entry->HasField(TEXT("variables")) ? Entry->GetArrayField(TEXT("variables")) : TArray<TSharedPtr<FJsonValue>>();
            TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>(); VarObj->SetStringField(TEXT("name"), VarName); VarObj->SetStringField(TEXT("type"), VarType);
            if (LocalPayload->HasField(TEXT("defaultValue"))) { VarObj->SetField(TEXT("defaultValue"), LocalPayload->TryGetField(TEXT("defaultValue")) ); }
            Vars.Add(MakeShared<FJsonValueObject>(VarObj)); Entry->SetArrayField(TEXT("variables"), Vars);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("variableName"), VarName); Resp->SetStringField(TEXT("blueprintPath"), Path);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable added to blueprint registry (plugin stub)."), Resp, FString()); return;
        }

        if (Lower.Equals(TEXT("blueprint_add_event")))
        {
            FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_event requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return; }
            FString EventType; LocalPayload->TryGetStringField(TEXT("eventType"), EventType); if (EventType.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("eventType required"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            FString CustomName; LocalPayload->TryGetStringField(TEXT("customEventName"), CustomName);
            TArray<TSharedPtr<FJsonValue>> ParamsArray = LocalPayload->HasField(TEXT("parameters")) ? LocalPayload->GetArrayField(TEXT("parameters")) : TArray<TSharedPtr<FJsonValue>>();
            // Record the requested event in the registry for deterministic test behaviour
            TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
            TArray<TSharedPtr<FJsonValue>> Events = Entry->HasField(TEXT("events")) ? Entry->GetArrayField(TEXT("events")) : TArray<TSharedPtr<FJsonValue>>();
            TSharedPtr<FJsonObject> Ev = MakeShared<FJsonObject>(); Ev->SetStringField(TEXT("type"), EventType); if (!CustomName.IsEmpty()) Ev->SetStringField(TEXT("name"), CustomName); Ev->SetArrayField(TEXT("params"), ParamsArray);
            Events.Add(MakeShared<FJsonValueObject>(Ev)); Entry->SetArrayField(TEXT("events"), Events);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("blueprintPath"), Path); Resp->SetStringField(TEXT("eventType"), EventType); if (!CustomName.IsEmpty()) Resp->SetStringField(TEXT("customEventName"), CustomName);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event recorded in blueprint registry (plugin stub)."), Resp, FString()); return;
        }

        if (Lower.Equals(TEXT("blueprint_add_function")))
        {
            FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_function requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return; }
            FString FunctionName; LocalPayload->TryGetStringField(TEXT("functionName"), FunctionName); if (FunctionName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("functionName required"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            const TArray<TSharedPtr<FJsonValue>> Inputs = LocalPayload->HasField(TEXT("inputs")) ? LocalPayload->GetArrayField(TEXT("inputs")) : TArray<TSharedPtr<FJsonValue>>();
            const TArray<TSharedPtr<FJsonValue>> Outputs = LocalPayload->HasField(TEXT("outputs")) ? LocalPayload->GetArrayField(TEXT("outputs")) : TArray<TSharedPtr<FJsonValue>>();
            TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
            TArray<TSharedPtr<FJsonValue>> Funcs = Entry->HasField(TEXT("functions")) ? Entry->GetArrayField(TEXT("functions")) : TArray<TSharedPtr<FJsonValue>>();
            TSharedPtr<FJsonObject> FObj = MakeShared<FJsonObject>(); FObj->SetStringField(TEXT("name"), FunctionName); FObj->SetArrayField(TEXT("inputs"), Inputs); FObj->SetArrayField(TEXT("outputs"), Outputs);
            Funcs.Add(MakeShared<FJsonValueObject>(FObj)); Entry->SetArrayField(TEXT("functions"), Funcs);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("blueprintPath"), Path); Resp->SetStringField(TEXT("functionName"), FunctionName);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Function recorded in blueprint registry (plugin stub)."), Resp, FString()); return;
        }

        if (Lower.Equals(TEXT("blueprint_set_variable_metadata")))
        {
            FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_set_variable_metadata requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return; }
            FString VarName; LocalPayload->TryGetStringField(TEXT("variableName"), VarName); if (VarName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("variableName required"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            const TSharedPtr<FJsonObject>* MetaObjPtr = nullptr; if (!LocalPayload->TryGetObjectField(TEXT("metadata"), MetaObjPtr) || !MetaObjPtr || !(*MetaObjPtr).IsValid()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("metadata object required"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
            // naive metadata storage at blueprint->metadata.variableName
            TSharedPtr<FJsonObject> MetadataRoot = Entry->GetObjectField(TEXT("metadata"));
            // Reuse the provided metadata object pointer for now
            MetadataRoot->SetObjectField(VarName, *MetaObjPtr);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("variableName"), VarName); Resp->SetStringField(TEXT("blueprintPath"), Path);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable metadata stored in plugin registry (stub)."), Resp, FString()); return;
        }

        if (Lower.Equals(TEXT("blueprint_add_construction_script")))
        {
            FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_construction_script requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return; }
            FString ScriptName; LocalPayload->TryGetStringField(TEXT("scriptName"), ScriptName); if (ScriptName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("scriptName required"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
            TArray<TSharedPtr<FJsonValue>> Scripts = Entry->HasField(TEXT("constructionScripts")) ? Entry->GetArrayField(TEXT("constructionScripts")) : TArray<TSharedPtr<FJsonValue>>(); Scripts.Add(MakeShared<FJsonValueString>(ScriptName)); Entry->SetArrayField(TEXT("constructionScripts"), Scripts);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("scriptName"), ScriptName); Resp->SetStringField(TEXT("blueprintPath"), Path);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Construction script recorded in plugin registry (stub)."), Resp, FString()); return;
        }

        if (Lower.Equals(TEXT("blueprint_set_default")))
        {
            FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_set_default requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return; }
            FString PropertyName; LocalPayload->TryGetStringField(TEXT("propertyName"), PropertyName); if (PropertyName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("propertyName required"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            const TSharedPtr<FJsonValue> Value = LocalPayload->TryGetField(TEXT("value")); if (!Value.IsValid()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("value required"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
            TSharedPtr<FJsonObject> Defaults = Entry->GetObjectField(TEXT("defaults")); Defaults->SetField(PropertyName, Value);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("blueprintPath"), Path); Resp->SetStringField(TEXT("propertyName"), PropertyName);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint default recorded in plugin registry (stub)."), Resp, FString()); return;
        }

        if (Lower.Equals(TEXT("blueprint_probe_subobject_handle")))
        {
            // Probe is editor-engine sensitive. Currently unimplemented in
            // this lightweight plugin; return explicit NOT_IMPLEMENTED so
            // the caller receives a clear error rather than a fallback.
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("SubobjectData handle probe is not implemented in MCP plugin."), nullptr, TEXT("NOT_IMPLEMENTED")); return;
        }

        if (Lower.Equals(TEXT("blueprint_compile")))
        {
            FString Req; LocalPayload->TryGetStringField(TEXT("requestedPath"), Req);
            bool bSaveAfter = false; if (LocalPayload->HasField(TEXT("saveAfterCompile"))) bSaveAfter = LocalPayload->GetBoolField(TEXT("saveAfterCompile"));
            if (Req.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_compile requires requestedPath"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            FString Normalized; FString LoadError; UBlueprint* BP = LoadBlueprintAsset(Req, Normalized, LoadError);
            if (!BP) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("blueprintPath"), Req); SendAutomationResponse(RequestingSocket, RequestId, false, LoadError, Err, TEXT("BLUEPRINT_NOT_FOUND")); return; }
            // Compile using kismet utilities
#if WITH_EDITOR
            FKismetEditorUtilities::CompileBlueprint(BP);
            if (bSaveAfter) { UEditorAssetLibrary::SaveLoadedAsset(BP); }
#endif
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("blueprintPath"), Normalized);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint compiled (plugin stub)."), Resp, FString()); return;
        }

        // Unknown blueprint_* action
        SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Blueprint action not implemented by plugin: %s"), *Action), nullptr, TEXT("NOT_IMPLEMENTED"));
        return;
        }

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown automation action: %s"), *Action), TEXT("UNKNOWN_ACTION"));
    }
    catch (const std::exception& E)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Unhandled exception processing automation request %s: %s"), *RequestId, ANSI_TO_TCHAR(E.what()));
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Internal error: %s"), ANSI_TO_TCHAR(E.what())), TEXT("INTERNAL_ERROR"));
    }
    catch (...)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Unhandled unknown exception processing automation request %s"), *RequestId);
        SendAutomationError(RequestingSocket, RequestId, TEXT("Internal error (unknown)."), TEXT("INTERNAL_ERROR"));
    }

    // Close the ProcessBody lambda, execute it, and return from the
    // ProcessAutomationRequest wrapper so subsequent functions are top-level.
    };
    ProcessBody();
    return;
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

    bool bSent = false;
    const int MaxAttempts = 3;
    for (int Attempt = 1; Attempt <= MaxAttempts && !bSent; ++Attempt)
    {
        // Try the original requesting socket first
        if (TargetSocket.IsValid() && TargetSocket->IsConnected())
        {
            bSent = TargetSocket->Send(Serialized);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Attempt %d: send automation_response RequestId=%s to requesting socket: %s (bytes=%d)"), Attempt, *RequestId, bSent ? TEXT("ok") : TEXT("failed"), Serialized.Len());
            if (bSent) break;
        }

        // Try mapping stored socket
        TSharedPtr<FMcpBridgeWebSocket>* Mapped = PendingRequestsToSockets.Find(RequestId);
        if (!bSent && Mapped && Mapped->IsValid() && (*Mapped)->IsConnected())
        {
            bSent = (*Mapped)->Send(Serialized);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Attempt %d: send automation_response RequestId=%s to mapped socket: %s (bytes=%d)"), Attempt, *RequestId, bSent ? TEXT("ok") : TEXT("failed"), Serialized.Len());
            if (bSent) break;
        }

        // Try other active sockets as best-effort
        if (!bSent)
        {
            for (const TSharedPtr<FMcpBridgeWebSocket>& Sock : ActiveSockets)
            {
                if (!Sock.IsValid() || !Sock->IsConnected()) continue;
                // Skip already attempted sockets
                if (Sock == TargetSocket) continue;
                if (Mapped && *Mapped == Sock) continue;
                if (Sock->Send(Serialized))
                {
                    bSent = true;
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Attempt %d: sent automation_response RequestId=%s via alternate socket (bytes=%d)."), Attempt, *RequestId, Serialized.Len());
                    break;
                }
            }
        }

        // If not sent and not last attempt, try again on next iteration.
        if (!bSent && Attempt < MaxAttempts)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Attempt %d failed to deliver automation_response for RequestId=%s; retrying..."), Attempt, *RequestId);
        }
    }

    if (!bSent)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Failed to deliver automation_response for RequestId=%s to any connected socket (activeSockets=%d). Payload: %s"), *RequestId, ActiveSockets.Num(), *Serialized);
    }

    // Clean up the request tracking regardless to avoid stale mappings
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

