// Run after an actual unified-engine run. This checks real data, not a UI fixture.
import {readFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
const state=JSON.parse(await readFile(new URL('../../PortSim/Saved/Dashboard/state.json',import.meta.url),'utf8'));
assert.equal(state.schema_version,1);assert.equal(state.unified,true);
assert.equal(new Set(state.actors.map(a=>a.id)).size,state.actors.length);
const byType=t=>state.actors.filter(a=>a.type===t);
assert.equal(byType('sts').length,9);assert.equal(byType('agv').length,9);assert.equal(byType('rmg').length,36);assert.equal(byType('container').length,1584);
assert.equal(state.vessels.length,3);assert.equal(state.vessels.reduce((n,v)=>n+v.initial_count,0),1584);
for(const a of state.actors)assert(a.position_m.every(Number.isFinite));
for(const a of byType('sts')){
 assert.equal(a.hoist_power_w,670000);assert(a.rated_payload_kg>65000);
 assert.equal(a.observation.corner_loads_n.length,4);assert.equal(a.observation.locks.length,4);
 if(a.carrying&&a.observation.valid)assert(Math.abs(a.observation.corner_loads_n.reduce((x,y)=>x+y,0)/9.80665-a.payload_kg)<1);
}
for(const a of byType('rmg'))assert.equal(a.observation,undefined);
console.log(`PASS real telemetry: ${state.actors.length} entities, ${state.initial_ship} cargo, 9 STS / 9 AGV / 36 RMG`);
