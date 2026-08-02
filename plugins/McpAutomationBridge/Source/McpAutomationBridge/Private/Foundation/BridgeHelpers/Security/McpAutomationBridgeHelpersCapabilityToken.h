#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "McpAutomationBridgeLog.h"
#include "McpAutomationBridgeSettings.h"

// Shared token-file contract (byte-identical with TS side):
//   Path:    <ProjectRoot>/Saved/MCP/capability-token
//   Format:  raw UTF-8, 64 lowercase hex chars (32 bytes), NO trailing newline
//   NEVER log the token value; never emit in health/telemetry/resource bodies
//
// McpAutomationBridgeSettings.h is included so this header is self-contained:
// ResolveEffectiveToken dereferences UMcpAutomationBridgeSettings, so any TU
// that includes this store first (without its own settings include) still
// compiles under -DisableUnity.

namespace McpCapabilityTokenStore
{
/** Path to the persisted token file. */
static inline FString TokenFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MCP"), TEXT("capability-token"));
}

/**
 * Generate a 32-byte random token as 64 lowercase hex chars.
 * Entropy source: two FGuid::NewGuid() calls (32 bytes), backed by the
 * platform RNG (CoCreateGuid on Windows, system entropy elsewhere). FGuid is
 * not documented as a CSPRNG on every platform, which is acceptable for a
 * per-install, editor-bound secret; FRandomStream/FMath::Rand are never used
 * for token generation.
 */
static inline FString GenerateRandomToken()
{
	uint8 RawBytes[32] = { 0 };
	const FGuid GuidA = FGuid::NewGuid();
	const FGuid GuidB = FGuid::NewGuid();
	FMemory::Memcpy(RawBytes, &GuidA, sizeof(FGuid));
	FMemory::Memcpy(RawBytes + sizeof(FGuid), &GuidB, sizeof(FGuid));

	// Encode as 64 lowercase hex chars
	FString Result;
	Result.Reserve(64);
	for (int32 i = 0; i < 32; ++i)
	{
		Result.Appendf(TEXT("%02x"), RawBytes[i]);
	}
	return Result;
}

/**
 * Write the token to the file, creating the parent directory if needed.
 * Returns true on success, false on failure.
 * On failure the log names the path; the token value is never emitted.
 */
static inline bool WriteTokenFile(const FString& Token)
{
	const FString Path = TokenFilePath();
	const FString Dir = FPaths::GetPath(Path);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Ensure directory exists (create tree)
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		if (!PlatformFile.CreateDirectoryTree(*Dir))
		{
			// Fail-closed: if we cannot create the directory we cannot persist
			// the token, so report failure rather than write a truncated file.
			UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
				TEXT("Capability token store: could not create directory \"%s\". "
				     "Token will not be persisted. A token file at \"%s\" is required "
				     "when bRequireCapabilityToken is enabled."),
				*Dir, *Path);
			return false;
		}
	}

	// Write raw UTF-8, no trailing newline
	if (!FFileHelper::SaveStringToFile(Token,
		*Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
			TEXT("Capability token store: failed to write token file \"%s\". "
			     "The token was generated but could not be persisted. "
			     "Without a persisted token, future editor restarts will generate "
			     "a new value and existing clients with the old token will be refused."),
			*Path);
		return false;
	}

	return true;
}

/**
 * Read and return the token from the file, or an empty string if absent or
 * malformed. A valid token is exactly 64 lowercase hex chars after trimming
 * surrounding whitespace; anything else is treated as absent (fail closed) so
 * the C++ and TypeScript sides can never disagree on a file's meaning.
 */
static inline FString ReadTokenFile()
{
	const FString Path = TokenFilePath();
	if (!FPaths::FileExists(Path))
	{
		return FString();
	}

	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path, FFileHelper::EHashOptions::None))
	{
		// File exists but unreadable — fail closed (empty token forces refusal).
		UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
			TEXT("Capability token store: could not read token file \"%s\"."), *Path);
		return FString();
	}

	const FString Token = Raw.TrimStartAndEnd();
	if (Token.Len() != 64)
	{
		UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
			TEXT("Capability token store: token file \"%s\" does not contain a 64-char token; ignoring it."),
			*Path);
		return FString();
	}
	for (int32 i = 0; i < 64; ++i)
	{
		const TCHAR C = Token[i];
		const bool bIsLowerHex =
			(C >= TEXT('0') && C <= TEXT('9')) || (C >= TEXT('a') && C <= TEXT('f'));
		if (!bIsLowerHex)
		{
			UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
				TEXT("Capability token store: token file \"%s\" contains a non-hex character; ignoring it."),
				*Path);
			return FString();
		}
	}

	return Token;
}

/**
 * Resolve the effective capability token.
 *
 * Resolution order:
 *   1. Explicit settings token (Settings->CapabilityToken) if non-empty — WINS.
 *   2. Token file at <ProjectRoot>/Saved/MCP/capability-token.
 *
 * If bRequireCapabilityToken is true and no token is available (no explicit
 * setting and no file), a new 32-byte token is generated, written to the file,
 * and returned — unless bAutoGenerate is false, in which case an empty string
 * is returned. Read-only callers (e.g. CORS preflight) must pass false so an
 * unauthenticated request can never create the token file as a side effect.
 * On write failure an empty string is returned, causing every auth attempt to
 * fail closed.
 *
 * If bRequireCapabilityToken is false and no token is available, an empty
 * string is returned.
 *
 * The returned token is NEVER logged; the token file path may appear in logs
 * on error, but the token value never does.
 *
 * CRITICAL: an empty return from this function is ambiguous. Callers MUST
 * check emptiness EXPLICITLY before calling McpConstantTimeTokenEquals,
 * because an empty-vs-empty compare would wrongly return true.
 */
static inline FString ResolveEffectiveToken(
	const UMcpAutomationBridgeSettings* Settings,
	bool bAutoGenerate = true)
{
	if (!Settings)
	{
		return FString();
	}

	// Step 1: explicit settings token wins
	if (!Settings->CapabilityToken.IsEmpty())
	{
		return Settings->CapabilityToken;
	}

	// Step 2: token file
	FString Token = ReadTokenFile();
	if (!Token.IsEmpty())
	{
		return Token;
	}

	// Step 3: auto-generate only when token IS required, nothing exists, and the
	// caller opted into generation. When token is not required (or the caller is
	// read-only), an empty token is valid (loopback passthrough / no CORS).
	if (!Settings->bRequireCapabilityToken || !bAutoGenerate)
	{
		return FString();
	}

	// Token required but nothing available — generate.
	UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
		TEXT("Capability token required but no token found. Auto-generating new token."));

	Token = GenerateRandomToken();

	if (Token.IsEmpty())
	{
		// Should be impossible but guard anyway
		UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
			TEXT("Capability token store: token generation returned empty string. "
			     "Auth will be refused. Check platform RNG availability."));
		return FString();
	}

	if (!WriteTokenFile(Token))
	{
		// Write failed — fail closed: token unobtainable, refuse auth.
		UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
			TEXT("Capability token store: token generated but could not be written to \"%s\". "
			     "Auth will be refused until this is resolved."),
			*TokenFilePath());
		return FString();
	}

	// First-run race: two callers (e.g. connection-manager init on the game
	// thread and a native socket thread) can both observe "no file" and both
	// generate. Re-read what is actually on disk and converge on it (last
	// writer wins) so the returned token always matches the file's content;
	// an unreadable/invalid file fails closed.
	return ReadTokenFile();
}

} // namespace McpCapabilityTokenStore
