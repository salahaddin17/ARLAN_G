import { describe, expect, it } from 'vitest';
import { MapGrid } from '../src/sim/grid';
import { findPath, nearestFreeTile, smoothPath, tileLineClear } from '../src/sim/pathfinding';

function gridFrom(rows: string[]): MapGrid {
  const g = new MapGrid();
  // остальная карта свободна; '#' — скала
  for (let y = 0; y < rows.length; y++) {
    for (let x = 0; x < rows[y].length; x++) {
      if (rows[y][x] === '#') g.cells[g.idx(x, y)] = 1;
    }
  }
  return g;
}

describe('A* поиск пути', () => {
  it('находит прямой путь по открытому полю', () => {
    const g = new MapGrid();
    const path = findPath(g, 0, 0, 5, 0);
    expect(path).not.toBeNull();
    expect(path![path!.length - 1]).toEqual({ x: 5, y: 0 });
    expect(path!.length).toBe(5);
  });

  it('обходит стену', () => {
    const g = gridFrom([
      '.....',
      '.###.',
      '.#...',
      '.#.##',
      '...#.',
    ]);
    const path = findPath(g, 0, 2, 4, 2)!;
    expect(path).not.toBeNull();
    expect(path[path.length - 1]).toEqual({ x: 4, y: 2 });
    // каждый шаг проходит по свободным клеткам
    for (const p of path) expect(g.isBlocked(p.x, p.y)).toBe(false);
    // путь длиннее прямого (обход)
    expect(path.length).toBeGreaterThan(4);
  });

  it('возвращает null для полностью отрезанной цели', () => {
    const g = gridFrom([
      '.....',
      '.....',
      '#####',
      '#####',
    ]);
    // отрезать нижнюю часть карты стеной во всю ширину
    for (let x = 0; x < g.size; x++) {
      g.cells[g.idx(x, 2)] = 1;
      g.cells[g.idx(x, 3)] = 1;
    }
    const path = findPath(g, 0, 0, 0, 10);
    expect(path).toBeNull();
  });

  it('НЕ срезает углы по диагонали', () => {
    // Цель — угол карты (0,0). Его ортогональные соседи (1,0) и (0,1) — скалы,
    // попасть в угол можно только диагональю (1,1)→(0,0), которая срезает угол.
    // Раз срезание запрещено — угол недостижим.
    const g = gridFrom([
      '.#',
      '#.',
    ]);
    const path = findPath(g, 5, 5, 0, 0);
    expect(path).toBeNull();
  });

  it('диагональ разрешена при свободных ортогоналях', () => {
    const g = new MapGrid();
    const path = findPath(g, 0, 0, 3, 3)!;
    expect(path.length).toBe(3); // чистая диагональ
    for (let i = 0; i < path.length; i++) {
      expect(path[i]).toEqual({ x: i + 1, y: i + 1 });
    }
  });

  it('срезание одного угла у препятствия запрещено', () => {
    const g = gridFrom([
      '..',
      '#.',
      '..',
    ]);
    // (0,0) → (0,2): прямой диагональный маршрут (0,0)→(1,1)→(0,2) допустим,
    // но шаг (0,0)→(1,1) касается блока (0,1)? Нет: ортогонали (1,0) и (0,1);
    // (0,1) — блок, значит диагональ запрещена и путь длиннее.
    const path = findPath(g, 0, 0, 0, 2)!;
    expect(path).not.toBeNull();
    for (let i = 0; i + 1 <= path.length - 1; i++) {
      const a = i === 0 ? { x: 0, y: 0 } : path[i - 1];
      const b = path[i];
      if (Math.abs(b.x - a.x) === 1 && Math.abs(b.y - a.y) === 1) {
        // проверить, что обе ортогонали свободны
        expect(g.isBlocked(a.x + (b.x - a.x), a.y)).toBe(false);
        expect(g.isBlocked(a.x, a.y + (b.y - a.y))).toBe(false);
      }
    }
  });

  it('цель в стене переносится на ближайшую свободную клетку', () => {
    const g = gridFrom([
      '.....',
      '.###.',
      '.###.',
      '.....',
    ]);
    const path = findPath(g, 0, 0, 2, 1)!;
    expect(path).not.toBeNull();
    const end = path[path.length - 1];
    expect(g.isBlocked(end.x, end.y)).toBe(false);
    // конечная клетка примыкает к запрошенной
    expect(Math.abs(end.x - 2) + Math.abs(end.y - 1)).toBeLessThanOrEqual(2);
  });

  it('nearestFreeTile находит свободную клетку рядом', () => {
    const g = gridFrom([
      '###',
      '###',
      '###',
    ]);
    const t = nearestFreeTile(g, 1, 1)!;
    expect(t).not.toBeNull();
    expect(g.isBlocked(t.x, t.y)).toBe(false);
  });

  it('сглаживание сохраняет проходимость и укорачивает путь', () => {
    const g = gridFrom([
      '..........',
      '..........',
      '..........',
    ]);
    const raw = findPath(g, 0, 0, 9, 2)!;
    const smooth = smoothPath(g, 0, 0, raw);
    expect(smooth.length).toBeLessThanOrEqual(raw.length);
    expect(smooth[smooth.length - 1]).toEqual(raw[raw.length - 1]);
    expect(tileLineClear(g, 0, 0, smooth[0].x, smooth[0].y)).toBe(true);
  });
});
