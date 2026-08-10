// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Misc/Guid.h"

/**
 * Opaque, deterministic pagination cursor for asset listings.
 *
 * Encodes the next offset, the canonical asset path, and a per-session catalog
 * revision. Clients must echo the cursor verbatim. A mismatch in the embedded
 * path (TOCTOU re-validation of path containment) or revision (stale cursor /
 * cross-session reuse) is rejected as a retryable STALE_CURSOR error by
 * HandleListAssets.
 */
struct FMcpAssetListCursor
{
    int32 Offset = 0;
    FString Path;
    FString Revision;
};

/** Stable per-session catalog revision embedded in every issued cursor. */
inline const FString& McpGetAssetListRevision()
{
    static const FString Revision = FGuid::NewGuid().ToString();
    return Revision;
}

/** Encode a cursor into an opaque, base64-wrapped JSON token (empty on failure). */
inline FString McpEncodeAssetListCursor(const FMcpAssetListCursor& Cursor)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("o"), Cursor.Offset);
    Obj->SetStringField(TEXT("p"), Cursor.Path);
    Obj->SetStringField(TEXT("r"), Cursor.Revision);

    FString Json;
    if (!FJsonSerializer::Serialize(Obj.ToSharedRef(), TJsonWriterFactory<>::Create(&Json)))
    {
        return FString();
    }
    return FBase64::Encode(Json);
}

/** Decode an opaque cursor token. Returns false on any malformed input. */
inline bool McpDecodeAssetListCursor(const FString& Encoded, FMcpAssetListCursor& OutCursor)
{
    if (Encoded.IsEmpty())
    {
        return false;
    }

    FString Json;
    if (!FBase64::Decode(Encoded, Json) || Json.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> Obj;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Obj) || !Obj.IsValid())
    {
        return false;
    }

    double Offset = 0.0;
    FString Path;
    FString Revision;
    if (!Obj->TryGetNumberField(TEXT("o"), Offset) ||
        !Obj->TryGetStringField(TEXT("p"), Path) ||
        !Obj->TryGetStringField(TEXT("r"), Revision))
    {
        return false;
    }

    OutCursor.Offset = FMath::Max(0, static_cast<int32>(FMath::RoundToDouble(Offset)));
    OutCursor.Path = Path;
    OutCursor.Revision = Revision;
    return true;
}
