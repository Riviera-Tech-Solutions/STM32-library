// The website and CLI use the same archive builder and source files.
const fs=require('node:fs'),path=require('node:path');
const suite=fs.existsSync(path.join(__dirname,'services/zipBuilder.js')) ? __dirname : path.resolve(__dirname,'..');
const {buildAndStreamZip}=require(path.join(suite,'services/zipBuilder.js'));
const catalog=require(path.join(suite,'data/mcuList.json'));
const treeData=require(path.join(suite,'data/folderTree.json'));
const [name,dest,...flags]=process.argv.slice(2);
if(!name || !dest || flags.some(f=>f!=='--freertos')){
 console.error('Usage: node create-project.cjs STM32F401RE project.zip [--freertos]');process.exit(2);
}
const mcu=catalog.mcus.find(m=>m.name.toLowerCase()===name.toLowerCase() || m.id.toLowerCase()===name.toLowerCase());
if(!mcu){console.error('Unknown MCU. Use an exact name from data/mcuList.json.');process.exit(2);}
const output=fs.createWriteStream(path.resolve(dest),{flags:'wx'});
output.setHeader=()=>{};output.status=()=>output;
output.on('error',e=>{console.error(e.message);process.exitCode=1;});
output.on('finish',()=>console.log(`Created ${dest}. Extract, then follow README.txt. No hardware validation is implied.`));
buildAndStreamZip({mcu,nodeIds:['drivers',...(flags.includes('--freertos')?['rtos_freertos']:[])],treeData,codeDir:__dirname},output,path.basename(dest));
