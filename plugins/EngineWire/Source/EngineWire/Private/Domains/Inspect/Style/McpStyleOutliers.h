#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// The statistics behind inspect audit_style (S4): robust outliers within groups, by the median and the median absolute
// deviation, so one wild value does not move the yardstick it is measured against.
namespace McpStyleOutliers
{
struct FSample
{
    FString Group; // what the values are comparable within (one mesh's copies)
    FString Name;  // the actor
    double Value = 0.0;
};

struct FOutlier
{
    FString Name;
    FString Group;
    double Value = 0.0;
    double Median = 0.0;
    double Score = 0.0; // |value - median| / (1.4826 MAD): about a standard deviation's worth per unit
};

double Median(TArray<double> Values);

/** Samples whose robust z within their group is above Threshold, in groups of at least MinGroup, largest first. A group
 *  whose values barely vary (MAD under 1% of its median) judges by that 1%, so identical copies do not flag a 0.5% one. */
TArray<FOutlier> Find(const TArray<FSample>& Samples, double Threshold = 3.5, int32 MinGroup = 5);

/** inspect audit_style (McpStyleAudit.cpp): copies of one mesh in the open level whose scale stands out. */
bool HandleAuditStyle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                      TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpStyleOutliers
