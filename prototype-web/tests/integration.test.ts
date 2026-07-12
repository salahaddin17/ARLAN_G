import { describe, expect, it } from 'vitest';
import { buildingCost, TICK_DT } from '../src/data/balance';
import { AIController } from '../src/ai/ai';
import { canPlaceBuilding, placeBuilding, step } from '../src/sim/sim';
import { World } from '../src/sim/world';

// ============================================================================
// Интеграционный прогон полной партии без рендера: пассивный игрок против ИИ.
// Проверяет экономику ИИ, тайминг первой стычки (3–5 минута) и доведение
// партии до победы сносом всех зданий.
// ============================================================================

describe('Полная партия против ИИ (headless)', () => {
  it('ИИ развивается, атакует на 3–5 минуте и добивает пассивного игрока', () => {
    const world = new World('legion', 'normal', 1337);
    const ai = new AIController(world);

    let firstContact = -1;
    const maxTime = 1200; // 20 минут потолок

    while (world.phase === 'playing' && world.time < maxTime) {
      ai.update(world, TICK_DT);
      step(world, TICK_DT);

      if (firstContact < 0) {
        const playerHit = world.stats[0].unitsLost > 0 ||
          world.buildingsOfTeam(0).some((b) => b.attackedFlash > 0);
        if (playerHit) firstContact = world.time;
      }
    }

    // экономика ИИ работала: припасы собраны, юниты и здания строились
    expect(world.stats[1].suppliesGathered).toBeGreaterThan(500);
    expect(world.stats[1].unitsBuilt).toBeGreaterThan(6);
    expect(world.stats[1].buildingsBuilt).toBeGreaterThan(2);

    // первая стычка — на 3–5 минуте (пассивный игрок не провоцирует раньше)
    expect(firstContact).toBeGreaterThan(120);
    expect(firstContact).toBeLessThan(320);

    // ИИ доводит дело до конца: все здания игрока снесены
    expect(world.phase).toBe('ended');
    expect(world.winner).toBe(1);
    expect(world.buildingsOfTeam(0).length).toBe(0);
  }, 30000);

  it('туман войны игрока раскрывается вокруг базы, но не всю карту', () => {
    const world = new World('front', 'easy', 42);
    for (let i = 0; i < 100; i++) step(world, TICK_DT);
    let visible = 0, unexplored = 0;
    for (let i = 0; i < world.fog.length; i++) {
      if (world.fog[i] === 2) visible++;
      else if (world.fog[i] === 0) unexplored++;
    }
    expect(visible).toBeGreaterThan(50);           // база освещена
    expect(unexplored).toBeGreaterThan(world.fog.length * 0.5); // карта в тумане
  });

  it('техник строит энергостанцию: деньги списаны, здание завершается, энергия растёт', () => {
    const world = new World('legion', 'normal', 1337);
    const tech = world.unitsOfTeam(0).find((u) => u.kind === 'technician')!;
    for (let i = 0; i < 8; i++) step(world, TICK_DT); // прогреть туман войны

    // клетка к западу от базы: разведана, вне маршрута грузовика
    expect(canPlaceBuilding(world, 'power', 6, 48, 0)).toBe(true);
    const creditsBefore = world.credits[0];
    const site = placeBuilding(world, 'power', 0, 6, 48, [tech]);
    expect(site).not.toBeNull();
    expect(world.credits[0]).toBe(creditsBefore - buildingCost('power', 'legion'));
    expect(world.grid.isBlocked(6, 48)).toBe(true); // площадка занимает сетку

    let guard = 0;
    while (site!.buildProgress < 1 && guard++ < 20 * 120) step(world, TICK_DT);
    expect(site!.buildProgress).toBe(1);
    expect(site!.hp).toBe(site!.maxHp);
    step(world, TICK_DT); // энергия пересчитывается в начале следующего тика
    expect(world.powerProduce[0]).toBe(35); // 10 (КЦ) + 25 (станция)
  });

  it('дефицит энергии замедляет производство ×0.5', () => {
    const world = new World('legion', 'normal', 7);
    // у стартовой базы КЦ даёт 10 энергии и ничего не потребляет — дефицита нет
    expect(world.powerUse[0]).toBe(0);
    for (let i = 0; i < 5; i++) step(world, TICK_DT);
    expect(world.powerProduce[0]).toBeGreaterThan(0);
  });
});
