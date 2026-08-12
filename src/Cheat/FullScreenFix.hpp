#pragma once

// Inicializa os hooks de fullscreen fix (User32 + D3D9/D3D9Ex)
void FullScreenFixHooks();

// Remove todos os hooks instalados pelo FullScreenFixHooks
void RemoveFullScreenFixHooks();