#pragma once

#include "Misc/OutputDevice.h"

class UEngineWireSubsystem;

class FMcpRequestErrorDevice : public FOutputDevice
{
public:
    explicit FMcpRequestErrorDevice(UEngineWireSubsystem* InSubsystem);

    void Serialize(
        const TCHAR* V,
        ELogVerbosity::Type Verbosity,
        const FName& Category) override;
    bool CanBeUsedOnAnyThread() const override;
    bool CanBeUsedOnMultipleThreads() const override;

private:
    UEngineWireSubsystem* Subsystem = nullptr;
};
