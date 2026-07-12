import {
  BUILDING_DEFS, buildingCost, MAP_SIZE, MAP_TILES, unitCost, UNIT_DEFS,
} from '../data/balance';
import type { ArmorType, BuildingKind, DamageType, UnitKind } from '../data/balance';
import { cancelQueueItem, powerDeficit, queueUnit } from '../sim/sim';
import type { Building, Unit } from '../sim/types';
import type { World } from '../sim/world';
import type { Camera } from '../render/camera';
import { drawHpBar, drawUnitToken, INK, mix } from '../render/render';
import type { Renderer } from '../render/render';
import type { Input } from '../input/input';

export const HUD_H = 170;
export const TOP_H = 34;

const BG = '#15171c';
const PANEL = '#1c1f26';
const BORDER = '#2b2f38';
const TEXT = '#d7d2c4';
const MUTED = '#8f8a7c';

const ARMOR_LABEL: Record<ArmorType, string> = {
  INF: 'пехота', LGT: 'лёгк. броня', HVY: 'тяж. броня', BLD: 'здание',
};
const DMG_LABEL: Record<DamageType, string> = {
  bullet: 'пули', rocket: 'ракеты', shell: 'снаряды', defense: 'оборонит.',
};

type ButtonAction =
  | { type: 'train'; buildingId: number; kind: UnitKind }
  | { type: 'place'; kind: BuildingKind }
  | { type: 'cancelQueue'; buildingId: number; index: number };

interface HudButton {
  x: number; y: number; w: number; h: number;
  action: ButtonAction;
  hotkey?: string;
  enabled: boolean;
}

export class Hud {
  private buttons: HudButton[] = [];
  minimapRect = { x: 12, y: 0, w: 150, h: 150 };
  private fogMini: HTMLCanvasElement;
  private fogMiniCtx: CanvasRenderingContext2D;
  private fogMiniImg: ImageData;

  constructor() {
    this.fogMini = document.createElement('canvas');
    this.fogMini.width = MAP_TILES;
    this.fogMini.height = MAP_TILES;
    this.fogMiniCtx = this.fogMini.getContext('2d')!;
    this.fogMiniImg = this.fogMiniCtx.createImageData(MAP_TILES, MAP_TILES);
  }

  inHud(x: number, y: number, screenH: number): boolean {
    return y < TOP_H || y > screenH - HUD_H;
  }

  inMinimap(x: number, y: number): boolean {
    const m = this.minimapRect;
    return x >= m.x && x <= m.x + m.w && y >= m.y && y <= m.y + m.h;
  }

  minimapToWorld(x: number, y: number): { x: number; y: number } {
    const m = this.minimapRect;
    return {
      x: ((x - m.x) / m.w) * MAP_SIZE,
      y: ((y - m.y) / m.h) * MAP_SIZE,
    };
  }

  handleClick(mx: number, my: number, world: World, input: Input): boolean {
    for (const b of this.buttons) {
      if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h) {
        if (b.enabled) this.activate(b.action, world, input);
        return true;
      }
    }
    return false;
  }

  tryHotkey(letter: string, world: World, input: Input): boolean {
    for (const b of this.buttons) {
      if (b.hotkey === letter && b.enabled) {
        this.activate(b.action, world, input);
        return true;
      }
    }
    return false;
  }

  private activate(a: ButtonAction, world: World, input: Input): void {
    switch (a.type) {
      case 'train': {
        const b = world.buildings.get(a.buildingId);
        if (b) queueUnit(world, b, a.kind);
        break;
      }
      case 'place': {
        input.startPlacement(a.kind);
        break;
      }
      case 'cancelQueue': {
        const b = world.buildings.get(a.buildingId);
        if (b) cancelQueueItem(world, b, a.index);
        break;
      }
    }
  }

  // =========================================================================

  render(
    ctx: CanvasRenderingContext2D, world: World, cam: Camera,
    input: Input, renderer: Renderer,
  ): void {
    this.buttons = [];
    const W = cam.screenW, H = cam.screenH;

    this.drawTopBar(ctx, world, input, W);

    // нижняя консоль
    const y0 = H - HUD_H;
    ctx.fillStyle = BG;
    ctx.fillRect(0, y0, W, HUD_H);
    ctx.fillStyle = BORDER;
    ctx.fillRect(0, y0, W, 2);

    this.minimapRect.y = y0 + 10;
    this.drawMinimap(ctx, world, cam, renderer);
    this.drawSelectionPanel(ctx, world, input, W, y0);
    this.drawCommandPanel(ctx, world, input, W, y0);
  }

  private drawTopBar(ctx: CanvasRenderingContext2D, world: World, input: Input, W: number): void {
    ctx.fillStyle = BG;
    ctx.fillRect(0, 0, W, TOP_H);
    ctx.fillStyle = BORDER;
    ctx.fillRect(0, TOP_H - 2, W, 2);

    ctx.textBaseline = 'middle';
    ctx.textAlign = 'left';
    ctx.font = '600 15px monospace';

    // припасы
    ctx.fillStyle = '#e8dfae';
    ctx.fillRect(14, 11, 12, 12);
    ctx.strokeStyle = INK; ctx.lineWidth = 1.5;
    ctx.strokeRect(14, 11, 12, 12);
    ctx.fillStyle = TEXT;
    ctx.fillText(String(Math.floor(world.credits[0])), 34, 18);

    // энергия
    const deficit = powerDeficit(world, 0);
    ctx.fillStyle = deficit ? '#c0452a' : '#7a9c47';
    ctx.fillText(`⚡ ${world.powerUse[0]}/${world.powerProduce[0]}`, 130, 18);
    if (deficit) {
      ctx.fillStyle = '#c0452a';
      ctx.font = '600 12px monospace';
      ctx.fillText('ДЕФИЦИТ: производство ×0.5', 240, 18);
      ctx.font = '600 15px monospace';
    }

    // время
    const t = Math.floor(world.time);
    const mm = String(Math.floor(t / 60)).padStart(2, '0');
    const ss = String(t % 60).padStart(2, '0');
    ctx.fillStyle = MUTED;
    ctx.textAlign = 'center';
    ctx.fillText(`${mm}:${ss}`, W / 2, 18);

    // режим
    ctx.textAlign = 'right';
    if (input.placement) {
      ctx.fillStyle = '#e0b13e';
      ctx.fillText(`СТРОЙКА: ${BUILDING_DEFS[input.placement.kind].name} · ПКМ — отмена`, W - 220, 18);
    } else if (input.attackMoveArmed) {
      ctx.fillStyle = '#c0452a';
      ctx.fillText('АТАКА-ДВИЖЕНИЕ: укажите точку', W - 220, 18);
    }

    const f = world.faction(0);
    ctx.fillStyle = f.color;
    ctx.fillText(f.name.toUpperCase(), W - 14, 18);
    ctx.textAlign = 'left';
  }

  // --- миникарта ------------------------------------------------------------

  private drawMinimap(ctx: CanvasRenderingContext2D, world: World, cam: Camera, renderer: Renderer): void {
    const m = this.minimapRect;
    ctx.save();
    ctx.fillStyle = PANEL;
    ctx.fillRect(m.x - 3, m.y - 3, m.w + 6, m.h + 6);
    ctx.drawImage(renderer.terrainMini, m.x, m.y, m.w, m.h);

    // здания
    for (const b of world.buildings.values()) {
      const visible = b.team === 0 || world.tileVisibleBy(0, b.x, b.y) || world.buildingMemory.has(b.id);
      if (!visible) continue;
      ctx.fillStyle = world.faction(b.team).color;
      const px = m.x + (b.x / MAP_SIZE) * m.w, py = m.y + (b.y / MAP_SIZE) * m.h;
      ctx.fillRect(px - 2.5, py - 2.5, 5, 5);
      if (b.team === 0 && b.attackedFlash > 0 && Math.floor(world.time * 4) % 2 === 0) {
        ctx.strokeStyle = '#ff5533';
        ctx.lineWidth = 1.5;
        ctx.strokeRect(px - 5, py - 5, 10, 10);
      }
    }
    // юниты
    for (const u of world.units.values()) {
      if (u.team === 1 && !world.tileVisibleBy(0, u.x, u.y)) continue;
      ctx.fillStyle = world.faction(u.team).color;
      const px = m.x + (u.x / MAP_SIZE) * m.w, py = m.y + (u.y / MAP_SIZE) * m.h;
      ctx.fillRect(px - 1.5, py - 1.5, 3, 3);
    }
    // склады
    for (const d of world.depots.values()) {
      if (d.amount <= 0) continue;
      ctx.fillStyle = '#e8dfae';
      const px = m.x + (d.x / MAP_SIZE) * m.w, py = m.y + (d.y / MAP_SIZE) * m.h;
      ctx.fillRect(px - 1.5, py - 1.5, 3, 3);
    }

    // туман поверх
    const data = this.fogMiniImg.data;
    for (let i = 0; i < world.fog.length; i++) {
      const f = world.fog[i];
      const o = i * 4;
      data[o] = 10; data[o + 1] = 11; data[o + 2] = 14;
      data[o + 3] = f === 2 ? 0 : f === 1 ? 90 : 220;
    }
    this.fogMiniCtx.putImageData(this.fogMiniImg, 0, 0);
    ctx.drawImage(this.fogMini, m.x, m.y, m.w, m.h);

    // видимая область камеры
    const halfW = cam.screenW / 2 / cam.zoom, halfH = (cam.screenH - HUD_H - TOP_H) / 2 / cam.zoom;
    ctx.strokeStyle = 'rgba(240,236,218,0.85)';
    ctx.lineWidth = 1;
    ctx.strokeRect(
      m.x + ((cam.x - halfW) / MAP_SIZE) * m.w,
      m.y + ((cam.y - halfH) / MAP_SIZE) * m.h,
      (2 * halfW / MAP_SIZE) * m.w,
      (2 * halfH / MAP_SIZE) * m.h,
    );
    ctx.strokeStyle = BORDER;
    ctx.lineWidth = 2;
    ctx.strokeRect(m.x - 1, m.y - 1, m.w + 2, m.h + 2);
    ctx.restore();
  }

  // --- панель выделения ----------------------------------------------------------

  private drawSelectionPanel(
    ctx: CanvasRenderingContext2D, world: World, input: Input, W: number, y0: number,
  ): void {
    const x0 = 190;
    const panelW = W - 190 - 380;
    ctx.fillStyle = PANEL;
    ctx.fillRect(x0, y0 + 10, Math.max(120, panelW), HUD_H - 20);
    ctx.strokeStyle = BORDER;
    ctx.strokeRect(x0 + 0.5, y0 + 10.5, Math.max(120, panelW), HUD_H - 20);

    const units: Unit[] = [];
    const buildings: Building[] = [];
    for (const id of input.selection) {
      const u = world.units.get(id);
      if (u) units.push(u);
      const b = world.buildings.get(id);
      if (b) buildings.push(b);
    }

    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';

    if (units.length === 0 && buildings.length === 0) {
      ctx.fillStyle = MUTED;
      ctx.font = '13px monospace';
      ctx.fillText('Нет выделения. ЛКМ — рамка, Ctrl+цифра — группа.', x0 + 14, y0 + HUD_H / 2);
      return;
    }

    if (units.length === 1 && buildings.length === 0) {
      const u = units[0];
      const def = UNIT_DEFS[u.kind];
      drawUnitToken(ctx, u.kind, world.faction(u.team).color, x0 + 34, y0 + 48, 0, 2);
      ctx.fillStyle = TEXT;
      ctx.font = '600 16px monospace';
      ctx.fillText(def.name, x0 + 70, y0 + 34);
      ctx.font = '13px monospace';
      ctx.fillStyle = MUTED;
      ctx.fillText(`Прочность ${Math.ceil(u.hp)}/${u.maxHp} · броня: ${ARMOR_LABEL[u.armor]}`, x0 + 70, y0 + 56);
      if (u.weapon) {
        ctx.fillText(
          `Урон ${u.weapon.damage} (${DMG_LABEL[u.weapon.type]}) · дальность ${u.weapon.range}` +
          (u.weapon.splash ? ` · сплеш ${u.weapon.splash}` : ''),
          x0 + 70, y0 + 74,
        );
      } else if (u.kind === 'truck') {
        ctx.fillText(`Груз: ${Math.floor(u.cargo)}/${def.cargo}`, x0 + 70, y0 + 74);
      } else {
        ctx.fillText(def.desc, x0 + 70, y0 + 74);
      }
      drawHpBar(ctx, x0 + 70, y0 + 90, 180, u.hp / u.maxHp);
      return;
    }

    if (buildings.length === 1 && units.length === 0) {
      const b = buildings[0];
      const def = BUILDING_DEFS[b.kind];
      ctx.fillStyle = TEXT;
      ctx.font = '600 16px monospace';
      ctx.fillText(def.name + (b.buildProgress < 1 ? ` — стройка ${Math.floor(b.buildProgress * 100)}%` : ''), x0 + 14, y0 + 34);
      ctx.font = '13px monospace';
      ctx.fillStyle = MUTED;
      ctx.fillText(`Прочность ${Math.ceil(b.hp)}/${b.maxHp} · ${def.desc}`, x0 + 14, y0 + 56);
      drawHpBar(ctx, x0 + 14, y0 + 70, 220, b.hp / b.maxHp);

      // очередь производства
      if (b.queue.length > 0) {
        ctx.fillStyle = MUTED;
        ctx.fillText('Очередь (ЛКМ — отменить):', x0 + 14, y0 + 92);
        for (let i = 0; i < b.queue.length; i++) {
          const q = b.queue[i];
          const bx = x0 + 14 + i * 46, by = y0 + 104;
          ctx.fillStyle = '#232732';
          ctx.fillRect(bx, by, 40, 40);
          ctx.strokeStyle = BORDER;
          ctx.strokeRect(bx + 0.5, by + 0.5, 40, 40);
          drawUnitToken(ctx, q.kind, world.faction(0).color, bx + 20, by + 18, 0, 1.2);
          ctx.fillStyle = 'rgba(20,18,14,0.8)';
          ctx.fillRect(bx + 2, by + 33, 36, 5);
          ctx.fillStyle = world.faction(0).color;
          ctx.fillRect(bx + 2, by + 33, 36 * Math.min(1, q.progress), 5);
          this.buttons.push({
            x: bx, y: by, w: 40, h: 40,
            action: { type: 'cancelQueue', buildingId: b.id, index: i },
            enabled: true,
          });
        }
      }
      return;
    }

    // группа: сетка жетонов
    ctx.fillStyle = TEXT;
    ctx.font = '600 14px monospace';
    ctx.fillText(`Выделено: ${units.length + buildings.length}`, x0 + 14, y0 + 28);
    const per = Math.floor((panelW - 30) / 34);
    units.slice(0, per * 3).forEach((u, i) => {
      const gx = x0 + 26 + (i % per) * 34;
      const gy = y0 + 56 + Math.floor(i / per) * 36;
      drawUnitToken(ctx, u.kind, world.faction(0).color, gx, gy, 0, 1.4);
      drawHpBar(ctx, gx - 12, gy + 14, 24, u.hp / u.maxHp);
    });
  }

  // --- панель команд (производство / стройменю) ------------------------------------

  private drawCommandPanel(
    ctx: CanvasRenderingContext2D, world: World, input: Input, W: number, y0: number,
  ): void {
    const x0 = W - 370;
    ctx.fillStyle = PANEL;
    ctx.fillRect(x0, y0 + 10, 358, HUD_H - 20);
    ctx.strokeStyle = BORDER;
    ctx.strokeRect(x0 + 0.5, y0 + 10.5, 358, HUD_H - 20);

    // приоритет: производственное здание → техник (стройменю)
    let prodBuilding: Building | null = null;
    let hasTech = false;
    for (const id of input.selection) {
      const b = world.buildings.get(id);
      if (b && b.team === 0 && b.buildProgress >= 1 && BUILDING_DEFS[b.kind].produces.length > 0 && !prodBuilding) {
        prodBuilding = b;
      }
      const u = world.units.get(id);
      if (u && u.kind === 'technician') hasTech = true;
    }

    ctx.font = '600 12px monospace';
    ctx.fillStyle = MUTED;
    ctx.textAlign = 'left';

    if (prodBuilding) {
      ctx.fillText('ПРОИЗВОДСТВО · ПКМ на карте — точка сбора', x0 + 12, y0 + 24);
      const kinds = BUILDING_DEFS[prodBuilding.kind].produces;
      kinds.forEach((kind, i) => {
        const def = UNIT_DEFS[kind];
        const cost = unitCost(kind, world.factions[0]);
        this.drawActionButton(
          ctx, world, input, x0 + 12 + (i % 3) * 114, y0 + 34 + Math.floor(i / 3) * 56,
          def.name, cost, def.hotkey,
          { type: 'train', buildingId: prodBuilding!.id, kind },
          world.credits[0] >= cost && prodBuilding!.queue.length < 5,
          (bctx, bx, by) => drawUnitToken(bctx, kind, world.faction(0).color, bx, by, 0, 1.1),
        );
      });
    } else if (hasTech) {
      ctx.fillText('СТРОИТЕЛЬСТВО · выберите место на карте', x0 + 12, y0 + 24);
      const kinds: BuildingKind[] = ['hq', 'power', 'barracks', 'factory', 'turret'];
      kinds.forEach((kind, i) => {
        const def = BUILDING_DEFS[kind];
        const cost = buildingCost(kind, world.factions[0]);
        this.drawActionButton(
          ctx, world, input, x0 + 12 + (i % 3) * 114, y0 + 34 + Math.floor(i / 3) * 56,
          def.name, cost, def.hotkey,
          { type: 'place', kind },
          world.credits[0] >= cost,
          (bctx, bx, by) => {
            bctx.fillStyle = mix(world.faction(0).color, '#d9d0b2', 0.5);
            bctx.fillRect(bx - 8, by - 6, 16, 12);
            bctx.strokeStyle = INK;
            bctx.lineWidth = 1.5;
            bctx.strokeRect(bx - 8, by - 6, 16, 12);
          },
        );
      });
    } else {
      ctx.fillText('КОМАНДЫ', x0 + 12, y0 + 24);
      ctx.font = '12px monospace';
      const lines = [
        'ПКМ — движение/атака/сбор',
        'A — атака-движение, S — стоп',
        'Ctrl+1..9 — назначить группу',
        '1..9 — выбрать (повторно — камера)',
        'Техник строит здания,',
        'грузовик возит припасы в штаб.',
      ];
      lines.forEach((l, i) => ctx.fillText(l, x0 + 12, y0 + 48 + i * 18));
    }
  }

  private drawActionButton(
    ctx: CanvasRenderingContext2D, world: World, input: Input,
    x: number, y: number, label: string, cost: number, hotkey: string,
    action: ButtonAction, enabled: boolean,
    icon: (ctx: CanvasRenderingContext2D, x: number, y: number) => void,
  ): void {
    const w = 106, h = 48;
    const hover = input.mouseX >= x && input.mouseX <= x + w && input.mouseY >= y && input.mouseY <= y + h;
    ctx.fillStyle = enabled ? (hover ? '#2a2f3a' : '#232732') : '#1a1c22';
    ctx.fillRect(x, y, w, h);
    ctx.strokeStyle = hover && enabled ? '#5a6070' : BORDER;
    ctx.strokeRect(x + 0.5, y + 0.5, w, h);

    ctx.save();
    if (!enabled) ctx.globalAlpha = 0.45;
    icon(ctx, x + 18, y + h / 2);
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    ctx.font = '600 11px monospace';
    ctx.fillStyle = TEXT;
    ctx.fillText(label.length > 11 ? label.slice(0, 11) + '…' : label, x + 34, y + 14);
    ctx.fillStyle = enabled ? '#e8dfae' : '#c0452a';
    ctx.font = '11px monospace';
    ctx.fillText(`${cost}`, x + 34, y + 30);
    ctx.fillStyle = MUTED;
    ctx.textAlign = 'right';
    ctx.fillText(hotkey, x + w - 6, y + h - 10);
    ctx.restore();

    this.buttons.push({ x, y, w, h, action, hotkey, enabled });
    void world;
  }
}
