#pragma once

// Simulation strings remain plain data.  Real adapter-specific transforms are
// intentionally not imported into the deterministic lab model.
#ifndef OBF
#  define OBF(value) value
#endif
#ifndef OBFW
#  define OBFW(value) value
#endif
