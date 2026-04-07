#pragma once

// Establishes persistence for the attack simulator.
// Copies the executables to C:\ProgramData\SystemOptimizer\
// and sets an HKCU Run key to launch it with --persistent.
bool EstablishPersistence();
