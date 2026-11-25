"""Generate private translation inputs from an extracted USA disc, then build Windows x64."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parent.parent

def run(args,env=None):subprocess.run(list(map(str,args)),cwd=ROOT,env=env,check=True)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--game-tree',type=Path,help='Extracted DATA directory containing sys/main.dol and files/*.rso');p.add_argument('--prepare-only',action='store_true',help='Validate inputs and generate analysis image/configuration, without translation or compilation');p.add_argument('--threads',type=int,default=2);a=p.parse_args()
 if a.threads<1:raise SystemExit('--threads must be positive')
 target=ROOT/'game/extracted/DATA'
 if a.game_tree:
  original=a.game_tree.expanduser().resolve()
  if not (original/'sys/main.dol').is_file() or not (original/'files/gs2.sel').is_file():raise SystemExit('Expected a DATA directory with sys/main.dol and files/gs2.sel')
  if target.exists() and target.resolve()!=original:raise SystemExit('game/extracted/DATA already exists. Move it aside explicitly or reuse that same tree.')
  if not target.exists():target.parent.mkdir(parents=True,exist_ok=True);target.symlink_to(original,target_is_directory=True)
 if not (target/'sys/main.dol').is_file():raise SystemExit('Provide --game-tree or prepare game/extracted/DATA. See docs/BUILDING.md.')
 expected=json.loads((ROOT/'config/game-inputs.json').read_text())
 for name,digest in expected['files'].items():
  source=target/name
  if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=digest:raise SystemExit('Wrong or modified game input: '+name+'; milestone requires the recorded USA revision.')
 base=ROOT/'combined-build';base.mkdir(exist_ok=True)
 for name in ['function-budgets.json','required-module-functions.json']:shutil.copy2(ROOT/'config'/name,base/name)
 run([sys.executable,'tooling/prepare_combined.py']);run([sys.executable,'tooling/prepare_rso_runtime.py'])
 project=base/'project.yml'
 data_project=base/'data-project.yml';data_project.write_text(project.read_text().replace(str(base/'combined-analysis.dol'),str(target/'sys/main.dol')))
 if a.prepare_only:print('Private analysis inputs prepared; no compilation performed.');return
 run([sys.executable,'scripts/prepare_dependencies.py','--check'])
 dotnet=shutil.which('dotnet')
 if not dotnet:raise SystemExit('Install .NET SDK 8 and put dotnet on PATH.')
 cli_project=ROOT/'project/Wiicompiled/translator/src/Translator.Cli/Translator.Cli.csproj'
 run([dotnet,'build',cli_project,'-c','Release'])
 cli=cli_project.parent/'bin/Release/net8.0/Translator.Cli.dll'
 env={**os.environ,'RECOMP_FUNCTION_BUDGETS':str(base/'function-budgets.json'),'RECOMP_RUNTIME_IMMEDIATES':str(base/'runtime-immediates.txt')}
 run([dotnet,cli,'translate-recursive','0x80006310','--project',project,'--threads',a.threads,'--output-metadata',base/'base_translation_output.json','--prune-stale'],env)
 run([sys.executable,'tooling/check_module_coverage.py'])
 run([dotnet,cli,'emit-build-shards','--project',project,'--base-metadata',base/'base_translation_output.json','--base-functions-dir',base/'generated/functions','--native-source-dir',ROOT/'port/native','--native-index',ROOT/'port/native-index.json','--out',base/'shards'],env)
 run([dotnet,cli,'generate-data-init','--project',data_project],env)
 run([sys.executable,'tooling/add_dsp_boot.py'])
 (base/'retranslation-required').unlink(missing_ok=True)
 run([sys.executable,'tooling/build_v9.py','windows','o1'])
 out=ROOT/'dist/v20-local';out.mkdir(parents=True,exist_ok=True);shutil.copy2(base/'windows/StarSuccessorBoot.exe',out/'StarSuccessor.exe')
 for source in (ROOT/'launcher').iterdir():
  if source.is_file():shutil.copy2(source,out/source.name)
 shutil.copytree(ROOT/'licenses',out/'licenses',dirs_exist_ok=True)
 identity={'revision':'v20-local-rebuild','executable_sha256':hashlib.sha256((out/'StarSuccessor.exe').read_bytes()).hexdigest(),'guest_optimization':'O1','runtime_optimization':'O2','windows_runtime_verified':False,'fps60_verified':False}
 (out/'BUILD.json').write_text(json.dumps(identity,indent=2)+'\n')
 print('Local private executable:',out/'StarSuccessor.exe');print('Do not upload dist/, game/, or combined-build/ to the public repository.')
if __name__=='__main__':main()
