import { BUILDING_DEFS, buildingCost, MAP_TILES, TILE, unitCost } from '../data/balance';
import type { BuildingKind, UnitKind } from '../data/balance';
import {
  canPlaceBuilding, orderAttackMove, orderBuild, orderHarvest, placeBuilding, queueUnit,
} from '../sim/sim';
import type { Building, Unit } from '../sim/types';
import type { World } from '../sim/world';

// ============================================================================
// ИИ: билд-ордер → экономика → оборона → волны нарастающей силы.
// Работает только через публичные приказы симуляции, юнитов не телепортирует.
// ============================================================================

const AI_TEAM = 1 as const;

export class AIController {
  private thinkTimer = 0;
  private buildPlan: BuildingKind[] = [
    'power', 'barracks', 'power', 'turret', 'factory', 'turret', 'power', 'factory', 'turret', 'turret',
  ];
  private planIdx = 0;
  private nextWaveAt: number;
  private waveTarget: number;
  private waveNumber = 0;
  private offensiveIds = new Set<number>();

  constructor(world: World) {
    const d = world.diff();
    this.nextWaveAt = d.firstWaveAt;
    this.waveTarget = d.waveStart;
  }

  update(world: World, dt: number): void {
    this.thinkTimer -= dt;
    if (this.thinkTimer > 0) return;
    this.thinkTimer = 1.0;

    const hq = world.buildingsOfTeam(AI_TEAM).find((b) => b.kind === 'hq' && b.buildProgress >= 1) ?? null;
    const myUnits = world.unitsOfTeam(AI_TEAM);
    const myBuildings = world.buildingsOfTeam(AI_TEAM);

    this.manageEconomy(world, hq, myUnits);
    this.manageConstruction(world, hq, myUnits, myBuildings);
    this.manageArmyProduction(world, myUnits, myBuildings);
    this.manageDefense(world, myUnits, myBuildings);
    this.manageWaves(world, myUnits);
  }

  // --- экономика: грузовики и техники ---------------------------------------

  private manageEconomy(world: World, hq: Building | null, myUnits: Unit[]): void {
    if (!hq) return;
    const d = world.diff();
    const wantTrucks = d.id === 'easy' ? 2 : 3;
    const trucks = myUnits.filter((u) => u.kind === 'truck').length;
    const techs = myUnits.filter((u) => u.kind === 'technician').length;
    const queued = (k: UnitKind) => hq.queue.filter((q) => q.kind === k).length;

    if (techs + queued('technician') < 1) {
      queueUnit(world, hq, 'technician');
    }
    if (trucks + queued('truck') < wantTrucks && hq.queue.length < 2) {
      queueUnit(world, hq, 'truck');
    }

    // простаивающие грузовики — обратно на маршрут
    for (const u of myUnits) {
      if (u.kind === 'truck' && u.order.kind === 'idle') {
        const depot = world.nearestDepotWithSupplies(u.x, u.y);
        if (depot) orderHarvest(world, [u], depot.id);
      }
    }
  }

  // --- стройка по плану --------------------------------------------------------

  private manageConstruction(
    world: World, hq: Building | null, myUnits: Unit[], myBuildings: Building[],
  ): void {
    const techs = myUnits.filter((u) => u.kind === 'technician');
    if (techs.length === 0) return;

    // техников без дела — на ближайшую недостроенную площадку
    const sites = myBuildings.filter((b) => b.buildProgress < 1);
    for (const tech of techs) {
      if (tech.order.kind === 'idle' && sites.length > 0) {
        let best = sites[0], bestD = Infinity;
        for (const s of sites) {
          const d = (s.x - tech.x) ** 2 + (s.y - tech.y) ** 2;
          if (d < bestD) { bestD = d; best = s; }
        }
        orderBuild(world, [tech], best.id);
      }
    }
    if (sites.length > 0) return; // одна стройка за раз

    const next = this.nextBuildItem(world, myBuildings);
    if (!next) return;
    const cost = buildingCost(next, world.factions[AI_TEAM]);
    if (world.credits[AI_TEAM] < cost) return;
    if (!hq) return;

    const spot = this.findSpot(world, next, hq);
    if (!spot) return;
    const freeTech = techs.find((t) => t.order.kind !== 'build') ?? techs[0];
    const placed = placeBuilding(world, next, AI_TEAM, spot.x, spot.y, [freeTech]);
    if (placed && this.planIdx < this.buildPlan.length && this.buildPlan[this.planIdx] === next) {
      this.planIdx++;
    }
  }

  private nextBuildItem(world: World, myBuildings: Building[]): BuildingKind | null {
    const count = (k: BuildingKind) => myBuildings.filter((b) => b.kind === k).length;
    // аварийные приоритеты: энергия и потерянные производственные здания
    if (world.powerUse[AI_TEAM] > world.powerProduce[AI_TEAM]) return 'power';
    if (count('barracks') === 0 && this.planIdx > 1) return 'barracks';
    if (count('factory') === 0 && this.planIdx > 4) return 'factory';
    if (count('hq') === 0) return 'hq';

    if (this.planIdx < this.buildPlan.length) return this.buildPlan[this.planIdx];

    // после плана: расширение при избытке денег
    if (world.credits[AI_TEAM] > 1400) {
      if (count('factory') < 3) return 'factory';
      if (count('turret') < 8) return 'turret';
      if (count('power') < 6) return 'power';
    }
    return null;
  }

  /** Место под здание: кольца вокруг КЦ; турели — со стороны противника. */
  private findSpot(world: World, kind: BuildingKind, hq: Building): { x: number; y: number } | null {
    const def = BUILDING_DEFS[kind];
    const htx = hq.tx + (hq.w >> 1), hty = hq.ty + (hq.h >> 1);
    const enemyHq = world.buildingsOfTeam(0).find((b) => b.kind === 'hq');
    const dirX = enemyHq ? Math.sign(enemyHq.x - hq.x) : -1;
    const dirY = enemyHq ? Math.sign(enemyHq.y - hq.y) : 1;

    let best: { x: number; y: number } | null = null;
    let bestScore = Infinity;
    for (let r = 3; r <= 16; r++) {
      for (let dy = -r; dy <= r; dy++) {
        for (let dx = -r; dx <= r; dx++) {
          if (Math.max(Math.abs(dx), Math.abs(dy)) !== r) continue;
          const tx = htx + dx - (def.w >> 1), ty = hty + dy - (def.h >> 1);
          if (tx < 1 || ty < 1 || tx + def.w > MAP_TILES - 1 || ty + def.h > MAP_TILES - 1) continue;
          if (!canPlaceBuilding(world, kind, tx, ty, AI_TEAM)) continue;
          let score = r;
          if (kind === 'turret') {
            // турели выдвигаем к фронту: ближе к направлению на игрока
            score = -(dx * dirX + dy * dirY) + Math.abs(r - 8) * 0.5;
          }
          if (score < bestScore) { bestScore = score; best = { x: tx, y: ty }; }
        }
      }
      if (best && kind !== 'turret' && bestScore <= r) break; // ближайшее кольцо найдено
    }
    return best;
  }

  // --- производство армии ---------------------------------------------------------

  private manageArmyProduction(world: World, myUnits: Unit[], myBuildings: Building[]): void {
    const d = world.diff();
    const army = myUnits.filter((u) => isCombat(u));
    if (army.length >= d.armyCap) return;

    // держим резерв на здания, пока план не выполнен
    const reserve = this.planIdx < this.buildPlan.length ? 320 : 0;

    for (const b of myBuildings) {
      if (b.buildProgress < 1 || b.queue.length >= 2) continue;
      if (b.kind === 'barracks') {
        const kind: UnitKind = world.rand() < 0.55 ? 'rifleman' : 'rocketeer';
        if (world.credits[AI_TEAM] - unitCost(kind, world.factions[AI_TEAM]) >= reserve) {
          queueUnit(world, b, kind);
        }
      } else if (b.kind === 'factory') {
        const roll = world.rand();
        let kind: UnitKind;
        if (this.waveNumber < 1) kind = roll < 0.5 ? 'scout' : 'tank';
        else if (roll < 0.5) kind = 'tank';
        else if (roll < 0.8) kind = 'artillery';
        else kind = 'scout';
        if (world.credits[AI_TEAM] - unitCost(kind, world.factions[AI_TEAM]) >= reserve) {
          queueUnit(world, b, kind);
        }
      }
    }
  }

  // --- оборона базы ----------------------------------------------------------------

  private manageDefense(world: World, myUnits: Unit[], myBuildings: Building[]): void {
    const attacked = myBuildings.find((b) => b.attackedFlash > 0);
    if (!attacked) return;
    for (const u of myUnits) {
      if (!isCombat(u) || this.offensiveIds.has(u.id)) continue;
      if (u.order.kind === 'idle' || u.order.kind === 'move') {
        orderAttackMove(world, [u], attacked.x, attacked.y);
      }
    }
  }

  // --- волны атак --------------------------------------------------------------------

  private manageWaves(world: World, myUnits: Unit[]): void {
    // чистка списка наступающих
    for (const id of [...this.offensiveIds]) {
      const u = world.units.get(id);
      if (!u || u.order.kind === 'idle') this.offensiveIds.delete(id);
    }

    if (world.time < this.nextWaveAt) return;
    const d = world.diff();
    const ready = myUnits.filter((u) => isCombat(u) && !this.offensiveIds.has(u.id));
    if (ready.length < Math.max(2, Math.floor(this.waveTarget * 0.6))) {
      this.nextWaveAt = world.time + 15; // армия не готова — короткая отсрочка
      return;
    }

    const target = this.pickAttackTarget(world);
    if (!target) return;
    const squad = ready.slice(0, this.waveTarget);
    orderAttackMove(world, squad, target.x, target.y);
    for (const u of squad) this.offensiveIds.add(u.id);

    this.waveNumber++;
    this.waveTarget += d.waveGrowth;
    this.nextWaveAt = world.time + Math.max(45, d.waveInterval - this.waveNumber * 3);
  }

  private pickAttackTarget(world: World): { x: number; y: number } | null {
    const enemyBuildings = world.buildingsOfTeam(0);
    if (enemyBuildings.length === 0) return null;
    const myHq = world.buildingsOfTeam(AI_TEAM)[0];
    const fx = myHq ? myHq.x : MAP_TILES * TILE * 0.8;
    const fy = myHq ? myHq.y : MAP_TILES * TILE * 0.2;
    let best = enemyBuildings[0], bestD = Infinity;
    for (const b of enemyBuildings) {
      const dd = (b.x - fx) ** 2 + (b.y - fy) ** 2;
      if (dd < bestD) { bestD = dd; best = b; }
    }
    return { x: best.x, y: best.y };
  }
}

function isCombat(u: Unit): boolean {
  return u.weapon !== undefined;
}
