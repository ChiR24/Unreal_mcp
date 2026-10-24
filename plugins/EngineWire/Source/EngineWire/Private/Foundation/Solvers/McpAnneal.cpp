// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpAnneal.h"

#include "HAL/PlatformTime.h"

namespace McpAnneal
{
FOutcome Minimize(double StartEnergy, const FSettings& Settings, TFunctionRef<double(FRandomStream&, double)> Propose,
                  TFunctionRef<void()> Revert, TFunctionRef<void()> KeepBest)
{
    FRandomStream Random(Settings.Seed);
    FOutcome Out;
    Out.BestEnergy = StartEnergy;
    KeepBest();
    double Energy = StartEnergy;
    const double Ratio = Settings.EndTemperature / Settings.StartTemperature;
    const double Started = FPlatformTime::Seconds();
    for (; Out.StepsRun < Settings.Steps; ++Out.StepsRun)
    {
        if ((Out.StepsRun & 255) == 0 && FPlatformTime::Seconds() - Started > Settings.DeadlineSeconds)
        {
            Out.bDeadline = true;
            break;
        }
        const double Progress = double(Out.StepsRun) / double(FMath::Max(Settings.Steps - 1, 1));
        const double Temperature = Settings.StartTemperature * FMath::Pow(Ratio, Progress);
        const double Delta = Propose(Random, Progress);
        if (Delta > 0.0 && Random.GetFraction() >= FMath::Exp(-Delta / Temperature))
        {
            Revert();
            continue;
        }
        Energy += Delta;
        ++Out.Accepted;
        if (Energy < Out.BestEnergy - 1e-12)
        {
            Out.BestEnergy = Energy;
            KeepBest();
        }
    }
    return Out;
}
} // namespace McpAnneal
