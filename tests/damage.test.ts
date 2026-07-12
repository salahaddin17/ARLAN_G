import { describe, expect, it } from 'vitest';
import {
  DAMAGE_MATRIX, damageMultiplier, effectiveDamage, FACTIONS,
  unitCost, unitHp, UNIT_DEFS,
} from '../src/data/balance';
import type { ArmorType, DamageType } from '../src/data/balance';

const DMG_TYPES: DamageType[] = ['bullet', 'rocket', 'shell', 'defense'];
const ARMOR_TYPES: ArmorType[] = ['INF', 'LGT', 'HVY', 'BLD'];

describe('Матрица урона', () => {
  it('заполнена полностью и все множители положительны', () => {
    for (const d of DMG_TYPES) {
      for (const a of ARMOR_TYPES) {
        const m = DAMAGE_MATRIX[d][a];
        expect(m, `${d} × ${a}`).toBeTypeOf('number');
        expect(m).toBeGreaterThan(0);
        expect(m).toBeLessThanOrEqual(2);
      }
    }
  });

  it('камень-ножницы-бумага: пули > пехота, ракеты > тяж. броня, снаряды > здания', () => {
    // пули лучше всего по пехоте и почти бесполезны по тяжёлой броне/зданиям
    expect(DAMAGE_MATRIX.bullet.INF).toBeGreaterThan(DAMAGE_MATRIX.bullet.HVY);
    expect(DAMAGE_MATRIX.bullet.INF).toBeGreaterThan(DAMAGE_MATRIX.bullet.BLD);
    // ракеты — лучший ответ тяжёлой броне
    expect(DAMAGE_MATRIX.rocket.HVY).toBeGreaterThan(DAMAGE_MATRIX.bullet.HVY);
    expect(DAMAGE_MATRIX.rocket.HVY).toBeGreaterThan(DAMAGE_MATRIX.rocket.INF);
    // снаряды ломают здания лучше остальных типов
    expect(DAMAGE_MATRIX.shell.BLD).toBeGreaterThan(DAMAGE_MATRIX.bullet.BLD);
    expect(DAMAGE_MATRIX.shell.BLD).toBeGreaterThan(DAMAGE_MATRIX.rocket.BLD);
    expect(DAMAGE_MATRIX.shell.BLD).toBeGreaterThan(DAMAGE_MATRIX.defense.BLD);
    // турели не годятся для сноса зданий (защитный, а не штурмовой урон)
    expect(DAMAGE_MATRIX.defense.BLD).toBeLessThanOrEqual(0.5);
  });

  it('пехота уязвима пулям сильнее, чем ракетам', () => {
    expect(DAMAGE_MATRIX.bullet.INF).toBeGreaterThan(DAMAGE_MATRIX.rocket.INF);
  });

  it('damageMultiplier и effectiveDamage согласованы', () => {
    for (const d of DMG_TYPES) {
      for (const a of ARMOR_TYPES) {
        expect(damageMultiplier(d, a)).toBe(DAMAGE_MATRIX[d][a]);
        expect(effectiveDamage(100, d, a)).toBeCloseTo(100 * DAMAGE_MATRIX[d][a]);
      }
    }
  });

  it('пример: ракетчик против танка эффективнее пехотинца против танка', () => {
    const rocketeer = UNIT_DEFS.rocketeer.weapon!;
    const rifleman = UNIT_DEFS.rifleman.weapon!;
    const vsTank = (dmg: number, t: DamageType) => effectiveDamage(dmg, t, 'HVY');
    const rocketDps = vsTank(rocketeer.damage, rocketeer.type) / rocketeer.cooldown;
    const bulletDps = vsTank(rifleman.damage, rifleman.type) / rifleman.cooldown;
    expect(rocketDps).toBeGreaterThan(bulletDps * 2);
  });
});

describe('Модификаторы фракций', () => {
  it('Легион дороже и крепче, Вольный фронт дешевле и быстрее', () => {
    expect(FACTIONS.legion.costMul).toBeCloseTo(1.15);
    expect(FACTIONS.legion.hpMul).toBeCloseTo(1.15);
    expect(FACTIONS.front.costMul).toBeCloseTo(0.85);
    expect(FACTIONS.front.hpMul).toBeCloseTo(0.9);
    expect(FACTIONS.front.speedMul).toBeGreaterThan(FACTIONS.legion.speedMul);
  });

  it('модификаторы применяются к юнитам', () => {
    expect(unitCost('tank', 'legion')).toBe(Math.round(UNIT_DEFS.tank.cost * 1.15));
    expect(unitCost('tank', 'front')).toBe(Math.round(UNIT_DEFS.tank.cost * 0.85));
    expect(unitHp('rifleman', 'legion')).toBeGreaterThan(unitHp('rifleman', 'front'));
  });
});
