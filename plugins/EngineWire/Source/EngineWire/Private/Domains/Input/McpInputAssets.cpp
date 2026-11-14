#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Input/McpInputAssetResolve.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Misc/PackageName.h"

namespace McpInputHandlers
{
namespace
{
FString NormalizeInputAssetPathForLoad(const FString& RawPath)
{
    return SanitizeProjectRelativePath(FPackageName::ExportTextPathToObjectPath(RawPath.TrimStartAndEnd()));
}

// OutNormalizedPath keeps the caller's (sanitized) form; a package path loads its same-named asset. StaticLoadObject
// rather than UEditorAssetLibrary, which refuses every call during PIE.
template <typename TAsset>
TAsset* LoadInputAsset(const FString& RawPath, FString& OutNormalizedPath)
{
    OutNormalizedPath = NormalizeInputAssetPathForLoad(RawPath);
    if (OutNormalizedPath.IsEmpty())
    {
        return nullptr;
    }
    const FString ObjectPath = McpToObjectPath(OutNormalizedPath);
    return Cast<TAsset>(StaticLoadObject(TAsset::StaticClass(), nullptr, *ObjectPath, nullptr, LOAD_NoWarn));
}
}

UInputAction* LoadInputActionAsset(const FString& RawPath, FString& OutNormalizedPath)
{
    return LoadInputAsset<UInputAction>(RawPath, OutNormalizedPath);
}

UInputMappingContext* LoadInputMappingContextAsset(const FString& RawPath, FString& OutNormalizedPath)
{
    return LoadInputAsset<UInputMappingContext>(RawPath, OutNormalizedPath);
}

UObject* LoadInputObjectAsset(const FString& RawPath, FString& OutNormalizedPath)
{
    return LoadInputAsset<UObject>(RawPath, OutNormalizedPath);
}
}
