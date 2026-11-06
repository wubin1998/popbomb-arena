#!/usr/bin/env node
/* Local smoke test: drives the remake in headless Chrome via CDP. */
const { spawn } = require("child_process");
const fs = require("fs");

const CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
const URL = "http://127.0.0.1:8123/index.html";
const PORT = 9333;
const PROFILE = "/tmp/qpopo-chrome-profile";

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function getTargets() {
  for (let i = 0; i < 40; i++) {
    try {
      const r = await fetch(`http://127.0.0.1:${PORT}/json/list`);
      const list = await r.json();
      const page = list.find((t) => t.type === "page");
      if (page) return page;
    } catch (e) { /* not up yet */ }
    await sleep(250);
  }
  throw new Error("chrome target not available");
}

async function main() {
  fs.rmSync(PROFILE, { recursive: true, force: true });
  const chrome = spawn(CHROME, [
    "--headless=new",
    "--disable-gpu",
    "--no-first-run",
    "--no-default-browser-check",
    `--remote-debugging-port=${PORT}`,
    `--user-data-dir=${PROFILE}`,
    "--window-size=900,650",
    "about:blank",
  ], { stdio: "ignore" });

  const logs = [];
  const errors = [];
  let ws;
  let nextId = 1;
  const pending = new Map();

  const send = (method, params = {}) => new Promise((resolve, reject) => {
    const id = nextId++;
    pending.set(id, { resolve, reject });
    ws.send(JSON.stringify({ id, method, params }));
    setTimeout(() => {
      if (pending.has(id)) {
        pending.delete(id);
        reject(new Error("timeout " + method));
      }
    }, 15000);
  });

  try {
    const target = await getTargets();
    ws = new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });

    ws.onmessage = (ev) => {
      const msg = JSON.parse(ev.data);
      if (msg.id && pending.has(msg.id)) {
        const p = pending.get(msg.id);
        pending.delete(msg.id);
        msg.error ? p.reject(new Error(msg.error.message)) : p.resolve(msg.result);
      } else if (msg.method === "Runtime.consoleAPICalled") {
        logs.push(msg.params.type + ": " + msg.params.args.map((a) => a.value ?? a.description ?? "").join(" "));
      } else if (msg.method === "Runtime.exceptionThrown") {
        const d = msg.params.exceptionDetails;
        errors.push(d.exception?.description || d.text);
      } else if (msg.method === "Log.entryAdded") {
        const e = msg.params.entry;
        if (e.level === "error") errors.push(e.text);
      }
    };

    await send("Runtime.enable");
    await send("Log.enable");
    await send("Page.enable");
    await send("Network.enable");
    await send("Network.setCacheDisabled", { cacheDisabled: true });
    await send("Page.navigate", { url: URL });

    const evalRaw = async (expression) => {
      const r = await send("Runtime.evaluate", { expression, returnByValue: true, awaitPromise: true });
      return r.result && r.result.value;
    };
    let loaded = false;
    for (let i = 0; i < 60; i++) {
      await sleep(500);
      loaded = await evalRaw(`!!window.BI && BI.assets.total > 0 && BI.assets.loaded >= BI.assets.total && !document.getElementById('soloBtn').disabled`);
      if (loaded) break;
    }
    if (!loaded) throw new Error("assets did not finish loading");

    const evalJs = async (expression) => {
      const r = await send("Runtime.evaluate", { expression, returnByValue: true, awaitPromise: true });
      if (r.exceptionDetails) throw new Error(r.exceptionDetails.text + " " + (r.exceptionDetails.exception?.description || ""));
      return r.result.value;
    };

    const shot = async (name) => {
      const r = await send("Page.captureScreenshot", { format: "png" });
      fs.writeFileSync(`/tmp/${name}.png`, Buffer.from(r.data, "base64"));
    };

    const report = {};
    report.assets = await evalJs(`({
      ready: !!BI && BI.assets.loaded + '/' + BI.assets.total,
      images: Object.keys(BI.assets.img).length,
      audio: Object.keys(BI.assets.sfx).length,
      soloDisabled: document.getElementById('soloBtn').disabled
    })`);
    await shot("qpopo_title");

    await evalJs(`document.getElementById('soloBtn').click()`);
    await sleep(400);
    await shot("qpopo_setup");
    report.setup = await evalJs(`({
      visible: document.getElementById('setupScreen').classList.contains('show'),
      p2row: getComputedStyle(document.getElementById('pickRow2')).display,
      startDisabled: document.getElementById('startBtn').disabled
    })`);

    await evalJs(`document.getElementById('startBtn').click()`);
    await sleep(3600);
    report.afterStart = await evalJs(`({
      hudVisible: !document.getElementById('hud').classList.contains('hidden'),
      countdown: document.querySelector('#countdown span').textContent,
      countdownVisible: !document.getElementById('countdown').classList.contains('hidden'),
      cards: document.querySelectorAll('#cards .card').length,
      canvas: [document.getElementById('game').width, document.getElementById('game').height]
    })`);
    report.map = await evalJs(`(() => {
      const g = window.__qpopo.game;
      const m = g.map;
      let stones = 0, bricks = 0, empty = 0;
      const stoneAt = (x, y) => m.cell(x, y).stone;
      const spawnDiags = m.spawns.map((s) =>
        [[-1,-1],[1,-1],[-1,1],[1,1]].map(([dx,dy]) => stoneAt(s.x+dx, s.y+dy)));
      const spawnClear = m.spawns.map((s) => !m.cell(s.x, s.y).brick && !m.cell(s.x, s.y).stone);
      for (let y = 0; y < BI.MAP_H; y++) for (let x = 0; x < BI.MAP_W; x++) {
        const c = m.cell(x, y);
        if (c.stone) stones++; else if (c.brick) bricks++; else empty++;
      }
      return { scene: m.scene.name, stones, bricks, empty, spawnDiags, spawnClear,
        stoneFrames: [...new Set(m.cells.flat().filter(c=>c.stone).map(c=>c.stoneFrame))].sort(),
        brickFrames: [...new Set(m.cells.flat().filter(c=>c.brick).map(c=>c.frame))].sort() };
    })()`);
    report.mapStress = await evalJs(`(() => {
      const out = { maps: 48, badDiag: 0, badSpawn: 0, badOrth: 0, minStones: 999, maxStones: 0, minBricks: 999, maxBricks: 0 };
      for (let i = 0; i < out.maps; i++) {
        const m = new BI.GameMap(i % 5);
        let stones = 0, bricks = 0;
        for (let y = 0; y < BI.MAP_H; y++) for (let x = 0; x < BI.MAP_W; x++) {
          const c = m.cell(x, y);
          if (c.stone) stones++; else if (c.brick) bricks++;
        }
        out.minStones = Math.min(out.minStones, stones);
        out.maxStones = Math.max(out.maxStones, stones);
        out.minBricks = Math.min(out.minBricks, bricks);
        out.maxBricks = Math.max(out.maxBricks, bricks);
        for (const s of m.spawns) {
          if (m.cell(s.x, s.y).brick || m.cell(s.x, s.y).stone) out.badSpawn++;
          for (const [dx, dy] of [[-1,0],[1,0],[0,-1],[0,1]]) {
            if (m.cell(s.x + dx, s.y + dy).stone) out.badOrth++;
          }
          for (const [dx, dy] of [[-1,-1],[1,-1],[-1,1],[1,1]]) {
            if (!m.cell(s.x + dx, s.y + dy).stone) out.badDiag++;
          }
        }
      }
      return out;
    })()`);
    await shot("qpopo_map");

    // pause test (before AI can end the round)
    await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyP',bubbles:true}))`);
    await sleep(300);
    report.pauseVisible = await evalJs(`document.getElementById('pauseScreen').classList.contains('show')`);
    await evalJs(`document.getElementById('resumeBtn').click()`);

    // Simulate human input: move right, place a bomb, then run up.
    await evalJs(`(async () => {
      const key = (type, code) => window.dispatchEvent(new KeyboardEvent(type, { code, bubbles: true }));
      key('keydown','ArrowRight'); await new Promise(r=>setTimeout(r,260));
      key('keyup','ArrowRight');
      key('keydown','ArrowDown'); await new Promise(r=>setTimeout(r,260));
      key('keyup','ArrowDown');
      key('keydown','Space'); await new Promise(r=>setTimeout(r,120));
      key('keyup','Space');
      await new Promise(r=>setTimeout(r,300));
      key('keydown','ArrowUp'); await new Promise(r=>setTimeout(r,260));
      key('keyup','ArrowUp');
      key('keydown','ArrowLeft'); await new Promise(r=>setTimeout(r,700));
      key('keyup','ArrowLeft');
      return true;
    })()`);
    await sleep(1100);
    await shot("qpopo_game_bomb");

    report.hud = await evalJs(`({
      timer: document.getElementById('timer').textContent,
      stage: document.getElementById('stageChip').textContent,
      scores: [...document.querySelectorAll('.cscore')].map(e=>e.textContent),
      hearts: [...document.querySelectorAll('.hearts')].map(e=>e.textContent)
    })`);

    await sleep(3500);
    report.hud2 = await evalJs(`({
      timer: document.getElementById('timer').textContent,
      scores: [...document.querySelectorAll('.cscore')].map(e=>e.textContent),
      overlay: document.getElementById('resultScreen').classList.contains('show')
    })`);
    await shot("qpopo_game_later");

    // Force a stage clear and verify the next-stage transition keeps HUD/flow intact.
    report.transition = await evalJs(`(async () => {
      const wait = (ms) => new Promise((r) => setTimeout(r, ms));
      const $ = (id) => document.getElementById(id);
      if ($('resultScreen').classList.contains('show')) {
        const primary = $('resultActions').querySelector('button[data-act]');
        if (primary) primary.click();
        await wait(4200);
      }
      const g = window.__qpopo.game;
      g.time = 100;
      g.endKind = 'win';
      g.state = 'ending';
      g.endTick = 1;
      await wait(500);
      const badge = $('resultBadge').textContent;
      const title = $('resultTitle').textContent;
      const nextBtn = $('resultActions').querySelector('[data-act="next"]');
      if (nextBtn) nextBtn.click();
      await wait(800);
      return {
        badge, title, hadNext: !!nextBtn,
        chip: $('stageChip').textContent,
        cards: document.querySelectorAll('#cards .card').length,
        resultHidden: !$('resultScreen').classList.contains('show'),
      };
    })()`);

    report.console = logs.slice(-25);
    report.errors = errors;
    console.log(JSON.stringify(report, null, 2));
  } catch (e) {
    console.error("TEST ERROR:", e.message);
    console.error("page errors:", errors);
    console.error("console:", logs.slice(-25));
    process.exitCode = 1;
  } finally {
    try { ws && ws.close(); } catch (e) {}
    chrome.kill("SIGKILL");
  }
}

main();
