#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_EffectHandlersPrivate.h"

namespace McpEffectHandlers
{
bool HandleNiagaraSpawnModules(const FEffectActionContext&)
{
    return false;
}

bool HandleNiagaraBehaviorModules(const FEffectActionContext&)
{
    return false;
}

bool HandleNiagaraRenderModules(const FEffectActionContext&)
{
    return false;
}

bool HandleNiagaraDataEventModules(const FEffectActionContext&)
{
    return false;
}

bool HandleNiagaraParameterModules(const FEffectActionContext&)
{
    return false;
}
}
