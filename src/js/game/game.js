/* ============================================================
   game.js — stage controller, timer, scoring, win/lose flow
   ============================================================ */
(function () {
  "use strict";
  const BI = window.BI;
  const G = BI.GRID;

  class Game {
    constructor(canvas, opts) {
      this.canvas = canvas;
      this.ctx = canvas.getContext("2d");
      this.ctx.imageSmoothingEnabled = true;

      this.players = opts.players || 1;
      this.diff = BI.DIFF[opts.diff] || BI.DIFF.normal;
      this.stage = opts.stage || 1;
      this.totalStage = opts.totalStage || this.diff.stages;
      this.sceneId = opts.sceneId !== undefined ? opts.sceneId : BI.randInt(0, 4);
      this.humanIds = opts.humanIds || [1];
      this.humanLives = opts.lives || this.humanIds.map(() => 1);
      this.specialPair = opts.specialPair || [7, 8];
      this.onEvent = opts.onEvent || function () {};

      this.map = new BI.GameMap(this.sceneId);
      this.pixelScale = 1;
      this.resize();
      this.fx = new BI.FX(this);
      this.particles = new BI.Particles();
      this.sound = BI.sound;
      this.heroes = [];
      this.tick = 0;
      this.time = BI.TOTAL_TIME;
      this.timeAcc = 0;
      this.hudDirty = true;
      this.paused = false;
      this.state = "countdown";
      this.countTicks = 116;
      this.endTick = 0;
      this.endKind = null;
      this.result = null;

      this.spawnHeroes();
    }

    resize() {
      const rect = this.canvas.getBoundingClientRect();
      const cssW = rect.width || BI.VIEW_W;
      const dpr = Math.min(3, window.devicePixelRatio || 1);
      const scale = BI.clamp((cssW / BI.VIEW_W) * dpr, 2, 4);
      this.pixelScale = scale;
      const w = Math.round(BI.VIEW_W * scale);
      const h = Math.round(BI.VIEW_H * scale);
      if (this.canvas.width !== w || this.canvas.height !== h) {
        this.canvas.width = w;
        this.canvas.height = h;
        this.ctx.imageSmoothingEnabled = true;
        this.ctx.imageSmoothingQuality = "high";
        this.map.bakeBackground(scale);
      }
    }

    spawnHeroes() {
      const used = new Set(this.humanIds);
      const aiIds = [];
      for (let id = 1; id <= 4; id++) if (!used.has(id)) aiIds.push(id);

      this.humanIds.forEach((heroId, i) => {
        const hero = new BI.Hero(this, {
          slot: i + 1,
          heroId,
          human: true,
          layout: this.players === 1 ? "solo" : i === 0 ? "p1" : "p2",
          spawn: BI.SPAWNS[heroId - 1],
          lives: this.humanLives[i],
        });
        // AS2 only attaches a hero when heroObj.life > 0.
        if (hero.lives <= 0) hero.alive = false;
        // AS2 gives human players a shield on easy.
        if (this.diff.key === "easy") hero.shield = true;
        this.heroes.push(hero);
      });

      aiIds.slice(0, Math.max(0, 4 - this.players)).forEach((heroId, i) => {
        this.heroes.push(new BI.Hero(this, {
          slot: this.players + i + 1,
          heroId,
          human: false,
          layout: null,
          spawn: BI.SPAWNS[heroId - 1],
          lives: 1,
        }));
      });
    }

    /* ---------------- simulation ---------------- */
    step() {
      if (this.paused || this.state === "over") return;
      this.tick++;
      BI.stepShake();

      if (this.state === "countdown") {
        this.countTicks--;
        if (this.countTicks <= 0) {
          this.state = "playing";
          this.onEvent("go");
        }
        return;
      }

      if (this.state === "ending") {
        this.fx.step();
        this.map.updateFire();
        this.map.updateIce();
        this.particles.update();
        if (--this.endTick <= 0) this.finishEnd();
        return;
      }

      this.timeAcc++;
      if (this.timeAcc >= BI.FPS) {
        this.timeAcc -= BI.FPS;
        this.time--;
        this.hudDirty = true;
        if (this.time <= 0) {
          this.time = 0;
          this.beginEnd("timeup");
          return;
        }
      }

      this.map.updateFire();
      this.map.updateIce();
      this.map.updateTools();
      for (const h of this.heroes) h.step();
      this.fx.step();
      this.particles.update();
      this.checkEnd();
    }

    checkEnd() {
      if (this.state !== "playing") return;
      const humansAlive = this.heroes.some((h) => h.human && h.alive);
      const aiAlive = this.heroes.some((h) => !h.human && h.alive);
      if (!humansAlive) this.beginEnd("lose");
      else if (!aiAlive) this.beginEnd("win");
    }

    beginEnd(kind) {
      this.state = "ending";
      this.endKind = kind;
      this.endTick = 40;
      this.onEvent(kind === "win" ? "stagewin" : kind === "timeup" ? "timeup" : "stagelose");
    }

    finishEnd() {
      let kind = this.endKind;
      const humans = this.heroes.filter((h) => h.human);

      if (kind === "timeup") {
        const bestHuman = Math.max(...humans.filter((h) => h.alive).map((h) => h.score));
        const bestAi = Math.max(0, ...this.heroes.filter((h) => !h.human && h.alive).map((h) => h.score));
        kind = bestHuman > bestAi ? "win" : "lose";
      }

      if (kind === "win") {
        for (const h of humans) {
          if (!h.alive) continue;
          h.addScore("bonus", 1000);
          if (this.endKind === "win") h.addScore("time", Math.floor(this.time) * 10);
        }
      }

      const scores = this.heroes.map((h) => ({
        slot: h.slot,
        heroId: h.heroId,
        name: h.info.name,
        color: h.info.color,
        key: h.info.key,
        human: h.human,
        lives: Math.max(0, h.lives),
        alive: h.alive,
        score: h.score,
        brickScore: h.brickScore,
        killScore: h.killScore,
        toolScore: h.toolScore,
        timeScore: h.timeScore,
        bonus: h.bonus,
      }));

      this.result = {
        kind,
        stage: this.stage,
        totalStage: this.totalStage,
        scene: this.map.scene.name,
        timeLeft: this.time,
        stageClear: kind === "win",
        final: kind === "win" && this.stage >= this.totalStage,
        retry: kind === "lose" && humans.some((h) => h.lives > 0),
        scores,
      };
      this.state = "over";
      this.onEvent("over", this.result);
    }

    /* ---------------- shared callbacks ---------------- */
    breakBrickAt(x, y, owner) {
      const c = this.map.cell(x, y);
      if (!c || !c.brick) return false;
      this.map.breakBrick(x, y);
      if (owner) owner.addScore("brick", this.diff.brickScore);
      this.fx.anim("brickExp", x, y, 11, { scale: 1.06 });
      const tool = BI.rollTool(this.specialPair);
      if (tool) {
        this.map.placeTool(x, y, {
          tool: tool.id,
          name: tool.name,
          age: 0,
          bob: BI.randInt(0, 8),
        });
      }
      this.hudDirty = true;
      return true;
    }

    // AS2 stores ice walls in the brick slot, so a blast destroys them.
    breakIceAt(x, y, owner) {
      if (!this.map.meltIce(x, y)) return false;
      if (owner) owner.addScore("brick", this.diff.brickScore);
      this.fx.anim("brickExp", x, y, 11, { scale: 1.06 });
      const tool = BI.rollTool(this.specialPair);
      if (tool) {
        this.map.placeTool(x, y, {
          tool: tool.id,
          name: tool.name,
          age: 0,
          bob: BI.randInt(0, 8),
        });
      }
      this.hudDirty = true;
      return true;
    }

    onToolPickup(hero, def) {
      this.hudDirty = true;
      this.onEvent("tool", { hero, def });
    }

    onHeroDown() {
      this.hudDirty = true;
    }

    /* ---------------- HUD data ---------------- */
    hud() {
      return {
        time: this.time,
        stage: this.stage,
        totalStage: this.totalStage,
        scene: this.map.scene.name,
        tick: this.tick,
        countdown: this.state === "countdown" ? Math.ceil(this.countTicks / BI.FPS) : 0,
        slots: this.heroes.map((h) => ({
          slot: h.slot,
          name: h.info.name,
          color: h.info.color,
          dark: h.info.dark,
          key: h.info.key,
          human: h.human,
          alive: h.alive,
          lives: Math.max(0, h.lives),
          score: h.score,
          bombs: h.bombsOut,
          capacity: h.totalBomb,
          power: h.bombLevel,
          glove: h.pushBombAble,
          shield: h.shield,
          special: h.special ? h.special.def.name : null,
          specialCount: h.special ? h.special.count : 0,
          speed: h.vLevel,
        })),
      };
    }

    /* ---------------- render ---------------- */
    render() {
      const ctx = this.ctx;
      const s = this.pixelScale || 1;
      ctx.setTransform(s, 0, 0, s, 0, 0);
      ctx.clearRect(0, 0, BI.VIEW_W, BI.VIEW_H);
      ctx.save();
      if (BI.shake.t > 0 && !this.paused) {
        const p = BI.shake.power * (BI.shake.t / 6);
        ctx.translate(BI.rand(-p, p), BI.rand(-p, p));
      }

      this.map.drawGround(ctx);

      const actors = [];
      for (const h of this.heroes) {
        if (!h.alive) continue;
        actors.push({ row: h.cy, sort: h.px, draw: (c) => h.draw(c) });
      }
      this.fx.collect(actors);

      const rows = [];
      for (let y = 0; y < BI.MAP_H; y++) rows.push([]);
      for (const a of actors) {
        const r = BI.clamp(Number.isFinite(a.row) ? Math.round(a.row) : 0, 0, BI.MAP_H - 1);
        rows[r].push(a);
      }
      for (let y = 0; y < BI.MAP_H; y++) {
        this.map.drawBlockRow(ctx, y);
        rows[y].sort((a, b) => b.sort - a.sort);
        for (const a of rows[y]) a.draw(ctx);
      }

      this.fx.drawTop(ctx);
      this.particles.draw(ctx);
      ctx.restore();
    }
  }

  BI.Game = Game;
})();
