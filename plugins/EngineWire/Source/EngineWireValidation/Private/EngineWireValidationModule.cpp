// Copyright (c) 2024 EngineWire Contributors

#include "Modules/ModuleManager.h"

// Nothing to start: the Data Validation subsystem finds UMcpLintValidator among the validator classes by itself.
// EngineWire's StartupModule loads this module, and only where DataValidation is on (LoadingPhase None).
IMPLEMENT_MODULE(FDefaultModuleImpl, EngineWireValidation)
