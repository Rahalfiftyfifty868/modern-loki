"""
Extract C++ examples from Markdown documentation and compile+run them.
Supports MSVC (Windows), GCC, and Clang (Linux/macOS).

Performance: vcvars64.bat called once at startup, env cached for all cl.exe
invocations (no per-example bat file or cmd.exe spawn).
"""

import re
import subprocess
import sys
import os
import platform
import shutil
import tempfile
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
os.environ['PYTHONIOENCODING'] = 'utf-8'

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
INCLUDE_DIR = os.path.join(PROJECT_ROOT, 'include')


# ── MSVC environment capture (once) ─────────────────────────────────────────

def _find_vcvars():
    vswhere_paths = [
        os.path.join(os.environ.get('ProgramFiles(x86)', ''),
                     'Microsoft Visual Studio', 'Installer', 'vswhere.exe'),
        os.path.join(os.environ.get('ProgramFiles', ''),
                     'Microsoft Visual Studio', 'Installer', 'vswhere.exe'),
    ]
    vswhere = next((p for p in vswhere_paths if os.path.isfile(p)), None)
    if vswhere is None:
        return None
    try:
        result = subprocess.run(
            [vswhere, '-latest', '-products', '*',
             '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
             '-property', 'installationPath'],
            capture_output=True, text=True, timeout=15,
        )
        vs_path = result.stdout.strip()
        if vs_path:
            candidate = os.path.join(vs_path, 'VC', 'Auxiliary', 'Build', 'vcvars64.bat')
            if os.path.isfile(candidate):
                return candidate
    except Exception:
        pass
    return None


def _capture_msvc_env(vcvars_path):
    """Run vcvars64.bat once, dump env with 'set', parse into a dict."""
    bat = tempfile.NamedTemporaryFile(mode='w', suffix='.bat', delete=False)
    try:
        bat.write(f'@call "{vcvars_path}" >nul 2>&1\n@set\n')
        bat.close()
        result = subprocess.run(
            ['cmd.exe', '/c', bat.name],
            capture_output=True, text=True, timeout=30,
        )
        env = {}
        for line in result.stdout.splitlines():
            eq = line.find('=')
            if eq > 0:
                env[line[:eq]] = line[eq + 1:]
        return env
    finally:
        os.unlink(bat.name)


# ── Compiler detection ──────────────────────────────────────────────────────

MSVC_ENV = None


def _find_compiler():
    global MSVC_ENV

    if platform.system() == 'Windows':
        vcvars = _find_vcvars()
        if vcvars:
            MSVC_ENV = _capture_msvc_env(vcvars)
            msvc_path = MSVC_ENV.get('PATH', MSVC_ENV.get('Path', ''))
            for d in msvc_path.split(';'):
                cl_candidate = os.path.join(d, 'cl.exe')
                if os.path.isfile(cl_candidate):
                    return 'msvc', cl_candidate
            cl = shutil.which('cl.exe', path=msvc_path)
            if cl:
                return 'msvc', cl

    for name, cid in [('g++', 'gcc'), ('clang++', 'clang')]:
        path = shutil.which(name)
        if path:
            return cid, path

    return None, None


COMPILER_ID, COMPILER_PATH = _find_compiler()


# ── Example extraction ──────────────────────────────────────────────────────

def extract_examples(md_path):
    with open(md_path, 'r', encoding='utf-8') as f:
        content = f.read()

    pattern = r'###\s+(.+?)\n.*?```cpp\n(.*?)```'
    matches = re.findall(pattern, content, re.DOTALL)

    examples = []
    for title, code in matches:
        title = title.strip()
        code = code.strip()
        if 'int main' in code:
            examples.append((title, code))
    return examples


# ── Compile + run ───────────────────────────────────────────────────────────

def _compile_msvc(tmp_cpp, tmp_exe, tmp_dir, tag):
    obj_file = os.path.join(tmp_dir, f'{tag}.obj')
    result = subprocess.run(
        [COMPILER_PATH, '/nologo', '/std:c++20', '/EHsc', '/W4',
         f'/I{INCLUDE_DIR}', tmp_cpp, f'/Fe:{tmp_exe}', f'/Fo:{obj_file}'],
        capture_output=True, text=True, timeout=60, cwd=tmp_dir,
        env=MSVC_ENV,
    )
    output = result.stdout + '\n' + result.stderr
    return result.returncode == 0, output.strip()


def _compile_gcc_clang(tmp_cpp, tmp_exe, tmp_dir, tag):
    result = subprocess.run(
        [COMPILER_PATH, '-std=c++20', '-Wall', '-Wextra', '-Wpedantic',
         f'-I{INCLUDE_DIR}', tmp_cpp, '-o', tmp_exe, '-pthread'],
        capture_output=True, text=True, timeout=60, cwd=tmp_dir,
    )
    output = result.stdout + '\n' + result.stderr
    return result.returncode == 0, output.strip()


def test_one(tag, code):
    """Compile and run a single example. Returns (status, output)."""
    tmp_dir = tempfile.mkdtemp(prefix='doc_test_')
    tmp_cpp = os.path.join(tmp_dir, f'{tag}.cpp')
    ext = '.exe' if platform.system() == 'Windows' else ''
    tmp_exe = os.path.join(tmp_dir, f'{tag}{ext}')

    with open(tmp_cpp, 'w', encoding='utf-8') as f:
        f.write(code)

    try:
        if COMPILER_ID == 'msvc':
            ok, output = _compile_msvc(tmp_cpp, tmp_exe, tmp_dir, tag)
        else:
            ok, output = _compile_gcc_clang(tmp_cpp, tmp_exe, tmp_dir, tag)

        if not ok:
            return 'COMPILE_ERROR', output

        try:
            run_env = MSVC_ENV if COMPILER_ID == 'msvc' else None
            run_result = subprocess.run(
                [tmp_exe], capture_output=True, text=True, timeout=10,
                env=run_env,
            )
            if run_result.returncode != 0:
                return 'RUNTIME_ERROR', (
                    f'Exit {run_result.returncode}\n'
                    f'{run_result.stdout}\n{run_result.stderr}'
                )
            return 'OK', run_result.stdout.strip()
        except subprocess.TimeoutExpired:
            return 'TIMEOUT', 'Execution timed out'
        except Exception as e:
            return 'RUN_ERROR', str(e)

    except subprocess.TimeoutExpired:
        return 'COMPILE_TIMEOUT', 'Compilation timed out'
    except Exception as e:
        return 'ERROR', str(e)
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)


# ── Main ────────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} <file.md> [file2.md ...]', file=sys.stderr)
        return 1

    if COMPILER_ID is None:
        print('ERROR: No C++20 compiler found (tried MSVC, g++, clang++).',
              file=sys.stderr)
        return 1

    print(f'Compiler : {COMPILER_ID} ({COMPILER_PATH})')

    md_files = sys.argv[1:]
    total_examples = 0
    for md_file in md_files:
        total_examples += len(extract_examples(md_file))
    print(f'Examples : {total_examples} across {len(md_files)} files\n', flush=True)

    wall_start = time.perf_counter()
    total_pass = 0
    total_fail = 0
    done = 0

    for md_file in md_files:
        basename = os.path.basename(md_file)
        base = os.path.splitext(basename)[0]
        examples = extract_examples(md_file)

        failures = []
        for i, (title, code) in enumerate(examples):
            tag = f'{base}_{i}'
            t0 = time.perf_counter()
            status, output = test_one(tag, code)
            dt = time.perf_counter() - t0
            done += 1
            marker = 'PASS' if status == 'OK' else 'FAIL'
            print(f'  [{done:3d}/{total_examples}] {marker}  {basename}#{i+1} {title}  ({dt:.1f}s)',
                  flush=True)
            if status != 'OK':
                failures.append((i + 1, title, status, output))
                for line in output.split('\n')[:5]:
                    print(f'           ! {line}')

        passed = len(examples) - len(failures)
        total_pass += passed
        total_fail += len(failures)

    elapsed = time.perf_counter() - wall_start
    print(f'\n=== TOTAL: {total_pass}/{total_pass + total_fail} passed  '
          f'({elapsed:.1f}s) ===')

    return total_fail


if __name__ == '__main__':
    sys.exit(main())
