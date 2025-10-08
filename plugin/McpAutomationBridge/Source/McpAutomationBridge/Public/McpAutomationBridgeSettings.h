#pragma once

#include "Engine/DeveloperSettings.h"
#include "McpAutomationBridgeSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, defaultconfig)
class MCPAUTOMATIONBRIDGE_API UMcpAutomationBridgeSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UMcpAutomationBridgeSettings();

    UPROPERTY(config, EditAnywhere, Category = "Connection")
    FString EndpointUrl;

    UPROPERTY(config, EditAnywhere, Category = "Security")
    FString CapabilityToken;

    UPROPERTY(config, EditAnywhere, Category = "Connection", meta = (ClampMin = "0.0"))
    float AutoReconnectDelay;

    virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
    virtual FText GetSectionText() const override;
};
