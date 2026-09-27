// McpResourceUri.h
// Content roots offered by completion and the project-name redaction used by
// the ue://project read body.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace McpResourceUri
{
	inline const TArray<FString>& ContentRoots()
	{
		static const TArray<FString> Roots = {
			TEXT("/Game"), TEXT("/Engine"), TEXT("/Script"), TEXT("/Temp"), TEXT("/Niagara"),
		};
		return Roots;
	}

	// The project name only, never a host path.
	inline FString RedactProjectName(const FString& Raw)
	{
		const FString Trimmed = Raw.TrimStartAndEnd();
		return Trimmed.IsEmpty() ? FString() : FPaths::GetBaseFilename(Trimmed);
	}
}
