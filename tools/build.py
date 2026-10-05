"""Build a self-contained checkout with VS 2022/2026, including Build Tools."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=('x64', 'Win32', 'ARM64'), default='x64')
    parser.add_argument('--configuration', choices=('Release', 'Debug'), default='Release')
    parser.add_argument('--toolset', choices=('v143', 'v145'))
    parser.add_argument('--tests', action='store_true', help='Build and run safe native x64 policy tests')
    parser.add_argument('--analysis', action='store_true', help='Enable MSVC static analysis')
    args = parser.parse_args()
    # Windows environment names are case-insensitive. Avoid duplicate Path/PATH
    # entries in MSBuild, without changing the parent or system environment.
    env = {key.upper(): value for key, value in os.environ.items()}
    temp = ROOT / 'artifacts' / 'temp'
    temp.mkdir(parents=True, exist_ok=True)
    env['TEMP'] = env['TMP'] = str(temp)
    vswhere = Path(env.get('PROGRAMFILES(X86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not vswhere.is_file():
        parser.error('Install Visual Studio 2022/2026 C++ desktop tools and a Windows SDK.')
    found = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], env=env, text=True).strip()
    if not found:
        parser.error('No Visual Studio C++ desktop installation was found.')
    installation = Path(found)
    msbuild = installation / 'MSBuild/Current/Bin/MSBuild.exe'
    version = (installation / 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt').read_text().strip()
    toolset = args.toolset or ('v145' if int(version.split('.')[1]) >= 50 else 'v143')
    platform = 'x64' if args.tests else args.platform
    output = ROOT / 'artifacts' / ('tests' if args.tests else f'{platform}/{args.configuration}')
    intermediate = ROOT / 'artifacts/obj' / ('tests' if args.tests else f'{platform}/{args.configuration}')
    output.mkdir(parents=True, exist_ok=True)
    project = ROOT / ('tests/policy_tests.vcxproj' if args.tests else 'memreduct.vcxproj')
    command = [str(msbuild), str(project), '/nologo', '/t:Build', '/v:minimal',
        f'/p:Configuration={args.configuration}', f'/p:Platform={platform}', f'/p:PlatformToolset={toolset}',
        f'/p:OutDir={output}\\', f'/p:IntDir={intermediate}\\', '/fl',
        f'/flp:logfile={output / "build.log"};verbosity=normal']
    if args.analysis:
        command.append('/p:RunCodeAnalysis=true')
    result = subprocess.run(command, cwd=ROOT, env=env)
    if result.returncode:
        return result.returncode
    if args.tests:
        return subprocess.run([str(output / 'policy_tests.exe')], cwd=ROOT, env=env).returncode
    print(f'Built: {output / "memreduct.exe"}')
    return 0

if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    sys.exit(main())
