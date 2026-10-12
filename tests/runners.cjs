// Stub external commands to verify arguments and failure propagation without hardware.
const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {spawnSync}=require('node:child_process');
const assert=require('node:assert/strict');
const {archive}=require('./download.cjs');
const root=fs.mkdtempSync(path.join(os.tmpdir(),'stm32 runner '));
(async()=>{
 const entries=await archive('F4','cortex-m4',[],'STM32F401CC');
 const dir=path.join(root,'project with spaces'),bin=path.join(root,'mock tools');fs.mkdirSync(dir);fs.mkdirSync(bin);
 for(const name of ['rts.sh','rts.ps1','rts.env'])fs.writeFileSync(path.join(dir,name),entries.get(name));
 fs.mkdirSync(path.join(dir,'build'));fs.writeFileSync(path.join(dir,'build/firmware.bin'),'test');
 const source=path.join(root,'mock.c');
 fs.writeFileSync(source,'#include <stdio.h>\n#include <stdlib.h>\nint main(int argc,char **argv){ FILE *f=fopen(getenv("RTS_TRACE"),"a"); for(int i=0;i<argc;i++)fprintf(f,"[%s]",argv[i]);fprintf(f,"\\n");fclose(f);return getenv("RTS_FAIL")?7:0;}');
 const exe=path.join(bin,process.platform==='win32'?'mock.exe':'mock');
 const compile=spawnSync(process.env.CC||'gcc',[source,'-o',exe],{encoding:'utf8'});assert.equal(compile.status,0,compile.stderr);
 for(const tool of ['cmake','ninja','arm-none-eabi-gcc','STM32_Programmer_CLI','openocd'])fs.copyFileSync(exe,path.join(bin,tool+(process.platform==='win32'?'.exe':'')));
 const runners=process.platform==='win32' ? [['powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',path.join(dir,'rts.ps1')]]] : [['sh',[path.join(dir,'rts.sh')]]];
 if(process.env.BASH)runners.push([process.env.BASH,[path.join(dir,'rts.sh')]]);
 for(const [program,prefix] of runners){
  const trace=path.join(root,'trace');
  const env={...process.env,PATH:bin+path.delimiter+process.env.PATH,RTS_TRACE:trace};
  const call=(command,fail=false)=>spawnSync(program,[...prefix,command],{cwd:root,env:{...env,...(fail?{RTS_FAIL:'1'}:{})},encoding:'utf8',timeout:15000});
  fs.writeFileSync(path.join(dir,'rts.env'),entries.get('rts.env'));
  assert.notEqual(call('flash').status,0);assert(!fs.existsSync(trace),'unconfirmed flash contacted a tool');
  assert.equal(call('build').status,0,'build should work without confirmed hardware');
  let config=entries.get('rts.env').toString().replace('BOARD_CONFIG_CONFIRMED=false','BOARD_CONFIG_CONFIRMED=true').replace('MEMORY_CONFIG_CONFIRMED=false','MEMORY_CONFIG_CONFIRMED=true').replace('BOARD_LABEL=SET_THIS_TO_YOUR_EXACT_BOARD','BOARD_LABEL=Board with spaces');
  fs.writeFileSync(path.join(dir,'rts.env'),config.replace('PROBE_SERIAL=','PROBE_SERIAL=probe 123'));
  assert.equal(call('flash').status,0,'confirmed mock flash failed');
  assert.match(fs.readFileSync(trace,'utf8'),/\[port=SWD\]\[sn=probe 123\]\[-w\]\[build\/firmware.bin\]\[0x08000000\]\[-v\]\[-rst\]/);
  fs.writeFileSync(path.join(dir,'rts.env'),config.replace('TOOL=stm32_programmer_cli','TOOL=openocd').replace('OPENOCD_CFG=','OPENOCD_CFG=board with spaces.cfg'));
  assert.equal(call('flash').status,0,'OpenOCD mock flash failed');
  assert.match(fs.readFileSync(trace,'utf8'),/\[-f\]\[board with spaces.cfg\]\[-c\]\[program build\/firmware.bin 0x08000000 verify reset exit\]/);
  fs.writeFileSync(trace,'');assert.notEqual(call('flash',true).status,0,'command failure must propagate');
  assert(!fs.readFileSync(trace,'utf8').includes('openocd'),'failed build still flashed');
  fs.writeFileSync(path.join(dir,'rts.env'),config.replace('TOOL=stm32_programmer_cli','TOOL=unknown'));
  assert.notEqual(call('flash').status,0,'unsupported tool accepted');
  fs.unlinkSync(trace);
  console.log(`PASS ${program}: build, confirmation guard, quoting, tool selection and failure propagation`);
 }
})().catch(e=>{console.error(e);process.exitCode=1;});
