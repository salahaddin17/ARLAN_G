import type { ArmorType, BuildingKind, UnitKind, WeaponDef } from '../data/balance';

export type Team = 0 | 1; // 0 — игрок, 1 — ИИ

export type Order =
  | { kind: 'idle' }
  | { kind: 'move'; x: number; y: number }
  | { kind: 'attackMove'; x: number; y: number }
  | { kind: 'attack'; targetId: number }
  | { kind: 'harvest'; depotId: number }
  | { kind: 'build'; buildingId: number };

export type HarvestPhase = 'toDepot' | 'loading' | 'toBase' | 'unloading';

export interface Unit {
  id: number;
  kind: UnitKind;
  team: Team;
  x: number;
  y: number;
  prevX: number;          // для интерполяции рендера
  prevY: number;
  angle: number;          // радианы, направление корпуса
  hp: number;
  maxHp: number;
  speed: number;
  radius: number;
  armor: ArmorType;
  vision: number;         // клетки
  weapon?: WeaponDef;
  cooldown: number;
  order: Order;
  path: { x: number; y: number }[] | null;
  pathIdx: number;
  goalX: number;          // конечная точка текущего пути
  goalY: number;
  repathCd: number;
  engageId: number;       // автоцель при attackMove/idle; 0 = нет
  scanCd: number;
  // грузовик
  cargo: number;
  harvestPhase: HarvestPhase;
  harvestTimer: number;
  lastDepotId: number;
  dead: boolean;
}

export interface QueueItem {
  kind: UnitKind;
  progress: number;       // 0..1
  cost: number;
}

export interface Building {
  id: number;
  kind: BuildingKind;
  team: Team;
  tx: number;             // клетка левого верхнего угла
  ty: number;
  w: number;
  h: number;
  x: number;              // центр в мире
  y: number;
  hp: number;
  maxHp: number;
  buildProgress: number;  // 0..1; <1 = стройплощадка
  queue: QueueItem[];
  rallyX: number;
  rallyY: number;
  weapon?: WeaponDef;
  cooldown: number;
  targetId: number;
  vision: number;
  dead: boolean;
  attackedFlash: number;  // сек, метка «нас бьют» для ИИ и миникарты
}

export interface Depot {
  id: number;
  x: number;
  y: number;
  amount: number;
  initial: number;
}

export interface Projectile {
  id: number;
  x: number;
  y: number;
  tx: number;
  ty: number;
  speed: number;
  damage: number;
  dmgType: 'shell';
  splash: number;
  team: Team;
  totalDist: number;
  flown: number;
}

// Событие для рендера (эффекты) — сим складывает, рендер потребляет.
export type FxEvent =
  | { kind: 'tracer'; x1: number; y1: number; x2: number; y2: number; dmgType: string }
  | { kind: 'hit'; x: number; y: number }
  | { kind: 'explosion'; x: number; y: number; r: number }
  | { kind: 'orderFlag'; x: number; y: number; attack: boolean };

export interface TeamStats {
  unitsBuilt: number;
  unitsLost: number;
  buildingsBuilt: number;
  buildingsLost: number;
  suppliesGathered: number;
  enemiesKilled: number;
}

export interface BuildingMemory {
  id: number;
  kind: BuildingKind;
  team: Team;
  tx: number;
  ty: number;
  w: number;
  h: number;
  x: number;
  y: number;
}
