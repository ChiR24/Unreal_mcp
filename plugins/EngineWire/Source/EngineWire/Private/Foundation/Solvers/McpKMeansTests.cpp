// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpKMeans.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpKMeansTest, "EngineWire.Foundation.Solvers.KMeans",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: three blobs of 30 points each come back as three clusters at the blobs' centres, heaviest first, and
// the same on a second run.
bool FMcpKMeansTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FVector3f Centres[3] = {FVector3f(0.f, 0.f, 0.f), FVector3f(10.f, 0.f, 0.f), FVector3f(0.f, 10.f, 0.f)};
    TArray<FVector3f> Points;
    TArray<float> Weights;
    for (int32 Blob = 0; Blob < 3; ++Blob)
    {
        for (int32 Index = 0; Index < 30; ++Index)
        {
            const float Jitter = float(Index % 5) * 0.1f - 0.2f; // a cross of points around the centre
            Points.Add(Centres[Blob] + (Index % 2 == 0 ? FVector3f(Jitter, 0.f, 0.f) : FVector3f(0.f, Jitter, 0.f)));
            Weights.Add(Blob == 2 ? 2.f : 1.f); // the third blob weighs twice as much
        }
    }
    const TArray<McpKMeans::FCluster> First = McpKMeans::Cluster(Points, &Weights, 3, 20, 7);
    TestEqual(TEXT("three clusters"), First.Num(), 3);
    if (First.Num() != 3)
    {
        return true;
    }
    TestTrue(TEXT("the heaviest is the third blob"), First[0].Center.Equals(Centres[2], 0.05f));
    TestEqual(TEXT("with its weight"), First[0].Weight, 60.0, 1e-6);
    for (const FVector3f& Centre : Centres)
    {
        TestTrue(FString::Printf(TEXT("a cluster sits at (%.0f, %.0f)"), Centre.X, Centre.Y),
                 First.ContainsByPredicate([&Centre](const McpKMeans::FCluster& Each) { return Each.Center.Equals(Centre, 0.05f); }));
    }
    const TArray<McpKMeans::FCluster> Again = McpKMeans::Cluster(Points, &Weights, 3, 20, 7);
    TestTrue(TEXT("the same on a second run"), Again.Num() == 3 && Again[1].Center.Equals(First[1].Center, 1e-6f));
    return true;
}
#endif
