/* ============================================================
   map.js — 15x13 grid, 60% brick generation, scene art
   ============================================================ */
(function () {
  "use strict";
  const BI = window.BI;
  const G = BI.GRID;

  const SPAWNS = [[3, 3], [11, 3], [3, 9], [11, 9]];

  class GameMap {
    constructor(sceneId) {
      this.scene = BI.SCENES[sceneId];
      this.cells = [];
      this.big = { x: 6, y: 5, frame: this.scene.big };
      this.spawns = SPAWNS.map(([x, y]) => ({ x, y }));
      this.fire = new Map();
      this.ice = new Map();
      this.generate();
      this.bakeBackground();
    }

    inBounds(x, y) { return x >= 0 && x < BI.MAP_W && y >= 0 && y < BI.MAP_H; }
    cell(x, y) { return this.inBounds(x, y) ? this.cells[y][x] : null; }

    isBigCell(x, y) {
      return x >= this.big.x && x < this.big.x + 2 && y >= this.big.y && y < this.big.y + 2;
    }

    generate() {
      const scene = this.scene;
      // AS2: brickStyle/unbreakAbleStyle are shuffled, only the first 4 brick
      // styles and the first 2 stone styles are ever used.
      const brickStyles = BI.shuffle(scene.bricks.slice());
      const brickLimit = Math.min(4, brickStyles.length);
      const stoneStyles = BI.shuffle(scene.stones.slice()).slice(0, 2);

      for (let y = 0; y < BI.MAP_H; y++) {
        const row = [];
        for (let x = 0; x < BI.MAP_W; x++) {
          row.push({ x, y, brick: false, stone: false, frame: 0, stoneFrame: 0, tool: null, bomb: null });
        }
        this.cells.push(row);
      }

      // centre 2x2 landmark (the big tree / tower)
      for (let y = this.big.y; y < this.big.y + 2; y++) {
        for (let x = this.big.x; x < this.big.x + 2; x++) {
          this.cell(x, y).stone = true;
        }
      }

      this.fillPockets(stoneStyles);

      // AS2 frame_2: 60% brick roll on every non-unbreakable cell, then the
      // spawn cell itself is cleared for its hero.
      for (let y = 0; y < BI.MAP_H; y++) {
        for (let x = 0; x < BI.MAP_W; x++) {
          const c = this.cell(x, y);
          if (c.stone) continue;
          c.brick = Math.random() < 0.6;
          c.frame = c.brick ? brickStyles[BI.randInt(0, brickLimit - 1)] : 0;
        }
      }
      for (const s of this.spawns) {
        const c = this.cell(s.x, s.y);
        c.brick = false;
        c.frame = 0;
      }

      // Never box a spawn in: if all four neighbours rolled bricks, open one.
      const sides = [[-1, 0], [0, -1], [1, 0], [0, 1]];
      for (const s of this.spawns) {
        const open = sides.some(([dx, dy]) => {
          const n = this.cell(s.x + dx, s.y + dy);
          return n && !n.brick && !n.stone;
        });
        if (open) continue;
        const [dx, dy] = sides[BI.randInt(0, 3)];
        const n = this.cell(s.x + dx, s.y + dy);
        if (n) {
          n.brick = false;
          n.frame = 0;
        }
      }
    }

    // AS2 pocket-fill: unbreakables only go where the 8-cell ring around an
    // empty cell is almost one continuous run (<= 3 empty/brick segments) and
    // never contains three consecutive blocked cells. Spawn diagonals are
    // pushed to the front so they are always evaluated.
    fillPockets(stoneStyles) {
      const aroundXy = [[-1, 0], [-1, -1], [0, -1], [1, -1], [1, 0], [1, 1], [0, 1], [-1, 1]];
      const blocked = (x, y) => {
        if (!this.inBounds(x, y)) return true;
        return this.cell(x, y).stone || this.isBigCell(x, y);
      };

      const ranMap = [];
      for (let y = 0; y < BI.MAP_H; y++) {
        for (let x = 0; x < BI.MAP_W; x++) {
          let skip = false;
          for (const s of this.spawns) {
            if (x === s.x && y === s.y) { skip = true; break; }
            if ((x === s.x - 1 || x === s.x + 1) && y === s.y) { skip = true; break; }
            if (x === s.x && (y === s.y - 1 || y === s.y + 1)) { skip = true; break; }
          }
          if (!skip) ranMap.push([x, y]);
        }
      }

      const ranMapL = ranMap.length;
      for (let i = 0; i < ranMapL; i++) {
        const r = BI.randInt(0, ranMapL - 1);
        const tmp = ranMap[i];
        ranMap[i] = ranMap[r];
        ranMap[r] = tmp;
      }
      for (const s of this.spawns) {
        for (let j = 1; j < 8; j += 2) {
          ranMap.unshift([s.x + aroundXy[j][0], s.y + aroundXy[j][1]]);
        }
      }

      for (let i = 0; i < ranMapL; i++) {
        const x0 = ranMap[i][0];
        const y0 = ranMap[i][1];
        if (!this.inBounds(x0, y0)) continue;
        if (this.cell(x0, y0).stone) continue;

        let j = 0;
        for (; j < 8; j += 2) {
          const k1 = (j + 1) % 8;
          const k2 = (j + 2) % 8;
          if (blocked(x0 + aroundXy[j][0], y0 + aroundXy[j][1]) &&
              blocked(x0 + aroundXy[k1][0], y0 + aroundXy[k1][1]) &&
              blocked(x0 + aroundXy[k2][0], y0 + aroundXy[k2][1])) break;
        }
        if (j < 8) continue;

        let empty = !blocked(x0 - 1, y0);
        let totalEmpty = empty ? 1 : 0;
        let totalBrick = empty ? 0 : 1;
        for (let k = 1; k < 8; k++) {
          const isBrick = blocked(x0 + aroundXy[k][0], y0 + aroundXy[k][1]);
          if (empty) {
            if (isBrick) { empty = false; totalBrick++; }
          } else if (!isBrick) {
            if (k % 2 === 0) {
              empty = true;
              totalEmpty++;
            } else {
              const kk = (k + 1) % 8;
              if (!blocked(x0 + aroundXy[kk][0], y0 + aroundXy[kk][1])) {
                empty = true;
                totalEmpty++;
              } else {
                k += 2;
              }
            }
          }
        }

        if (totalEmpty + totalBrick <= 3) {
          const c = this.cell(x0, y0);
          c.stone = true;
          c.stoneFrame = stoneStyles[BI.randInt(0, stoneStyles.length - 1)];
        }
      }
    }

    bakeBackground(scale = 1) {
      const cv = document.createElement("canvas");
      cv.width = Math.round(BI.VIEW_W * scale);
      cv.height = Math.round(BI.VIEW_H * scale);
      const c = cv.getContext("2d");
      c.scale(scale, scale);
      const list = this.scene.bg;
      for (let y = 0; y < BI.MAP_H; y++) {
        for (let x = 0; x < BI.MAP_W; x++) {
          const f = list[(x * 5 + y * 3) % list.length];
          const img = BI.assets.get("bg_" + f);
          if (img && img.complete) c.drawImage(img, x * G, y * G, G, G);
          else {
            c.fillStyle = this.scene.tint;
            c.fillRect(x * G, y * G, G, G);
          }
        }
      }
      const v = c.createRadialGradient(225, 190, 90, 225, 195, 300);
      v.addColorStop(0, "rgba(255,255,255,0.05)");
      v.addColorStop(1, "rgba(40,10,40,0.22)");
      c.fillStyle = v;
      c.fillRect(0, 0, BI.VIEW_W, BI.VIEW_H);
      this.bgCanvas = cv;
    }

    isSolid(x, y) {
      const c = this.cell(x, y);
      if (!c) return true;
      return c.brick || c.stone || c.tool === "__ice";
    }

    isStone(x, y) {
      const c = this.cell(x, y);
      return !c || c.stone;
    }

    canWalk(x, y, hero) {
      const c = this.cell(x, y);
      if (!c) return false;
      if (c.brick || c.stone || c.tool === "__ice") return false;
      if (c.bomb) return !!(hero && hero.glove && c.bomb.owner === hero);
      return true;
    }

    breakBrick(x, y) {
      const c = this.cell(x, y);
      if (!c || !c.brick) return 0;
      c.brick = false;
      const f = c.frame;
      c.frame = 0;
      return f;
    }

    placeTool(x, y, tool) {
      const c = this.cell(x, y);
      if (c) c.tool = tool;
    }

    takeTool(x, y) {
      const c = this.cell(x, y);
      if (!c || !c.tool || c.tool === "__ice") return null;
      const t = c.tool;
      c.tool = null;
      return t;
    }

    setBomb(x, y, bomb) {
      const c = this.cell(x, y);
      if (c) c.bomb = bomb;
    }

    clearBomb(x, y, bomb) {
      const c = this.cell(x, y);
      if (c && c.bomb === bomb) c.bomb = null;
    }

    ignite(x, y, ticks, owner) {
      if (!this.inBounds(x, y)) return;
      const k = x + "," + y;
      const cur = this.fire.get(k);
      if (cur) {
        cur.t = Math.max(cur.t, ticks);
        if (owner) cur.owner = owner;
      } else {
        this.fire.set(k, { t: ticks, owner: owner || null });
      }
    }

    fireAt(x, y) {
      const e = this.fire.get(x + "," + y);
      return !!(e && e.t > 0);
    }

    updateFire() {
      for (const [k, v] of this.fire) {
        const e = v;
        if (e.t <= 1) this.fire.delete(k);
        else e.t--;
      }
    }

    fireOwnerAt(x, y) {
      const e = this.fire.get(x + "," + y);
      return e ? e.owner : null;
    }

    updateTools() {
      for (let y = 0; y < BI.MAP_H; y++) {
        for (let x = 0; x < BI.MAP_W; x++) {
          const t = this.cells[y][x].tool;
          if (t && t !== "__ice") t.age++;
        }
      }
    }

    clearFire() { this.fire.clear(); }

    setIce(x, y, ticks) {
      const c = this.cell(x, y);
      if (!c || c.tool === "__ice") return;
      c._iceTool = c.tool;
      c.tool = "__ice";
      this.ice.set(x + "," + y, ticks);
    }

    meltIce(x, y) {
      const k = x + "," + y;
      if (!this.ice.has(k)) return false;
      this.ice.delete(k);
      const c = this.cell(x, y);
      if (c && c.tool === "__ice") {
        c.tool = c._iceTool || null;
        c._iceTool = null;
      }
      return true;
    }

    updateIce() {
      for (const [k, v] of this.ice) {
        if (v <= 1) {
          this.ice.delete(k);
          const [x, y] = k.split(",").map(Number);
          const c = this.cell(x, y);
          if (c && c.tool === "__ice") {
            c.tool = c._iceTool || null;
            c._iceTool = null;
          }
        } else {
          this.ice.set(k, v - 1);
        }
      }
    }

    clearIce() {
      for (const k of this.ice.keys()) {
        const [x, y] = k.split(",").map(Number);
        const c = this.cell(x, y);
        if (c && c.tool === "__ice") {
          c.tool = c._iceTool || null;
          c._iceTool = null;
        }
      }
      this.ice.clear();
    }

    drawGround(ctx) {
      ctx.drawImage(this.bgCanvas, 0, 0, BI.VIEW_W, BI.VIEW_H);
      for (let y = 0; y < BI.MAP_H; y++) {
        for (let x = 0; x < BI.MAP_W; x++) {
          const c = this.cells[y][x];
          if (c.tool === "__ice") this.drawIce(ctx, x, y);
          else if (c.tool) this.drawTool(ctx, x, y, c.tool);
        }
      }
    }

    drawTool(ctx, x, y, tool) {
      const img = BI.assets.get("tool_" + tool.tool);
      if (!img || !img.complete) return;
      const bob = Math.sin((tool.bob + tool.age) * 0.18) * 1.6;
      const s = 1.1;
      const w = BI.sw(img) * s;
      const h = BI.sh(img) * s;
      ctx.save();
      ctx.fillStyle = "rgba(0,0,0,0.18)";
      ctx.beginPath();
      ctx.ellipse(x * G + G / 2, y * G + G - 3, 9, 3, 0, 0, BI.tau);
      ctx.fill();
      ctx.drawImage(img, x * G + G / 2 - w / 2, y * G + G - h - 3 + bob, w, h);
      ctx.restore();
    }

    drawIce(ctx, x, y) {
      const img = BI.assets.get("freezeEffect_1");
      ctx.save();
      ctx.fillStyle = "rgba(150,230,255,0.45)";
      BI.roundRect(ctx, x * G + 1, y * G + 1, G - 2, G - 2, 7);
      ctx.fill();
      ctx.strokeStyle = "rgba(255,255,255,0.8)";
      ctx.lineWidth = 1.5;
      ctx.stroke();
      if (img && img.complete) {
        ctx.globalAlpha = 0.7;
        ctx.drawImage(img, x * G - 3, y * G - 6, G + 6, G + 10);
      }
      ctx.restore();
    }

    drawBlocks(ctx) {
      for (let y = 0; y < BI.MAP_H; y++) this.drawBlockRow(ctx, y);
    }

    drawBlockRow(ctx, y) {
      for (let x = 0; x < BI.MAP_W; x++) {
        const c = this.cells[y][x];
        if (c.stone) {
          if (!this.isBigCell(x, y)) this.drawStone(ctx, x, y, c.stoneFrame);
        } else if (c.brick) {
          this.drawBrick(ctx, x, y, c.frame);
        }
      }
      if (y === this.big.y + 1) this.drawBig(ctx);
    }

    drawBrick(ctx, x, y, frame) {
      const img = BI.assets.get("brick_" + frame);
      if (!img || !img.complete) {
        ctx.fillStyle = "#c98a4b";
        BI.roundRect(ctx, x * G + 2, y * G + 4, G - 4, G - 8, 5);
        ctx.fill();
        return;
      }
      const w = BI.sw(img) * 0.86;
      const h = BI.sh(img) * 0.86;
      ctx.drawImage(img, x * G + G / 2 - w / 2, y * G + G - h + 2, w, h);
    }

    drawStone(ctx, x, y, frame) {
      const img = BI.assets.get("brick_" + frame);
      if (!img || !img.complete) {
        ctx.fillStyle = "#8b93a8";
        BI.roundRect(ctx, x * G + 1, y * G + 3, G - 2, G - 4, 6);
        ctx.fill();
        return;
      }
      const w = Math.max(30, BI.sw(img) * 0.8);
      const h = Math.max(30, BI.sh(img) * 0.8);
      ctx.drawImage(img, x * G + G / 2 - w / 2, y * G + G - h + 2, w, h);
    }

    drawBig(ctx) {
      const img = BI.assets.get("brick_" + this.big.frame);
      const cx = (this.big.x + 1) * G;
      const bottom = (this.big.y + 2) * G + 4;
      if (img && img.complete) {
        const w = 68;
        const h = Math.min(74, BI.sh(img) * 0.78);
        ctx.drawImage(img, cx - w / 2, bottom - h, w, h);
      } else {
        ctx.fillStyle = "#b0844a";
        BI.roundRect(ctx, cx - 30, bottom - 60, 60, 60, 12);
        ctx.fill();
      }
      ctx.save();
      ctx.globalAlpha = 0.3 + Math.sin(performance.now() / 600) * 0.12;
      ctx.strokeStyle = this.scene.tint;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.ellipse(cx, (this.big.y + 2) * G + 2, 36, 10, 0, 0, BI.tau);
      ctx.stroke();
      ctx.restore();
    }
  }

  BI.GameMap = GameMap;
  BI.SPAWNS = SPAWNS.map(([x, y]) => ({ x, y }));
})();
