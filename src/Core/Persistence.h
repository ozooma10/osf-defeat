#pragma once

#include "pch.h"

// =============================================================================
// Persistence — serialize the defeat registry into the SFSE co-save so defeat
// state survives save/load. Register() must be called once during plugin load
// (before kDataLoaded).
// =============================================================================
namespace OSFDefeat::Persistence
{
	void Register();
}
