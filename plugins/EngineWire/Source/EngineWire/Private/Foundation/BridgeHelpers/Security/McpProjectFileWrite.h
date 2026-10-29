#pragma once

#include "CoreMinimal.h"

/**
 * Writes Bytes over the file at AbsolutePath, a path the project-file resolver accepted, making the folders it needs.
 * The resolver's check (inside the project, no link on the way) and the write are apart in time, and a link made in
 * between sent the write outside the project: here the file is opened first, and what was opened must be that path
 * before a byte is written. False with OutError otherwise, with nothing written outside the project.
 */
bool McpWriteProjectFile(const FString& AbsolutePath, TArrayView<const uint8> Bytes, FString& OutError);

/** McpWriteProjectFile for text, written as UTF-8 without a byte-order mark. */
bool McpWriteProjectText(const FString& AbsolutePath, const FString& Text, FString& OutError);
