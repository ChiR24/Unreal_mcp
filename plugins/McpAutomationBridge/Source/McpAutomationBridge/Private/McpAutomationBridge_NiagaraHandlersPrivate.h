#pragma once

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Async/Async.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "UObject/Package.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

#if __has_include("ViewModels/Stack/NiagaraStackGraphUtilities.h")
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

#if __has_include("NiagaraEmitterFactoryNew.h") && !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
#include "NiagaraEmitterFactoryNew.h"
#define MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW 1
#else
#define MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW 0
#endif

#if __has_include("NiagaraSystemFactoryNew.h") && !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
#include "NiagaraSystemFactoryNew.h"
#define MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW 1
#else
#define MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW 0
#endif
#endif
