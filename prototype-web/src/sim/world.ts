import {
  BUILDING_DEFS, buildingHp, DIFFICULTIES, ECONOMY, FACTIONS, MAP_TILES, TILE,
  unitHp, unitSpeed, UNIT_DEFS,
} from '../data/balance';
import type {
  BuildingKind, DifficultyId, FactionId, UnitKind,
} from '../data/balance';
import { generateMap, mulberry32 } from './grid';
import type { MapGrid, MapLayout } from './grid';
import type {
  Building, BuildingMemory, Depot, FxEvent, Projectile, Team, TeamStats, Unit,
} from './types';

export type GamePhase = 'playing' | 'ended';

export class World {
  grid: MapGrid;
  layout: MapLayout;
  rand: () => number;

  units = new Map<number, Unit>();
  buildings = new Map<number, Building>();
  depots = new Map<number, Depot>();
  projectiles: Projectile[] = [];
  fx: FxEvent[] = [];

  credits: [number, number];
  powerProduce: [number, number] = [0, 0];
  powerUse: [number, number] = [0, 0];

  factions: [FactionId, FactionId];
  difficulty: DifficultyId;
  time = 0;
  tick = 0;
  phase: GamePhase = 'playing';
  winner: Team | -1 = -1;

  stats: [TeamStats, TeamStats];

  // туман войны игрока: 0 — не разведано, 1 — разведано, 2 — видно сейчас
  fog = new Uint8Array(MAP_TILES * MAP_TILES);
  /** память о зданиях противника, виденных сквозь туман */
  buildingMemory = new Map<number, BuildingMemory>();

  private nextId = 1;

  constructor(playerFaction: FactionId, difficulty: DifficultyId, seed = 1337) {
    this.rand = mulberry32(seed);
    this.layout = generateMap(seed);
    this.grid = this.layout.grid;
    this.difficulty = difficulty;
    this.factions = [playerFaction, playerFaction === 'legion' ? 'front' : 'legion'];
    this.credits = [ECONOMY.startCredits, ECONOMY.startCredits];
    this.stats = [emptyStats(), emptyStats()];

    for (const spot of this.layout.depotSpots) {
      const id = this.nextId++;
      const amount = spot.central ? ECONOMY.depotCenter : ECONOMY.depotNearBase;
      this.depots.set(id, { id, x: spot.x, y: spot.y, amount, initial: amount });
    }

    for (const team of [0, 1] as Team[]) {
      const hqTile = this.layout.hqTiles[team];
      const hq = this.spawnBuilding('hq', team, hqTile.x, hqTile.y, true);
      const bx = hq.x, by = hq.y;
      const below = team === 0 ? 1 : -1;
      this.spawnUnit('technician', team, bx - 40, by + below * 70);
      const truck = this.spawnUnit('truck', team, bx + 40, by + below * 70);
      const depot = this.nearestDepotWithSupplies(truck.x, truck.y);
      if (depot) truck.order = { kind: 'harvest', depotId: depot.id };
      this.spawnUnit('rifleman', team, bx - 12, by + below * 95);
      this.spawnUnit('rifleman', team, bx + 12, by + below * 95);
    }
  }

  faction(team: Team) {
    return FACTIONS[this.factions[team]];
  }
  diff() {
    return DIFFICULTIES[this.difficulty];
  }

  spawnUnit(kind: UnitKind, team: Team, x: number, y: number): Unit {
    const def = UNIT_DEFS[kind];
    const f = this.factions[team];
    const hp = unitHp(kind, f);
    const u: Unit = {
      id: this.nextId++, kind, team,
      x, y, prevX: x, prevY: y,
      angle: team === 0 ? -Math.PI / 2 : Math.PI / 2,
      hp, maxHp: hp,
      speed: unitSpeed(kind, f),
      radius: def.radius,
      armor: def.armor,
      vision: def.vision,
      weapon: def.weapon,
      cooldown: 0,
      order: { kind: 'idle' },
      path: null, pathIdx: 0, goalX: x, goalY: y, repathCd: 0,
      engageId: 0, scanCd: this.rand(),
      cargo: 0, harvestPhase: 'toDepot', harvestTimer: 0, lastDepotId: 0,
      dead: false,
    };
    this.units.set(u.id, u);
    this.stats[team].unitsBuilt++;
    return u;
  }

  /** Создать здание. completed=true — сразу готовое (стартовые КЦ). */
  spawnBuilding(kind: BuildingKind, team: Team, tx: number, ty: number, completed: boolean): Building {
    const def = BUILDING_DEFS[kind];
    const f = this.factions[team];
    const maxHp = buildingHp(kind, f);
    const x = (tx + def.w / 2) * TILE;
    const y = (ty + def.h / 2) * TILE;
    const b: Building = {
      id: this.nextId++, kind, team, tx, ty, w: def.w, h: def.h, x, y,
      hp: completed ? maxHp : Math.max(1, Math.round(maxHp * 0.1)),
      maxHp,
      buildProgress: completed ? 1 : 0,
      queue: [],
      rallyX: x, rallyY: y + (team === 0 ? (def.h / 2) * TILE + 30 : -(def.h / 2) * TILE - 30),
      weapon: def.weapon, cooldown: 0, targetId: 0,
      vision: def.vision,
      dead: false, attackedFlash: 0,
    };
    this.grid.setBuilding(tx, ty, def.w, def.h, true);
    this.buildings.set(b.id, b);
    if (completed) this.stats[team].buildingsBuilt++;
    return b;
  }

  removeBuilding(b: Building): void {
    b.dead = true;
    this.grid.setBuilding(b.tx, b.ty, b.w, b.h, false);
    this.buildings.delete(b.id);
    this.buildingMemory.delete(b.id);
  }

  // --- запросы -------------------------------------------------------------

  unitsOfTeam(team: Team): Unit[] {
    const out: Unit[] = [];
    for (const u of this.units.values()) if (u.team === team) out.push(u);
    return out;
  }
  buildingsOfTeam(team: Team): Building[] {
    const out: Building[] = [];
    for (const b of this.buildings.values()) if (b.team === team) out.push(b);
    return out;
  }

  nearestDepotWithSupplies(x: number, y: number): Depot | null {
    let best: Depot | null = null;
    let bestD = Infinity;
    for (const d of this.depots.values()) {
      if (d.amount <= 0) continue;
      const dd = (d.x - x) ** 2 + (d.y - y) ** 2;
      if (dd < bestD) { bestD = dd; best = d; }
    }
    return best;
  }

  nearestHq(team: Team, x: number, y: number): Building | null {
    let best: Building | null = null;
    let bestD = Infinity;
    for (const b of this.buildings.values()) {
      if (b.team !== team || b.kind !== 'hq' || b.buildProgress < 1) continue;
      const dd = (b.x - x) ** 2 + (b.y - y) ** 2;
      if (dd < bestD) { bestD = dd; best = b; }
    }
    return best;
  }

  /** Видимость клетки игроком (для наведения и отрисовки). team 1 видит всё. */
  tileVisibleBy(team: Team, wx: number, wy: number): boolean {
    if (team === 1) return true;
    const tx = Math.floor(wx / TILE), ty = Math.floor(wy / TILE);
    if (tx < 0 || ty < 0 || tx >= MAP_TILES || ty >= MAP_TILES) return false;
    return this.fog[ty * MAP_TILES + tx] === 2;
  }

  addFx(e: FxEvent): void {
    if (this.fx.length < 400) this.fx.push(e);
  }
}

function emptyStats(): TeamStats {
  return {
    unitsBuilt: 0, unitsLost: 0, buildingsBuilt: 0, buildingsLost: 0,
    suppliesGathered: 0, enemiesKilled: 0,
  };
}
