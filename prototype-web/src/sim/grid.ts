import { MAP_TILES, TILE } from '../data/balance';

// ============================================================================
// Сетка проходимости и генерация степной карты (детерминированный шум).
// ============================================================================

export function mulberry32(seed: number): () => number {
  let a = seed >>> 0;
  return () => {
    a |= 0; a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export class MapGrid {
  readonly size = MAP_TILES;
  /** 0 — свободно, 1 — скала (статично), 2 — здание */
  readonly cells: Uint8Array;
  /** высота рельефа 0..1 по клеткам (для изолиний) */
  readonly height: Float32Array;

  constructor() {
    this.cells = new Uint8Array(this.size * this.size);
    this.height = new Float32Array(this.size * this.size);
  }

  inBounds(x: number, y: number): boolean {
    return x >= 0 && y >= 0 && x < this.size && y < this.size;
  }
  idx(x: number, y: number): number {
    return y * this.size + x;
  }
  isBlocked(x: number, y: number): boolean {
    if (!this.inBounds(x, y)) return true;
    return this.cells[this.idx(x, y)] !== 0;
  }
  isRock(x: number, y: number): boolean {
    return this.inBounds(x, y) && this.cells[this.idx(x, y)] === 1;
  }
  setBuilding(x: number, y: number, w: number, h: number, on: boolean): void {
    for (let ty = y; ty < y + h; ty++)
      for (let tx = x; tx < x + w; tx++)
        if (this.inBounds(tx, ty)) this.cells[this.idx(tx, ty)] = on ? 2 : 0;
  }
}

/** Плавный value-noise: билинейная интерполяция решётки случайных значений. */
function makeNoise(rand: () => number, gridN: number): (x: number, y: number) => number {
  const g = new Float32Array((gridN + 1) * (gridN + 1));
  for (let i = 0; i < g.length; i++) g[i] = rand();
  const at = (x: number, y: number) => g[y * (gridN + 1) + x];
  return (x: number, y: number) => {
    const fx = x * gridN, fy = y * gridN;
    const x0 = Math.min(gridN - 1, Math.max(0, Math.floor(fx)));
    const y0 = Math.min(gridN - 1, Math.max(0, Math.floor(fy)));
    let tx = fx - x0, ty = fy - y0;
    tx = tx * tx * (3 - 2 * tx);
    ty = ty * ty * (3 - 2 * ty);
    const a = at(x0, y0), b = at(x0 + 1, y0), c = at(x0, y0 + 1), d = at(x0 + 1, y0 + 1);
    return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty;
  };
}

export interface MapLayout {
  grid: MapGrid;
  /** клетка левого-верхнего угла КЦ каждой команды */
  hqTiles: [{ x: number; y: number }, { x: number; y: number }];
  depotSpots: { x: number; y: number; central: boolean }[]; // мировые координаты
  heightAt: (wx: number, wy: number) => number;             // для изолиний рендера
}

export const ROCK_LEVEL = 0.66;

export function generateMap(seed: number): MapLayout {
  const rand = mulberry32(seed);
  const n1 = makeNoise(rand, 6);
  const n2 = makeNoise(rand, 13);
  const n3 = makeNoise(rand, 27);
  const grid = new MapGrid();
  const N = grid.size;

  const heightAt = (wx: number, wy: number): number => {
    const u = wx / (N * TILE), v = wy / (N * TILE);
    let h = n1(u, v) * 0.55 + n2(u, v) * 0.3 + n3(u, v) * 0.15;
    // приподнять центр чуть-чуть, прижать края — степная чаша
    const dx = u - 0.5, dy = v - 0.5;
    h += 0.06 - (dx * dx + dy * dy) * 0.25;
    return Math.min(1, Math.max(0, h));
  };

  for (let y = 0; y < N; y++) {
    for (let x = 0; x < N; x++) {
      const h = heightAt((x + 0.5) * TILE, (y + 0.5) * TILE);
      grid.height[grid.idx(x, y)] = h;
      if (h > ROCK_LEVEL) grid.cells[grid.idx(x, y)] = 1;
    }
  }

  // Базы: игрок — юго-запад, ИИ — северо-восток.
  const hqTiles: [{ x: number; y: number }, { x: number; y: number }] = [
    { x: 8, y: 50 },
    { x: 52, y: 9 },
  ];

  const clearRect = (cx: number, cy: number, r: number) => {
    for (let y = cy - r; y <= cy + r; y++)
      for (let x = cx - r; x <= cx + r; x++)
        if (grid.inBounds(x, y) && grid.cells[grid.idx(x, y)] === 1) {
          grid.cells[grid.idx(x, y)] = 0;
          grid.height[grid.idx(x, y)] = Math.min(grid.height[grid.idx(x, y)], ROCK_LEVEL - 0.04);
        }
  };

  // Расчистить базы и гарантировать связность: два Г-образных коридора.
  clearRect(hqTiles[0].x + 2, hqTiles[0].y + 1, 9);
  clearRect(hqTiles[1].x + 2, hqTiles[1].y + 1, 9);
  const carvePath = (pts: { x: number; y: number }[]) => {
    for (let i = 0; i + 1 < pts.length; i++) {
      const a = pts[i], b = pts[i + 1];
      const steps = Math.max(Math.abs(b.x - a.x), Math.abs(b.y - a.y));
      for (let s = 0; s <= steps; s++) {
        const x = Math.round(a.x + ((b.x - a.x) * s) / Math.max(1, steps));
        const y = Math.round(a.y + ((b.y - a.y) * s) / Math.max(1, steps));
        clearRect(x, y, 2);
      }
    }
  };
  carvePath([{ x: 10, y: 52 }, { x: 32, y: 32 }, { x: 54, y: 11 }]); // диагональ
  carvePath([{ x: 10, y: 52 }, { x: 10, y: 11 }, { x: 54, y: 11 }]); // запад-север
  carvePath([{ x: 10, y: 52 }, { x: 54, y: 52 }, { x: 54, y: 11 }]); // юг-восток

  // Склады припасов: пара у каждой базы + центральные.
  const depotTiles = [
    { x: 16, y: 45, central: false },
    { x: 6, y: 42, central: false },
    { x: 47, y: 18, central: false },
    { x: 57, y: 21, central: false },
    { x: 30, y: 30, central: true },
    { x: 34, y: 35, central: true },
    { x: 12, y: 14, central: true },
    { x: 51, y: 49, central: true },
  ];
  const depotSpots = depotTiles.map((d) => {
    clearRect(d.x, d.y, 2);
    return { x: (d.x + 0.5) * TILE, y: (d.y + 0.5) * TILE, central: d.central };
  });

  return { grid, hqTiles, depotSpots, heightAt };
}
