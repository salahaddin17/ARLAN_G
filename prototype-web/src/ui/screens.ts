import { DIFFICULTIES, FACTIONS } from '../data/balance';
import type { DifficultyId, FactionId } from '../data/balance';
import type { World } from '../sim/world';

// ============================================================================
// DOM-экраны: главное меню (фракция + сложность) и финальный экран со штампом.
// ============================================================================

export class Screens {
  private menu = document.getElementById('menu-overlay')!;
  private end = document.getElementById('end-overlay')!;
  private stamp = document.getElementById('end-stamp')!;
  private endStats = document.getElementById('end-stats')!;

  faction: FactionId = 'legion';
  difficulty: DifficultyId = 'normal';

  constructor(private onStart: () => void, onAgain: () => void) {
    const legion = document.getElementById('pick-legion')!;
    const front = document.getElementById('pick-front')!;
    legion.addEventListener('click', () => {
      this.faction = 'legion';
      legion.classList.add('sel-teal');
      front.classList.remove('sel-orange');
    });
    front.addEventListener('click', () => {
      this.faction = 'front';
      front.classList.add('sel-orange');
      legion.classList.remove('sel-teal');
    });

    const diffs: [string, DifficultyId][] = [
      ['diff-easy', 'easy'], ['diff-normal', 'normal'], ['diff-hard', 'hard'],
    ];
    for (const [elId, id] of diffs) {
      document.getElementById(elId)!.addEventListener('click', () => {
        this.difficulty = id;
        for (const [otherEl] of diffs) {
          document.getElementById(otherEl)!.classList.toggle('sel', otherEl === elId);
        }
      });
    }

    document.getElementById('start-btn')!.addEventListener('click', () => {
      this.hideMenu();
      this.onStart();
    });
    document.getElementById('again-btn')!.addEventListener('click', () => {
      this.end.classList.add('hidden');
      this.menu.classList.remove('hidden');
      onAgain();
    });
  }

  hideMenu(): void {
    this.menu.classList.add('hidden');
  }

  showEnd(world: World): void {
    const won = world.winner === 0;
    this.stamp.textContent = won ? 'ПОБЕДА' : 'РАЗГРОМ';
    this.stamp.className = `stamp ${won ? 'win' : 'lose'}`;

    const t = Math.floor(world.time);
    const time = `${String(Math.floor(t / 60)).padStart(2, '0')}:${String(t % 60).padStart(2, '0')}`;
    const s0 = world.stats[0], s1 = world.stats[1];
    const f0 = FACTIONS[world.factions[0]], f1 = FACTIONS[world.factions[1]];

    this.endStats.innerHTML = `
      <table class="stats-table">
        <tr>
          <th>Итоги операции · ${time} · ${DIFFICULTIES[world.difficulty].name}</th>
          <th style="color:${f0.color}">${esc(f0.name)}</th>
          <th style="color:${f1.color}">${esc(f1.name)} (ИИ)</th>
        </tr>
        <tr><td>Собрано припасов</td><td>${s0.suppliesGathered}</td><td>${s1.suppliesGathered}</td></tr>
        <tr><td>Юнитов выпущено</td><td>${s0.unitsBuilt}</td><td>${s1.unitsBuilt}</td></tr>
        <tr><td>Юнитов потеряно</td><td>${s0.unitsLost}</td><td>${s1.unitsLost}</td></tr>
        <tr><td>Зданий построено</td><td>${s0.buildingsBuilt}</td><td>${s1.buildingsBuilt}</td></tr>
        <tr><td>Зданий потеряно</td><td>${s0.buildingsLost}</td><td>${s1.buildingsLost}</td></tr>
        <tr><td>Целей уничтожено</td><td>${s0.enemiesKilled}</td><td>${s1.enemiesKilled}</td></tr>
      </table>`;
    this.end.classList.remove('hidden');
  }
}

function esc(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
