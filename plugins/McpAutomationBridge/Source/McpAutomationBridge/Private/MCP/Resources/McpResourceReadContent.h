// McpResourceReadContent.h
// Task 38 lane A: bounded, socket-thread-safe read content for the static
// resources the native transport can answer without a game-thread editor scan,
// plus the read classification that separates a socket-readable uri from a
// known-but-not-served uri (RESOURCE_UNAVAILABLE) and from a genuinely unknown
// uri (RESOURCE_NOT_FOUND). Mirrors the typed error codes of the TypeScript
// ResourceReadRouter without touching editor world state. The body builders read
// the immutable capability store, process-local transport state and static app
// info only; none of them opens a socket or scans the editor.
//
// EVERY uri McpResourceCatalog::AllListedResources() advertises classifies as
// SocketReadable here. That is the invariant this file exists to hold: native
// never advertises a resource it will refuse to read.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "MCP/Primitives/McpResourceRevision.h"
#include "MCP/Resources/McpResourceCatalog.h"

namespace McpResourceRead
{
	struct FReadBody
	{
		FMcpResourceRevision Revision = McpInitialResourceRevision;
		FString Text;
	};

	enum class EReadKind : uint8
	{
		SocketReadable,
		EditorUnavailable,
		Unknown,
	};

	EReadKind Classify(const FString& Uri);

	FString UnavailableMessage(const FString& Uri);

	FReadBody BuildReadBody(const FString& Uri, FMcpResourceRevision Revision);

	FString BuildReadBodyText(const FString& Uri, FMcpResourceRevision Revision);

	TSharedPtr<FJsonValue> ListEntry(const FMcpResourceDefinition& Def);

	TSharedPtr<FJsonValue> TemplateEntry(const FMcpResourceTemplateDefinition& Def);
}
