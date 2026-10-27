#include "Domains/ControlActor/Placement/McpPartPlacement.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Domains/SystemControl/McpSystemPrivate.h"
#include "Engine/Blueprint.h"
#include "Materials/MaterialInterface.h"
#include "Safety/McpSafeOperationsAssetSave.h"
#include "UObject/Package.h"

// FMcpPreviewMaterialSaver: validate_assets materialUsage names a needed flag that is set but not on disk (a material
// instance's own flag on 5.8); this saves those assets. Only project content (/Game) is saved; engine and plugin
// materials are left as the editor leaves them.
namespace McpPartPlacement
{
FMcpPreviewMaterialSaver::~FMcpPreviewMaterialSaver()
{
    for (const TSharedPtr<FJsonValue>& Entry : McpSystemControlHandlers::McpMissingMaterialUsage(Blueprint.Get()))
    {
        const TSharedPtr<FJsonObject>& Row = Entry->AsObject();
        bool bUnsaved = false;
        UMaterialInterface* Material = Row->TryGetBoolField(TEXT("unsaved"), bUnsaved) && bUnsaved
            ? FindObject<UMaterialInterface>(nullptr, *Row->GetStringField(TEXT("material")))
            : nullptr;
        if (Material && Material->GetOutermost()->GetName().StartsWith(TEXT("/Game/")) && McpSafeOperations::McpSafeAssetSave(Material))
        {
            UE_LOG(LogTemp, Log, TEXT("EngineWire: saved %s, which needs the %s usage flag it was given"),
                   *Material->GetPathName(), *Row->GetStringField(TEXT("usage")));
        }
    }
}
} // namespace McpPartPlacement
