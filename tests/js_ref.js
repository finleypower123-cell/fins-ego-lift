const fs=require('fs'); const dir=process.argv[2], out=process.argv[3];
const src=fs.readFileSync(dir+'/engine.js','utf8')+'\n'+fs.readFileSync(dir+'/shared.js','utf8')+'\nmodule.exports={FinEngine,FEL_PRESETS,felPresetState,felApplyEgo,felSynthDemo};';
const m={exports:{}};new Function('module',src)(m);
const {FinEngine,FEL_PRESETS,felPresetState,felApplyEgo,felSynthDemo}=m.exports;
const SR=48000, demo=felSynthDemo(SR); fs.writeFileSync(out+'/demo.f32', Buffer.from(demo.buffer));
for(let i=0;i<FEL_PRESETS.length;i++) for(const ego of [0.5,1]){
  const st=felPresetState(i); st.ego=ego; if(process.env.NOCRUSH) st.crush=0; const e=new FinEngine(SR); {const ee=felApplyEgo(st); if(process.env.NOCRUSH) ee.crush=0; e.setParams(ee,true);}
  const n=demo.length+SR*2, L=new Float32Array(n), R=new Float32Array(n), B=128, ib=new Float32Array(B), o1=new Float32Array(B), o2=new Float32Array(B);
  for(let s=0;s<n;s+=B){const c=Math.min(B,n-s);for(let k=0;k<c;k++)ib[k]=s+k<demo.length?demo[s+k]:0;e.process(ib,ib,o1,o2,c);for(let k=0;k<c;k++){L[s+k]=o1[k];R[s+k]=o2[k];}}
  fs.writeFileSync(`${out}/js_${i}_${ego}.f32`, Buffer.from(L.buffer)); fs.writeFileSync(`${out}/jsR_${i}_${ego}.f32`, Buffer.from(R.buffer));
}
console.log('latency', new FinEngine(SR).latency);
