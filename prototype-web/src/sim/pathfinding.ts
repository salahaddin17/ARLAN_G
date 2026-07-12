import type { MapGrid } from './grid';

// ============================================================================
// A* по сетке 64×64, 8 направлений, БЕЗ срезания углов: диагональный шаг
// разрешён только если обе смежные ортогональные клетки свободны.
// Бинарная куча, октильная эвристика, сглаживание по линии видимости.
// ============================================================================

const SQRT2 = Math.SQRT2;

class MinHeap {
  private nodes: number[] = []; // индексы клеток
  private prio: Float32Array;
  constructor(size: number) {
    this.prio = new Float32Array(size);
  }
  get length(): number {
    return this.nodes.length;
  }
  push(node: number, priority: number): void {
    this.prio[node] = priority;
    this.nodes.push(node);
    let i = this.nodes.length - 1;
    while (i > 0) {
      const p = (i - 1) >> 1;
      if (this.prio[this.nodes[p]] <= this.prio[this.nodes[i]]) break;
      [this.nodes[p], this.nodes[i]] = [this.nodes[i], this.nodes[p]];
      i = p;
    }
  }
  pop(): number {
    const top = this.nodes[0];
    const last = this.nodes.pop()!;
    if (this.nodes.length > 0) {
      this.nodes[0] = last;
      let i = 0;
      for (;;) {
        const l = i * 2 + 1, r = l + 1;
        let m = i;
        if (l < this.nodes.length && this.prio[this.nodes[l]] < this.prio[this.nodes[m]]) m = l;
        if (r < this.nodes.length && this.prio[this.nodes[r]] < this.prio[this.nodes[m]]) m = r;
        if (m === i) break;
        [this.nodes[m], this.nodes[i]] = [this.nodes[i], this.nodes[m]];
        i = m;
      }
    }
    return top;
  }
}

function octile(dx: number, dy: number): number {
  const ax = Math.abs(dx), ay = Math.abs(dy);
  return ax > ay ? ax + (SQRT2 - 1) * ay : ay + (SQRT2 - 1) * ax;
}

/** Ближайшая свободная клетка к (tx,ty) — кольцевой обход по радиусу. */
export function nearestFreeTile(
  grid: MapGrid, tx: number, ty: number, maxR = 12,
): { x: number; y: number } | null {
  if (!grid.isBlocked(tx, ty)) return { x: tx, y: ty };
  for (let r = 1; r <= maxR; r++) {
    let best: { x: number; y: number } | null = null;
    let bestD = Infinity;
    for (let dy = -r; dy <= r; dy++) {
      for (let dx = -r; dx <= r; dx++) {
        if (Math.max(Math.abs(dx), Math.abs(dy)) !== r) continue;
        const x = tx + dx, y = ty + dy;
        if (grid.inBounds(x, y) && !grid.isBlocked(x, y)) {
          const d = dx * dx + dy * dy;
          if (d < bestD) { bestD = d; best = { x, y }; }
        }
      }
    }
    if (best) return best;
  }
  return null;
}

/**
 * Поиск пути в клетках. Возвращает массив клеток от старта (не включая его)
 * до цели, либо null, если цель недостижима.
 */
export function findPath(
  grid: MapGrid, sx: number, sy: number, txIn: number, tyIn: number,
): { x: number; y: number }[] | null {
  const N = grid.size;
  if (!grid.inBounds(sx, sy)) return null;

  const t = nearestFreeTile(grid, txIn, tyIn);
  if (!t) return null;
  const tx = t.x, ty = t.y;

  // старт внутри препятствия (задавили зданием) — выбираемся в ближайшую свободную
  let startX = sx, startY = sy;
  if (grid.isBlocked(sx, sy)) {
    const s = nearestFreeTile(grid, sx, sy, 4);
    if (!s) return null;
    startX = s.x; startY = s.y;
  }
  if (startX === tx && startY === ty) return [{ x: tx, y: ty }];

  const total = N * N;
  const gScore = new Float32Array(total).fill(Infinity);
  const from = new Int32Array(total).fill(-1);
  const closed = new Uint8Array(total);
  const heap = new MinHeap(total);

  const startIdx = startY * N + startX;
  const targetIdx = ty * N + tx;
  gScore[startIdx] = 0;
  heap.push(startIdx, octile(tx - startX, ty - startY));

  const DIRS = [
    [1, 0, 1], [-1, 0, 1], [0, 1, 1], [0, -1, 1],
    [1, 1, SQRT2], [1, -1, SQRT2], [-1, 1, SQRT2], [-1, -1, SQRT2],
  ] as const;

  while (heap.length > 0) {
    const cur = heap.pop();
    if (cur === targetIdx) {
      const path: { x: number; y: number }[] = [];
      let i = cur;
      while (i !== startIdx && i !== -1) {
        path.push({ x: i % N, y: (i / N) | 0 });
        i = from[i];
      }
      path.reverse();
      return path;
    }
    if (closed[cur]) continue;
    closed[cur] = 1;
    const cx = cur % N, cy = (cur / N) | 0;

    for (const [dx, dy, cost] of DIRS) {
      const nx = cx + dx, ny = cy + dy;
      if (!grid.inBounds(nx, ny) || grid.isBlocked(nx, ny)) continue;
      // запрет срезания углов: по диагонали — только если обе ортогонали свободны
      if (dx !== 0 && dy !== 0 && (grid.isBlocked(cx + dx, cy) || grid.isBlocked(cx, cy + dy))) {
        continue;
      }
      const ni = ny * N + nx;
      if (closed[ni]) continue;
      const g = gScore[cur] + cost;
      if (g < gScore[ni]) {
        gScore[ni] = g;
        from[ni] = cur;
        heap.push(ni, g + octile(tx - nx, ty - ny));
      }
    }
  }
  return null;
}

/**
 * Линия видимости по клеткам (супер-покрытие): true, если отрезок между
 * центрами клеток не задевает блокированные клетки.
 */
export function tileLineClear(grid: MapGrid, ax: number, ay: number, bx: number, by: number): boolean {
  let x = ax, y = ay;
  const dx = Math.abs(bx - ax), dy = Math.abs(by - ay);
  const sx = ax < bx ? 1 : -1, sy = ay < by ? 1 : -1;
  let err = dx - dy;
  for (;;) {
    if (grid.isBlocked(x, y)) return false;
    if (x === bx && y === by) return true;
    const e2 = 2 * err;
    if (e2 > -dy && e2 < dx) {
      // диагональный шаг — обе смежные клетки должны быть свободны
      if (grid.isBlocked(x + sx, y) || grid.isBlocked(x, y + sy)) return false;
      err -= dy; err += dx; x += sx; y += sy;
    } else if (e2 > -dy) {
      err -= dy; x += sx;
    } else {
      err += dx; y += sy;
    }
  }
}

/** Сгладить путь: выкинуть промежуточные точки, видимые по прямой. */
export function smoothPath(
  grid: MapGrid, sx: number, sy: number, path: { x: number; y: number }[],
): { x: number; y: number }[] {
  if (path.length <= 2) return path;
  const out: { x: number; y: number }[] = [];
  let ax = sx, ay = sy;
  let i = 0;
  while (i < path.length) {
    let j = path.length - 1;
    while (j > i && !tileLineClear(grid, ax, ay, path[j].x, path[j].y)) j--;
    out.push(path[j]);
    ax = path[j].x; ay = path[j].y;
    i = j + 1;
  }
  return out;
}
