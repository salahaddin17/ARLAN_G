import { BUILDING_DEFS, TILE } from '../data/balance';
import type { BuildingKind } from '../data/balance';
import {
  orderAttackMove, orderAttackTarget, orderBuild, orderHarvest, orderMove,
  orderStop, placeBuilding, setRally,
} from '../sim/sim';
import type { Building, Depot, Unit } from '../sim/types';
import type { World } from '../sim/world';
import type { Camera } from '../render/camera';
import type { PlacementState } from '../render/render';
import type { Hud } from '../ui/hud';

const DRAG_THRESHOLD = 5;
const EDGE_MARGIN = 24;
const EDGE_SPEED = 16; // экранных пикселей за кадр (×dt·60)

export class Input {
  selection = new Set<number>();
  marquee = { active: false, x0: 0, y0: 0, x1: 0, y1: 0 };
  placement: PlacementState | null = null;
  attackMoveArmed = false;
  mouseX = 0;
  mouseY = 0;
  mouseInside = true;

  private groups = new Map<number, number[]>();
  private keys = new Set<string>();
  private leftDown = false;
  private downX = 0;
  private downY = 0;
  private minimapDrag = false;
  private lastGroupDigit = -1;
  private lastGroupTime = 0;

  constructor(
    private canvas: HTMLCanvasElement,
    private cam: Camera,
    private getWorld: () => World | null,
    private hud: Hud,
  ) {
    canvas.addEventListener('contextmenu', (e) => e.preventDefault());
    canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
    window.addEventListener('mousemove', (e) => this.onMouseMove(e));
    window.addEventListener('mouseup', (e) => this.onMouseUp(e));
    canvas.addEventListener('wheel', (e) => this.onWheel(e), { passive: false });
    window.addEventListener('keydown', (e) => this.onKeyDown(e));
    window.addEventListener('keyup', (e) => this.keys.delete(e.code));
    document.addEventListener('mouseleave', () => { this.mouseInside = false; });
    document.addEventListener('mouseenter', () => { this.mouseInside = true; });
    window.addEventListener('blur', () => this.keys.clear());
  }

  reset(): void {
    this.selection.clear();
    this.groups.clear();
    this.placement = null;
    this.attackMoveArmed = false;
    this.marquee.active = false;
  }

  startPlacement(kind: BuildingKind): void {
    this.attackMoveArmed = false;
    this.placement = { kind, tx: 0, ty: 0, valid: false };
    this.updatePlacementTile();
  }

  // --- события мыши ---------------------------------------------------------

  private onMouseDown(e: MouseEvent): void {
    const world = this.getWorld();
    if (!world || world.phase !== 'playing') return;
    const mx = e.offsetX, my = e.offsetY;

    if (e.button === 0) {
      // HUD и миникарта
      if (this.hud.inHud(mx, my, this.cam.screenH)) {
        if (this.hud.inMinimap(mx, my)) {
          const w = this.hud.minimapToWorld(mx, my);
          this.cam.centerOn(w.x, w.y);
          this.minimapDrag = true;
          return;
        }
        this.hud.handleClick(mx, my, world, this);
        return;
      }

      if (this.placement) {
        this.tryPlace(world, e.shiftKey);
        return;
      }

      const wp = this.cam.screenToWorld(mx, my);
      if (this.attackMoveArmed) {
        this.issueAttackMove(world, wp.x, wp.y);
        this.attackMoveArmed = false;
        return;
      }

      this.leftDown = true;
      this.downX = mx; this.downY = my;
      this.marquee = { active: false, x0: mx, y0: my, x1: mx, y1: my };
      return;
    }

    if (e.button === 2) {
      if (this.placement) { this.placement = null; return; }
      if (this.attackMoveArmed) { this.attackMoveArmed = false; return; }
      if (this.hud.inHud(mx, my, this.cam.screenH)) return;
      const wp = this.cam.screenToWorld(mx, my);
      this.issueContextOrder(world, wp.x, wp.y);
    }
  }

  private onMouseMove(e: MouseEvent): void {
    const rect = this.canvas.getBoundingClientRect();
    this.mouseX = e.clientX - rect.left;
    this.mouseY = e.clientY - rect.top;
    this.mouseInside = this.mouseX >= 0 && this.mouseY >= 0 &&
      this.mouseX <= rect.width && this.mouseY <= rect.height;

    if (this.minimapDrag) {
      const w = this.hud.minimapToWorld(
        Math.min(Math.max(this.mouseX, this.hud.minimapRect.x), this.hud.minimapRect.x + this.hud.minimapRect.w),
        Math.min(Math.max(this.mouseY, this.hud.minimapRect.y), this.hud.minimapRect.y + this.hud.minimapRect.h),
      );
      this.cam.centerOn(w.x, w.y);
      return;
    }

    if (this.leftDown) {
      this.marquee.x1 = this.mouseX;
      this.marquee.y1 = this.mouseY;
      if (Math.abs(this.mouseX - this.downX) + Math.abs(this.mouseY - this.downY) > DRAG_THRESHOLD) {
        this.marquee.active = true;
      }
    }

    if (this.placement) this.updatePlacementTile();
  }

  private onMouseUp(e: MouseEvent): void {
    if (e.button !== 0) return;
    this.minimapDrag = false;
    if (!this.leftDown) return;
    this.leftDown = false;

    const world = this.getWorld();
    if (!world || world.phase !== 'playing') { this.marquee.active = false; return; }

    if (this.marquee.active) {
      this.selectInMarquee(world, e.shiftKey);
      this.marquee.active = false;
    } else {
      this.selectAtPoint(world, e.shiftKey);
    }
  }

  private onWheel(e: WheelEvent): void {
    e.preventDefault();
    const factor = Math.exp(-e.deltaY * 0.0012);
    this.cam.zoomAt(e.offsetX, e.offsetY, factor);
  }

  // --- клавиатура --------------------------------------------------------------

  private onKeyDown(e: KeyboardEvent): void {
    const world = this.getWorld();
    if (!world || world.phase !== 'playing') return;
    this.keys.add(e.code);

    // группы: Ctrl+1..9 назначить, 1..9 выбрать (повтор — камера к группе)
    if (e.code.startsWith('Digit')) {
      const digit = Number(e.code.slice(5));
      if (digit >= 1 && digit <= 9) {
        e.preventDefault();
        if (e.ctrlKey || e.metaKey) {
          this.groups.set(digit, [...this.selection]);
        } else {
          this.recallGroup(world, digit);
        }
        return;
      }
    }

    switch (e.code) {
      case 'Escape':
        if (this.placement) this.placement = null;
        else if (this.attackMoveArmed) this.attackMoveArmed = false;
        else this.selection.clear();
        break;
      case 'KeyA': {
        // атака-движение, если есть боевые юниты; иначе — камера (обрабатывается в update)
        if (!e.ctrlKey && this.armedSelected(world).length > 0) {
          this.attackMoveArmed = true;
          this.placement = null;
        }
        break;
      }
      case 'KeyS': {
        const mine = this.selectedUnits(world);
        if (mine.length > 0) {
          orderStop(world, mine);
          this.attackMoveArmed = false;
        }
        break;
      }
      default: {
        // горячие клавиши HUD (Q/W/E/R/T)
        if (!e.repeat && ['KeyQ', 'KeyW', 'KeyE', 'KeyR', 'KeyT'].includes(e.code)) {
          const letter = e.code.slice(3);
          // W — камера тоже; кнопки HUD в приоритете, если что-то выделено
          if (this.hud.tryHotkey(letter, world, this)) {
            e.preventDefault();
            this.keys.delete(e.code); // не панорамировать камеру этим нажатием
          }
        }
      }
    }
  }

  private recallGroup(world: World, digit: number): void {
    const ids = this.groups.get(digit);
    if (!ids || ids.length === 0) return;
    const alive = ids.filter((id) => world.units.has(id) || world.buildings.has(id));
    this.groups.set(digit, alive);
    if (alive.length === 0) return;

    const now = performance.now();
    if (this.lastGroupDigit === digit && now - this.lastGroupTime < 450) {
      // повторное нажатие — камера к группе
      let sx = 0, sy = 0;
      for (const id of alive) {
        const ent = world.units.get(id) ?? world.buildings.get(id)!;
        sx += ent.x; sy += ent.y;
      }
      this.cam.centerOn(sx / alive.length, sy / alive.length);
    }
    this.selection = new Set(alive);
    this.lastGroupDigit = digit;
    this.lastGroupTime = now;
  }

  // --- выделение ------------------------------------------------------------------

  private selectInMarquee(world: World, additive: boolean): void {
    const m = this.marquee;
    const a = this.cam.screenToWorld(Math.min(m.x0, m.x1), Math.min(m.y0, m.y1));
    const b = this.cam.screenToWorld(Math.max(m.x0, m.x1), Math.max(m.y0, m.y1));
    const picked: number[] = [];
    for (const u of world.units.values()) {
      if (u.team !== 0) continue;
      if (u.x >= a.x && u.x <= b.x && u.y >= a.y && u.y <= b.y) picked.push(u.id);
    }
    if (!additive) this.selection.clear();
    for (const id of picked) this.selection.add(id);
  }

  private selectAtPoint(world: World, additive: boolean): void {
    const wp = this.cam.screenToWorld(this.mouseX, this.mouseY);
    const unit = this.pickOwnUnit(world, wp.x, wp.y);
    const building = unit ? null : this.pickOwnBuilding(world, wp.x, wp.y);
    if (!additive) this.selection.clear();
    if (unit) {
      if (additive && this.selection.has(unit.id)) this.selection.delete(unit.id);
      else this.selection.add(unit.id);
    } else if (building) {
      this.selection.add(building.id);
    }
  }

  private pickOwnUnit(world: World, x: number, y: number): Unit | null {
    let best: Unit | null = null;
    let bestD = 18 * 18;
    for (const u of world.units.values()) {
      if (u.team !== 0) continue;
      const d = (u.x - x) ** 2 + (u.y - y) ** 2;
      if (d < bestD) { bestD = d; best = u; }
    }
    return best;
  }

  private pickOwnBuilding(world: World, x: number, y: number): Building | null {
    for (const b of world.buildings.values()) {
      if (b.team !== 0) continue;
      if (x >= b.tx * TILE && x <= (b.tx + b.w) * TILE && y >= b.ty * TILE && y <= (b.ty + b.h) * TILE) {
        return b;
      }
    }
    return null;
  }

  private pickEnemy(world: World, x: number, y: number): Unit | Building | null {
    let best: Unit | null = null;
    let bestD = 18 * 18;
    for (const u of world.units.values()) {
      if (u.team !== 1 || !world.tileVisibleBy(0, u.x, u.y)) continue;
      const d = (u.x - x) ** 2 + (u.y - y) ** 2;
      if (d < bestD) { bestD = d; best = u; }
    }
    if (best) return best;
    for (const b of world.buildings.values()) {
      if (b.team !== 1) continue;
      const known = world.tileVisibleBy(0, b.x, b.y) || world.buildingMemory.has(b.id);
      if (!known) continue;
      if (x >= b.tx * TILE && x <= (b.tx + b.w) * TILE && y >= b.ty * TILE && y <= (b.ty + b.h) * TILE) {
        return b;
      }
    }
    return null;
  }

  private pickDepot(world: World, x: number, y: number): Depot | null {
    for (const d of world.depots.values()) {
      if (d.amount <= 0) continue;
      if ((d.x - x) ** 2 + (d.y - y) ** 2 < 30 * 30) return d;
    }
    return null;
  }

  // --- приказы -----------------------------------------------------------------------

  private selectedUnits(world: World): Unit[] {
    const out: Unit[] = [];
    for (const id of this.selection) {
      const u = world.units.get(id);
      if (u && u.team === 0) out.push(u);
    }
    return out;
  }

  private armedSelected(world: World): Unit[] {
    return this.selectedUnits(world).filter((u) => u.weapon !== undefined);
  }

  private issueAttackMove(world: World, x: number, y: number): void {
    const armed = this.armedSelected(world);
    if (armed.length === 0) return;
    orderAttackMove(world, armed, x, y);
    world.addFx({ kind: 'orderFlag', x, y, attack: true });
  }

  private issueContextOrder(world: World, x: number, y: number): void {
    const units = this.selectedUnits(world);

    // только здание выделено → точка сбора
    if (units.length === 0) {
      for (const id of this.selection) {
        const b = world.buildings.get(id);
        if (b && b.team === 0 && BUILDING_DEFS[b.kind].produces.length > 0) {
          setRally(b, x, y);
          world.addFx({ kind: 'orderFlag', x, y, attack: false });
        }
      }
      return;
    }

    const enemy = this.pickEnemy(world, x, y);
    if (enemy) {
      const armed = units.filter((u) => u.weapon);
      const unarmed = units.filter((u) => !u.weapon);
      if (armed.length > 0) orderAttackTarget(world, armed, enemy.id);
      if (unarmed.length > 0) orderMove(world, unarmed, x, y);
      world.addFx({ kind: 'orderFlag', x, y, attack: true });
      return;
    }

    const depot = this.pickDepot(world, x, y);
    const trucks = units.filter((u) => u.kind === 'truck');
    if (depot && trucks.length > 0) {
      orderHarvest(world, trucks, depot.id);
      const rest = units.filter((u) => u.kind !== 'truck');
      if (rest.length > 0) orderMove(world, rest, x, y);
      world.addFx({ kind: 'orderFlag', x, y, attack: false });
      return;
    }

    // своё недостроенное/повреждённое здание → техники строят/чинят
    const own = this.pickOwnBuilding(world, x, y);
    const techs = units.filter((u) => u.kind === 'technician');
    if (own && techs.length > 0 && (own.buildProgress < 1 || own.hp < own.maxHp)) {
      orderBuild(world, techs, own.id);
      const rest = units.filter((u) => u.kind !== 'technician');
      if (rest.length > 0) orderMove(world, rest, x, y);
      world.addFx({ kind: 'orderFlag', x: own.x, y: own.y, attack: false });
      return;
    }

    orderMove(world, units, x, y);
    world.addFx({ kind: 'orderFlag', x, y, attack: false });
  }

  // --- размещение зданий ----------------------------------------------------------------

  private updatePlacementTile(): void {
    if (!this.placement) return;
    const def = BUILDING_DEFS[this.placement.kind];
    const wp = this.cam.screenToWorld(this.mouseX, this.mouseY);
    this.placement.tx = Math.round(wp.x / TILE - def.w / 2);
    this.placement.ty = Math.round(wp.y / TILE - def.h / 2);
  }

  private tryPlace(world: World, keepPlacing: boolean): void {
    if (!this.placement) return;
    const techs = this.selectedUnits(world).filter((u) => u.kind === 'technician');
    if (techs.length === 0) { this.placement = null; return; }
    const placed = placeBuilding(world, this.placement.kind, 0, this.placement.tx, this.placement.ty, techs);
    if (placed && !keepPlacing) this.placement = null;
  }

  // --- покадровое обновление (камера) ------------------------------------------------------

  update(dt: number): void {
    const world = this.getWorld();
    if (!world) return;

    const move = EDGE_SPEED * dt * 60;
    let dx = 0, dy = 0;

    // WASD и стрелки (A/S заняты приказами только при активном выделении)
    if (this.keys.has('ArrowLeft') || (this.keys.has('KeyA') && this.armedSelected(world).length === 0)) dx -= move;
    if (this.keys.has('ArrowRight') || this.keys.has('KeyD')) dx += move;
    if (this.keys.has('ArrowUp') || this.keys.has('KeyW')) dy -= move;
    if (this.keys.has('ArrowDown') || (this.keys.has('KeyS') && this.selection.size === 0)) dy += move;

    // края экрана
    if (this.mouseInside && world.phase === 'playing') {
      if (this.mouseX < EDGE_MARGIN) dx -= move;
      if (this.mouseX > this.cam.screenW - EDGE_MARGIN) dx += move;
      if (this.mouseY < EDGE_MARGIN) dy -= move;
      if (this.mouseY > this.cam.screenH - EDGE_MARGIN) dy += move;
    }

    if (dx !== 0 || dy !== 0) this.cam.pan(dx, dy);
    if (this.placement) this.updatePlacementTile();

    // чистка выделения от погибших
    for (const id of [...this.selection]) {
      if (!world.units.has(id) && !world.buildings.has(id)) this.selection.delete(id);
    }
  }
}
