// ARM compile matrix. ARM_CC and ARM_OBJDUMP can be absolute toolchain paths.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const code = path.resolve(__dirname, '..');
const out = fs.mkdtempSync(path.join(os.tmpdir(), 'stm32-build-'));
const cc = process.env.ARM_CC || 'arm-none-eabi-gcc';
const families = {
  F0:'m0', F1:'m3', F2:'m3', F3:'m4', F4:'m4', F7:'m7',
  G0:'m0plus', G4:'m4', H5:'m33', H7:'m7', L0:'m0plus', L1:'m3',
  L4:'m4', L4PLUS:'m4', L5:'m33', U5:'m33', WB:'m4', WL:'m4'
};
const drivers = ['gpio','i2c','spi','uart','tim','adc','wdg','fault'];
let builds = 0;
let links = 0;
function run(args) {
  const r = spawnSync(cc,args,{encoding:'utf8'});
  if (r.status !== 0) throw new Error(`${args.join(' ')}\n${r.error || r.stderr}`);
  return r;
}
for (const [family,cpu] of Object.entries(families)) {
  for (const driver of drivers) {
    run(['-std=c11','-mcpu=cortex-'+cpu,'-mthumb','-O2','-Wall','-Wextra',
      '-Werror','-Wno-error=cpp','-ffreestanding','-DSTM32'+family,
      '-c',path.join(code,'drivers',driver,`stm32_${driver}.c`),
      '-o',path.join(out,`${family}-${driver}.o`)]);
    builds++;
  }
  const core=path.join(code,'core','cortex-'+cpu,'stm32'+family.toLowerCase());
  const startup=path.join(core,'startup','startup_stm32'+family.toLowerCase()+'.s');
  const system=path.join(core,'system','system_stm32'+family.toLowerCase()+'.c');
  const smoke=path.join(out,`${family}-main.c`);
  fs.writeFileSync(smoke,'#include "'+path.join(code,'drivers/fault/stm32_fault.h').replaceAll('\\','/')+'"\nvolatile int initialized_data=7; volatile int zero_data; int main(void) { fault_record_t f; zero_data=fault_get_last(&f); return initialized_data; }');
  const elf=path.join(out,`${family}.elf`);
  run(['-mcpu=cortex-'+cpu,'-mthumb','-O2','-DSTM32'+family,'-nostdlib',
    '-Wl,-e,Reset_Handler','-T',path.join(core,'linker.ld'),startup,system,smoke,
    ...drivers.map(d=>path.join(out,`${family}-${d}.o`)),
    '-Wl,--start-group','-lc','-lgcc','-Wl,--end-group','-o',elf]);
  const objdump=process.env.ARM_OBJDUMP || cc.replace(/gcc(\.exe)?$/, 'objdump$1');
  const dump=spawnSync(objdump,['-h','-t',elf],{encoding:'utf8'});
  if(dump.status!==0) throw new Error(dump.error||dump.stderr);
  const lines=dump.stdout.split('\n');
  const retained=lines.findIndex(l=>/\s\.noinit\s/.test(l));
  if(retained<0 || /LOAD|CONTENTS/.test(lines[retained+1])) throw new Error(`${family}: crash record must be NOLOAD`);
  if(!lines.some(l=>/\bg\s+F\s+\.text\s+\w+\s+HardFault_Handler/.test(l))) throw new Error(`${family}: fault handler did not override startup`);
  links++;
  const builder=path.resolve(code,'../services/zipBuilder.js');
  if ((family==='F4'||family==='G0') && fs.existsSync(builder)) {
    const {buildStartupContent,buildLinkerContent}=require(builder);
    const generatedStartup=path.join(out,`${family}-download.s`),generatedLinker=path.join(out,`${family}-download.ld`);
    fs.writeFileSync(generatedStartup,buildStartupContent('cortex-'+cpu));
    fs.writeFileSync(generatedLinker,buildLinkerContent(family));
    run(['-mcpu=cortex-'+cpu,'-mthumb','-O2','-DSTM32'+family,'-nostdlib',
      '-Wl,-e,Reset_Handler','-T',generatedLinker,generatedStartup,system,smoke,
      ...drivers.map(d=>path.join(out,`${family}-${d}.o`)),
      '-Wl,--start-group','-lc','-lgcc','-Wl,--end-group','-o',path.join(out,`${family}-download.elf`)]);
    links++;
  }
}
console.log(`PASS: ${builds} ARM driver compilations and ${links} firmware links with retained fault records. Output: ${out}`);
