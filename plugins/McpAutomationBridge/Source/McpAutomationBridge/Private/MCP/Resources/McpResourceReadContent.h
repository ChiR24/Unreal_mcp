// McpResourceReadContent.h
// Task 38 lane A: bounded, socket-thread-safe read content for the static
// resources the native transport can answer without a game-thread editor scan
// (`ue://capability/catalog`, `ue://project`, `ue://state/revisions`, and since
// Task 47 `ue://health`), plus the read classification that
// separates a socket-readable uri from a known editor-state uri (which returns
// RESOURCE_UNAVAILABLE) and from a genuinely unknown uri (RESOURCE_NOT_FOUND).
// Mirrors the typed error codes of the TypeScript ResourceReadRouter without
// touching editor world state. The body builder reads the immutable capability
// store and static app info only; it never opens a socket or scans the editor.
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

	FReadBody BuildReadBody(const FString& Uri, FMcpResourceRevision Revision);

	FString BuildReadBodyText(const FString& Uri, FMcpResourceRevision Revision);

	TSharedPtr<FJsonValue> ListEntry(const FMcpResourceDefinition& Def);

	TSharedPtr<FJsonValue> TemplateEntry(const FMcpResourceTemplateDefinition& Def);
}
