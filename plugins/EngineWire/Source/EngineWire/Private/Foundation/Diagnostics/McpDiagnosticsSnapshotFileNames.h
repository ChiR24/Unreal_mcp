#pragma once

#include "CoreMinimal.h"

// Snapshot file names shared by the diagnostics .cpp files. They live in a
// named namespace of inline functions rather than per-file anonymous
// namespaces because these sources compile into shared unity blobs: duplicate
// anonymous-namespace copies collide (C2084) once merged, and a cross-file call
// otherwise depends on the merge to resolve at all.
namespace McpDiagnosticsSnapshotFileNames
{
inline const TCHAR* CurrentFileName() { return TEXT("current-session.json"); }
inline const TCHAR* CurrentTempName() { return TEXT("current-session.json.tmp"); }
inline const TCHAR* PreviousFileName() { return TEXT("previous-session.json"); }
inline const TCHAR* PreviousTempName() { return TEXT("previous-session.json.tmp"); }
}
