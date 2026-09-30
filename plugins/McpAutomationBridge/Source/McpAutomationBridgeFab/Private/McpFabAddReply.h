// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "McpFabProvider.h"

namespace McpFabAddOperation
{
/**
 * What the add's page script told the caller, read out of its reply.
 *
 * Parsed even when the page refused: the reply says why, and gating the parse on success turned every
 * refusal into a bare FAB_REJECTED that named nothing. A refusal comes back with ErrorCode and an Error
 * that names the step that failed and quotes Fab's own words where the page gave them.
 */
FMcpFabAddResult ParseAddReply(bool bSuccess, const FString& Payload);

/** Names the import at the head of a full queue: its listing and title, phase, progress and elapsed time. */
FString DescribeQueueHead(const FMcpFabImportStatus& Head, int32 QueueLimit);
} // namespace McpFabAddOperation
