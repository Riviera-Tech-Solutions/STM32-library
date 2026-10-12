// Exercises the real archive builder; no server or network required.
const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {Writable}=require('node:stream');
const {inflateRawSync}=require('node:zlib');
const assert=require('node:assert/strict');
const suite=fs.existsSync(path.resolve(__dirname,'../../services/zipBuilder.js')) ? path.resolve(__dirname,'../..') : path.resolve(__dirname,'..');
const {buildAndStreamZip}=require(path.join(suite,'services/zipBuilder'));
const treeData=require(path.join(suite,'data/folderTree.json'));
function unzip(buffer) {
  const entries=new Map();
  let end=buffer.length-22;
  while(end>=0 && buffer.readUInt32LE(end)!==0x06054b50)end--;
  assert(end>=0);
  let offset=buffer.readUInt32LE(end+16);
  const count=buffer.readUInt16LE(end+10);
  for(let i=0;i<count;i++) {
    assert.equal(buffer.readUInt32LE(offset),0x02014b50);
    const method=buffer.readUInt16LE(offset+10),size=buffer.readUInt32LE(offset+20);
    const nl=buffer.readUInt16LE(offset+28),el=buffer.readUInt16LE(offset+30),cl=buffer.readUInt16LE(offset+32);
    const local=buffer.readUInt32LE(offset+42),name=buffer.subarray(offset+46,offset+46+nl).toString();
    assert(!entries.has(name),'duplicate archive entry: '+name);
    const start=local+30+buffer.readUInt16LE(local+26)+buffer.readUInt16LE(local+28);
    const packed=buffer.subarray(start,start+size);
    entries.set(name,method===8?inflateRawSync(packed):packed);
    offset+=46+nl+el+cl;
  }
  return entries;
}
async function archive(family,core, nodeIds=['drivers'], name='test') {
  const chunks=[];
  const res=new Writable({write(c,e,done){chunks.push(c);done();}});
  res.setHeader=()=>{};res.status=()=>res;res.json=()=>{throw new Error('archive failure');};
  const done=new Promise((resolve,reject)=>{res.on('finish',resolve);res.on('error',reject);});
  const codeDir=fs.existsSync(path.join(suite,'code/core')) ? path.join(suite,'code') : suite;
  buildAndStreamZip({mcu:{id:name.toLowerCase(),name,family,core},nodeIds,treeData,codeDir},res,'test.zip');
  await done;return unzip(Buffer.concat(chunks));
}
module.exports={archive};
if(require.main===module) (async()=>{
  const out=fs.mkdtempSync(path.join(os.tmpdir(),'stm32-download-'));
  for(const [family,core] of [['F4','cortex-m4'],['G0','cortex-m0plus']]) {
    const entries=await archive(family,core);
    for(const name of ['drivers/common/stm32_family.h','drivers/common/stm32_irq.h','drivers/common/stm32_wait.h','drivers/README.md']) assert(entries.has(name),name);
    assert(entries.has('drivers/wdg/stm32_wdg.c'));assert(entries.has('drivers/fault/stm32_fault.c'));
    assert.match(entries.get('drivers/spi/stm32_spi.c').toString(),/Always drain/);
    const ld=entries.get('core/linker.ld').toString();
    assert.match(ld,/\.noinit \(NOLOAD\)/);assert.match(ld,/_sheap = _enoinit/);
    const asm=entries.get('core/startup/startup.s').toString();
    assert.match(asm,/\.syntax unified/);assert.match(asm,/\.weak/);
    const dir=path.join(out,family);fs.mkdirSync(dir);
    for(const name of ['core/linker.ld','core/startup/startup.s']) fs.writeFileSync(path.join(dir,path.basename(name)),entries.get(name));
    console.log(`PASS generated ${family} archive (${entries.size} unique entries)`);
  }
  console.log('Generated linker/startup samples: '+out);
})().catch(e=>{console.error(e);process.exitCode=1;});
