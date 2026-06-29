/* Minimal libascend_dump stub for WSL SIM (SIM_DIRECT / CaModel).
 * Real libascend_dump FPEs at dl_init (InitHardwareInfo950); acl_rt_impl still
 * needs a few Adx dump entry points — provide no-op stubs here. */
#include <stddef.h>

extern "C" void ascend_dump_sim_stub_anchor(void)
{
}

extern "C" void AdxDataDumpServerInit(void)
{
}

extern "C" void AdxDataDumpServerUnInit(void)
{
}

namespace Adx {
struct DumpConfigInfo {};

void AdumpUnSetDump(void)
{
}

void AdumpSetDumpConfig(DumpConfigInfo)
{
}
} // namespace Adx
