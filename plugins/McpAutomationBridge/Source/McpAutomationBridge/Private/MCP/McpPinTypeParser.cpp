// McpPinTypeParser.cpp — implementation. Filled progressively in Tasks 3-6.

#include "MCP/McpPinTypeParser.h"

#if WITH_EDITOR

bool FMcpPinTypeParser::Parse(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutError)
{
	OutError = TEXT("Not yet implemented");
	return false;
}

FString FMcpPinTypeParser::Serialize(const FEdGraphPinType& PinType, FString& OutWarning)
{
	OutWarning = TEXT("Not yet implemented");
	return TEXT("");
}

#endif // WITH_EDITOR
