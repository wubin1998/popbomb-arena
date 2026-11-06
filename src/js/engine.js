/* ============================================================
   Q版泡泡堂 · HTML5 Remake
   engine.js — constants, assets, input, audio, fixed 35 Hz loop
   Reverse-engineered from Qpaopaotang.swf (AS2, 550x400, 35 fps)
   ============================================================ */
(function () {
  "use strict";
  const BI = (window.BI = window.BI || {});

  BI.FPS = 35;
  BI.STEP = 1000 / 35;
  BI.GRID = 30;
  BI.MAP_W = 15;
  BI.MAP_H = 13;
  BI.VIEW_W = 450;
  BI.VIEW_H = 390;
  BI.TOTAL_TIME = 180;

  BI.HEROES = [
    { id: 1, name: "红泡泡", color: "#ff5b9d", dark: "#c92f70", key: "hero1" },
    { id: 2, name: "黄泡泡", color: "#ffcf3d", dark: "#d79a12", key: "hero2" },
    { id: 3, name: "蓝泡泡", color: "#56b7ff", dark: "#2277cc", key: "hero3" },
    { id: 4, name: "绿泡泡", color: "#6ee08a", dark: "#2f9a53", key: "hero4" },
  ];

  // sceneStyle 0..4 -> background frames, destructible bricks, stone bricks, big tree
  BI.SCENES = [
    { id: 0, name: "森林系", bg: [1, 2, 3], bricks: [1, 2, 3, 4, 5, 6], stones: [7, 8, 9], big: 52, tint: "#68c96b" },
    { id: 1, name: "沙漠系", bg: [4, 5], bricks: [10, 11, 12], stones: [13, 14, 15, 16, 17, 18, 19], big: 53, tint: "#e7b45a" },
    { id: 2, name: "城市系", bg: [6, 7], bricks: [20, 21, 22, 23], stones: [24, 25, 26, 27, 28, 29, 30, 31], big: 55, tint: "#8ea4c8" },
    { id: 3, name: "藏宝系", bg: [8, 9, 10], bricks: [32, 33, 34, 35, 36, 37, 38], stones: [39, 40, 41], big: 57, tint: "#d6a75a" },
    { id: 4, name: "糖果屋系", bg: [11, 12], bricks: [42, 43, 44, 45, 46, 47], stones: [48, 49, 50, 51], big: 58, tint: "#ff9ecb" },
  ];

  BI.DIFF = {
    easy: { key: "easy", name: "简单", stages: 15, brickScore: 5, escape: 20 },
    normal: { key: "normal", name: "普通", stages: 20, brickScore: 10, escape: 60 },
    hard: { key: "hard", name: "困难", stages: 25, brickScore: 15, escape: 100 },
  };

  /* ---------------- helpers ---------------- */
  BI.clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
  BI.lerp = (a, b, t) => a + (b - a) * t;
  BI.rand = (a = 1, b) => (b === undefined ? Math.random() * a : a + Math.random() * (b - a));
  BI.randInt = (a, b) => Math.floor(BI.rand(a, b + 1));
  BI.pick = (arr) => arr[Math.floor(Math.random() * arr.length)];
  BI.chance = (p) => Math.random() < p;
  BI.tau = Math.PI * 2;
  BI.shuffle = (arr) => {
    for (let i = arr.length - 1; i > 0; i--) {
      const j = Math.floor(Math.random() * (i + 1));
      [arr[i], arr[j]] = [arr[j], arr[i]];
    }
    return arr;
  };

  BI.roundRect = function (ctx, x, y, w, h, r) {
    const rr = Math.min(r, w / 2, h / 2);
    ctx.beginPath();
    ctx.moveTo(x + rr, y);
    ctx.arcTo(x + w, y, x + w, y + h, rr);
    ctx.arcTo(x + w, y + h, x, y + h, rr);
    ctx.arcTo(x, y + h, x, y, rr);
    ctx.arcTo(x, y, x + w, y, rr);
    ctx.closePath();
  };

  BI.hexToRgba = (hex, a) => {
    const h = hex.replace("#", "");
    const n = parseInt(h.length === 3 ? h.split("").map((c) => c + c).join("") : h, 16);
    return `rgba(${(n >> 16) & 255},${(n >> 8) & 255},${n & 255},${a})`;
  };

  BI.fmtTime = (sec) => {
    const s = Math.max(0, Math.ceil(sec));
    return String(Math.floor(s / 60)).padStart(2, "0") + ":" + String(s % 60).padStart(2, "0");
  };

  BI.dirVec = [[-1, 0], [0, -1], [1, 0], [0, 1]]; // left, up, right, down (original xy)

  /* ---------------- assets ---------------- */
  // HiDPI sprites live in src/assets/img@2x and are drawn at half their
  // natural pixel size. Images fall back to the 1x folder if a 2x file is
  // missing, and each image remembers which density it came from.
  BI.spriteScale = (img) => (img && img.__s) || 1;
  BI.sw = (img) => img.naturalWidth / BI.spriteScale(img);
  BI.sh = (img) => img.naturalHeight / BI.spriteScale(img);

  const IMG_SETS = {
    bg: 12, brick: 58, tool: 12, bomb: 40, exp: 12, brickExp: 11,
    heroExp: 36, dieSmoke: 36, rocket: 4, rocketSmoke: 4, mine: 60,
    grenade: 4, gas: 101, fireEffect: 40, freezeEffect: 40, pointer: 60,
    hero1: 4, hero2: 4, hero3: 4, hero4: 4,
  };

  const AUDIO = {
    place: "place", explode: "explode", pickup: "pickup", push: "push",
    rocket: "rocket", grenade: "grenade", mine: "mine", stageWin: "stageWin",
    stageLose: "stageLose", menu: "menu", bgm1: "bgm1", bgm2: "bgm2",
    bgm3: "bgm3", bgm4: "bgm4", sfx315: "sfx315", sfx319: "sfx319", sfx325: "sfx325",
  };

  class Assets {
    constructor() {
      this.img = {};
      this.sfx = {};
      this.loaded = 0;
      this.total = 1;
      this.onProgress = null;
    }

    image(key, src, hd) {
      this.total++;
      const im = new Image();
      let done = false;
      const settle = () => {
        if (done) return;
        done = true;
        this.loaded++;
        if (this.onProgress) this.onProgress(this.loaded / this.total);
      };
      im.onload = settle;
      im.onerror = () => {
        if (im.__s === 2) {
          im.__s = 1;
          im.src = src;
        } else {
          settle();
        }
      };
      im.__s = hd ? 2 : 1;
      im.src = hd || src;
      this.img[key] = im;
      return im;
    }

    audio(key, src) {
      const a = new Audio();
      a.src = src;
      a.preload = "auto";
      this.sfx[key] = a;
    }

    load(onDone) {
      Object.keys(IMG_SETS).forEach((set) => {
        const n = IMG_SETS[set];
        for (let i = 1; i <= n; i++) {
          const path = `${set}/${i}.png`;
          this.image(set + "_" + i, `src/assets/img/${path}`, `src/assets/img@2x/${path}`);
        }
      });
      Object.keys(AUDIO).forEach((k) => this.audio(k, `src/assets/audio/${AUDIO[k]}.mp3`));

      this.loaded++;
      const tick = () => {
        if (this.onProgress) this.onProgress(this.loaded / this.total);
        if (this.loaded >= this.total) {
          onDone && onDone();
        } else {
          setTimeout(tick, 40);
        }
      };
      tick();
    }

    get(key) { return this.img[key]; }
  }

  BI.assets = new Assets();

  /* ---------------- audio ---------------- */
  class Sound {
    constructor() {
      this.enabled = localStorage.getItem("qpopo.sound") !== "off";
      this.bgm = null;
      this.bgmKey = null;
      this.musicVolume = 0.28;
    }

    play(name, vol = 0.7) {
      if (!this.enabled) return;
      const base = BI.assets.sfx[name];
      if (!base) return;
      try {
        const n = base.cloneNode();
        n.volume = vol;
        const p = n.play();
        if (p && p.catch) p.catch(() => {});
      } catch (e) { /* autoplay guard */ }
    }

    music(name) {
      if (this.bgmKey === name) {
        if (this.bgm && this.bgm.paused && this.enabled) this.bgm.play().catch(() => {});
        return;
      }
      this.stopMusic();
      this.bgmKey = name;
      const base = BI.assets.sfx[name];
      if (!base) return;
      const a = base.cloneNode();
      a.loop = true;
      a.volume = this.musicVolume;
      this.bgm = a;
      if (this.enabled) a.play().catch(() => {});
    }

    stopMusic() {
      if (this.bgm) {
        this.bgm.pause();
        this.bgm.currentTime = 0;
        this.bgm = null;
      }
      this.bgmKey = null;
    }

    toggle() {
      this.enabled = !this.enabled;
      localStorage.setItem("qpopo.sound", this.enabled ? "on" : "off");
      if (!this.enabled) this.stopMusic();
      else if (this.bgmKey) {
        const k = this.bgmKey;
        this.bgmKey = null;
        this.music(k);
      }
      return this.enabled;
    }
  }

  BI.sound = new Sound();

  /* ---------------- input ---------------- */
  class Input {
    constructor() {
      this.down = new Set();
      this.hit = new Set();
      window.addEventListener("keydown", (e) => {
        if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Space", "Enter"].includes(e.code)) e.preventDefault();
        if (e.repeat) return;
        this.down.add(e.code);
        this.hit.add(e.code);
      });
      window.addEventListener("keyup", (e) => this.down.delete(e.code));
      window.addEventListener("blur", () => this.down.clear());
    }

    isDown(code) { return this.down.has(code); }
    pressed(code) { return this.hit.has(code); }

    player(layout) {
      const keys = layout === "p1"
        ? { left: "KeyA", up: "KeyW", right: "KeyD", down: "KeyS", fire: "Space" }
        : layout === "p2"
          ? { left: "ArrowLeft", up: "ArrowUp", right: "ArrowRight", down: "ArrowDown", fire: "Enter" }
          : { left: "ArrowLeft", up: "ArrowUp", right: "ArrowRight", down: "ArrowDown", fire: "Space" };
      const st = {};
      for (const k in keys) st[k] = this.down.has(keys[k]);
      st.firePressed = this.hit.has(keys.fire);
      st.anyHit = this.hit.size > 0;
      st.hit = this.hit;
      return st;
    }

    endFrame() { this.hit.clear(); }
  }

  BI.input = new Input();

  /* ---------------- fixed loop ---------------- */
  class Loop {
    constructor(step, render) {
      this.stepFn = step;
      this.renderFn = render;
      this.acc = 0;
      this.last = performance.now();
      this.running = false;
      this.frames = 0;
      this._tick = this._tick.bind(this);
    }

    start() {
      if (this.running) return;
      this.running = true;
      this.last = performance.now();
      requestAnimationFrame(this._tick);
    }

    stop() { this.running = false; }

    _tick(now) {
      if (!this.running) return;
      let dt = now - this.last;
      this.last = now;
      if (dt > 240) dt = 240;
      this.acc += dt;
      let guard = 0;
      while (this.acc >= BI.STEP && guard++ < 5) {
        this.acc -= BI.STEP;
        this.frames++;
        this.stepFn();
        BI.input.endFrame();
      }
      this.renderFn(this.acc / BI.STEP);
      requestAnimationFrame(this._tick);
    }
  }

  BI.Loop = Loop;

  /* ---------------- particles ---------------- */
  class Particles {
    constructor() { this.list = []; }

    burst(x, y, opts = {}) {
      const n = opts.count || 10;
      const colors = opts.colors || ["#ffd166", "#ff8a3d", "#ff4d6d"];
      for (let i = 0; i < n; i++) {
        const a = opts.angle !== undefined ? opts.angle + BI.rand(-0.8, 0.8) : BI.rand(0, BI.tau);
        const sp = BI.rand(opts.speedMin || 0.6, opts.speedMax || 2.4);
        this.list.push({
          x, y,
          vx: Math.cos(a) * sp,
          vy: Math.sin(a) * sp - (opts.lift || 0),
          life: 0,
          max: BI.randInt(opts.lifeMin || 10, opts.lifeMax || 22),
          size: BI.rand(opts.sizeMin || 1.5, opts.sizeMax || 4),
          color: BI.pick(colors),
          gravity: opts.gravity === undefined ? 0.08 : opts.gravity,
          shape: opts.shape || "circle",
        });
      }
    }

    update() {
      for (let i = this.list.length - 1; i >= 0; i--) {
        const p = this.list[i];
        p.x += p.vx;
        p.y += p.vy;
        p.vy += p.gravity;
        p.vx *= 0.97;
        p.life++;
        if (p.life >= p.max) this.list.splice(i, 1);
      }
    }

    draw(ctx) {
      for (const p of this.list) {
        const t = 1 - p.life / p.max;
        ctx.globalAlpha = Math.max(0, t);
        ctx.fillStyle = p.color;
        if (p.shape === "square") ctx.fillRect(p.x - p.size / 2, p.y - p.size / 2, p.size, p.size);
        else {
          ctx.beginPath();
          ctx.arc(p.x, p.y, p.size * (0.5 + t * 0.7), 0, BI.tau);
          ctx.fill();
        }
      }
      ctx.globalAlpha = 1;
    }

    clear() { this.list.length = 0; }
  }

  BI.Particles = Particles;

  BI.shake = { t: 0, power: 0 };
  BI.addShake = (power, time) => {
    BI.shake.power = Math.max(BI.shake.power, power);
    BI.shake.t = Math.max(BI.shake.t, time || 6);
  };
  BI.stepShake = () => {
    if (BI.shake.t > 0) {
      BI.shake.t--;
      if (BI.shake.t === 0) BI.shake.power = 0;
    }
  };
})();
