import argparse
import os
from pathlib import Path, PureWindowsPath

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--debug', default='-all')
parser.add_argument('--set', action='append', default=[])
parser.add_argument('command', nargs=argparse.REMAINDER)
args = parser.parse_args()
if not args.command:
    parser.error('a Windows command is required')

# Inherit only the ordinary host environment required by Wine, never API keys.
env = {key: value for key, value in os.environ.items()
       if key in {'HOME', 'USER', 'LOGNAME', 'PATH', 'TMPDIR', 'LANG', 'SHELL',
                  '__CF_USER_TEXT_ENCODING'} or key.startswith('LC_')}
env.update({
    'WINEPREFIX': str(root / 'prefix'),
    'WINEDEBUG': args.debug,
    'WINEMSYNC': '0', 'WINEESYNC': '0',
    'WINEDLLOVERRIDES': 'winemenubuilder.exe=d;dxgi,d3d10core,d3d11=n,b',
    'WINEDLLPATH_PREPEND': str(root / 'renderers/dxvk/wine'),
    'DXVK_RELAX_FEATURES': '1',
    'MVK_CONFIG_SHADER_SOURCE_COMPRESSION_ALGORITHM': '1',
    'MVK_CONFIG_RESUME_LOST_DEVICE': '1',
    'DYLD_FALLBACK_LIBRARY_PATH': ':'.join(map(str, [
        root / 'engine/lib', root / 'frameworks',
        root / 'frameworks/GStreamer.framework/Versions/1.0/lib'])),
    'DYLD_FALLBACK_FRAMEWORK_PATH': str(root / 'frameworks'),
    'GST_PLUGIN_PATH': str(root / 'frameworks/GStreamer.framework/Versions/1.0/lib/gstreamer-1.0'),
    'CX_FWD_COMPAT_GL_CTX': '1',
    'FEX_X87REDUCEDPRECISION': '1',
    'SteamAppId': '965320', 'SteamGameId': '965320',
    'DXVK_CONFIG': 'd3d9.splitSamplerSlots=True',
    'DXVK_SHADER_COMPILATION_METHOD': 'none',
    'DXVK_HUD': 'fps',
    'MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS': '1',
    'MVK_CONFIG_LOG_LEVEL': '1',
})
for setting in args.set:
    key, value = setting.split('=', 1)
    env[key] = value

exe = args.command[0]
windows_exe = PureWindowsPath(exe)
if windows_exe.drive.lower() == 'c:' and windows_exe.is_absolute():
    local = root / 'prefix/drive_c' / Path(*windows_exe.parts[1:])
    if local.is_file():
        os.chdir(local.parent)
os.execve(root / 'engine/bin/wine', [str(root / 'engine/bin/wine'), *args.command], env)
