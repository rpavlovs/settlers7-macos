#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
compat_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
game_data=$(CDPATH= cd -- "$compat_dir/.." && pwd)
cx_root=${CX_ROOT:-/Applications/CrossOver.app/Contents/SharedSupport/CrossOver}
wine=${CROSSOVER_WINE:-$cx_root/bin/wine}
bottle=${SETTLERS7_BOTTLE:-${CX_BOTTLE:-}}
if [ -z "$bottle" ]; then
    echo 'No CrossOver bottle supplied. Start with Steam Play inside CrossOver.' >&2
    exit 2
fi
if [ "$#" -eq 0 ] || [ ! -x "$wine" ] || [ ! -f "$compat_dir/libMoltenVK.dylib" ]; then
    echo 'Missing game command, CrossOver executable, or local libMoltenVK.dylib.' >&2
    exit 2
fi
export CX_LIBVULKAN="$compat_dir/libMoltenVK.dylib"
# Re-enter the normal CrossOver wrapper so its per-launch options take effect.
unset CX_INITIALIZED
exec "$wine" --bottle "$bottle" --workdir "$game_data" \
  --env 'SteamAppId=965320 SteamGameId=965320 CX_GRAPHICS_BACKEND=wined3d CX_ACTIVE_GRAPHICS_BACKEND=wined3d WINEDLLOVERRIDES=d3d9=n,b;gameoverlayrenderer,gameoverlayrenderer64,overlay,overlay64= DXVK_CONFIG=d3d9.splitSamplerSlots=True DXVK_SHADER_COMPILATION_METHOD=none DXVK_HUD=fps MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1 MVK_CONFIG_LOG_LEVEL=1' \
  --cx-log "$compat_dir/last-launch.log" \
  --debugmsg '-all,-module,-process,-loaddll,-seh,-unwind,-threadname' --cx-app "$@"
