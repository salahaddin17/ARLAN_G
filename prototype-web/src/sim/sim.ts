import {
  BUILDING_DEFS, buildingCost, DAMAGE_MATRIX, ECONOMY, MAP_TILES, TILE,
  unitCost, UNIT_DEFS,
} from '../data/balance';
import type { BuildingKind, UnitKind } from '../data/balance';
import { findPath, nearestFreeTile, smoothPath } from './pathfinding';
import { recomputeFog } from './fog';
import type { Building, Depot, Team, Unit } from './types';
import type { World } from './world';

// ============================================================================
// Симуляция: фиксированный тик 20 Гц. Никакого доступа к DOM/canvas.
// ============================================================================

const tileOf = (w: number) => Math.floor(w / TILE);

// --- Приказы (вызываются input'ом и ИИ) -------------------------------------

export function setDestination(world: World, u: Unit, x: number, y: number): boolean {
  x = Math.min(MAP_TILES * TILE - 8, Math.max(8, x));
  y = Math.min(MAP_TILES * TILE - 8, Math.max(8, y));
  const sx = tileOf(u.x), sy = tileOf(u.y);
  const tiles = findPath(world.grid, sx, sy, tileOf(x), tileOf(y));
  if (!tiles) { u.path = null; return false; }
  const sm = smoothPath(world.grid, sx, sy, tiles);
  u.path = sm.map((t) => ({ x: (t.x + 0.5) * TILE, y: (t.y + 0.5) * TILE }));
  // точная конечная точка, если её клетка свободна
  const last = sm[sm.length - 1];
  if (last.x === tileOf(x) && last.y === tileOf(y) && !world.grid.isBlocked(last.x, last.y)) {
    u.path[u.path.length - 1] = { x, y };
  }
  u.pathIdx = 0;
  u.goalX = x; u.goalY = y;
  u.repathCd = 1.5 + world.rand() * 0.5;
  return true;
}

/** Смещения «коробочкой» вокруг точки для группового приказа. */
function formationOffsets(n: number): { x: number; y: number }[] {
  const out: { x: number; y: number }[] = [{ x: 0, y: 0 }];
  const spacing = 26;
  let ring = 1;
  while (out.length < n) {
    const per = ring * 6;
    for (let i = 0; i < per && out.length < n; i++) {
      const a = (i / per) * Math.PI * 2 + ring * 0.5;
      out.push({ x: Math.cos(a) * ring * spacing, y: Math.sin(a) * ring * spacing });
    }
    ring++;
  }
  return out;
}

export function orderMove(world: World, units: Unit[], x: number, y: number): void {
  const offs = formationOffsets(units.length);
  units.forEach((u, i) => {
    u.order = { kind: 'move', x: x + offs[i].x, y: y + offs[i].y };
    u.engageId = 0;
    setDestination(world, u, x + offs[i].x, y + offs[i].y);
  });
}

export function orderAttackMove(world: World, units: Unit[], x: number, y: number): void {
  const offs = formationOffsets(units.length);
  units.forEach((u, i) => {
    u.order = { kind: 'attackMove', x: x + offs[i].x, y: y + offs[i].y };
    u.engageId = 0;
    setDestination(world, u, x + offs[i].x, y + offs[i].y);
  });
}

export function orderAttackTarget(world: World, units: Unit[], targetId: number): void {
  for (const u of units) {
    if (!u.weapon) continue;
    u.order = { kind: 'attack', targetId };
    u.engageId = 0;
    u.path = null;
  }
}

export function orderHarvest(world: World, units: Unit[], depotId: number): void {
  for (const u of units) {
    if (u.kind !== 'truck') continue;
    u.order = { kind: 'harvest', depotId };
    u.harvestPhase = u.cargo >= (UNIT_DEFS.truck.cargo ?? 40) ? 'toBase' : 'toDepot';
    u.path = null;
  }
}

export function orderStop(world: World, units: Unit[]): void {
  for (const u of units) {
    u.order = { kind: 'idle' };
    u.path = null;
    u.engageId = 0;
  }
}

export function orderBuild(world: World, techs: Unit[], buildingId: number): void {
  for (const u of techs) {
    if (u.kind !== 'technician') continue;
    u.order = { kind: 'build', buildingId };
    u.path = null;
  }
}

// --- Строительство -----------------------------------------------------------

export function canPlaceBuilding(
  world: World, kind: BuildingKind, tx: number, ty: number, team: Team,
): boolean {
  const def = BUILDING_DEFS[kind];
  if (tx < 0 || ty < 0 || tx + def.w > MAP_TILES || ty + def.h > MAP_TILES) return false;
  for (let y = ty; y < ty + def.h; y++) {
    for (let x = tx; x < tx + def.w; x++) {
      if (world.grid.isBlocked(x, y)) return false;
      // игрок строит только на разведанной территории
      if (team === 0 && world.fog[y * MAP_TILES + x] === 0) return false;
    }
  }
  // юниты не должны стоять в контуре
  const x0 = tx * TILE - 6, y0 = ty * TILE - 6;
  const x1 = (tx + def.w) * TILE + 6, y1 = (ty + def.h) * TILE + 6;
  for (const u of world.units.values()) {
    if (u.x > x0 && u.x < x1 && u.y > y0 && u.y < y1) return false;
  }
  // склады припасов не застраиваем
  for (const d of world.depots.values()) {
    if (d.amount > 0 && d.x > x0 - 20 && d.x < x1 + 20 && d.y > y0 - 20 && d.y < y1 + 20) return false;
  }
  return true;
}

/** Разместить стройплощадку и отправить техников строить. Снимает деньги. */
export function placeBuilding(
  world: World, kind: BuildingKind, team: Team, tx: number, ty: number, techs: Unit[],
): Building | null {
  if (!canPlaceBuilding(world, kind, tx, ty, team)) return null;
  const cost = buildingCost(kind, world.factions[team]);
  if (world.credits[team] < cost) return null;
  world.credits[team] -= cost;
  const b = world.spawnBuilding(kind, team, tx, ty, false);
  orderBuild(world, techs, b.id);
  return b;
}

// --- Производство юнитов --------------------------------------------------------

export function queueUnit(world: World, b: Building, kind: UnitKind): boolean {
  if (b.buildProgress < 1 || b.dead) return false;
  if (!BUILDING_DEFS[b.kind].produces.includes(kind)) return false;
  if (b.queue.length >= 5) return false;
  const cost = unitCost(kind, world.factions[b.team]);
  if (world.credits[b.team] < cost) return false;
  world.credits[b.team] -= cost;
  b.queue.push({ kind, progress: 0, cost });
  return true;
}

export function cancelQueueItem(world: World, b: Building, index: number): void {
  const item = b.queue[index];
  if (!item) return;
  world.credits[b.team] += item.cost;
  b.queue.splice(index, 1);
}

export function setRally(b: Building, x: number, y: number): void {
  b.rallyX = x;
  b.rallyY = y;
}

// --- Урон -----------------------------------------------------------------------

export function dealDamage(
  world: World, attackerTeam: Team, attackerId: number,
  target: Unit | Building, amount: number, dmgType: keyof typeof DAMAGE_MATRIX,
): void {
  if (target.dead) return;
  const mul = DAMAGE_MATRIX[dmgType][isUnit(target) ? target.armor : 'BLD'];
  target.hp -= amount * mul;
  if (isUnit(target)) {
    // ответка: свободный боец разворачивается на обидчика
    if (target.weapon && target.engageId === 0 &&
        (target.order.kind === 'idle' || target.order.kind === 'attackMove')) {
      target.engageId = attackerId;
    }
  } else {
    target.attackedFlash = 3;
  }
  if (target.hp <= 0) {
    if (isUnit(target)) killUnit(world, target, attackerTeam);
    else killBuilding(world, target, attackerTeam);
  }
}

function isUnit(e: Unit | Building): e is Unit {
  return (e as Unit).speed !== undefined;
}

function killUnit(world: World, u: Unit, killerTeam: Team): void {
  u.dead = true;
  world.units.delete(u.id);
  world.stats[u.team].unitsLost++;
  world.stats[killerTeam].enemiesKilled++;
  world.addFx({ kind: 'explosion', x: u.x, y: u.y, r: u.radius * 2.2 });
}

function killBuilding(world: World, b: Building, killerTeam: Team): void {
  // вернуть очередь производства? нет — сгорела вместе со зданием
  world.stats[b.team].buildingsLost++;
  world.stats[killerTeam].enemiesKilled++;
  world.addFx({ kind: 'explosion', x: b.x, y: b.y, r: Math.max(b.w, b.h) * TILE * 0.9 });
  world.removeBuilding(b);
}

// --- Основной шаг -----------------------------------------------------------------

export function step(world: World, dt: number): void {
  if (world.phase !== 'playing') return;
  world.time += dt;
  world.tick++;

  updatePower(world);

  for (const b of [...world.buildings.values()]) updateBuilding(world, b, dt);
  for (const u of [...world.units.values()]) updateUnit(world, u, dt);

  applySeparation(world, dt);
  updateProjectiles(world, dt);

  if (world.tick % 4 === 0) recomputeFog(world);
  if (world.tick % 20 === 0) checkVictory(world);
}

function updatePower(world: World): void {
  world.powerProduce = [0, 0];
  world.powerUse = [0, 0];
  for (const b of world.buildings.values()) {
    if (b.buildProgress < 1) continue;
    const def = BUILDING_DEFS[b.kind];
    world.powerProduce[b.team] += def.powerProduce;
    world.powerUse[b.team] += def.powerUse;
  }
}

export function powerDeficit(world: World, team: Team): boolean {
  return world.powerUse[team] > world.powerProduce[team];
}

// --- Здания ------------------------------------------------------------------------

function updateBuilding(world: World, b: Building, dt: number): void {
  if (b.dead) return;
  b.attackedFlash = Math.max(0, b.attackedFlash - dt);
  b.cooldown = Math.max(0, b.cooldown - dt);
  if (b.buildProgress < 1) return;

  const speedMul = powerDeficit(world, b.team) ? ECONOMY.lowPowerMul : 1;

  // производство
  if (b.queue.length > 0) {
    const item = b.queue[0];
    item.progress += (dt / UNIT_DEFS[item.kind].buildTime) * speedMul;
    if (item.progress >= 1) {
      b.queue.shift();
      const spot = findExitSpot(world, b);
      const u = world.spawnUnit(item.kind, b.team, spot.x, spot.y);
      if (item.kind === 'truck') {
        const depot = world.nearestDepotWithSupplies(u.x, u.y);
        if (depot) {
          u.order = { kind: 'harvest', depotId: depot.id };
        }
      } else {
        u.order = { kind: 'move', x: b.rallyX, y: b.rallyY };
        setDestination(world, u, b.rallyX, b.rallyY);
      }
    }
  }

  // турель
  if (b.weapon) {
    const w = b.weapon;
    let target = validTarget(world, b.targetId);
    if (!target || dist2(b.x, b.y, target.x, target.y) > w.range * w.range) {
      target = acquireForTurret(world, b, w.range);
      b.targetId = target ? target.id : 0;
    }
    if (target && b.cooldown <= 0) {
      dealDamage(world, b.team, b.id, target, w.damage, w.type);
      b.cooldown = w.cooldown * (powerDeficit(world, b.team) ? 2 : 1);
      world.addFx({ kind: 'tracer', x1: b.x, y1: b.y - 6, x2: target.x, y2: target.y, dmgType: w.type });
      world.addFx({ kind: 'hit', x: target.x, y: target.y });
    }
  }
}

function validTarget(world: World, id: number): Unit | Building | null {
  if (!id) return null;
  return world.units.get(id) ?? world.buildings.get(id) ?? null;
}

function acquireForTurret(world: World, b: Building, range: number): Unit | Building | null {
  let best: Unit | Building | null = null;
  let bestD = range * range;
  for (const u of world.units.values()) {
    if (u.team === b.team) continue;
    const d = dist2(b.x, b.y, u.x, u.y);
    if (d < bestD) { bestD = d; best = u; }
  }
  return best;
}

function findExitSpot(world: World, b: Building): { x: number; y: number } {
  const below = b.team === 0 ? 1 : -1;
  const candidates: { x: number; y: number }[] = [];
  for (let dx = 0; dx < b.w; dx++) {
    candidates.push({ x: b.tx + dx, y: below === 1 ? b.ty + b.h : b.ty - 1 });
  }
  for (const c of candidates) {
    if (!world.grid.isBlocked(c.x, c.y)) return { x: (c.x + 0.5) * TILE, y: (c.y + 0.5) * TILE };
  }
  const free = nearestFreeTile(world.grid, b.tx + (b.w >> 1), b.ty + (b.h >> 1), 8);
  if (free) return { x: (free.x + 0.5) * TILE, y: (free.y + 0.5) * TILE };
  return { x: b.x, y: b.y + below * (b.h / 2 + 1) * TILE };
}

// --- Юниты -------------------------------------------------------------------------

function updateUnit(world: World, u: Unit, dt: number): void {
  if (u.dead) return;
  u.prevX = u.x; u.prevY = u.y;
  u.cooldown = Math.max(0, u.cooldown - dt);
  u.repathCd -= dt;
  u.scanCd -= dt;

  switch (u.order.kind) {
    case 'idle': {
      if (u.weapon) {
        const target = pickEngageTarget(world, u, u.weapon.range + 25);
        if (target) fireAt(world, u, target, dt, false);
      }
      break;
    }
    case 'move': {
      if (followPath(world, u, dt) === 'arrived') u.order = { kind: 'idle' };
      break;
    }
    case 'attackMove': {
      const target = pickEngageTarget(world, u, Math.max((u.weapon?.range ?? 0) * 1.2, 170));
      if (target && u.weapon) {
        fireAt(world, u, target, dt, true);
      } else if (followPath(world, u, dt) === 'arrived') {
        u.order = { kind: 'idle' };
      }
      break;
    }
    case 'attack': {
      const target = validTarget(world, u.order.targetId);
      if (!target || target.dead || !u.weapon) {
        u.order = { kind: 'idle' };
        u.path = null;
        break;
      }
      fireAt(world, u, target, dt, true);
      break;
    }
    case 'harvest':
      updateHarvest(world, u, dt);
      break;
    case 'build':
      updateConstructionWork(world, u, dt);
      break;
  }
}

/** Автоцель: ближайший видимый враг (юниты приоритетнее зданий). */
function pickEngageTarget(world: World, u: Unit, radius: number): Unit | Building | null {
  const cur = u.engageId ? validTarget(world, u.engageId) : null;
  if (cur && !cur.dead && dist2(u.x, u.y, cur.x, cur.y) < (radius + 60) * (radius + 60) &&
      world.tileVisibleBy(u.team, cur.x, cur.y)) {
    return cur;
  }
  u.engageId = 0;
  if (u.scanCd > 0) return null;
  u.scanCd = 0.25;
  const r2 = radius * radius;
  let best: Unit | Building | null = null;
  let bestD = r2;
  for (const e of world.units.values()) {
    if (e.team === u.team) continue;
    const d = dist2(u.x, u.y, e.x, e.y);
    if (d < bestD && world.tileVisibleBy(u.team, e.x, e.y)) { bestD = d; best = e; }
  }
  if (!best) {
    bestD = r2;
    for (const e of world.buildings.values()) {
      if (e.team === u.team) continue;
      const d = dist2(u.x, u.y, e.x, e.y);
      if (d < bestD && world.tileVisibleBy(u.team, e.x, e.y)) { bestD = d; best = e; }
    }
  }
  if (best) u.engageId = best.id;
  return best;
}

/** Стрельба с преследованием (chase=true) либо только с места. */
function fireAt(world: World, u: Unit, target: Unit | Building, dt: number, chase: boolean): void {
  const w = u.weapon!;
  const d = Math.sqrt(dist2(u.x, u.y, target.x, target.y));
  const inRange = d <= w.range && d >= (w.minRange ?? 0);
  const visible = world.tileVisibleBy(u.team, target.x, target.y);

  if (inRange && visible) {
    u.path = null;
    u.angle = rotateToward(u.angle, Math.atan2(target.y - u.y, target.x - u.x), dt * 8);
    if (u.cooldown <= 0) {
      u.cooldown = w.cooldown;
      if (w.projectileSpeed && w.splash) {
        world.projectiles.push({
          id: 0, x: u.x, y: u.y, tx: target.x, ty: target.y,
          speed: w.projectileSpeed, damage: w.damage, dmgType: 'shell',
          splash: w.splash, team: u.team,
          totalDist: d, flown: 0,
        });
        world.addFx({ kind: 'hit', x: u.x + Math.cos(u.angle) * u.radius, y: u.y + Math.sin(u.angle) * u.radius });
      } else {
        dealDamage(world, u.team, u.id, target, w.damage, w.type);
        world.addFx({ kind: 'tracer', x1: u.x, y1: u.y, x2: target.x, y2: target.y, dmgType: w.type });
        world.addFx({ kind: 'hit', x: target.x, y: target.y });
      }
    }
    return;
  }

  if (!chase) return;

  // догнать цель: перепрокладка, если цель ушла или путь протух
  const goalMoved = dist2(u.goalX, u.goalY, target.x, target.y) > 60 * 60;
  if (!u.path || goalMoved || u.repathCd <= 0) {
    // артиллерия при цели ближе minRange отходит
    if (w.minRange && d < w.minRange && d > 1) {
      const ux = (u.x - target.x) / d, uy = (u.y - target.y) / d;
      setDestination(world, u, u.x + ux * (w.minRange - d + 30), u.y + uy * (w.minRange - d + 30));
    } else {
      setDestination(world, u, target.x, target.y);
    }
  }
  followPath(world, u, dt);
}

// --- Движение по пути -----------------------------------------------------------

type MoveResult = 'moving' | 'arrived';

function followPath(world: World, u: Unit, dt: number): MoveResult {
  if (!u.path || u.pathIdx >= u.path.length) {
    u.path = null;
    return 'arrived';
  }
  const wp = u.path[u.pathIdx];
  const dx = wp.x - u.x, dy = wp.y - u.y;
  const d = Math.hypot(dx, dy);
  const isLast = u.pathIdx === u.path.length - 1;
  const arrive = isLast ? 6 : 12;
  if (d <= arrive) {
    u.pathIdx++;
    if (u.pathIdx >= u.path.length) {
      u.path = null;
      return 'arrived';
    }
    return 'moving';
  }
  // путь перегорожен свежепостроенным зданием — перепроложить
  const ntx = tileOf(u.x + (dx / d) * TILE * 0.8), nty = tileOf(u.y + (dy / d) * TILE * 0.8);
  if (world.grid.isBlocked(ntx, nty) && u.repathCd <= 0) {
    setDestination(world, u, u.goalX, u.goalY);
    return 'moving';
  }
  const stepLen = Math.min(d, u.speed * dt);
  u.x += (dx / d) * stepLen;
  u.y += (dy / d) * stepLen;
  u.angle = rotateToward(u.angle, Math.atan2(dy, dx), dt * 7);
  return 'moving';
}

function rotateToward(a: number, b: number, maxStep: number): number {
  let diff = b - a;
  while (diff > Math.PI) diff -= Math.PI * 2;
  while (diff < -Math.PI) diff += Math.PI * 2;
  if (Math.abs(diff) <= maxStep) return b;
  return a + Math.sign(diff) * maxStep;
}

/** Расталкивание юнитов + выдавливание из блокированных клеток. */
function applySeparation(world: World, dt: number): void {
  const cell = 48;
  const buckets = new Map<number, Unit[]>();
  const key = (x: number, y: number) => Math.floor(x / cell) * 4096 + Math.floor(y / cell);
  for (const u of world.units.values()) {
    const k = key(u.x, u.y);
    let arr = buckets.get(k);
    if (!arr) { arr = []; buckets.set(k, arr); }
    arr.push(u);
  }
  const push = dt * 60;
  for (const u of world.units.values()) {
    const bx = Math.floor(u.x / cell), by = Math.floor(u.y / cell);
    for (let gy = by - 1; gy <= by + 1; gy++) {
      for (let gx = bx - 1; gx <= bx + 1; gx++) {
        const arr = buckets.get(gx * 4096 + gy);
        if (!arr) continue;
        for (const v of arr) {
          if (v.id <= u.id) continue;
          const dx = v.x - u.x, dy = v.y - u.y;
          const minD = u.radius + v.radius + 2;
          const d2 = dx * dx + dy * dy;
          if (d2 >= minD * minD || d2 === 0) continue;
          const d = Math.sqrt(d2);
          const overlap = (minD - d) / d;
          const px = dx * overlap * 0.5, py = dy * overlap * 0.5;
          const cap = push;
          u.x -= clampAbs(px, cap); u.y -= clampAbs(py, cap);
          v.x += clampAbs(px, cap); v.y += clampAbs(py, cap);
        }
      }
    }
    // из блокированной клетки — к ближайшей свободной
    const tx = tileOf(u.x), ty = tileOf(u.y);
    if (world.grid.isBlocked(tx, ty)) {
      const free = nearestFreeTile(world.grid, tx, ty, 4);
      if (free) {
        const cx = (free.x + 0.5) * TILE, cy = (free.y + 0.5) * TILE;
        const dx = cx - u.x, dy = cy - u.y;
        const d = Math.hypot(dx, dy) || 1;
        const s = Math.min(d, 90 * dt);
        u.x += (dx / d) * s; u.y += (dy / d) * s;
      }
    }
    u.x = Math.min(MAP_TILES * TILE - 4, Math.max(4, u.x));
    u.y = Math.min(MAP_TILES * TILE - 4, Math.max(4, u.y));
  }
}

function clampAbs(v: number, cap: number): number {
  return Math.max(-cap, Math.min(cap, v));
}

// --- Грузовики ----------------------------------------------------------------

function updateHarvest(world: World, u: Unit, dt: number): void {
  if (u.order.kind !== 'harvest') return;
  const capacity = UNIT_DEFS.truck.cargo ?? 40;

  switch (u.harvestPhase) {
    case 'toDepot': {
      let depot: Depot | null = world.depots.get(u.order.depotId) ?? null;
      if (!depot || depot.amount <= 0) {
        depot = world.nearestDepotWithSupplies(u.x, u.y);
        if (!depot) { u.order = { kind: 'idle' }; return; }
        u.order = { kind: 'harvest', depotId: depot.id };
      }
      if (dist2(u.x, u.y, depot.x, depot.y) < 42 * 42) {
        u.harvestPhase = 'loading';
        u.path = null;
      } else if (!u.path) {
        setDestination(world, u, depot.x, depot.y);
      } else {
        followPath(world, u, dt);
      }
      break;
    }
    case 'loading': {
      const depot = world.depots.get(u.order.depotId);
      if (!depot || depot.amount <= 0) { u.harvestPhase = u.cargo > 0 ? 'toBase' : 'toDepot'; return; }
      const take = Math.min(ECONOMY.loadRate * dt, capacity - u.cargo, depot.amount);
      u.cargo += take;
      depot.amount -= take;
      if (u.cargo >= capacity - 0.01 || depot.amount <= 0) {
        u.cargo = Math.min(capacity, u.cargo);
        u.harvestPhase = 'toBase';
        u.lastDepotId = depot.id;
      }
      break;
    }
    case 'toBase': {
      const hq = world.nearestHq(u.team, u.x, u.y);
      if (!hq) { u.order = { kind: 'idle' }; return; }
      const nearDist = (Math.max(hq.w, hq.h) / 2) * TILE + 34;
      if (dist2(u.x, u.y, hq.x, hq.y) < nearDist * nearDist) {
        u.harvestPhase = 'unloading';
        u.harvestTimer = ECONOMY.unloadTime;
        u.path = null;
      } else if (!u.path) {
        setDestination(world, u, hq.x, hq.y + (u.team === 0 ? (hq.h / 2) * TILE + 24 : -(hq.h / 2) * TILE - 24));
      } else {
        followPath(world, u, dt);
      }
      break;
    }
    case 'unloading': {
      u.harvestTimer -= dt;
      if (u.harvestTimer <= 0) {
        const incomeMul = u.team === 1 ? world.diff().incomeMul : 1;
        const gained = Math.round(u.cargo * incomeMul);
        world.credits[u.team] += gained;
        world.stats[u.team].suppliesGathered += gained;
        u.cargo = 0;
        u.harvestPhase = 'toDepot';
        // вернуться к прежнему складу, если там ещё есть припасы
        const prev = world.depots.get(u.lastDepotId);
        if (prev && prev.amount > 0 && u.order.kind === 'harvest') {
          u.order = { kind: 'harvest', depotId: prev.id };
        }
      }
      break;
    }
  }
}

// --- Техник строит --------------------------------------------------------------

function updateConstructionWork(world: World, u: Unit, dt: number): void {
  if (u.order.kind !== 'build') return;
  const b = world.buildings.get(u.order.buildingId);
  if (!b || b.dead || b.team !== u.team) {
    u.order = { kind: 'idle' };
    return;
  }
  const done = b.buildProgress >= 1 && b.hp >= b.maxHp - 0.5;
  if (done) {
    u.order = { kind: 'idle' };
    return;
  }
  // расстояние до прямоугольника здания
  const x0 = b.tx * TILE, y0 = b.ty * TILE, x1 = (b.tx + b.w) * TILE, y1 = (b.ty + b.h) * TILE;
  const cx = Math.max(x0, Math.min(x1, u.x));
  const cy = Math.max(y0, Math.min(y1, u.y));
  const d = Math.hypot(u.x - cx, u.y - cy);
  if (d > 26) {
    if (!u.path) {
      const ok = setDestination(world, u, b.x, b.y); // findPath сам подберёт ближайшую свободную клетку
      if (!ok) u.order = { kind: 'idle' };
    } else {
      followPath(world, u, dt);
    }
    return;
  }
  u.path = null;
  const def = BUILDING_DEFS[b.kind];
  if (b.buildProgress < 1) {
    const rate = dt / def.buildTime;
    b.buildProgress = Math.min(1, b.buildProgress + rate);
    b.hp = Math.min(b.maxHp, b.hp + rate * b.maxHp * 0.9);
    if (b.buildProgress >= 1) {
      b.hp = b.maxHp;
      world.stats[b.team].buildingsBuilt++;
      world.addFx({ kind: 'hit', x: b.x, y: b.y });
    }
  } else {
    // ремонт
    b.hp = Math.min(b.maxHp, b.hp + (dt / def.buildTime) * b.maxHp * ECONOMY.repairSpeedMul);
  }
}

// --- Снаряды (артиллерия) ----------------------------------------------------------

function updateProjectiles(world: World, dt: number): void {
  const alive: typeof world.projectiles = [];
  for (const p of world.projectiles) {
    const dx = p.tx - p.x, dy = p.ty - p.y;
    const remaining = Math.hypot(dx, dy);
    const stepLen = p.speed * dt;
    if (remaining <= stepLen) {
      explodeShell(world, p);
      continue;
    }
    p.x += (dx / remaining) * stepLen;
    p.y += (dy / remaining) * stepLen;
    p.flown += stepLen;
    alive.push(p);
  }
  world.projectiles = alive;
}

function explodeShell(world: World, p: { tx: number; ty: number; damage: number; splash: number; team: Team }): void {
  world.addFx({ kind: 'explosion', x: p.tx, y: p.ty, r: p.splash });
  const r2 = p.splash * p.splash;
  const hitEntity = (e: Unit | Building) => {
    if (e.team === p.team || e.dead) return;
    const d2v = dist2(p.tx, p.ty, e.x, e.y);
    if (d2v > r2 * 1.44) return; // здания крупные — чуть шире захват
    const falloff = Math.max(0.45, 1 - Math.sqrt(d2v) / (p.splash * 1.4));
    dealDamage(world, p.team, 0, e, p.damage * falloff, 'shell');
  };
  for (const u of [...world.units.values()]) hitEntity(u);
  for (const b of [...world.buildings.values()]) hitEntity(b);
}

// --- Победа ------------------------------------------------------------------------

function checkVictory(world: World): void {
  const alive0 = world.buildingsOfTeam(0).length > 0;
  const alive1 = world.buildingsOfTeam(1).length > 0;
  if (alive0 && alive1) return;
  world.phase = 'ended';
  world.winner = alive0 ? 0 : 1;
}

function dist2(ax: number, ay: number, bx: number, by: number): number {
  const dx = bx - ax, dy = by - ay;
  return dx * dx + dy * dy;
}
