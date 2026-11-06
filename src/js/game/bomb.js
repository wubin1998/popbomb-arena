/* ============================================================
   bomb.js — bombs, explosions, special weapons, one-shot effects
   Timings taken from the AS2 source:
   fuse 60 ticks · blast grows one ring per tick · 10 hold · fade
   ============================================================ */
(function () {
  "use strict";
  const BI = window.BI;
  const G = BI.GRID;

  /* ---------------- bomb ---------------- */
  class Bomb {
    constructor(game, x, y, owner, power) {
      this.game = game;
      this.x = x;
      this.y = y;
      this.owner = owner;
      this.power = Math.max(1, power || 1);
      this.fuse = 60;
      this.t = 0;
      this.px = x * G;
      this.py = y * G;
      this.dead = false;
      this.slide = null;
      game.map.setBomb(x, y, this);
    }

    step() {
      if (this.dead) return;
      this.t++;
      if (this.slide) {
        const s = this.slide;
        s.t++;
        const k = Math.min(1, s.t / s.ticks);
        this.px = BI.lerp(s.fx, this.x * G, k);
        this.py = BI.lerp(s.fy, this.y * G, k);
        if (s.t >= s.ticks) this.slide = null;
        return;
      }
      if (--this.fuse <= 0 || this.game.map.fireAt(this.x, this.y)) this.explode();
    }

    explode() {
      if (this.dead) return;
      this.dead = true;
      this.game.map.clearBomb(this.x, this.y, this);
      if (this.owner && this.owner.bombsOut > 0) this.owner.bombsOut--;
      this.game.fx.boomAt(this.x, this.y, this.owner, this.power);
    }

    push(dx, dy) {
      const nx = this.x + dx;
      const ny = this.y + dy;
      const map = this.game.map;
      const c = map.cell(nx, ny);
      if (!c || c.brick || c.stone || c.bomb || c.tool === "__ice") return false;
      map.clearBomb(this.x, this.y, this);
      const fx = this.px;
      const fy = this.py;
      this.x = nx;
      this.y = ny;
      map.setBomb(nx, ny, this);
      this.px = nx * G;
      this.py = ny * G;
      this.slide = { fx, fy, t: 0, ticks: 6 };
      BI.sound.play("push", 0.45);
      return true;
    }

    draw(ctx) {
      const img = BI.assets.get("bomb_" + (1 + (this.t % 40)));
      const cx = this.px + G / 2;
      const by = this.py + G + 3;
      const s = 1.02;
      ctx.save();
      ctx.fillStyle = "rgba(0,0,0,0.22)";
      ctx.beginPath();
      ctx.ellipse(cx, by - 2, 10, 4, 0, 0, BI.tau);
      ctx.fill();
      if (img && img.complete) {
        const w = BI.sw(img) * s;
        const h = BI.sh(img) * s;
        const pulse = 1 + Math.sin(this.t * 0.6) * 0.03;
        ctx.drawImage(img, cx - (w * pulse) / 2, by - h * pulse, w * pulse, h * pulse);
      } else {
        ctx.fillStyle = "#2b2b3a";
        ctx.beginPath();
        ctx.arc(cx, by - 14, 11, 0, BI.tau);
        ctx.fill();
      }
      ctx.restore();
    }
  }

  /* ---------------- explosion ---------------- */
  class Explosion {
    constructor(game, x, y, owner, power) {
      this.game = game;
      this.x = x;
      this.y = y;
      this.owner = owner;
      this.power = power;
      this.ring = 0;
      this.t = 0;
      this.dead = false;
      this.heroId = owner ? owner.heroId : 1;
      this.cells = [];
      this.dirs = [true, true, true, true];
      this.addCell(x, y, -1, 0, true);
      for (let d = 0; d < 4; d++) {
        const c = game.map.cell(x + BI.dirVec[d][0], y + BI.dirVec[d][1]);
        if (!c || c.stone) this.dirs[d] = false;
      }
      BI.addShake(3, 5);
    }

    addCell(x, y, dir, ring, center) {
      const map = this.game.map;
      this.cells.push({ x, y, dir, ring, center });
      // AS2 keeps each cell hot through expansion + a 10-frame hold.
      map.ignite(x, y, this.power + 10 - Math.max(0, ring), this.owner);
      const c = map.cell(x, y);
      if (c && c.tool === "__ice") this.game.breakIceAt(x, y, this.owner);
      else this.game.breakBrickAt(x, y, this.owner);
    }

    step() {
      this.t++;
      if (this.ring < this.power) {
        this.ring++;
        for (let d = 0; d < 4; d++) {
          if (!this.dirs[d]) continue;
          const v = BI.dirVec[d];
          const x = this.x + v[0] * this.ring;
          const y = this.y + v[1] * this.ring;
          const c = this.game.map.cell(x, y);
          if (!c || c.stone) { this.dirs[d] = false; continue; }
          const blocked = c.brick || c.tool === "__ice";
          this.addCell(x, y, d, this.ring, false);
          if (blocked || this.ring >= this.power) this.dirs[d] = false;
        }
      }
      if (this.t >= this.power + 15) this.dead = true;
    }

    draw(ctx) {
      const fade = this.t > this.power + 10 ? Math.max(0, 1 - (this.t - this.power - 10) / 5) : 1;
      ctx.save();
      ctx.globalAlpha = fade;
      for (const cell of this.cells) {
        let frame;
        if (cell.center) frame = this.heroId;
        else if (cell.ring >= this.power) frame = this.heroId + 8;
        else frame = this.heroId + 4;
        const img = BI.assets.get("exp_" + frame);
        if (!img || !img.complete) continue;
        const rot = cell.dir < 0 ? 0 : ((cell.dir + 2) * Math.PI) / 2;
        const size = cell.center ? 36 : 34;
        ctx.save();
        ctx.translate(cell.x * G + G / 2, cell.y * G + G / 2);
        ctx.rotate(rot);
        ctx.drawImage(img, -size / 2, -size / 2, size, size);
        ctx.restore();
      }
      ctx.restore();
    }
  }

  /* ---------------- special weapons ---------------- */
  class Rocket {
    constructor(fx, hero) {
      this.fx = fx;
      this.hero = hero;
      this.x = hero.x;
      this.y = hero.y;
      this.px = this.x * G;
      this.py = this.y * G;
      const v = BI.dirVec[hero.dir];
      this.dx = v[0];
      this.dy = v[1];
      this.vx = 0;
      this.vy = 0;
      this.t = 0;
      this.dead = false;
    }
    row() { return Math.round(this.py / G); }
    sortX() { return this.px; }
    step() {
      this.t++;
      this.vx += this.dx;
      this.vy += this.dy;
      this.px += this.vx;
      this.py += this.vy;
      const g = this.fx.game;
      if (this.t % 2 === 0) this.fx.smoke(this.px + G / 2, this.py + G / 2);
      const cx = Math.round(this.px / G);
      const cy = Math.round(this.py / G);
      for (const h of g.heroes) {
        if (h === this.hero || !h.alive) continue;
        const dx = h.px - this.px;
        const dy = h.py - this.py;
        if (dx * dx + dy * dy < G * G / 4) {
          this.fx.boomAt(cx, cy, this.hero, 1);
          this.dead = true;
          return;
        }
      }
      const c = g.map.cell(cx, cy);
      if (!c || c.stone || c.brick || c.bomb) {
        this.fx.boomAt(cx, cy, this.hero, 1);
        this.dead = true;
        return;
      }
      this.x = cx;
      this.y = cy;
      if (this.t > 220) this.dead = true;
    }
    draw(ctx) {
      const img = BI.assets.get("rocket_" + (this.hero.dir + 1));
      const w = img && img.complete ? Math.max(18, BI.sw(img)) : 20;
      const h = img && img.complete ? Math.max(18, BI.sh(img)) : 20;
      ctx.save();
      ctx.translate(this.px + G / 2, this.py + G / 2);
      if (img && img.complete) ctx.drawImage(img, -w / 2, -h / 2, w, h);
      else {
        ctx.fillStyle = "#ff8a3d";
        ctx.beginPath();
        ctx.arc(0, 0, 8, 0, BI.tau);
        ctx.fill();
      }
      ctx.restore();
    }
  }

  class Mine {
    constructor(fx, hero) {
      this.fx = fx;
      this.hero = hero;
      this.x = hero.x;
      this.y = hero.y;
      this.t = 0;
      this.arm = 0;
      this.dead = false;
    }
    row() { return this.y; }
    sortX() { return this.x * G; }
    step() {
      this.t++;
      if (this.arm++ < 10) return;
      const g = this.fx.game;
      if (g.map.fireAt(this.x, this.y)) { this.boom(); return; }
      for (const h of g.heroes) {
        if (h === this.hero || !h.alive) continue;
        if (h.x === this.x && h.y === this.y) { this.boom(); return; }
      }
    }
    boom() {
      this.dead = true;
      this.fx.boomAt(this.x, this.y, this.hero, 0);
    }
    draw(ctx) {
      const img = BI.assets.get("mine_" + (1 + (this.t % 60)));
      if (!img || !img.complete) return;
      const w = Math.max(20, BI.sw(img));
      const h = Math.max(20, BI.sh(img));
      ctx.drawImage(img, this.x * G + G / 2 - w / 2, this.y * G + G - h, w, h);
    }
  }

  class Grenade {
    constructor(fx, hero, tx, ty) {
      this.fx = fx;
      this.hero = hero;
      this.sx = hero.x;
      this.sy = hero.y;
      this.tx = tx;
      this.ty = ty;
      this.t = 0;
      this.life = 22;
      this.dead = false;
      BI.sound.play("grenade", 0.6);
    }
    row() { return Math.round(BI.lerp(this.sy, this.ty, Math.min(1, this.t / this.life))); }
    sortX() { return BI.lerp(this.sx, this.tx, Math.min(1, this.t / this.life)) * G; }
    step() {
      this.t++;
      if (this.t >= this.life) {
        this.dead = true;
        this.fx.boomAt(this.tx, this.ty, this.hero, 1);
      }
    }
    draw(ctx) {
      const k = Math.min(1, this.t / this.life);
      const px = BI.lerp(this.sx, this.tx, k) * G;
      const py = BI.lerp(this.sy, this.ty, k) * G - Math.sin(k * Math.PI) * 64;
      const img = BI.assets.get("grenade_" + (1 + (Math.floor(this.t / 5) % 4)));
      if (!img || !img.complete) return;
      const w = Math.max(16, BI.sw(img));
      const h = Math.max(16, BI.sh(img));
      ctx.drawImage(img, px + G / 2 - w / 2, py + G - h, w, h);
    }
  }

  class GasBomb {
    constructor(fx, hero) {
      this.fx = fx;
      this.hero = hero;
      this.x = hero.x;
      this.y = hero.y;
      this.t = 0;
      this.dead = false;
    }
    row() { return this.y; }
    sortX() { return this.x * G; }
    step() {
      this.t++;
      const g = this.fx.game;
      if (g.map.fireAt(this.x, this.y)) { this.dead = true; return; }
      for (const h of g.heroes) {
        if (h === this.hero || !h.alive) continue;
        if (h.x === this.x && h.y === this.y) {
          h.slowTime = 600;
          h.slowVLevel = 20;
          h.slowTick = g.tick;
          BI.sound.play("sfx319", 0.4);
          this.dead = true;
          return;
        }
      }
    }
    draw(ctx) {
      const img = BI.assets.get("gas_" + (1 + (this.t % 101)));
      if (!img || !img.complete) return;
      const w = Math.max(24, BI.sw(img));
      const h = Math.max(24, BI.sh(img));
      ctx.save();
      ctx.globalAlpha = 0.9;
      ctx.drawImage(img, this.x * G + G / 2 - w / 2, this.y * G + G - h, w, h);
      ctx.restore();
    }
  }

  class IceArrow {
    constructor(fx, hero) {
      this.fx = fx;
      this.hero = hero;
      this.x = hero.x;
      this.y = hero.y;
      const v = BI.dirVec[hero.dir];
      this.dx = v[0];
      this.dy = v[1];
      this.t = 0;
      this.offset = 0;
      this.dead = false;
      this.cells = [];
    }
    row() { return this.y + this.dy * this.offset; }
    sortX() { return (this.x + this.dx * this.offset) * G; }
    step() {
      this.t++;
      if (this.t <= 12) {
        if (this.t % 4 === 0) {
          this.offset++;
          const x = this.x + this.dx * this.offset;
          const y = this.y + this.dy * this.offset;
          const c = this.fx.game.map.cell(x, y);
          if (c && !c.brick && !c.stone && !c.bomb) {
            this.fx.game.map.setIce(x, y, 40);
            this.cells.push({ x, y });
            this.fx.anim("freezeEffect", x, y, 40, { alpha: 0.8 });
          } else {
            this.t = 13;
          }
        }
      } else if (this.cells.length || this.t > 16) {
        this.dead = true;
      }
    }
    draw() {}
  }

  class FireArrow {
    constructor(fx, hero) {
      this.fx = fx;
      this.hero = hero;
      this.x = hero.x;
      this.y = hero.y;
      const v = BI.dirVec[hero.dir];
      this.dx = v[0];
      this.dy = v[1];
      this.t = 0;
      this.offset = 0;
      this.dead = false;
    }
    row() { return this.y + this.dy * this.offset; }
    sortX() { return (this.x + this.dx * this.offset) * G; }
    step() {
      this.t++;
      if (this.t <= 12) {
        if (this.t % 4 === 0) {
          this.offset++;
          const g = this.fx.game;
          const x = this.x + this.dx * this.offset;
          const y = this.y + this.dy * this.offset;
          const c = g.map.cell(x, y);
          if (!c || c.stone) {
            this.t = 13;
            return;
          }
          g.map.ignite(x, y, 44, this.hero);
          g.breakBrickAt(x, y, this.hero);
          this.fx.anim("fireEffect", x, y, 44, { alpha: 0.9 });
        }
      } else if (this.t > 16) {
        this.dead = true;
      }
      if (this.t > 60) this.dead = true;
    }
    draw() {}
  }

  /* ---------------- one-shot frame animation ---------------- */
  class Anim {
    constructor(key, x, y, frames, opts) {
      this.key = key;
      this.x = x;
      this.y = y;
      this.frames = frames;
      this.t = 0;
      this.dead = false;
      this.scale = (opts && opts.scale) || 1;
      this.dx = (opts && opts.dx) || 0;
      this.dy = (opts && opts.dy) || 0;
      this.alpha = (opts && opts.alpha) || 1;
      this.rot = (opts && opts.rot) || 0;
    }
    step() {
      if (++this.t > this.frames) this.dead = true;
    }
    draw(ctx) {
      const img = BI.assets.get(this.key + "_" + Math.min(this.frames, Math.max(1, this.t)));
      if (!img || !img.complete) return;
      const w = BI.sw(img) * this.scale;
      const h = BI.sh(img) * this.scale;
      ctx.save();
      ctx.globalAlpha = this.alpha;
      if (this.rot) {
        ctx.translate(this.x * G + G / 2, this.y * G + G / 2);
        ctx.rotate(this.rot);
        ctx.drawImage(img, -w / 2, -h / 2, w, h);
      } else {
        ctx.drawImage(img, this.x * G + G / 2 - w / 2 + this.dx, this.y * G + G - h + this.dy, w, h);
      }
      ctx.restore();
    }
  }

  /* ---------------- fx manager ---------------- */
  class FX {
    constructor(game) {
      this.game = game;
      this.tick = 0;
      this.bombs = [];
      this.weapons = [];
      this.explosions = [];
      this.effects = [];
    }

    plant(hero) {
      const b = new Bomb(this.game, hero.x, hero.y, hero, hero.bombLevel);
      this.bombs.push(b);
      BI.sound.play("place", 0.5);
      return b;
    }

    boomAt(x, y, owner, power) {
      const e = new Explosion(this.game, x, y, owner, power || 0);
      this.explosions.push(e);
      BI.sound.play("explode", 0.55);
      BI.addShake(power > 1 ? 5 : 3, 6);
      this.game.particles.burst(x * G + G / 2, y * G + G / 2, {
        count: 8 + power * 3, colors: ["#ffd166", "#ff8a3d", "#ff4d6d", "#fff3c4"],
        speedMin: 0.6, speedMax: 2.6, sizeMin: 1.5, sizeMax: 4.2,
      });
      return e;
    }

    rocket(hero) {
      this.weapons.push(new Rocket(this, hero));
      BI.sound.play("rocket", 0.6);
    }

    mine(hero) {
      this.weapons.push(new Mine(this, hero));
      BI.sound.play("mine", 0.35);
    }

    grenade(hero) {
      const v = BI.dirVec[hero.dir];
      const tx = hero.x + v[0] * 2;
      const ty = hero.y + v[1] * 2;
      const c = this.game.map.cell(tx, ty);
      if (!c || c.stone) return false;
      this.weapons.push(new Grenade(this, hero, tx, ty));
      return true;
    }

    gas(hero) {
      this.weapons.push(new GasBomb(this, hero));
      BI.sound.play("sfx315", 0.4);
    }

    iceArrow(hero) {
      const v = BI.dirVec[hero.dir];
      const c = this.game.map.cell(hero.x + v[0], hero.y + v[1]);
      if (!c || c.brick || c.stone) return false;
      this.weapons.push(new IceArrow(this, hero));
      BI.sound.play("sfx325", 0.45);
      return true;
    }

    fireArrow(hero) {
      const v = BI.dirVec[hero.dir];
      const c = this.game.map.cell(hero.x + v[0], hero.y + v[1]);
      if (!c || c.stone) return false;
      this.weapons.push(new FireArrow(this, hero));
      BI.sound.play("explode", 0.4);
      return true;
    }

    anim(key, x, y, frames, opts) {
      const a = new Anim(key, x, y, frames, opts);
      this.effects.push(a);
      return a;
    }

    smoke(px, py) {
      this.effects.push(new Anim("rocketSmoke",
        Math.floor(px / G), Math.floor(py / G), 4, { scale: 1.1, alpha: 0.7 }));
    }

    step() {
      this.tick++;
      for (const b of this.bombs) b.step();
      for (const w of this.weapons) w.step();
      for (const e of this.explosions) e.step();
      for (const a of this.effects) a.step();
      this.bombs = this.bombs.filter((b) => !b.dead);
      this.weapons = this.weapons.filter((w) => !w.dead);
      this.explosions = this.explosions.filter((e) => !e.dead);
      this.effects = this.effects.filter((a) => !a.dead);
    }

    collect(list) {
      // AS2 draws bombs and weapons at calDepth - 5, behind heroes standing
      // on the same cell. Sort values are logical pixels like hero.px.
      for (const b of this.bombs) {
        list.push({ row: b.y, sort: b.px + 5, draw: (ctx) => b.draw(ctx) });
      }
      for (const w of this.weapons) {
        list.push({ row: w.row(), sort: w.sortX() + 5, draw: (ctx) => w.draw(ctx) });
      }
    }

    drawTop(ctx) {
      for (const e of this.explosions) e.draw(ctx);
      for (const a of this.effects) a.draw(ctx);
    }

    clear() {
      this.bombs.length = 0;
      this.weapons.length = 0;
      this.explosions.length = 0;
      this.effects.length = 0;
    }
  }

  BI.Bomb = Bomb;
  BI.FX = FX;
})();
