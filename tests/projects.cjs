// Test the downloaded files, not a hand-written substitute project.
const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {spawnSync}=require('node:child_process');
const assert=require('node:assert/strict');
const {archive}=require('./download.cjs');
const root=fs.mkdtempSync(path.join(os.tmpdir(),'stm32 projects '));
const cmake=process.env.CMAKE || 'cmake';
const families={F0:'m0',F1:'m3',F2:'m3',F3:'m4',F4:'m4',F7:'m7',G0:'m0plus',G4:'m4',H5:'m33',H7:'m7',L0:'m0plus',L1:'m3',L4:'m4',L4PLUS:'m4',L5:'m33',U5:'m33',WB:'m4',WL:'m4'};
const cases=process.env.FULL_MATRIX ? Object.entries(families).flatMap(([f,c])=>[[f,`cortex-${c}`,`STM32${f}`,false],[f,`cortex-${c}`,`STM32${f}`,true]]) : [
 ['F4','cortex-m4','STM32F401CC',false],['F4','cortex-m4','STM32F401RE',true],
 ['G0','cortex-m0plus','STM32G071RB',false],['G0','cortex-m0plus','STM32G071RB',true],
 ['F1','cortex-m3','STM32F103C8',true]
];
function run(exe,args,cwd,expected=0){
 const r=spawnSync(exe,args,{cwd,encoding:'utf8',timeout:120000});
 assert.equal(r.status,expected,`${exe} ${args.join(' ')}\n${r.error||''}\n${r.stdout}\n${r.stderr}`);
 return r.stdout+r.stderr;
}
(async()=>{
 let count=0;
 for(const [family,core,name,rtos] of cases){
  const entries=await archive(family,core,['drivers','peripherals',...(rtos?['rtos_freertos']:[])],name);
  const dir=path.join(root,`${name}-${rtos?'rtos':'bare'}`);fs.mkdirSync(dir);
  for(const [file,bytes] of entries){const dest=path.join(dir,file);fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest,bytes);}
  for(const file of ['main.c','CMakeLists.txt','AGENTS.md','rts.env','rts.ps1','rts.sh'])assert(entries.has(file),file);
  assert(!entries.has('Makefile'));
  if(name==='STM32F401CC')assert.match(entries.get('core/linker.ld').toString(),/LENGTH = 64K/);
  if(name==='STM32F401RE')assert.match(entries.get('core/linker.ld').toString(),/LENGTH = 96K/);
  if(rtos){const cfg=entries.get('rtos/freertos/config/FreeRTOSConfig.h').toString();assert(!cfg.includes('configUSE_16_BIT_TICKS'));assert.match(cfg,/vPortSVCHandler SVC_Handler/);}
  run(cmake,['-S','.','-B','build','-G','Ninja'],dir);
  run(cmake,['--build','build'],dir);
  assert(fs.statSync(path.join(dir,'build/firmware.bin')).size>0);
  const binary=fs.readFileSync(path.join(dir,'build/firmware.bin'));
  const expectedStack={STM32F401CC:0x20010000,STM32F401RE:0x20018000,STM32G071RB:0x20009000,STM32F103C8:0x20005000}[name];
  if(expectedStack)assert.equal(binary.readUInt32LE(0),expectedStack,'stack must match exact-part RAM');
  assert(binary.readUInt32LE(4)&1,'reset vector must select Thumb state');
  const objdump=process.env.ARM_OBJDUMP||'arm-none-eabi-objdump';
  const symbols=run(objdump,['-t','build/firmware.elf'],dir);
  if(rtos) for(const handler of ['SVC_Handler','PendSV_Handler','SysTick_Handler'])assert.match(symbols,new RegExp('g\\s+F\\s+\\.text\\s+\\w+\\s+'+handler));
  // No hardware commands: unconfirmed flash must fail before invoking a probe.
  if(process.platform==='win32'){
   run('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File','rts.ps1','build'],dir);
   run('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File','rts.ps1','flash'],dir,1);
  }else{
   run('sh',['rts.sh','build'],dir);
   run('sh',['rts.sh','flash'],dir,2);
  }
  console.log(`PASS downloaded ${name} ${rtos?'FreeRTOS':'bare-metal'} project`);count++;
 }
 // Select a peripheral alone and ensure its transitive dependency is included.
 const sensor=await archive('F4','cortex-m4',['periph_bme280'],'STM32F401CC');
 assert(sensor.has('drivers/i2c/stm32_i2c.c'),'peripheral dependency missing');
 const grouped=await archive('G0','cortex-m0plus',['root'],'STM32G071RB');
 assert(!Array.from(grouped.keys()).some(n=>n.includes('/ARM_CM4F/')),'group selection included incorrect RTOS ports');
 console.log(`PASS ${count} real ZIP builds and flash guards. Artifacts: ${root}`);
})().catch(e=>{console.error(e);process.exitCode=1;});
