// McpResourceRevision.h
// The revision a resource read body carries (mirrors resource-revision.ts).
#pragma once

#include "CoreMinimal.h"

using FMcpResourceRevision = int64;

inline constexpr FMcpResourceRevision McpInitialResourceRevision = 1;
