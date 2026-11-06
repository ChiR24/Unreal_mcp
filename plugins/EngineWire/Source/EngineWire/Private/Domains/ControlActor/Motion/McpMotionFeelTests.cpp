// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Motion/McpMotionFeel.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpMotionFeelTest, "EngineWire.ControlActor.MotionFeel",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers, from the physics of a synthetic run sampled at 60 frames a second: the jump leaves at 0.1 s at 420 cm/s
// under 980 cm/s2 (apex 0.4286 s later and 90 cm up, down again 0.857 s after leaving); the run speeds up at 2000 cm/s2
// from 0.1 s to 600 cm/s (90% after 0.27 s), its key goes up at 1.2 s and it stops in 0.3 s; both keys went down one
// frame before the motion, which shows on the frame after it: 2 frames. Each sampled answer is within a frame of the
// exact one. A pawn standing still has no jump, no run and keys that moved nothing.
bool FMcpMotionFeelTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const double Dt = 1.0 / 60.0;
    TArray<double> Times;
    TArray<FVector> Points, Cameras, Still;
    double X = 0.0, Speed = 0.0;
    for (int32 Frame = 0; Frame < 120; ++Frame)
    {
        const double Time = Frame * Dt;
        const double Air = Time - 0.1;
        const double Z = Air > 0.0 && Air < 0.857 ? 420.0 * Air - 490.0 * Air * Air : 0.0;
        if (Frame > 0)
        {
            Speed = Time > 0.1 && Time <= 1.2 ? FMath::Min(600.0, Speed + 2000.0 * Dt) : FMath::Max(0.0, Speed - 2000.0 * Dt);
            X += Speed * Dt;
        }
        Times.Add(Time);
        Points.Add(FVector(X, 0.0, Z));
        Cameras.Add(FVector(X - 300.0, 0.0, 200.0 + 0.5 * Z));
        Still.Add(FVector(10.0, 20.0, 30.0));
    }
    const TArray<FMcpFeelPress> Presses = {{TEXT("SpaceBar"), 0.1 - Dt, 0.3}, {TEXT("D"), 0.1 - Dt, 1.2}};
    const TSharedPtr<FJsonObject> Feel = McpMotionFeelSummary(Times, Points, Cameras, Presses);
    const TSharedPtr<FJsonObject>* Jump = nullptr;
    const TSharedPtr<FJsonObject>* Run = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
    if (!TestTrue(TEXT("a jump"), Feel->TryGetObjectField(TEXT("jump"), Jump)) || !TestTrue(TEXT("a run"), Feel->TryGetObjectField(TEXT("run"), Run)) ||
        !TestTrue(TEXT("two keys"), Feel->TryGetArrayField(TEXT("keys"), Keys) && Keys->Num() == 2))
    {
        return false;
    }
    TestEqual(TEXT("jump height"), (*Jump)->GetNumberField(TEXT("height")), 90.0, 1.0);
    TestEqual(TEXT("time to apex"), (*Jump)->GetNumberField(TEXT("timeToApex")), 0.4286, Dt);
    TestEqual(TEXT("air time"), (*Jump)->GetNumberField(TEXT("airTime")), 0.857, Dt);
    TestEqual(TEXT("as long down as up"), (*Jump)->GetNumberField(TEXT("fallOverRise")), 1.0, 0.1);
    TestEqual(TEXT("top speed"), (*Run)->GetNumberField(TEXT("topSpeed")), 600.0, 1.0);
    TestEqual(TEXT("time to 90%"), (*Run)->GetNumberField(TEXT("timeTo90")), 0.27, Dt);
    TestEqual(TEXT("stop time"), (*Run)->GetNumberField(TEXT("stopTime")), 0.3, Dt);
    TestEqual(TEXT("stop distance"), (*Run)->GetNumberField(TEXT("stopDistance")), 90.0, 12.0); // v^2 / 2a
    TestEqual(TEXT("latency in frames"), static_cast<int32>((*Keys)[0]->AsObject()->GetNumberField(TEXT("latencyFrames"))), 2);
    TestEqual(TEXT("latency"), (*Keys)[0]->AsObject()->GetNumberField(TEXT("latency")), 2.0 * Dt, 0.001);
    TestTrue(TEXT("a camera"), Feel->HasField(TEXT("camera")));

    const TSharedPtr<FJsonObject> Idle = McpMotionFeelSummary(Times, Still, TArray<FVector>(), Presses);
    TestFalse(TEXT("standing: no jump"), Idle->HasField(TEXT("jump")));
    TestFalse(TEXT("standing: no run"), Idle->HasField(TEXT("run")));
    TestFalse(TEXT("standing: no camera without its frames"), Idle->HasField(TEXT("camera")));
    TestFalse(TEXT("standing: the key moved nothing"), Idle->GetArrayField(TEXT("keys"))[0]->AsObject()->GetBoolField(TEXT("moved")));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
