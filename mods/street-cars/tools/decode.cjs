// Decode a self-contained meshopt GLB without changing geometry or textures.
const fs=require('fs');
(async()=>{
 const {MeshoptDecoder}=await import('../../ferrari/tools/node_modules/meshoptimizer/index.js');
 await MeshoptDecoder.ready;
 const b=fs.readFileSync(process.argv[2]);const len=b.readUInt32LE(12);
 const j=JSON.parse(b.subarray(20,20+len));const bin=b.subarray(28+len);
 let size=0;const chunks=[];
 for(const v of j.bufferViews){
  const ext=v.extensions?.EXT_meshopt_compression; let data;
  if(ext){data=Buffer.alloc(ext.count*ext.byteStride);MeshoptDecoder.decodeGltfBuffer(data,ext.count,ext.byteStride,bin.subarray(ext.byteOffset||0,(ext.byteOffset||0)+ext.byteLength),ext.mode,ext.filter);delete v.extensions.EXT_meshopt_compression;}
  else data=Buffer.from(bin.subarray(v.byteOffset||0,(v.byteOffset||0)+v.byteLength));
  v.buffer=0;v.byteOffset=size;v.byteLength=data.length;chunks.push(data);size+=data.length;
  const pad=(4-size%4)%4;if(pad){chunks.push(Buffer.alloc(pad));size+=pad;}
 }
 j.buffers=[{byteLength:size}];
 for(const key of ['extensionsUsed','extensionsRequired'])if(j[key])j[key]=j[key].filter(x=>x!=='EXT_meshopt_compression');
 let json=Buffer.from(JSON.stringify(j));json=Buffer.concat([json,Buffer.alloc((4-json.length%4)%4,32)]);
 const head=Buffer.alloc(20);head.write('glTF');head.writeUInt32LE(2,4);head.writeUInt32LE(28+json.length+size,8);head.writeUInt32LE(json.length,12);head.writeUInt32LE(0x4e4f534a,16);
 const bh=Buffer.alloc(8);bh.writeUInt32LE(size);bh.writeUInt32LE(0x004e4942,4);
 fs.writeFileSync(process.argv[3],Buffer.concat([head,json,bh,...chunks]));console.log(process.argv[3],size);
})();
