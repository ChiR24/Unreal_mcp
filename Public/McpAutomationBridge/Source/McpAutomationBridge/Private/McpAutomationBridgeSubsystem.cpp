#include "McpAutomationBridgeSubsystem.h"

#include "McpBridgeWebSocket.h"
#include "IPythonScriptPlugin.h"
#include "McpAutomationBridgeSettings.h"
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
#include "ScopedTransaction.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/SceneComponent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpAutomationBridgeSubsystem, Log, All);

namespace
{
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
    EndpointUrl = Settings->EndpointUrl;
    CapabilityToken = Settings->CapabilityToken;
    AutoReconnectDelaySeconds = FMath::Max(Settings->AutoReconnectDelay, 0.0f);
    bReconnectEnabled = AutoReconnectDelaySeconds > 0.0f;
    TimeUntilReconnect = 0.0f;
    StartBridge();
}

void UMcpAutomationBridgeSubsystem::Deinitialize()
{
    StopBridge();
    Super::Deinitialize();
}

bool UMcpAutomationBridgeSubsystem::SendRawMessage(const FString& Message)
{
    if (!ActiveSocket.IsValid() || !ActiveSocket->IsConnected())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Bridge socket not connected; message dropped."));
        return false;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Outbound automation message: %s"), *Message);
    ActiveSocket->Send(Message);
    return true;
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

    if (!ActiveSocket.IsValid() && BridgeState == EMcpAutomationBridgeState::Connecting)
    {
        BridgeState = EMcpAutomationBridgeState::Disconnected;
    }

    return true;
}

void UMcpAutomationBridgeSubsystem::AttemptConnection()
{
    if (!bBridgeAvailable)
    {
        return;
    }

    if (EndpointUrl.IsEmpty())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation bridge endpoint is empty; skipping connection."));
        BridgeState = EMcpAutomationBridgeState::Disconnected;
        bReconnectEnabled = false;
        return;
    }

    if (ActiveSocket.IsValid())
    {
        ActiveSocket->OnConnected().RemoveAll(this);
        ActiveSocket->OnConnectionError().RemoveAll(this);
        ActiveSocket->OnClosed().RemoveAll(this);
        ActiveSocket->OnMessage().RemoveAll(this);
        ActiveSocket->Close();
        ActiveSocket.Reset();
    }

    TMap<FString, FString> Headers;
    if (!CapabilityToken.IsEmpty())
    {
        Headers.Add(TEXT("X-MCP-Capability"), CapabilityToken);
    }

    ActiveSocket = MakeShared<FMcpBridgeWebSocket>(EndpointUrl, TEXT(""), Headers);

    if (!ActiveSocket.IsValid())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Failed to create WebSocket for endpoint %s"), *EndpointUrl);
        BridgeState = EMcpAutomationBridgeState::Disconnected;
        TimeUntilReconnect = AutoReconnectDelaySeconds;
        return;
    }

    ActiveSocket->OnConnected().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleConnected);
    ActiveSocket->OnConnectionError().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleConnectionError);
    ActiveSocket->OnClosed().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleClosed);
    ActiveSocket->OnMessage().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleMessage);

    BridgeState = EMcpAutomationBridgeState::Connecting;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Connecting to MCP automation endpoint %s"), *EndpointUrl);
    ActiveSocket->Connect();
}

void UMcpAutomationBridgeSubsystem::HandleConnected()
{
    BridgeState = EMcpAutomationBridgeState::Connected;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("MCP automation bridge connected."));

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

    if (ActiveSocket.IsValid())
    {
        ActiveSocket->Send(HelloPayload);
    }

    FMcpAutomationMessage Handshake;
    Handshake.Type = TEXT("bridge_started");
    Handshake.PayloadJson = TEXT("{}");
    OnMessageReceived.Broadcast(Handshake);
}

void UMcpAutomationBridgeSubsystem::HandleConnectionError(const FString& Error)
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
    if (ActiveSocket.IsValid())
    {
        ActiveSocket->OnConnected().RemoveAll(this);
        ActiveSocket->OnConnectionError().RemoveAll(this);
        ActiveSocket->OnClosed().RemoveAll(this);
        ActiveSocket->OnMessage().RemoveAll(this);
        ActiveSocket.Reset();
    }
}

void UMcpAutomationBridgeSubsystem::HandleClosed(const int32 StatusCode, const FString& Reason, const bool bWasClean)
{
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation bridge closed (code %d, clean=%s): %s"), StatusCode, bWasClean ? TEXT("true") : TEXT("false"), *Reason);
    BridgeState = EMcpAutomationBridgeState::Disconnected;
    TimeUntilReconnect = AutoReconnectDelaySeconds;
    if (ActiveSocket.IsValid())
    {
        ActiveSocket->OnConnected().RemoveAll(this);
        ActiveSocket->OnConnectionError().RemoveAll(this);
        ActiveSocket->OnClosed().RemoveAll(this);
        ActiveSocket->OnMessage().RemoveAll(this);
        ActiveSocket.Reset();
    }
}

void UMcpAutomationBridgeSubsystem::HandleMessage(const FString& Message)
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
                    SendAutomationError(RequestId, TEXT("Automation request missing action."), TEXT("INVALID_ACTION"));
                }
                else
                {
                    TSharedPtr<FJsonObject> Payload;
                    if (JsonObject->HasTypedField<EJson::Object>(TEXT("payload")))
                    {
                        Payload = JsonObject->GetObjectField(TEXT("payload"));
                    }
                    ProcessAutomationRequest(RequestId, Action, Payload);
                }
            }
            return;
        }

        if (Parsed.Type.Equals(TEXT("bridge_ack")))
        {
            FString ServerVersion;
            JsonObject->TryGetStringField(TEXT("serverVersion"), ServerVersion);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Automation bridge handshake acknowledged (server version: %s)"), ServerVersion.IsEmpty() ? TEXT("unknown") : *ServerVersion);
        }
        else if (Parsed.Type.Equals(TEXT("bridge_error")))
        {
            FString ErrorCode;
            JsonObject->TryGetStringField(TEXT("error"), ErrorCode);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Automation bridge reported error: %s"), ErrorCode.IsEmpty() ? TEXT("UNKNOWN_ERROR") : *ErrorCode);
        }
    }

    OnMessageReceived.Broadcast(Parsed);
}

void UMcpAutomationBridgeSubsystem::ProcessAutomationRequest(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload)
{
    if (Action.Equals(TEXT("execute_editor_python"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestId, TEXT("execute_editor_python payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        FString Script;
        if (!Payload->TryGetStringField(TEXT("script"), Script) || Script.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestId, TEXT("execute_editor_python requires a non-empty script."), TEXT("INVALID_ARGUMENT"));
            return;
        }

        if (!FModuleManager::Get().IsModuleLoaded(TEXT("PythonScriptPlugin")))
        {
            FModuleManager::LoadModulePtr<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
        }

        IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
        if (!PythonPlugin)
        {
            SendAutomationError(RequestId, TEXT("PythonScriptPlugin is not available. Enable the Python Editor Script Plugin."), TEXT("PYTHON_PLUGIN_DISABLED"));
            return;
        }

        const bool bSuccess = PythonPlugin->ExecPythonCommand(*Script);
        const FString ResultMessage = bSuccess
            ? TEXT("Python script executed via MCP Automation Bridge.")
            : TEXT("Python script executed but returned false.");

        SendAutomationResponse(RequestId, bSuccess, ResultMessage, nullptr, bSuccess ? FString() : TEXT("PYTHON_EXEC_FAILED"));
        return;
    }

    if (Action.Equals(TEXT("set_object_property"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestId, TEXT("set_object_property payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        FString ObjectPath;
        if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) || ObjectPath.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestId, TEXT("set_object_property requires a non-empty objectPath."), TEXT("INVALID_OBJECT"));
            return;
        }

        FString PropertyName;
        if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) || PropertyName.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestId, TEXT("set_object_property requires a non-empty propertyName."), TEXT("INVALID_PROPERTY"));
            return;
        }

        const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
        if (!ValueField.IsValid())
        {
            SendAutomationError(RequestId, TEXT("set_object_property payload missing value field."), TEXT("INVALID_VALUE"));
            return;
        }

        UObject* TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
        if (!TargetObject)
        {
            SendAutomationError(RequestId, FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath), TEXT("OBJECT_NOT_FOUND"));
            return;
        }

        FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
        if (!Property)
        {
            SendAutomationError(RequestId, FString::Printf(TEXT("Property %s not found on object %s."), *PropertyName, *ObjectPath), TEXT("PROPERTY_NOT_FOUND"));
            return;
        }

        FString ConversionError;

#if WITH_EDITOR
        TargetObject->Modify();
#endif

        if (!ApplyJsonValueToProperty(TargetObject, Property, ValueField, ConversionError))
        {
            SendAutomationError(RequestId, ConversionError, TEXT("PROPERTY_CONVERSION_FAILED"));
            return;
        }

        bool bMarkDirty = true;
        if (Payload->HasField(TEXT("markDirty")))
        {
            if (!Payload->TryGetBoolField(TEXT("markDirty"), bMarkDirty))
            {
                SendAutomationError(RequestId, TEXT("markDirty must be a boolean."), TEXT("INVALID_MARK_DIRTY"));
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

        SendAutomationResponse(RequestId, true, TEXT("Property value updated."), ResultPayload, FString());
        return;
    }

    if (Action.Equals(TEXT("get_object_property"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestId, TEXT("get_object_property payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        FString ObjectPath;
        if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) || ObjectPath.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestId, TEXT("get_object_property requires a non-empty objectPath."), TEXT("INVALID_OBJECT"));
            return;
        }

        FString PropertyName;
        if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) || PropertyName.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestId, TEXT("get_object_property requires a non-empty propertyName."), TEXT("INVALID_PROPERTY"));
            return;
        }

        UObject* TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
        if (!TargetObject)
        {
            SendAutomationError(RequestId, FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath), TEXT("OBJECT_NOT_FOUND"));
            return;
        }

        FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
        if (!Property)
        {
            SendAutomationError(RequestId, FString::Printf(TEXT("Property %s not found on object %s."), *PropertyName, *ObjectPath), TEXT("PROPERTY_NOT_FOUND"));
            return;
        }

        const TSharedPtr<FJsonValue> CurrentValue = ExportPropertyToJsonValue(TargetObject, Property);
        if (!CurrentValue.IsValid())
        {
            SendAutomationError(RequestId, FString::Printf(TEXT("Unable to export property %s."), *PropertyName), TEXT("PROPERTY_EXPORT_FAILED"));
            return;
        }

        TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
        ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
        ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
        ResultPayload->SetField(TEXT("value"), CurrentValue);

        SendAutomationResponse(RequestId, true, TEXT("Property value retrieved."), ResultPayload, FString());
        return;
    }

    if (Action.Equals(TEXT("blueprint_modify_scs"), ESearchCase::IgnoreCase))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestId, TEXT("blueprint_modify_scs payload missing."), TEXT("INVALID_PAYLOAD"));
            return;
        }

        FString BlueprintPath;
        if (!Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) || BlueprintPath.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestId, TEXT("blueprint_modify_scs requires a non-empty blueprintPath."), TEXT("INVALID_BLUEPRINT"));
            return;
        }

        const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("operations"), OperationsArray) || OperationsArray == nullptr)
        {
            SendAutomationError(RequestId, TEXT("blueprint_modify_scs requires an operations array."), TEXT("INVALID_OPERATIONS"));
            return;
        }

        FString NormalizedBlueprintPath;
        FString LoadError;
        UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedBlueprintPath, LoadError);
        if (!Blueprint)
        {
            SendAutomationError(RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
            return;
        }

        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            SendAutomationError(RequestId, TEXT("Blueprint does not expose a SimpleConstructionScript."), TEXT("SCS_UNAVAILABLE"));
            return;
        }

        bool bCompile = false;
        if (Payload->HasField(TEXT("compile")))
        {
            if (!Payload->TryGetBoolField(TEXT("compile"), bCompile))
            {
                SendAutomationError(RequestId, TEXT("compile must be a boolean."), TEXT("INVALID_COMPILE_FLAG"));
                return;
            }
        }

        bool bSave = false;
        if (Payload->HasField(TEXT("save")))
        {
            if (!Payload->TryGetBoolField(TEXT("save"), bSave))
            {
                SendAutomationError(RequestId, TEXT("save must be a boolean."), TEXT("INVALID_SAVE_FLAG"));
                return;
            }
        }

        if (OperationsArray->Num() == 0)
        {
            TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
            ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
            ResultPayload->SetArrayField(TEXT("operations"), TArray<TSharedPtr<FJsonValue>>());
            SendAutomationResponse(RequestId, true, TEXT("No SCS operations supplied."), ResultPayload, FString());
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
                SendAutomationError(RequestId, FString::Printf(TEXT("Operation at index %d is not an object."), Index), TEXT("INVALID_OPERATION_PAYLOAD"));
                return;
            }

            const TSharedPtr<FJsonObject> OperationObject = OperationValue->AsObject();
            FString OperationType;
            if (!OperationObject->TryGetStringField(TEXT("type"), OperationType) || OperationType.TrimStartAndEnd().IsEmpty())
            {
                SendAutomationError(RequestId, FString::Printf(TEXT("Operation at index %d missing type."), Index), TEXT("INVALID_OPERATION_TYPE"));
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
                    SendAutomationError(RequestId, FString::Printf(TEXT("add_component operation at index %d missing componentName."), Index), TEXT("INVALID_COMPONENT_NAME"));
                    return;
                }

                FString ComponentClassPath;
                if (!OperationObject->TryGetStringField(TEXT("componentClass"), ComponentClassPath) || ComponentClassPath.TrimStartAndEnd().IsEmpty())
                {
                    SendAutomationError(RequestId, FString::Printf(TEXT("add_component operation at index %d missing componentClass."), Index), TEXT("INVALID_COMPONENT_CLASS"));
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
                if (!ComponentClass)
                {
                    SendAutomationError(RequestId, FString::Printf(TEXT("Unable to load component class %s."), *ComponentClassPath), TEXT("COMPONENT_CLASS_NOT_FOUND"));
                    return;
                }

                if (!ComponentClass->IsChildOf(UActorComponent::StaticClass()))
                {
                    SendAutomationError(RequestId, FString::Printf(TEXT("Class %s is not a component."), *ComponentClassPath), TEXT("INVALID_COMPONENT_CLASS"));
                    return;
                }

                if (FindScsNodeByName(SCS, ComponentName))
                {
                    SendAutomationError(RequestId, FString::Printf(TEXT("Component %s already exists on Blueprint."), *ComponentName), TEXT("COMPONENT_ALREADY_EXISTS"));
                    return;
                }

                USCS_Node* NewNode = SCS->CreateNode(ComponentClass, *ComponentName);
                if (!NewNode)
                {
                    SendAutomationError(RequestId, FString::Printf(TEXT("Failed to create SCS node for %s."), *ComponentName), TEXT("NODE_CREATION_FAILED"));
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
                            SendAutomationError(RequestId, PropertyError, TEXT("COMPONENT_PROPERTY_FAILED"));
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
                    SendAutomationError(RequestId, FString::Printf(TEXT("remove_component operation at index %d missing componentName."), Index), TEXT("INVALID_COMPONENT_NAME"));
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
            else if (NormalizedType == TEXT("set_component_properties"))
            {
                FString ComponentName;
                if (!OperationObject->TryGetStringField(TEXT("componentName"), ComponentName) || ComponentName.TrimStartAndEnd().IsEmpty())
                {
                    SendAutomationError(RequestId, FString::Printf(TEXT("set_component_properties operation at index %d missing componentName."), Index), TEXT("INVALID_COMPONENT_NAME"));
                    return;
                }

                const TSharedPtr<FJsonObject>* PropertyOverrides = nullptr;
                if (!OperationObject->TryGetObjectField(TEXT("properties"), PropertyOverrides) || PropertyOverrides == nullptr || !PropertyOverrides->IsValid())
                {
                    SendAutomationError(RequestId, FString::Printf(TEXT("set_component_properties operation at index %d missing properties object."), Index), TEXT("INVALID_PROPERTIES"));
                    return;
                }

                if (USCS_Node* TargetNode = FindScsNodeByName(SCS, ComponentName))
                {
                    UActorComponent* Template = TargetNode->ComponentTemplate;
                    if (!Template)
                    {
                        SendAutomationError(RequestId, FString::Printf(TEXT("Component %s has no template for property assignment."), *ComponentName), TEXT("COMPONENT_TEMPLATE_MISSING"));
                        return;
                    }

                    FString PropertyError;
                    if (!ApplyPropertyOverrides(Template, *PropertyOverrides, AccumulatedWarnings, PropertyError))
                    {
                        SendAutomationError(RequestId, PropertyError, TEXT("COMPONENT_PROPERTY_FAILED"));
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
                    SendAutomationError(RequestId, FString::Printf(TEXT("Component %s not found for property assignment."), *ComponentName), TEXT("COMPONENT_NOT_FOUND"));
                    return;
                }
            }
            else
            {
                SendAutomationError(RequestId, FString::Printf(TEXT("Unknown SCS operation type: %s"), *OperationType), TEXT("UNKNOWN_OPERATION"));
                return;
            }

            OperationSummaries.Add(MakeShared<FJsonValueObject>(OperationSummary));
        }

        if (bAnyChanges)
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        }

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

        const FString Message = FString::Printf(TEXT("Processed %d SCS operation(s)."), OperationSummaries.Num());
        SendAutomationResponse(RequestId, true, Message, ResultPayload, FString());
        return;
    }

    SendAutomationError(RequestId, FString::Printf(TEXT("Unknown automation action: %s"), *Action), TEXT("UNKNOWN_ACTION"));
}

void UMcpAutomationBridgeSubsystem::SendAutomationResponse(const FString& RequestId, const bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode)
{
    if (!ActiveSocket.IsValid() || !ActiveSocket->IsConnected())
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

    ActiveSocket->Send(Serialized);
}

void UMcpAutomationBridgeSubsystem::SendAutomationError(const FString& RequestId, const FString& Message, const FString& ErrorCode)
{
    const FString ResolvedError = ErrorCode.IsEmpty() ? TEXT("AUTOMATION_ERROR") : ErrorCode;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation request failed (%s): %s"), *ResolvedError, *Message);
    SendAutomationResponse(RequestId, false, Message, nullptr, ResolvedError);
}

void UMcpAutomationBridgeSubsystem::StartBridge()
{
    if (!TickerHandle.IsValid())
    {
        const FTickerDelegate TickDelegate = FTickerDelegate::CreateUObject(this, &UMcpAutomationBridgeSubsystem::Tick);
        TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate, 0.25f);
    }

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

    if (ActiveSocket.IsValid())
    {
        ActiveSocket->OnConnected().RemoveAll(this);
        ActiveSocket->OnConnectionError().RemoveAll(this);
        ActiveSocket->OnClosed().RemoveAll(this);
        ActiveSocket->OnMessage().RemoveAll(this);
        ActiveSocket->Close();
        ActiveSocket.Reset();
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Automation bridge stopped."));
}
