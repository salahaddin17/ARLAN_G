import { MAP_TILES, TILE } from '../data/balance';
import type { World } from './world';

// ============================================================================
// Туман войны игрока, два уровня: 0 — не разведано, 1 — разведано (память),
// 2 — видно сейчас. Пересчёт периодический (каждые 4 тика).
// ============================================================================

export function recomputeFog(world: World): void {
  const fog = world.fog;
  // понизить «видно» до «разведано»
  for (let i = 0; i < fog.length; i++) if (fog[i] === 2) fog[i] = 1;

  const stamp = (wx: number, wy: number, visionTiles: number) => {
    const cx = Math.floor(wx / TILE), cy = Math.floor(wy / TILE);
    const r = visionTiles;
    const r2 = (r + 0.5) * (r + 0.5);
    for (let dy = -r; dy <= r; dy++) {
      const y = cy + dy;
      if (y < 0 || y >= MAP_TILES) continue;
      for (let dx = -r; dx <= r; dx++) {
        const x = cx + dx;
        if (x < 0 || x >= MAP_TILES) continue;
        if (dx * dx + dy * dy <= r2) fog[y * MAP_TILES + x] = 2;
      }
    }
  };

  for (const u of world.units.values()) {
    if (u.team === 0) stamp(u.x, u.y, u.vision);
  }
  for (const b of world.buildings.values()) {
    if (b.team === 0) stamp(b.x, b.y, b.buildProgress >= 1 ? b.vision : 2);
  }

  // обновить память о зданиях противника
  for (const b of world.buildings.values()) {
    if (b.team !== 1) continue;
    const tx = Math.floor(b.x / TILE), ty = Math.floor(b.y / TILE);
    const i = Math.min(fog.length - 1, Math.max(0, ty * MAP_TILES + tx));
    if (fog[i] === 2) {
      world.buildingMemory.set(b.id, {
        id: b.id, kind: b.kind, team: b.team,
        tx: b.tx, ty: b.ty, w: b.w, h: b.h, x: b.x, y: b.y,
      });
    }
  }
}
