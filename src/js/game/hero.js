/* ============================================================
   hero.js — tile-step movement, tools, combat, original AI
   vLevel 6 ticks/tile · speed shoes down to 3
   AI: 20% wander · 5% bomb-and-run · escapeRate on danger
   ============================================================ */
(function () {
  "use strict";
  const BI = window.BI;
  const G = BI.GRID;

  class Hero {
    constructor(game, opts) {
      this.game = game;
      this.slot = opts.slot;
      this.heroId = opts.heroId;
      this.info = BI.HEROES[this.heroId - 1];
      this.human = !!opts.human;
      this.layout = opts.layout || null;
      this.spawn = opts.spawn;
      this.lives = opts.lives !== undefined ? opts.lives : 1;

      this.x = this.spawn.x;
      this.y = this.spawn.y;
      this.px = this.x * G;
      this.py = this.y * G;
      this.dir = 3;
      this.move = null;
      this.moving = false;

      this.vLevel = 6;
      this.slowVLevel = 20;
      this.slowTime = -1;
      this.slowTick = 0;
      this.stopTime = 0;

      this.bombLevel = 1;
      this.totalBomb = 1;
      this.bombsOut = 0;
      this.pushBombAble = false;
      this.shield = false;
      this.varShield = 60 + BI.randInt(0, 3);
      this.special = null;

      this.score = 0;
      this.brickScore = 0;
      this.killScore = 0;
      this.toolScore = 0;
      this.timeScore = 0;
      this.bonus = 0;

      this.alive = true;
      this.deadTimer = 0;
      this.age = BI.randInt(0, 30);
      this.prevFire = false;
      this.lastFireTick = -99;
    }

    get cx() { return Math.round(this.px / G); }
    get cy() { return Math.round(this.py / G); }

    cellX() { return this.move ? this.move.tx : this.x; }
    cellY() { return this.move ? this.move.ty : this.y; }

    addScore(kind, v) {
      this.score += v;
      if (kind === "brick") this.brickScore += v;
      else if (kind === "kill") this.killScore += v;
      else if (kind === "tool") this.toolScore += v;
      else if (kind === "time") this.timeScore += v;
      else if (kind === "bonus") this.bonus += v;
      this.game.hudDirty = true;
    }

    invulnerable() { return this.varShield > 0; }

    step() {
      if (!this.alive) return;
      this.age++;
      if (this.varShield > 0) this.varShield--;
      if (this.slowTime > 0) this.slowTime--;

      const c = this.game.map.cell(this.cx, this.cy);
      if (c && c.tool === "__ice") this.stopTime = 300;

      if (this.move) {
        this.advanceMove();
        return;
      }

      if (this.game.map.fireAt(this.cx, this.cy)) {
        this.hurt(this.game.map.fireOwnerAt(this.cx, this.cy));
        if (!this.alive) return;
      }

      if (this.stopTime > 0) {
        this.stopTime--;
        return;
      }

      if (this.human) this.humanThink();
      else this.aiThink();
    }

    /* ---------------- movement ---------------- */
    advanceMove() {
      const m = this.move;
      m.t++;
      const k = Math.min(1, m.t / m.ticks);
      this.px = (m.sx + m.dx * k) * G;
      this.py = (m.sy + m.dy * k) * G;
      if (m.t >= m.ticks) {
        this.x = m.tx;
        this.y = m.ty;
        this.px = this.x * G;
        this.py = this.y * G;
        this.move = null;
        this.moving = false;
        this.pickupHere();
      }
    }

    tryMove(dir) {
      const v = BI.dirVec[dir];
      const tx = this.x + v[0];
      const ty = this.y + v[1];
      const map = this.game.map;
      const c = map.cell(tx, ty);
      this.dir = dir;
      if (!c || c.brick || c.stone || c.tool === "__ice") return false;
      if (c.bomb) {
        if (!this.pushBombAble || c.bomb.owner !== this) return false;
        if (!c.bomb.push(v[0], v[1])) return false;
      }
      const ticks = this.slowTime > 0 ? this.slowVLevel : this.vLevel;
      this.move = {
        sx: this.x, sy: this.y, tx, ty, dx: v[0], dy: v[1],
        t: 0, ticks,
      };
      this.moving = true;
      return true;
    }

    pickupHere() {
      const c = this.game.map.cell(this.x, this.y);
      if (!c || !c.tool || c.tool === "__ice") return;
      if (!this.human && this.game.diff.key === "easy") return;
      const tool = c.tool;
      const def = BI.toolById(tool.tool);
      if (!this.human && this.game.diff.key !== "hard" &&
          (def.kind === "speed" || BI.isSpecial(def))) return;
      if (this.game.map.takeTool(this.x, this.y) == null) return;
      this.applyTool(def, tool);
    }

    applyTool(def, tool) {
      const game = this.game;
      if (def.kind === "life") this.lives++;
      else if (def.kind === "glove") this.pushBombAble = true;
      else if (def.kind === "bomb") this.totalBomb++;
      else if (def.kind === "shield") this.shield = true;
      else if (def.kind === "power") this.bombLevel = Math.min(10, this.bombLevel + 1);
      else if (def.kind === "speed") this.vLevel = Math.max(3, this.vLevel - 1);
      else {
        this.special = { def, count: def.kind === "ice" ? 1 : 2 };
      }
      this.addScore("tool", 100);
      game.sound.play("pickup", 0.5);
      game.particles.burst(this.px + G / 2, this.py + G / 2, {
        count: 8, colors: [def.kind === "shield" ? "#8fe6ff" : "#ffd166", "#ffffff"],
        speedMin: 0.4, speedMax: 1.8, gravity: -0.02, lifeMax: 18,
      });
      game.onToolPickup(this, def, tool);
    }

    /* ---------------- input ---------------- */
    humanThink() {
      const st = BI.input.player(this.layout);
      const order = [0, 1, 2, 3];
      for (const d of order) {
        if (st[["left", "up", "right", "down"][d]]) {
          this.tryMove(d);
          break;
        }
      }
      const firing = st.fire || this.prevFire;
      if (firing && !this.moving) {
        this.prevFire = false;
        this.tryFire();
      }
      if (this.moving && st.fire) this.prevFire = true;
    }

    tryFire() {
      if (this.game.tick - this.lastFireTick < 5) return;
      this.lastFireTick = this.game.tick;
      const c = this.game.map.cell(this.x, this.y);
      if (!c || c.bomb) return;
      if (this.special && this.special.count > 0) {
        if (this.fireSpecial()) {
          this.special.count--;
          if (this.special.count <= 0) this.special = null;
        }
        return;
      }
      this.putBomb();
    }

    putBomb() {
      if (this.bombsOut >= this.totalBomb) return false;
      const c = this.game.map.cell(this.x, this.y);
      if (!c || c.bomb) return false;
      this.game.fx.plant(this);
      this.bombsOut++;
      this.game.hudDirty = true;
      return true;
    }

    fireSpecial() {
      const def = this.special.def;
      const fx = this.game.fx;
      switch (def.kind) {
        case "rocket": fx.rocket(this); return true;
        case "mine": fx.mine(this); return true;
        case "grenade": return fx.grenade(this);
        case "gas": fx.gas(this); return true;
        case "ice": return fx.iceArrow(this);
        case "fire": return fx.fireArrow(this);
        default: return false;
      }
    }

    /* ---------------- AI ---------------- */
    aiThink() {
      if (this.stopTime > 0) { this.stopTime--; return; }
      const escapeRate = this.game.diff.escape;
      if (this.isSaveCell(this.x, this.y)) {
        if (Math.random() * 100 < 20) this.aiSeek();
        else if (Math.random() * 100 < 5) this.aiBombAndFlee();
      } else if (Math.random() * 100 < escapeRate) {
        this.aiEscape();
      }
    }

    aiSeek() {
      let best = -1;
      let bestDir = -1;
      const start = BI.randInt(0, 3);
      for (let i = 0; i < 4; i++) {
        const d = (start + i) % 4;
        const v = BI.dirVec[d];
        let s = this.getScore(this.x + v[0], this.y + v[1]);
        if (s > 0) {
          const c = this.game.map.cell(this.x + v[0], this.y + v[1]);
          if (c && c.tool) s += 5;
          s += BI.randInt(0, 2);
          if (s >= best) { best = s; bestDir = d; }
        }
      }
      if (bestDir >= 0) this.tryMove(bestDir);
    }

    aiBombAndFlee() {
      let best = 0;
      let bestDir = -1;
      const start = BI.randInt(0, 3);
      for (let i = 0; i < 4; i++) {
        const d = (start + i) % 4;
        const v = BI.dirVec[d];
        const s = this.getScore(this.x + v[0], this.y + v[1]);
        if (s > 0 && s >= best) { best = s; bestDir = d; }
      }
      if (bestDir >= 0 && best >= 2) {
        this.tryFire();
        this.tryMove(bestDir);
      }
    }

    aiEscape() {
      let best = 0;
      let bestDir = -1;
      const start = BI.randInt(0, 3);
      for (let i = 0; i < 4; i++) {
        const d = (start + i) % 4;
        const v = BI.dirVec[d];
        const s = this.getScore(this.x + v[0], this.y + v[1]);
        if (s >= best) { best = s; bestDir = d; }
      }
      if (bestDir < 0) bestDir = BI.randInt(0, 3);
      this.tryMove(bestDir);
    }

    isSaveCell(x, y) {
      const map = this.game.map;
      const c = map.cell(x, y);
      if (!c || c.brick || c.stone || c.bomb || c.tool === "__ice") return false;
      if (map.fireAt(x, y)) return false;
      for (let d = 0; d < 4; d++) {
        const v = BI.dirVec[d];
        let cx = x;
        let cy = y;
        let dist = 0;
        let cell = c;
        while (cell && !cell.brick && !cell.stone) {
          if (map.fireAt(cx, cy)) return false;
          if (cell.bomb && cell.bomb.power >= dist) return false;
          cx += v[0];
          cy += v[1];
          dist++;
          cell = map.cell(cx, cy);
        }
      }
      return true;
    }

    getScore(x, y) {
      if (!this.isSaveCell(x, y)) return -1;
      let n = 0;
      for (let d = 0; d < 4; d++) {
        const v = BI.dirVec[d];
        if (this.isSaveCell(x + v[0], y + v[1])) n++;
      }
      return n;
    }

    /* ---------------- damage ---------------- */
    hurt(owner) {
      if (!this.alive || this.varShield > 0) return false;
      if (this.shield) {
        this.shield = false;
        this.varShield = 60;
        this.game.sound.play("sfx319", 0.5);
        this.game.particles.burst(this.px + G / 2, this.py + G / 2, {
          count: 14, colors: ["#9fe8ff", "#ffffff"], speedMin: 0.8, speedMax: 2.4,
        });
        return true;
      }
      if (owner && owner !== this) owner.addScore("kill", 1000);
      this.lives--;
      this.game.fx.anim("dieSmoke", this.x, this.y, 36, { scale: 1.1, alpha: 0.9 });
      this.game.sound.play("stageLose", 0.35);
      this.game.particles.burst(this.px + G / 2, this.py + G / 2, {
        count: 18, colors: [this.info.color, "#ffffff", "#ffd166"],
        speedMin: 1, speedMax: 3.2, lifeMax: 28,
      });
      // AI with a spare life survives in place; humans are out for the stage.
      if (!this.human && this.lives > 0) {
        this.varShield = 60;
        return true;
      }
      this.alive = false;
      this.game.fx.anim("heroExp", this.x, this.y, 36, { scale: 1.2 });
      this.game.onHeroDown(this);
      return true;
    }

    /* ---------------- draw ---------------- */
    draw(ctx) {
      if (!this.alive) return;
      const img = BI.assets.get(this.info.key + "_" + (this.dir + 1));
      const bob = this.moving ? Math.sin((this.move ? this.move.t / this.move.ticks : 0) * Math.PI) * 2.4 : 0;
      const cx = this.px + G / 2;
      const by = this.py + G + 3;
      const w = img && img.complete ? Math.max(28, BI.sw(img) * 1.02) : 34;
      const h = img && img.complete ? Math.max(40, BI.sh(img) * 1.02) : 54;

      ctx.save();
      ctx.fillStyle = "rgba(0,0,0,0.22)";
      ctx.beginPath();
      ctx.ellipse(cx, by - 1, w * 0.32, 4.4, 0, 0, BI.tau);
      ctx.fill();

      if (this.varShield > 0 && this.varShield % 2 === 1) ctx.globalAlpha = 0.42;
      if (img && img.complete) {
        ctx.drawImage(img, cx - w / 2, by - h - bob, w, h);
      } else {
        ctx.fillStyle = this.info.color;
        BI.roundRect(ctx, cx - 14, by - h, 28, h, 10);
        ctx.fill();
      }
      ctx.globalAlpha = 1;

      if (this.shield) {
        ctx.strokeStyle = BI.hexToRgba(this.info.color, 0.85);
        ctx.lineWidth = 2.4;
        ctx.beginPath();
        ctx.ellipse(cx, by - h * 0.52, w * 0.56, h * 0.5, 0, 0, BI.tau);
        ctx.stroke();
        ctx.fillStyle = "rgba(255,255,255,0.12)";
        ctx.fill();
      }

      if (this.slowTime > 0) {
        ctx.fillStyle = "rgba(120,220,255,0.28)";
        ctx.beginPath();
        ctx.ellipse(cx, by - h * 0.5, w * 0.5, h * 0.44, 0, 0, BI.tau);
        ctx.fill();
      }

      if (this.human) {
        const label = this.layout === "p2" ? "P2" : this.game.players === 2 ? "P1" : "";
        if (label) {
          const py = by - h - 12;
          ctx.fillStyle = this.info.dark;
          BI.roundRect(ctx, cx - 12, py, 24, 14, 7);
          ctx.fill();
          ctx.fillStyle = "#fff";
          ctx.font = "bold 10px system-ui, sans-serif";
          ctx.textAlign = "center";
          ctx.textBaseline = "middle";
          ctx.fillText(label, cx, py + 7.5);
        }
      }

      if (this.special) {
        const icon = BI.assets.get("tool_" + this.special.def.id);
        if (icon && icon.complete) {
          ctx.drawImage(icon, cx - 9, by - h - 26, 18, 18);
          ctx.fillStyle = "#fff";
          ctx.font = "bold 10px system-ui, sans-serif";
          ctx.textAlign = "center";
          ctx.fillText("x" + this.special.count, cx + 16, by - h - 14);
        }
      }

      if (this.stopTime > 0) {
        const ice = BI.assets.get("freezeEffect_" + (1 + (this.age % 40)));
        if (ice && ice.complete) {
          ctx.globalAlpha = 0.55;
          ctx.drawImage(ice, cx - w * 0.6, by - h * 1.02, w * 1.2, h * 1.05);
          ctx.globalAlpha = 1;
        }
      }
      ctx.restore();
    }
  }

  BI.Hero = Hero;
})();
