#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY(LogMcpSafeOperations);

void McpHandlerUtils::ApplyProgressiveLods(UStaticMesh* Mesh, int32 NumLODs)
{
    Mesh->Modify();
    Mesh->SetNumSourceModels(NumLODs);
    for (int32 LODIndex = 1; LODIndex < NumLODs; ++LODIndex)
    {
        FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(LODIndex);
        // 50%, 25%, 12.5%...
        const float ReductionPercent = 1.0f / FMath::Pow(2.0f, static_cast<float>(LODIndex));
        SourceModel.ReductionSettings.PercentTriangles = ReductionPercent;
        SourceModel.ReductionSettings.PercentVertices = ReductionPercent;
        SourceModel.BuildSettings.bRecomputeNormals = false;
        SourceModel.BuildSettings.bRecomputeTangents = false;
        SourceModel.BuildSettings.bUseMikkTSpace = true;
    }
    Mesh->Build();
    Mesh->PostEditChange();
    McpSafeOperations::McpSafeAssetSave(Mesh);
}
