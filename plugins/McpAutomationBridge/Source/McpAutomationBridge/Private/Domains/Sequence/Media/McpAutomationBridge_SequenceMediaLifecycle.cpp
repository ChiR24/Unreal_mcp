#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"

namespace McpSequenceMedia {

void DiscardCreatedMediaAsset(FMediaAssetCreateResult &Created) {
  if (!Created.bCreated || !Created.Object) {
    return;
  }
  UObject *Object = Created.Object;
  UPackage *Package = Object->GetOutermost();
  FAssetRegistryModule::AssetDeleted(Object);
  Object->ClearFlags(RF_Public | RF_Standalone);
  Object->Rename(nullptr, GetTransientPackage(),
                 REN_DontCreateRedirectors | REN_NonTransactional);
  Object->MarkAsGarbage();
  if (Package) {
    Package->SetDirtyFlag(false);
  }
  Created = FMediaAssetCreateResult();
}

}
