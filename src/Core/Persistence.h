#pragma once

#include "pch.h"

// =============================================================================
// Persistence — serialize the defeat registry through OSF Animation's shared
// .osf sidecar so defeat state survives save/load. Register at SFSE
// kPostDataLoad, after all native plugins have loaded.
// =============================================================================
namespace OSFDefeat::Persistence
{
	void Register();
}
