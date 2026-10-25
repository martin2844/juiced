const fs=require('fs');
const draco=require('draco3dgltf');
(async()=>{
 const {MeshoptSimplifier}=await import('meshoptimizer'); await MeshoptSimplifier.ready;
 const b=fs.readFileSync(process.argv[2]),len=b.readUInt32LE(12),j=JSON.parse(b.subarray(20,20+len));
 const bin=b.subarray(28+len); const d=await draco.createDecoderModule({});
 for(const m of j.meshes)for(const p of m.primitives){
  const e=p.extensions.KHR_draco_mesh_compression, v=j.bufferViews[e.bufferView];
  const decoder=new d.Decoder(), buffer=new d.DecoderBuffer();buffer.Init(new Int8Array(bin.subarray(v.byteOffset,v.byteOffset+v.byteLength)),v.byteLength);
  const mesh=new d.Mesh(); const status=decoder.DecodeBufferToMesh(buffer,mesh);if(!status.ok())throw Error(status.error_msg());
  p.decoded={};
  for(const [name,id] of Object.entries(e.attributes)){
   const a=decoder.GetAttributeByUniqueId(mesh,id), values=new d.DracoFloat32Array();decoder.GetAttributeFloatForAllPoints(mesh,a,values);
   p.decoded[name]=Array.from({length:values.size()},(_,i)=>values.GetValue(i));d.destroy(values);
  }
  const face=new d.DracoInt32Array();p.decoded.indices=[];
  for(let i=0;i<mesh.num_faces();i++){decoder.GetFaceFromMesh(mesh,i,face);for(let k=0;k<3;k++)p.decoded.indices.push(face.GetValue(k));}
  if(p.decoded.indices.length>900){
   const [indices,error]=MeshoptSimplifier.simplifyWithAttributes(new Uint32Array(p.decoded.indices),new Float32Array(p.decoded.POSITION),3,new Float32Array(p.decoded.NORMAL),3,[1,1,1],null,Math.floor(p.decoded.indices.length*.25/3)*3,.003,['Permissive']);
   p.decoded.indices=Array.from(indices);p.decoded.simplificationError=error;
  }
  d.destroy(face);d.destroy(mesh);d.destroy(buffer);d.destroy(decoder);
 }
 fs.writeFileSync(process.argv[3],JSON.stringify(j));
 console.log('decoded',j.meshes.length,'meshes',j.meshes.reduce((n,m)=>n+m.primitives.reduce((n,p)=>n+p.decoded.indices.length/3,0),0),'triangles');
})();
