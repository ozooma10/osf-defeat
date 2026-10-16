#pragma once

namespace OSFDefeat::Reattach
{
    // Installs the Actor::Set3D entry hook (AddrLib ID 101138) so a defeated actor's held
    // down-state is re-asserted when its 3D is rebuilt (cell change / save load / coc), which
    // otherwise silently strips lifeState/ghost/bleedout. (OSF RE defeat-3d-reattach, 2026-06-26)
    // Called once at kPostDataLoad.
    void Install();
}
