#pragma once

#include "Misc/OutputDevice.h"

class UMcpAutomationBridgeSubsystem;

class FMcpRequestErrorDevice : public FOutputDevice
{
public:
    explicit FMcpRequestErrorDevice(UMcpAutomationBridgeSubsystem* InSubsystem);

    void Serialize(
        const TCHAR* V,
        ELogVerbosity::Type Verbosity,
        const FName& Category) override;
    bool CanBeUsedOnAnyThread() const override;
    bool CanBeUsedOnMultipleThreads() const override;

private:
    UMcpAutomationBridgeSubsystem* Subsystem = nullptr;
};
