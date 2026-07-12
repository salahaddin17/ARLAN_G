import { TICK_DT } from './data/balance';
import { AIController } from './ai/ai';
import { Camera } from './render/camera';
import { Renderer } from './render/render';
import { step } from './sim/sim';
import { World } from './sim/world';
import { Input } from './input/input';
import { Hud } from './ui/hud';
import { Screens } from './ui/screens';

// ============================================================================
// Точка входа: цикл кадра (rAF) + фиксированный тик симуляции 20 Гц.
// ============================================================================

const canvas = document.getElementById('game') as HTMLCanvasElement;
const ctx = canvas.getContext('2d')!;

interface Session {
  world: World;
  renderer: Renderer;
  ai: AIController;
  ended: boolean;
}

let session: Session | null = null;
const cam = new Camera();
const hud = new Hud();
const input = new Input(canvas, cam, () => session?.world ?? null, hud);
const screens = new Screens(startGame, () => { session = null; });

function resize(): void {
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.floor(window.innerWidth * dpr);
  canvas.height = Math.floor(window.innerHeight * dpr);
  canvas.style.width = `${window.innerWidth}px`;
  canvas.style.height = `${window.innerHeight}px`;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  cam.screenW = window.innerWidth;
  cam.screenH = window.innerHeight;
  cam.clamp();
}
window.addEventListener('resize', resize);
resize();

function startGame(): void {
  const seed = (Math.random() * 0xffffffff) >>> 0;
  const world = new World(screens.faction, screens.difficulty, seed);
  session = {
    world,
    renderer: new Renderer(world),
    ai: new AIController(world),
    ended: false,
  };
  input.reset();
  const hq = world.buildingsOfTeam(0)[0];
  cam.zoom = 1.1;
  cam.centerOn(hq.x, hq.y);
}

// отладочное ускорение симуляции: ?speed=4 (1..16)
const SPEED = Math.min(16, Math.max(1, Number(new URLSearchParams(location.search).get('speed')) || 1));

let last = performance.now();
let acc = 0;

function frame(now: number): void {
  const rawDt = Math.min(0.25, (now - last) / 1000);
  last = now;

  if (session) {
    const { world, ai, renderer } = session;

    if (world.phase === 'playing') {
      acc += rawDt * SPEED;
      let steps = 0;
      const maxSteps = 6 * SPEED;
      while (acc >= TICK_DT && steps < maxSteps) {
        ai.update(world, TICK_DT);
        step(world, TICK_DT);
        acc -= TICK_DT;
        steps++;
      }
      if (steps === maxSteps) acc = 0; // вкладка была в фоне — не навёрстываем лавину тиков
    }

    input.update(rawDt);
    const alpha = world.phase === 'playing' ? Math.min(1, acc / TICK_DT) : 1;
    renderer.render(ctx, world, cam, input, alpha, rawDt);
    hud.render(ctx, world, cam, input, renderer);

    if (world.phase === 'ended' && !session.ended) {
      session.ended = true;
      screens.showEnd(world);
    }
  } else {
    ctx.fillStyle = '#101115';
    ctx.fillRect(0, 0, cam.screenW, cam.screenH);
  }

  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
