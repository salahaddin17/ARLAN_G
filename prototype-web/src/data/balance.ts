// ============================================================================
// РУБЕЖ: АРЛАН — таблицы баланса. Единственный источник числовых данных игры.
// ============================================================================

export const TILE = 32;              // размер клетки в мировых пикселях
export const MAP_TILES = 64;         // карта 64×64 клетки
export const MAP_SIZE = TILE * MAP_TILES;
export const TICK_RATE = 20;         // Гц симуляции
export const TICK_DT = 1 / TICK_RATE;

export type FactionId = 'legion' | 'front';
export type DamageType = 'bullet' | 'rocket' | 'shell' | 'defense';
export type ArmorType = 'INF' | 'LGT' | 'HVY' | 'BLD';
export type UnitKind =
  | 'technician' | 'truck' | 'rifleman' | 'rocketeer' | 'scout' | 'tank' | 'artillery';
export type BuildingKind = 'hq' | 'power' | 'barracks' | 'factory' | 'turret';
export type DifficultyId = 'easy' | 'normal' | 'hard';

// --- Матрица «тип урона × тип брони» -----------------------------------
// bullet  — стрелковое: косит пехоту, бессильно против брони и стен
// rocket  — кумулятивное: жжёт тяжёлую технику, слабо по пехоте
// shell   — осколочно-фугасное: универсально, ломает здания
// defense — турельное: сдерживает всё, но здания не штурмует
export const DAMAGE_MATRIX: Record<DamageType, Record<ArmorType, number>> = {
  bullet:  { INF: 1.25, LGT: 0.75, HVY: 0.40, BLD: 0.35 },
  rocket:  { INF: 0.60, LGT: 1.00, HVY: 1.30, BLD: 0.90 },
  shell:   { INF: 0.80, LGT: 1.15, HVY: 1.00, BLD: 1.25 },
  defense: { INF: 1.00, LGT: 1.00, HVY: 0.80, BLD: 0.50 },
};

export function damageMultiplier(dmg: DamageType, armor: ArmorType): number {
  return DAMAGE_MATRIX[dmg][armor];
}

export function effectiveDamage(base: number, dmg: DamageType, armor: ArmorType): number {
  return base * DAMAGE_MATRIX[dmg][armor];
}

// --- Фракции ------------------------------------------------------------
export interface FactionDef {
  id: FactionId;
  name: string;
  color: string;
  colorDark: string;
  costMul: number;
  hpMul: number;
  speedMul: number;
}

export const FACTIONS: Record<FactionId, FactionDef> = {
  legion: {
    id: 'legion', name: 'Легион',
    color: '#0e8f83', colorDark: '#075e56',
    costMul: 1.15, hpMul: 1.15, speedMul: 1.0,
  },
  front: {
    id: 'front', name: 'Вольный фронт',
    color: '#d95f18', colorDark: '#8f3d0d',
    costMul: 0.85, hpMul: 0.90, speedMul: 1.10,
  },
};

// --- Оружие ---------------------------------------------------------------
export interface WeaponDef {
  damage: number;
  range: number;          // мировые пиксели
  cooldown: number;       // секунды
  type: DamageType;
  splash?: number;        // радиус сплеша (артиллерия)
  projectileSpeed?: number; // если задан — снаряд летит, иначе мгновенное попадание
  minRange?: number;
}

// --- Юниты ----------------------------------------------------------------
export interface UnitDef {
  kind: UnitKind;
  name: string;
  cost: number;
  hp: number;
  speed: number;          // пикс/с
  armor: ArmorType;
  vision: number;         // клетки
  buildTime: number;      // секунды
  radius: number;         // пиксели, для столкновений/отрисовки
  weapon?: WeaponDef;
  cargo?: number;         // вместимость грузовика
  isInfantry: boolean;    // круглый жетон vs прямоугольная фишка
  producedAt: BuildingKind;
  hotkey: string;
  desc: string;
}

export const UNIT_DEFS: Record<UnitKind, UnitDef> = {
  technician: {
    kind: 'technician', name: 'Техник', cost: 120, hp: 60, speed: 55,
    armor: 'INF', vision: 5, buildTime: 8, radius: 7, isInfantry: true,
    producedAt: 'hq', hotkey: 'Q', desc: 'Строит и чинит здания',
  },
  truck: {
    kind: 'truck', name: 'Грузовик', cost: 140, hp: 150, speed: 78,
    armor: 'LGT', vision: 5, buildTime: 10, radius: 10, cargo: 40, isInfantry: false,
    producedAt: 'hq', hotkey: 'W', desc: 'Возит припасы со складов',
  },
  rifleman: {
    kind: 'rifleman', name: 'Пехотинец', cost: 60, hp: 70, speed: 50,
    armor: 'INF', vision: 6, buildTime: 5, radius: 7, isInfantry: true,
    producedAt: 'barracks', hotkey: 'Q', desc: 'Стрелок: силён против пехоты',
    weapon: { damage: 9, range: 110, cooldown: 0.7, type: 'bullet' },
  },
  rocketeer: {
    kind: 'rocketeer', name: 'Ракетчик', cost: 110, hp: 60, speed: 46,
    armor: 'INF', vision: 6, buildTime: 7, radius: 7, isInfantry: true,
    producedAt: 'barracks', hotkey: 'W', desc: 'РПГ: жжёт технику',
    weapon: { damage: 26, range: 145, cooldown: 1.9, type: 'rocket' },
  },
  scout: {
    kind: 'scout', name: 'Разведмашина', cost: 130, hp: 120, speed: 112,
    armor: 'LGT', vision: 9, buildTime: 8, radius: 10, isInfantry: false,
    producedAt: 'factory', hotkey: 'Q', desc: 'Быстрая, дальний обзор',
    weapon: { damage: 8, range: 120, cooldown: 0.35, type: 'bullet' },
  },
  tank: {
    kind: 'tank', name: 'Танк', cost: 320, hp: 340, speed: 62,
    armor: 'HVY', vision: 6, buildTime: 14, radius: 12, isInfantry: false,
    producedAt: 'factory', hotkey: 'W', desc: 'Ударная броня',
    weapon: { damage: 46, range: 150, cooldown: 2.2, type: 'shell' },
  },
  artillery: {
    kind: 'artillery', name: 'Артиллерия', cost: 380, hp: 160, speed: 46,
    armor: 'LGT', vision: 6, buildTime: 16, radius: 12, isInfantry: false,
    producedAt: 'factory', hotkey: 'E', desc: 'Дальний сплеш-урон',
    weapon: {
      damage: 60, range: 265, cooldown: 4.6, type: 'shell',
      splash: 38, projectileSpeed: 190, minRange: 70,
    },
  },
};

// --- Здания -----------------------------------------------------------------
export interface BuildingDef {
  kind: BuildingKind;
  name: string;
  cost: number;
  hp: number;
  w: number;              // клетки
  h: number;
  buildTime: number;      // секунды стройки техником
  vision: number;         // клетки
  powerProduce: number;
  powerUse: number;
  produces: UnitKind[];
  weapon?: WeaponDef;
  hotkey: string;
  desc: string;
}

export const BUILDING_DEFS: Record<BuildingKind, BuildingDef> = {
  hq: {
    kind: 'hq', name: 'Командный центр', cost: 900, hp: 1600, w: 4, h: 3,
    buildTime: 45, vision: 7, powerProduce: 10, powerUse: 0,
    produces: ['technician', 'truck'], hotkey: 'Q',
    desc: 'Приём припасов, выпуск техников и грузовиков',
  },
  power: {
    kind: 'power', name: 'Энергостанция', cost: 220, hp: 500, w: 2, h: 2,
    buildTime: 14, vision: 4, powerProduce: 25, powerUse: 0,
    produces: [], hotkey: 'W', desc: '+25 энергии',
  },
  barracks: {
    kind: 'barracks', name: 'Казармы', cost: 300, hp: 700, w: 3, h: 2,
    buildTime: 18, vision: 5, powerProduce: 0, powerUse: 10,
    produces: ['rifleman', 'rocketeer'], hotkey: 'E', desc: 'Готовит пехоту',
  },
  factory: {
    kind: 'factory', name: 'Завод техники', cost: 520, hp: 1000, w: 3, h: 3,
    buildTime: 26, vision: 5, powerProduce: 0, powerUse: 15,
    produces: ['scout', 'tank', 'artillery'], hotkey: 'R', desc: 'Выпускает технику',
  },
  turret: {
    kind: 'turret', name: 'Турель', cost: 260, hp: 420, w: 1, h: 1,
    buildTime: 12, vision: 6, powerProduce: 0, powerUse: 10,
    produces: [], hotkey: 'T', desc: 'Автономная оборона',
    weapon: { damage: 22, range: 185, cooldown: 1.1, type: 'defense' },
  },
};

// --- Экономика ----------------------------------------------------------------
export const ECONOMY = {
  startCredits: 600,
  depotNearBase: 2600,     // запас в складах у баз
  depotCenter: 4200,       // запас в центральных складах
  loadRate: 22,            // припасов/с при погрузке
  unloadTime: 1.2,         // секунды разгрузки у КЦ
  lowPowerMul: 0.5,        // множитель производства при дефиците энергии
  repairSpeedMul: 0.5,     // ремонт медленнее стройки
};

// --- ИИ / сложность --------------------------------------------------------------
export interface DifficultyDef {
  id: DifficultyId;
  name: string;
  incomeMul: number;       // множитель дохода ИИ
  firstWaveAt: number;     // секунда старта первой волны (стычка на 3–5 минуте)
  waveInterval: number;    // базовый интервал между волнами
  waveStart: number;       // размер первой волны
  waveGrowth: number;      // прирост размера волны
  armyCap: number;         // максимум боевых юнитов ИИ
}

export const DIFFICULTIES: Record<DifficultyId, DifficultyDef> = {
  easy:   { id: 'easy',   name: 'Рекрут',   incomeMul: 0.8,  firstWaveAt: 230, waveInterval: 110, waveStart: 3, waveGrowth: 2, armyCap: 14 },
  normal: { id: 'normal', name: 'Ветеран',  incomeMul: 1.0,  firstWaveAt: 190, waveInterval: 90,  waveStart: 5, waveGrowth: 3, armyCap: 22 },
  hard:   { id: 'hard',   name: 'Комиссар', incomeMul: 1.25, firstWaveAt: 165, waveInterval: 72,  waveStart: 6, waveGrowth: 4, armyCap: 32 },
};

// --- Модификаторы фракций -----------------------------------------------------------
export function unitCost(kind: UnitKind, faction: FactionId): number {
  return Math.round(UNIT_DEFS[kind].cost * FACTIONS[faction].costMul);
}
export function unitHp(kind: UnitKind, faction: FactionId): number {
  return Math.round(UNIT_DEFS[kind].hp * FACTIONS[faction].hpMul);
}
export function unitSpeed(kind: UnitKind, faction: FactionId): number {
  return UNIT_DEFS[kind].speed * FACTIONS[faction].speedMul;
}
export function buildingCost(kind: BuildingKind, faction: FactionId): number {
  return Math.round(BUILDING_DEFS[kind].cost * FACTIONS[faction].costMul);
}
export function buildingHp(kind: BuildingKind, faction: FactionId): number {
  return Math.round(BUILDING_DEFS[kind].hp * FACTIONS[faction].hpMul);
}
