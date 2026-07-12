import { MAP_SIZE, MAP_TILES, TILE } from '../data/balance';
import { mulberry32, ROCK_LEVEL } from '../sim/grid';
import type { MapLayout } from '../sim/grid';

// ============================================================================
// Предзапечённый террейн «штабной карты»: бумажно-песчаный фон, изолинии
// (marching squares по полю высот), штриховка скал, слабая координатная сетка.
// Печётся один раз при старте партии.
// ============================================================================

const PAPER = '#c8bb8f';
const INK_FAINT = 'rgba(92, 76, 48, 0.35)';
const INK_ROCK = 'rgba(74, 60, 38, 0.8)';

export function bakeTerrain(layout: MapLayout): HTMLCanvasElement {
  const canvas = document.createElement('canvas');
  canvas.width = MAP_SIZE;
  canvas.height = MAP_SIZE;
  const ctx = canvas.getContext('2d')!;
  const rand = mulberry32(20260712);

  // --- бумага -----------------------------------------------------------
  ctx.fillStyle = PAPER;
  ctx.fillRect(0, 0, MAP_SIZE, MAP_SIZE);

  // высотная тонировка: чем выше, тем суше/темнее
  for (let ty = 0; ty < MAP_TILES; ty++) {
    for (let tx = 0; tx < MAP_TILES; tx++) {
      const h = layout.grid.height[layout.grid.idx(tx, ty)];
      const band = Math.floor(h * 8) / 8;
      ctx.fillStyle = `rgba(120, 100, 58, ${band * 0.14})`;
      ctx.fillRect(tx * TILE, ty * TILE, TILE, TILE);
    }
  }

  // зерно бумаги
  for (let i = 0; i < 5200; i++) {
    const x = rand() * MAP_SIZE, y = rand() * MAP_SIZE;
    ctx.fillStyle = rand() < 0.5 ? 'rgba(80,65,40,0.05)' : 'rgba(255,250,230,0.05)';
    ctx.fillRect(x, y, 1.6, 1.6);
  }

  // координатная сетка каждые 4 клетки
  ctx.strokeStyle = 'rgba(70, 58, 34, 0.09)';
  ctx.lineWidth = 1;
  for (let i = 0; i <= MAP_TILES; i += 4) {
    ctx.beginPath();
    ctx.moveTo(i * TILE + 0.5, 0); ctx.lineTo(i * TILE + 0.5, MAP_SIZE);
    ctx.moveTo(0, i * TILE + 0.5); ctx.lineTo(MAP_SIZE, i * TILE + 0.5);
    ctx.stroke();
  }

  drawIsolines(ctx, layout);
  drawRocks(ctx, layout);
  drawGridLabels(ctx);

  return canvas;
}

/** Marching squares по полю высот для набора уровней. */
function drawIsolines(ctx: CanvasRenderingContext2D, layout: MapLayout): void {
  const step = 16; // пикселей на ячейку сэмплирования
  const n = MAP_SIZE / step;
  const samples = new Float32Array((n + 1) * (n + 1));
  for (let j = 0; j <= n; j++) {
    for (let i = 0; i <= n; i++) {
      samples[j * (n + 1) + i] = layout.heightAt(i * step, j * step);
    }
  }
  const at = (i: number, j: number) => samples[j * (n + 1) + i];
  const levels = [0.3, 0.38, 0.46, 0.54, ROCK_LEVEL];

  for (const level of levels) {
    const isRockEdge = level === ROCK_LEVEL;
    ctx.strokeStyle = isRockEdge ? INK_ROCK : INK_FAINT;
    ctx.lineWidth = isRockEdge ? 1.8 : 1;
    ctx.beginPath();
    for (let j = 0; j < n; j++) {
      for (let i = 0; i < n; i++) {
        const a = at(i, j), b = at(i + 1, j), c = at(i + 1, j + 1), d = at(i, j + 1);
        let caseId = 0;
        if (a > level) caseId |= 1;
        if (b > level) caseId |= 2;
        if (c > level) caseId |= 4;
        if (d > level) caseId |= 8;
        if (caseId === 0 || caseId === 15) continue;

        const x = i * step, y = j * step;
        // интерполяция точек на рёбрах
        const top = { x: x + step * frac(a, b, level), y };
        const right = { x: x + step, y: y + step * frac(b, c, level) };
        const bottom = { x: x + step * frac(d, c, level), y: y + step };
        const left = { x, y: y + step * frac(a, d, level) };

        const seg = (p: { x: number; y: number }, q: { x: number; y: number }) => {
          ctx.moveTo(p.x, p.y); ctx.lineTo(q.x, q.y);
        };
        switch (caseId) {
          case 1: case 14: seg(left, top); break;
          case 2: case 13: seg(top, right); break;
          case 3: case 12: seg(left, right); break;
          case 4: case 11: seg(right, bottom); break;
          case 5: seg(left, top); seg(right, bottom); break;
          case 6: case 9: seg(top, bottom); break;
          case 7: case 8: seg(left, bottom); break;
          case 10: seg(top, right); seg(left, bottom); break;
        }
      }
    }
    ctx.stroke();
  }
}

function frac(v0: number, v1: number, level: number): number {
  if (Math.abs(v1 - v0) < 1e-6) return 0.5;
  return Math.min(1, Math.max(0, (level - v0) / (v1 - v0)));
}

/** Скалы: затемнение + диагональная штриховка по блокированным клеткам. */
function drawRocks(ctx: CanvasRenderingContext2D, layout: MapLayout): void {
  const grid = layout.grid;
  ctx.fillStyle = 'rgba(96, 80, 52, 0.30)';
  for (let ty = 0; ty < MAP_TILES; ty++) {
    for (let tx = 0; tx < MAP_TILES; tx++) {
      if (grid.isRock(tx, ty)) ctx.fillRect(tx * TILE, ty * TILE, TILE, TILE);
    }
  }
  ctx.save();
  // клип по области скал
  ctx.beginPath();
  for (let ty = 0; ty < MAP_TILES; ty++) {
    for (let tx = 0; tx < MAP_TILES; tx++) {
      if (grid.isRock(tx, ty)) ctx.rect(tx * TILE, ty * TILE, TILE, TILE);
    }
  }
  ctx.clip();
  ctx.strokeStyle = 'rgba(64, 52, 32, 0.5)';
  ctx.lineWidth = 1.4;
  ctx.beginPath();
  for (let d = -MAP_SIZE; d < MAP_SIZE; d += 9) {
    ctx.moveTo(d, 0);
    ctx.lineTo(d + MAP_SIZE, MAP_SIZE);
  }
  ctx.stroke();
  ctx.restore();
}

/** Буквенно-цифровые метки сетки по краям — штабной шик. */
function drawGridLabels(ctx: CanvasRenderingContext2D): void {
  ctx.fillStyle = 'rgba(70, 58, 34, 0.4)';
  ctx.font = '600 13px monospace';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  const letters = 'АБВГДЕЖЗИКЛМНОПР';
  for (let i = 0; i < MAP_TILES / 4; i++) {
    const c = (i * 4 + 2) * TILE;
    ctx.fillText(letters[i] ?? '·', c, 14);
    ctx.fillText(letters[i] ?? '·', c, MAP_SIZE - 14);
    ctx.fillText(String(i + 1), 14, c);
    ctx.fillText(String(i + 1), MAP_SIZE - 16, c);
  }
}
