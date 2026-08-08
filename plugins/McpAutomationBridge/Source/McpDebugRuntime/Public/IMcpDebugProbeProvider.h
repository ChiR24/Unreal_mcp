#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

class FJsonObject;

class MCPDEBUGRUNTIME_API IMcpDebugProbeProvider : public IModularFeature
{
public:
    static FName GetModularFeatureName()
    {
        static const FName FeatureName(TEXT("McpDebugProbeProvider"));
        return FeatureName;
    }

    virtual ~IMcpDebugProbeProvider() = default;

    virtual FName GetProbeName() const = 0;
    virtual int32 GetSchemaVersion() const = 0;
    virtual double GetSimulationTimeSeconds() const = 0;
    virtual bool CaptureSnapshot(TSharedRef<FJsonObject> OutSnapshot) const = 0;
};
