// Host register-model regressions. Production source is independently ARM-built.
// Instrument only MMIO pointer expressions and host-incompatible barrier asm.
const fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const {spawnSync}=require('node:child_process');
const code=path.resolve(__dirname,'..'),out=fs.mkdtempSync(path.join(os.tmpdir(),'stm32-test-'));
const cc=process.env.CC||'gcc';
function instrument(file) {
  let s=fs.readFileSync(file,'utf8').replaceAll('\r\n','\n');
  s=s.replace(/#include "([^"]+)"/g,(_,p)=>`#include "${path.resolve(path.dirname(file),p).replaceAll('\\','/')}"`);
  const cast=/\(volatile (uint32_t|uint8_t)\s*\*\)/g;
  let m;
  while((m=cast.exec(s))) {
    const start=m.index,end=cast.lastIndex;
    let pos=end;while(/\s/.test(s[pos]))pos++;
    let stop=pos;
    if(s[pos]==='(') { let depth=0; do {if(s[stop]==='(')depth++;if(s[stop]===')')depth--;stop++;}while(depth); }
    else {while(/[\w]/.test(s[stop]))stop++;}
    const replacement=`mock${m[1]==='uint8_t'?'8':'32'}(${s.slice(pos,stop)})`;
    s=s.slice(0,start)+replacement+s.slice(stop);cast.lastIndex=start+replacement.length;
  }
  return s.replace(/__asm volatile \("dsb"\);/g,'(void)0;');
}
function exec(args) {const r=spawnSync(cc,args,{encoding:'utf8'});if(r.status!==0)throw new Error(r.error||r.stderr);}
let count=0;
for(const [driver,family] of [['spi','F4'],['spi','G0'],['i2c','F4'],['i2c','G0'],['gpio','F1'],['tim','F4'],['uart','F4'],['wdg','F4'],...['F0','F1','F4','F7','G0','L4'].map(f=>['adc',f])]) {
  const source=path.join(out,`${driver}-${family}.c`),exe=path.join(out,`${driver}-${family}${process.platform==='win32'?'.exe':''}`);
  fs.writeFileSync(source,instrument(path.join(code,'drivers',driver,`stm32_${driver}.c`)));
  exec(['-std=c11','-O0','-DSTM32'+family,'-DSPI_TIMEOUT_LOOPS=10','-DUART_TX_TIMEOUT_LOOPS=10','-DWDG_TIMEOUT_LOOPS=10','-DADC_TIMEOUT_LOOPS=10','-DI2C_MAX_RETRIES=0',
    '-DDRIVER_SOURCE="'+source.replaceAll('\\','/')+'"',path.join(__dirname,driver+'.c'),'-o',exe]);
  const r=spawnSync(exe,[],{encoding:'utf8',timeout:3000});
  if(r.status!==0)throw new Error(`${driver}/${family}: ${r.error||r.stderr}`);
  count++;console.log(`PASS ${driver}/${family}`);
}
const wait=path.join(code,'drivers/common/stm32_wait.h').replaceAll('\\','/');
const waitSource=path.join(out,'wait.c'),waitExe=path.join(out,'wait.exe');
fs.writeFileSync(waitSource,`#include <stdint.h>\n#include <assert.h>\nstatic uint32_t tick;\n#define STM32_WAIT_USE_TICK 1\n#define STM32_WAIT_GET_TICK_MS() tick\n#include "${wait}"\nint main(void){ volatile uint32_t r=0; assert(!stm32_wait_mask_eq32(&r,1,1,1,10)); r=1; assert(stm32_wait_mask_eq32(&r,1,1,1,0)); return 0; }`);
exec([waitSource,'-o',waitExe]);let r=spawnSync(waitExe,[],{encoding:'utf8',timeout:1000});if(r.status!==0)throw new Error(r.error||r.stderr);
fs.writeFileSync(waitSource,`#define STM32_WAIT_USE_TICK 1\n#include "${wait}"`);
r=spawnSync(cc,['-fsyntax-only',waitSource],{encoding:'utf8'});if(r.status===0||!r.stderr.includes('Tick waits require'))throw new Error('Missing tick hook must fail compilation');
console.log(`PASS ${count} register-model cases; frozen tick and missing-hook rejection. Output: ${out}`);
