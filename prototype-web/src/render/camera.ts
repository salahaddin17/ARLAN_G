import { MAP_SIZE } from '../data/balance';

export class Camera {
  x = MAP_SIZE / 2;
  y = MAP_SIZE / 2;
  zoom = 1.1;
  screenW = 1280;
  screenH = 720;

  worldToScreen(wx: number, wy: number): { x: number; y: number } {
    return {
      x: (wx - this.x) * this.zoom + this.screenW / 2,
      y: (wy - this.y) * this.zoom + this.screenH / 2,
    };
  }

  screenToWorld(sx: number, sy: number): { x: number; y: number } {
    return {
      x: (sx - this.screenW / 2) / this.zoom + this.x,
      y: (sy - this.screenH / 2) / this.zoom + this.y,
    };
  }

  pan(dx: number, dy: number): void {
    this.x += dx / this.zoom;
    this.y += dy / this.zoom;
    this.clamp();
  }

  zoomAt(sx: number, sy: number, factor: number): void {
    const before = this.screenToWorld(sx, sy);
    this.zoom = Math.min(2.4, Math.max(0.45, this.zoom * factor));
    const after = this.screenToWorld(sx, sy);
    this.x += before.x - after.x;
    this.y += before.y - after.y;
    this.clamp();
  }

  centerOn(wx: number, wy: number): void {
    this.x = wx;
    this.y = wy;
    this.clamp();
  }

  clamp(): void {
    const halfW = this.screenW / 2 / this.zoom;
    const halfH = this.screenH / 2 / this.zoom;
    const pad = 60;
    this.x = Math.min(MAP_SIZE + pad - halfW, Math.max(halfW - pad, this.x));
    this.y = Math.min(MAP_SIZE + pad - halfH, Math.max(halfH - pad, this.y));
    if (MAP_SIZE + 2 * pad < 2 * halfW) this.x = MAP_SIZE / 2;
    if (MAP_SIZE + 2 * pad < 2 * halfH) this.y = MAP_SIZE / 2;
  }
}
