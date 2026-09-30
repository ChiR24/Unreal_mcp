// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabPageReadiness.h"

#include "McpFabBrowserWidgetSearch.h"

namespace McpFabPageReadiness
{
bool IsFabUrl(const FString& Url)
{
	for (const TCHAR* Origin : {TEXT("https://www.fab.com"), TEXT("https://fab.com")})
	{
		const int32 Length = FCString::Strlen(Origin);
		if (Url.StartsWith(Origin, ESearchCase::IgnoreCase) &&
			(Url.Len() == Length || Url[Length] == TEXT('/') || Url[Length] == TEXT('?') || Url[Length] == TEXT('#')))
		{
			return true;
		}
	}
	return false;
}

FPageState Probe()
{
	FPageState State;
	FString Url;
	State.bTabFound = McpFabBrowserSession::ReadPageState(Url, State.bLoading, State.Diagnostic);
	State.bUrlIsFab = State.bTabFound && IsFabUrl(Url);
	return State;
}
} // namespace McpFabPageReadiness
