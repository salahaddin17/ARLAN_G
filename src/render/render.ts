import { BUILDING_DEFS, MAP_SIZE, MAP_TILES, TILE } from '../data/balance';
import type { BuildingKind, UnitKind } from '../data/balance';
import { canPlaceBuilding } from '../sim/sim';
import type { Building, Depot, Unit } from '../sim/types';
import type { World } from '../sim/world';
import type { Camera } from './camera';
import { bakeTerrain } from './terrain';

export const INK = '#26221c';

interface Fx {
  kind: string;
  x: number; y: number; x2?: number; y2?: number;
  r?: number; ttl: number; max: number; attack?: boolean; dmgType?: string;
}

export interface PlacementState {
  kind: BuildingKind;
  tx: number;
  ty: number;
  valid: boolean;
}

/** Что рендеру нужно знать о вводе (реализует Input). */
export interface InputView {
  selection: Set<number>;
  marquee: { active: boolean; x0: number; y0: number; x1: number; y1: number };
  placement: PlacementState | null;
  attackMoveArmed: boolean;
}

export class Renderer {
  private terrain: HTMLCanvasElement;
  terrainMini: HTMLCanvasElement;
  private fogCanvas: HTMLCanvasElement;
  private fogCtx: CanvasRenderingContext2D;
  private fogImage: ImageData;
  private fx: Fx[] = [];

  constructor(world: World) {
    this.terrain = bakeTerrain(world.layout);
    this.terrainMini = document.createElement('canvas');
    this.terrainMini.width = 256;
    this.terrainMini.height = 256;
    this.terrainMini.getContext('2d')!.drawImage(this.terrain, 0, 0, 256, 256);
    this.fogCanvas = document.createElement('canvas');
    this.fogCanvas.width = MAP_TILES;
    this.fogCanvas.height = MAP_TILES;
    this.fogCtx = this.fogCanvas.getContext('2d')!;
    this.fogImage = this.fogCtx.createImageData(MAP_TILES, MAP_TILES);
  }

  render(
    ctx: CanvasRenderingContext2D, world: World, cam: Camera,
    input: InputView, alpha: number, dt: number,
  ): void {
    this.drainFx(world);
    for (const f of this.fx) f.ttl -= dt;
    this.fx = this.fx.filter((f) => f.ttl > 0);

    ctx.fillStyle = '#101115';
    ctx.fillRect(0, 0, cam.screenW, cam.screenH);

    ctx.save();
    ctx.translate(cam.screenW / 2, cam.screenH / 2);
    ctx.scale(cam.zoom, cam.zoom);
    ctx.translate(-cam.x, -cam.y);

    ctx.drawImage(this.terrain, 0, 0);

    for (const d of world.depots.values()) this.drawDepot(ctx, world, d);

    // память о зданиях врага в тумане
    for (const m of world.buildingMemory.values()) {
      if (!world.tileVisibleBy(0, m.x, m.y)) {
        const b = world.buildings.get(m.id);
        if (b) {
          ctx.globalAlpha = 0.45;
          this.drawBuilding(ctx, world, b, true);
          ctx.globalAlpha = 1;
        }
      }
    }

    for (const b of world.buildings.values()) {
      if (b.team === 1 && !world.tileVisibleBy(0, b.x, b.y)) continue;
      this.drawBuilding(ctx, world, b, false);
    }

    for (const u of world.units.values()) {
      if (u.team === 1 && !world.tileVisibleBy(0, u.x, u.y)) continue;
      this.drawUnit(ctx, world, u, input, alpha);
    }

    // снаряды
    for (const p of world.projectiles) {
      if (!world.tileVisibleBy(0, p.x, p.y)) continue;
      const t = p.flown / Math.max(1, p.totalDist);
      const arc = Math.sin(Math.PI * Math.min(1, t)) * p.totalDist * 0.12;
      ctx.fillStyle = 'rgba(30,26,20,0.35)';
      ctx.beginPath(); ctx.ellipse(p.x, p.y, 4, 2, 0, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = INK;
      ctx.beginPath(); ctx.arc(p.x, p.y - arc, 3, 0, Math.PI * 2); ctx.fill();
    }

    this.drawFx(ctx, world);
    this.drawRallyLines(ctx, world, input);
    if (input.placement) this.drawPlacement(ctx, world, input.placement);

    this.updateFogCanvas(world);
    ctx.imageSmoothingEnabled = true;
    ctx.drawImage(this.fogCanvas, 0, 0, MAP_TILES, MAP_TILES, 0, 0, MAP_SIZE, MAP_SIZE);

    // рамка карты
    ctx.strokeStyle = INK;
    ctx.lineWidth = 3 / cam.zoom;
    ctx.strokeRect(0, 0, MAP_SIZE, MAP_SIZE);

    ctx.restore();

    // рамка выделения (экранные координаты)
    if (input.marquee.active) {
      const m = input.marquee;
      ctx.strokeStyle = 'rgba(235, 230, 210, 0.9)';
      ctx.lineWidth = 1.5;
      ctx.setLineDash([6, 4]);
      ctx.strokeRect(Math.min(m.x0, m.x1), Math.min(m.y0, m.y1), Math.abs(m.x1 - m.x0), Math.abs(m.y1 - m.y0));
      ctx.setLineDash([]);
      ctx.fillStyle = 'rgba(235, 230, 210, 0.08)';
      ctx.fillRect(Math.min(m.x0, m.x1), Math.min(m.y0, m.y1), Math.abs(m.x1 - m.x0), Math.abs(m.y1 - m.y0));
    }
  }

  // --- туман -----------------------------------------------------------------

  private updateFogCanvas(world: World): void {
    const data = this.fogImage.data;
    for (let i = 0; i < world.fog.length; i++) {
      const f = world.fog[i];
      const o = i * 4;
      data[o] = 14; data[o + 1] = 15; data[o + 2] = 18;
      data[o + 3] = f === 2 ? 0 : f === 1 ? 105 : 236;
    }
    this.fogCtx.putImageData(this.fogImage, 0, 0);
  }

  // --- склады ------------------------------------------------------------------

  private drawDepot(ctx: CanvasRenderingContext2D, world: World, d: Depot): void {
    if (d.amount <= 0) return;
    const explored = world.fog[Math.floor(d.y / TILE) * MAP_TILES + Math.floor(d.x / TILE)] >= 1;
    if (!explored) return;
    ctx.save();
    ctx.translate(d.x, d.y);
    // площадка
    ctx.fillStyle = 'rgba(90, 74, 44, 0.25)';
    ctx.beginPath(); ctx.arc(0, 0, 26, 0, Math.PI * 2); ctx.fill();
    // ящики
    const frac = d.amount / d.initial;
    const boxes = Math.max(1, Math.ceil(frac * 6));
    ctx.strokeStyle = INK;
    ctx.lineWidth = 1.6;
    for (let i = 0; i < boxes; i++) {
      const a = (i / 6) * Math.PI * 2;
      const bx = Math.cos(a) * 10, by = Math.sin(a) * 10;
      ctx.fillStyle = '#a8925c';
      ctx.fillRect(bx - 6, by - 6, 12, 12);
      ctx.strokeRect(bx - 6, by - 6, 12, 12);
    }
    // индикатор запаса
    ctx.fillStyle = 'rgba(30,26,20,0.6)';
    ctx.fillRect(-20, 22, 40, 4);
    ctx.fillStyle = '#7a9c47';
    ctx.fillRect(-20, 22, 40 * frac, 4);
    ctx.restore();
  }

  // --- здания -------------------------------------------------------------------

  drawBuilding(ctx: CanvasRenderingContext2D, world: World, b: Building, ghost: boolean): void {
    const def = BUILDING_DEFS[b.kind];
    const f = world.faction(b.team);
    const x = b.tx * TILE, y = b.ty * TILE, w = b.w * TILE, h = b.h * TILE;
    ctx.save();

    if (b.buildProgress < 1) {
      // стройплощадка
      ctx.fillStyle = 'rgba(60, 52, 34, 0.18)';
      ctx.fillRect(x, y, w, h);
      ctx.setLineDash([7, 5]);
      ctx.strokeStyle = f.colorDark;
      ctx.lineWidth = 2;
      ctx.strokeRect(x + 2, y + 2, w - 4, h - 4);
      ctx.setLineDash([]);
      // леса-штриховка по прогрессу
      ctx.save();
      ctx.beginPath(); ctx.rect(x, y, w, h * b.buildProgress); ctx.clip();
      ctx.strokeStyle = f.color;
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      for (let dxy = -h; dxy < w + h; dxy += 8) {
        ctx.moveTo(x + dxy, y); ctx.lineTo(x + dxy + h, y + h);
      }
      ctx.stroke();
      ctx.restore();
      // полоса прогресса
      ctx.fillStyle = 'rgba(20,18,14,0.7)';
      ctx.fillRect(x, y - 8, w, 5);
      ctx.fillStyle = f.color;
      ctx.fillRect(x, y - 8, w * b.buildProgress, 5);
      ctx.restore();
      return;
    }

    // тень-подложка и корпус
    ctx.fillStyle = 'rgba(40, 33, 22, 0.35)';
    ctx.fillRect(x + 3, y + 4, w, h);
    ctx.fillStyle = mix(f.color, '#d9d0b2', 0.55);
    ctx.fillRect(x, y, w, h);
    ctx.strokeStyle = INK;
    ctx.lineWidth = 2.4;
    ctx.strokeRect(x, y, w, h);
    // полоса фракции
    ctx.fillStyle = f.color;
    ctx.fillRect(x, y, w, 6);

    ctx.fillStyle = INK;
    ctx.strokeStyle = INK;
    const cx = x + w / 2, cy = y + h / 2;

    switch (b.kind) {
      case 'hq': {
        // звезда командования
        drawStar(ctx, cx, cy, Math.min(w, h) * 0.26, 5);
        ctx.font = `600 ${Math.max(9, w * 0.09)}px monospace`;
        ctx.textAlign = 'center';
        ctx.fillText('ШТАБ', cx, y + h - 8);
        break;
      }
      case 'power': {
        // молния
        ctx.beginPath();
        ctx.moveTo(cx + 6, y + 8); ctx.lineTo(cx - 7, cy + 3); ctx.lineTo(cx - 1, cy + 3);
        ctx.lineTo(cx - 6, y + h - 8); ctx.lineTo(cx + 8, cy - 2); ctx.lineTo(cx + 2, cy - 2);
        ctx.closePath(); ctx.fill();
        break;
      }
      case 'barracks': {
        // пехотный крест (жетон)
        ctx.lineWidth = 3;
        const r = Math.min(w, h) * 0.26;
        ctx.beginPath();
        ctx.moveTo(cx - r, cy - r); ctx.lineTo(cx + r, cy + r);
        ctx.moveTo(cx + r, cy - r); ctx.lineTo(cx - r, cy + r);
        ctx.stroke();
        break;
      }
      case 'factory': {
        // шестерня упрощённая
        ctx.lineWidth = 3;
        ctx.beginPath(); ctx.arc(cx, cy, Math.min(w, h) * 0.2, 0, Math.PI * 2); ctx.stroke();
        for (let i = 0; i < 8; i++) {
          const a = (i / 8) * Math.PI * 2;
          const r1 = Math.min(w, h) * 0.2, r2 = r1 + 6;
          ctx.beginPath();
          ctx.moveTo(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1);
          ctx.lineTo(cx + Math.cos(a) * r2, cy + Math.sin(a) * r2);
          ctx.stroke();
        }
        break;
      }
      case 'turret': {
        ctx.beginPath(); ctx.arc(cx, cy, 9, 0, Math.PI * 2); ctx.fillStyle = f.color; ctx.fill();
        ctx.strokeStyle = INK; ctx.lineWidth = 2; ctx.stroke();
        const t = world.units.get(b.targetId) ?? world.buildings.get(b.targetId);
        const ang = t ? Math.atan2(t.y - cy, t.x - cx) : (b.team === 0 ? -Math.PI / 4 : Math.PI * 0.75);
        ctx.lineWidth = 4;
        ctx.beginPath();
        ctx.moveTo(cx, cy);
        ctx.lineTo(cx + Math.cos(ang) * 17, cy + Math.sin(ang) * 17);
        ctx.stroke();
        break;
      }
    }

    // hp-бар при повреждении
    if (!ghost && b.hp < b.maxHp) {
      drawHpBar(ctx, x, y - 8, w, b.hp / b.maxHp);
    }
    void def;
    ctx.restore();
  }

  // --- юниты ----------------------------------------------------------------------

  private drawUnit(
    ctx: CanvasRenderingContext2D, world: World, u: Unit, input: InputView, alpha: number,
  ): void {
    const x = u.prevX + (u.x - u.prevX) * alpha;
    const y = u.prevY + (u.y - u.prevY) * alpha;
    const f = world.faction(u.team);
    const selected = u.team === 0 && input.selection.has(u.id);

    if (selected) {
      ctx.strokeStyle = 'rgba(240, 236, 218, 0.95)';
      ctx.lineWidth = 1.6;
      ctx.setLineDash([5, 4]);
      ctx.beginPath();
      ctx.arc(x, y, u.radius + 6, 0, Math.PI * 2);
      ctx.stroke();
      ctx.setLineDash([]);
    }

    drawUnitToken(ctx, u.kind, f.color, x, y, u.angle, 1);

    // груз грузовика
    if (u.kind === 'truck' && u.cargo > 0) {
      ctx.fillStyle = '#e8dfae';
      ctx.fillRect(x - 6, y + u.radius + 3, 12 * (u.cargo / 40), 3);
    }

    if (u.hp < u.maxHp || selected) {
      drawHpBar(ctx, x - u.radius - 2, y - u.radius - 9, u.radius * 2 + 4, u.hp / u.maxHp);
    }
  }

  // --- эффекты ------------------------------------------------------------------------

  private drainFx(world: World): void {
    for (const e of world.fx) {
      switch (e.kind) {
        case 'tracer':
          this.fx.push({ kind: 'tracer', x: e.x1, y: e.y1, x2: e.x2, y2: e.y2, ttl: 0.1, max: 0.1, dmgType: e.dmgType });
          break;
        case 'hit':
          this.fx.push({ kind: 'hit', x: e.x, y: e.y, ttl: 0.16, max: 0.16 });
          break;
        case 'explosion':
          this.fx.push({ kind: 'explosion', x: e.x, y: e.y, r: e.r, ttl: 0.5, max: 0.5 });
          break;
        case 'orderFlag':
          this.fx.push({ kind: 'orderFlag', x: e.x, y: e.y, ttl: 0.5, max: 0.5, attack: e.attack });
          break;
      }
    }
    world.fx.length = 0;
  }

  private drawFx(ctx: CanvasRenderingContext2D, world: World): void {
    for (const f of this.fx) {
      const t = 1 - f.ttl / f.max; // 0..1
      switch (f.kind) {
        case 'tracer': {
          if (!world.tileVisibleBy(0, f.x, f.y) && !world.tileVisibleBy(0, f.x2!, f.y2!)) break;
          ctx.strokeStyle = f.dmgType === 'rocket' ? 'rgba(255,180,80,0.8)' : 'rgba(50,42,30,0.75)';
          ctx.lineWidth = f.dmgType === 'rocket' ? 2.4 : 1.4;
          ctx.globalAlpha = 1 - t;
          ctx.beginPath();
          ctx.moveTo(f.x, f.y);
          ctx.lineTo(f.x2!, f.y2!);
          ctx.stroke();
          ctx.globalAlpha = 1;
          break;
        }
        case 'hit': {
          ctx.fillStyle = `rgba(255, 220, 120, ${0.9 * (1 - t)})`;
          ctx.beginPath(); ctx.arc(f.x, f.y, 3 + t * 5, 0, Math.PI * 2); ctx.fill();
          break;
        }
        case 'explosion': {
          const r = (f.r ?? 20) * (0.4 + t * 0.8);
          ctx.strokeStyle = `rgba(60, 46, 28, ${0.85 * (1 - t)})`;
          ctx.lineWidth = 3 * (1 - t) + 1;
          ctx.beginPath(); ctx.arc(f.x, f.y, r, 0, Math.PI * 2); ctx.stroke();
          ctx.fillStyle = `rgba(220, 150, 60, ${0.5 * (1 - t)})`;
          ctx.beginPath(); ctx.arc(f.x, f.y, r * 0.6, 0, Math.PI * 2); ctx.fill();
          break;
        }
        case 'orderFlag': {
          const s = 1 - t;
          ctx.strokeStyle = f.attack ? `rgba(200, 70, 45, ${s})` : `rgba(120, 170, 80, ${s})`;
          ctx.lineWidth = 2;
          ctx.beginPath(); ctx.arc(f.x, f.y, 6 + 14 * t, 0, Math.PI * 2); ctx.stroke();
          break;
        }
      }
    }
  }

  // --- линии сбора и призрак размещения ------------------------------------------------

  private drawRallyLines(ctx: CanvasRenderingContext2D, world: World, input: InputView): void {
    for (const id of input.selection) {
      const b = world.buildings.get(id);
      if (!b || b.team !== 0 || b.buildProgress < 1) continue;
      if (BUILDING_DEFS[b.kind].produces.length === 0) continue;
      ctx.strokeStyle = 'rgba(240, 236, 218, 0.9)';
      ctx.setLineDash([8, 6]);
      ctx.lineWidth = 1.4;
      ctx.strokeRect(b.tx * TILE - 3, b.ty * TILE - 3, b.w * TILE + 6, b.h * TILE + 6);
      ctx.beginPath();
      ctx.moveTo(b.x, b.y);
      ctx.lineTo(b.rallyX, b.rallyY);
      ctx.stroke();
      ctx.setLineDash([]);
      ctx.beginPath();
      ctx.arc(b.rallyX, b.rallyY, 5, 0, Math.PI * 2);
      ctx.stroke();
    }
  }

  private drawPlacement(ctx: CanvasRenderingContext2D, world: World, p: PlacementState): void {
    const def = BUILDING_DEFS[p.kind];
    const x = p.tx * TILE, y = p.ty * TILE, w = def.w * TILE, h = def.h * TILE;
    const ok = canPlaceBuilding(world, p.kind, p.tx, p.ty, 0);
    ctx.fillStyle = ok ? 'rgba(100, 180, 90, 0.3)' : 'rgba(200, 70, 45, 0.35)';
    ctx.fillRect(x, y, w, h);
    ctx.strokeStyle = ok ? 'rgba(70, 140, 60, 0.9)' : 'rgba(180, 50, 30, 0.9)';
    ctx.lineWidth = 2;
    ctx.setLineDash([6, 4]);
    ctx.strokeRect(x, y, w, h);
    ctx.setLineDash([]);
    // сетка клеток призрака
    ctx.strokeStyle = 'rgba(40, 34, 24, 0.3)';
    ctx.lineWidth = 1;
    for (let i = 1; i < def.w; i++) {
      ctx.beginPath(); ctx.moveTo(x + i * TILE, y); ctx.lineTo(x + i * TILE, y + h); ctx.stroke();
    }
    for (let i = 1; i < def.h; i++) {
      ctx.beginPath(); ctx.moveTo(x, y + i * TILE); ctx.lineTo(x + w, y + i * TILE); ctx.stroke();
    }
  }
}

// ============================================================================
// Общие помощники отрисовки (используются и в HUD)
// ============================================================================

/** Тактическая фишка юнита: пехота — жетон-круг, техника — прямоугольник. */
export function drawUnitToken(
  ctx: CanvasRenderingContext2D, kind: UnitKind, color: string,
  x: number, y: number, angle: number, scale: number,
): void {
  ctx.save();
  ctx.translate(x, y);
  ctx.scale(scale, scale);
  ctx.lineWidth = 2;

  const infantry = kind === 'rifleman' || kind === 'rocketeer' || kind === 'technician';
  if (infantry) {
    // круглый жетон
    ctx.fillStyle = color;
    ctx.strokeStyle = INK;
    ctx.beginPath(); ctx.arc(0, 0, 8, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
    ctx.strokeStyle = INK;
    ctx.lineWidth = 1.8;
    if (kind === 'rifleman') {
      ctx.beginPath();
      ctx.moveTo(-4.2, -4.2); ctx.lineTo(4.2, 4.2);
      ctx.moveTo(4.2, -4.2); ctx.lineTo(-4.2, 4.2);
      ctx.stroke();
    } else if (kind === 'rocketeer') {
      ctx.beginPath();
      ctx.moveTo(0, -5); ctx.lineTo(3.6, 3.6); ctx.lineTo(-3.6, 3.6);
      ctx.closePath(); ctx.stroke();
      ctx.beginPath(); ctx.arc(0, 0.5, 1.2, 0, Math.PI * 2); ctx.fill();
    } else {
      // техник: гаечный контур
      ctx.beginPath(); ctx.arc(0, 0, 3.4, 0, Math.PI * 2); ctx.stroke();
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2 + Math.PI / 4;
        ctx.beginPath();
        ctx.moveTo(Math.cos(a) * 3.4, Math.sin(a) * 3.4);
        ctx.lineTo(Math.cos(a) * 6, Math.sin(a) * 6);
        ctx.stroke();
      }
    }
  } else {
    // техника: прямоугольная фишка с поворотом
    ctx.rotate(angle);
    ctx.fillStyle = color;
    ctx.strokeStyle = INK;
    const L = kind === 'artillery' ? 26 : kind === 'tank' ? 24 : 22;
    const W = kind === 'truck' ? 14 : 15;
    ctx.beginPath();
    roundRect(ctx, -L / 2, -W / 2, L, W, 3);
    ctx.fill(); ctx.stroke();
    ctx.lineWidth = 1.8;
    if (kind === 'truck') {
      // кабина + кузов
      ctx.strokeRect(L / 2 - 7, -W / 2 + 2, 5, W - 4);
      ctx.beginPath();
      ctx.moveTo(-L / 2 + 3, -W / 2 + 3); ctx.lineTo(L / 2 - 9, -W / 2 + 3);
      ctx.moveTo(-L / 2 + 3, W / 2 - 3); ctx.lineTo(L / 2 - 9, W / 2 - 3);
      ctx.stroke();
    } else if (kind === 'scout') {
      // шеврон вперёд
      ctx.beginPath();
      ctx.moveTo(-2, -4); ctx.lineTo(4, 0); ctx.lineTo(-2, 4);
      ctx.moveTo(-7, -4); ctx.lineTo(-1, 0); ctx.lineTo(-7, 4);
      ctx.stroke();
    } else if (kind === 'tank') {
      ctx.beginPath(); ctx.arc(-1, 0, 4.4, 0, Math.PI * 2); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(-1, 0); ctx.lineTo(L / 2 + 6, 0); ctx.stroke();
    } else if (kind === 'artillery') {
      ctx.beginPath(); ctx.moveTo(-4, 0); ctx.lineTo(L / 2 + 10, 0); ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(-L / 2 + 4, -W / 2 + 2); ctx.lineTo(-L / 2 + 4, W / 2 - 2);
      ctx.stroke();
    }
  }
  ctx.restore();
}

export function drawHpBar(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, frac: number): void {
  frac = Math.max(0, Math.min(1, frac));
  ctx.fillStyle = 'rgba(20, 18, 14, 0.75)';
  ctx.fillRect(x, y, w, 4);
  ctx.fillStyle = frac > 0.55 ? '#6fa544' : frac > 0.25 ? '#c9a12e' : '#c0452a';
  ctx.fillRect(x + 0.5, y + 0.5, (w - 1) * frac, 3);
}

function drawStar(ctx: CanvasRenderingContext2D, cx: number, cy: number, r: number, points: number): void {
  ctx.beginPath();
  for (let i = 0; i < points * 2; i++) {
    const a = (i / (points * 2)) * Math.PI * 2 - Math.PI / 2;
    const rr = i % 2 === 0 ? r : r * 0.45;
    const px = cx + Math.cos(a) * rr, py = cy + Math.sin(a) * rr;
    if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
  }
  ctx.closePath();
  ctx.fill();
}

function roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number): void {
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

export function mix(a: string, b: string, t: number): string {
  const pa = hex(a), pb = hex(b);
  const c = pa.map((v, i) => Math.round(v + (pb[i] - v) * t));
  return `rgb(${c[0]}, ${c[1]}, ${c[2]})`;
}

function hex(s: string): number[] {
  const m = s.replace('#', '');
  return [parseInt(m.slice(0, 2), 16), parseInt(m.slice(2, 4), 16), parseInt(m.slice(4, 6), 16)];
}
