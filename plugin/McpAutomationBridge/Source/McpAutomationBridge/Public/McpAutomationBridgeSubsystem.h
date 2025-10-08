#pragma once

#include "Containers/Ticker.h"
#include "EditorSubsystem.h"
#include "Templates/SharedPointer.h"
#include "McpAutomationBridgeSubsystem.generated.h"

UENUM(BlueprintType)
enum class EMcpAutomationBridgeState : uint8
{
    Disconnected,
    Connecting,
    Connected
};

/** Minimal payload wrapper for incoming automation messages. */
USTRUCT(BlueprintType)
struct MCPAUTOMATIONBRIDGE_API FMcpAutomationMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MCP Automation")
    FString Type;

    UPROPERTY(BlueprintReadOnly, Category = "MCP Automation")
    FString PayloadJson;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMcpAutomationMessageReceived, const FMcpAutomationMessage&, Message);

class FMcpBridgeWebSocket;
class FJsonObject;

UCLASS()
class MCPAUTOMATIONBRIDGE_API UMcpAutomationBridgeSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "MCP Automation")
    bool IsBridgeActive() const { return bBridgeAvailable; }

    UFUNCTION(BlueprintCallable, Category = "MCP Automation")
    EMcpAutomationBridgeState GetBridgeState() const { return BridgeState; }

    UFUNCTION(BlueprintCallable, Category = "MCP Automation")
    bool SendRawMessage(const FString& Message);

    UPROPERTY(BlueprintAssignable, Category = "MCP Automation")
    FMcpAutomationMessageReceived OnMessageReceived;

private:
    bool Tick(float DeltaTime);

    void AttemptConnection();
    void HandleConnected();
    void HandleConnectionError(const FString& Error);
    void HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
    void HandleMessage(const FString& Message);
    void ProcessAutomationRequest(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload);
    void SendAutomationResponse(const FString& RequestId, bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Result = nullptr, const FString& ErrorCode = FString());
    void SendAutomationError(const FString& RequestId, const FString& Message, const FString& ErrorCode);

    void StartBridge();
    void StopBridge();

    bool bBridgeAvailable = false;
    EMcpAutomationBridgeState BridgeState = EMcpAutomationBridgeState::Disconnected;
    FTSTicker::FDelegateHandle TickerHandle;
    TSharedPtr<FMcpBridgeWebSocket> ActiveSocket;
    float TimeUntilReconnect = 0.0f;
    float AutoReconnectDelaySeconds = 5.0f;
    FString CapabilityToken;
    FString EndpointUrl;
    bool bReconnectEnabled = true;
};
