#pragma once

// Whether MetaSound is there, and which of its builder APIs. It is an engine plugin, so its headers are probed; the audio
// authoring umbrella and the Blueprint sound check both read the answer from here.
#include "Core/Compatibility/McpVersionCompatibility.h"

#if __has_include("MetasoundSource.h")
#include "MetasoundSource.h"
#define MCP_HAS_METASOUND 1
#else
#define MCP_HAS_METASOUND 0
#endif

#if __has_include("Metasound.h")
#include "Metasound.h"
#endif

#if __has_include("MetasoundBuilderSubsystem.h")
#include "MetasoundBuilderSubsystem.h"
#endif

#if __has_include("MetasoundFrontendDocumentBuilder.h")
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocument.h"
#if __has_include("MetasoundFrontendDocumentBuilderRegistry.h")
#include "MetasoundFrontendDocumentBuilderRegistry.h"
#endif
#define MCP_HAS_METASOUND_FRONTEND 1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#define MCP_HAS_METASOUND_FRONTEND_V2 1
#else
#define MCP_HAS_METASOUND_FRONTEND_V2 0
#endif
#else
#define MCP_HAS_METASOUND_FRONTEND 0
#define MCP_HAS_METASOUND_FRONTEND_V2 0
#endif
