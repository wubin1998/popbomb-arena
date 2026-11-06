/* ============================================================
   main.js — menus, run flow, HUD binding, fixed loop
   ============================================================ */
(function () {
  "use strict";
  const BI = window.BI;
  const $ = (id) => document.getElementById(id);

  const canvas = $("game");
  const titleScreen = $("titleScreen");
  const setupScreen = $("setupScreen");
  const howScreen = $("howScreen");
  const pauseScreen = $("pauseScreen");
  const resultScreen = $("resultScreen");
  const countdownEl = $("countdown");
  const countdownNum = countdownEl.querySelector("span");
  const toastEl = $("toast");
  const cardsEl = $("cards");
  const hudEl = $("hud");
  const timerEl = $("timer");
  const timerBar = $("timerBar");
  const stageChip = $("stageChip");
  const pauseBtn = $("pauseBtn");
  const soundBtn = $("soundBtn");
  const loadingText = $("loadingText");

  const state = { game: null, run: null, ready: false, unlocked: false };
  const cardEls = {};
  let toastTimer = null;
  let lastHud = null;

  const setup = { players: 1, diff: "normal", picks: [1, 2] };

  /* ---------------- helpers ---------------- */
  function imgSrc(key, frame) { return `src/assets/img/${key}/${frame}.png`; }

  function showMenu(id) {
    [titleScreen, setupScreen, howScreen].forEach((el) => el.classList.toggle("show", el.id === id));
    pauseScreen.classList.remove("show");
    resultScreen.classList.remove("show");
  }

  function hideScreens() {
    [titleScreen, setupScreen, howScreen, pauseScreen, resultScreen].forEach((el) => el.classList.remove("show"));
  }

  function toast(msg, ms) {
    toastEl.textContent = msg;
    toastEl.classList.add("show");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toastEl.classList.remove("show"), ms || 1600);
  }

  function setSoundLabel() {
    soundBtn.textContent = BI.sound.enabled ? "音效：开" : "音效：关";
  }

  /* ---------------- title ---------------- */
  function buildTitleHeroes() {
    const row = $("heroRow");
    row.innerHTML = BI.HEROES.map((h) =>
      `<img src="${imgSrc(h.key, 4)}" alt="${h.name}">`).join("");
  }

  function unlockAudio() {
    if (state.unlocked) return;
    state.unlocked = true;
    if (state.ready && !state.game) BI.sound.music("menu");
  }
  window.addEventListener("pointerdown", unlockAudio, { once: true });
  window.addEventListener("keydown", unlockAudio, { once: true });

  $("soloBtn").addEventListener("click", () => {
    setup.players = 1;
    openSetup();
  });
  $("duoBtn").addEventListener("click", () => {
    setup.players = 2;
    if (setup.picks[1] === setup.picks[0] || !setup.picks[1]) {
      setup.picks[1] = [1, 2, 3, 4].find((id) => id !== setup.picks[0]);
    }
    openSetup();
  });
  $("howBtn").addEventListener("click", () => showMenu("howScreen"));
  $("howClose").addEventListener("click", () => showMenu("titleScreen"));
  $("setupBack").addEventListener("click", () => showMenu("titleScreen"));

  /* ---------------- setup ---------------- */
  function buildPickers() {
    [0, 1].forEach((slot) => {
      const box = $("pick" + slot);
      box.innerHTML = BI.HEROES.map((h) =>
        `<button class="pick-btn" data-id="${h.id}" title="${h.name}">
           <img src="${imgSrc(h.key, 4)}" alt="${h.name}">
           <span>${h.name}</span>
         </button>`).join("");
      box.addEventListener("click", (e) => {
        const btn = e.target.closest(".pick-btn");
        if (!btn) return;
        pickHero(slot, Number(btn.dataset.id));
      });
    });
  }

  function pickHero(slot, id) {
    if (slot === 1 && setup.players === 1) return;
    if (slot === 1 && id === setup.picks[0]) return;
    if (slot === 0 && setup.players === 2 && id === setup.picks[1]) return;
    setup.picks[slot] = id;
    updatePickers();
  }

  function updatePickers() {
    $("pickRow2").style.display = setup.players === 2 ? "" : "none";
    [0, 1].forEach((slot) => {
      const box = $("pick" + slot);
      box.querySelectorAll(".pick-btn").forEach((btn) => {
        const id = Number(btn.dataset.id);
        const sel = setup.picks[slot] === id;
        const taken = setup.players === 2 &&
          ((slot === 0 && setup.picks[1] === id) || (slot === 1 && setup.picks[0] === id));
        btn.classList.toggle("sel", sel);
        btn.classList.toggle("disabled", taken && !sel);
      });
    });

    const valid = setup.picks[0] > 0 &&
      (setup.players === 1 || (setup.picks[1] > 0 && setup.picks[1] !== setup.picks[0]));
    $("startBtn").disabled = !valid;

    const diff = BI.DIFF[setup.diff];
    const names = setup.picks.slice(0, setup.players).map((id) => BI.HEROES[id - 1].name).join(" + ");
    $("setupSummary").textContent =
      `${diff.name} · 共 ${diff.stages} 关 · ${setup.players === 1 ? "1P vs 3 AI" : "2P + 2 AI"} · ${names}`;
    $("setupSub").textContent = setup.players === 1 ? "单人挑战：选择难度与角色" : "双人同屏：为两位玩家各选一名角色";
    $("pickHint").textContent = setup.players === 2
      ? "选择两名不同角色；其余位置由 AI 补满。"
      : "选择一名角色，其余位置由 AI 补满。";
  }

  $("diffSeg").addEventListener("click", (e) => {
    const btn = e.target.closest("button[data-diff]");
    if (!btn) return;
    setup.diff = btn.dataset.diff;
    $("diffSeg").querySelectorAll("button").forEach((b) => b.classList.toggle("on", b === btn));
    updatePickers();
  });

  function openSetup() {
    updatePickers();
    showMenu("setupScreen");
  }

  $("startBtn").addEventListener("click", () => {
    if (!state.ready) return;
    startRun();
  });

  /* ---------------- run / stage ---------------- */
  function nextSpecialPair(run) {
    // AS2: pairs advance through a shuffled order, reshuffling every 3 stages.
    const cycle = (run.stage - 1) % 4;
    if (cycle === 0 || cycle === 3) {
      run.specialOrder = BI.shuffle([7, 8, 9, 10, 11, 12]);
      return run.specialOrder.slice(0, 2);
    }
    return run.specialOrder.slice(cycle * 2, cycle * 2 + 2);
  }

  function startRun() {
    state.run = {
      players: setup.players,
      diff: setup.diff,
      humanIds: setup.picks.slice(0, setup.players),
      totals: new Array(setup.players).fill(0),
      lives: new Array(setup.players).fill(1),
      stage: 1,
      totalStage: BI.DIFF[setup.diff].stages,
      specialOrder: BI.shuffle([7, 8, 9, 10, 11, 12]),
      specialPair: null,
    };
    state.run.specialPair = nextSpecialPair(state.run);
    startStage();
  }

  function startStage() {
    const run = state.run;
    if (!run) return;
    run.livesAtStage = run.lives.slice();

    const game = new BI.Game(canvas, {
      players: run.players,
      diff: run.diff,
      stage: run.stage,
      totalStage: run.totalStage,
      humanIds: run.humanIds,
      lives: run.lives,
      specialPair: run.specialPair,
      onEvent: handleGameEvent,
    });
    state.game = game;
    state.ready = true;
    window.__qpopo = state; // handle used by tools/cdp_test.js
    lastHud = null;

    hideScreens();
    hudEl.classList.remove("hidden");
    pauseBtn.textContent = "暂停";
    buildHUD(game);
    setSoundLabel();
    BI.sound.music("bgm" + (1 + ((run.stage - 1) % 4)));
    toast(`第 ${run.stage} 关 · ${game.map.scene.name}`, 1400);
  }

  function handleGameEvent(type, data) {
    if (type === "tool") {
      toast(`${data.hero.info.name} 获得「${data.def.name}」`, 1300);
    } else if (type === "go") {
      toast("GO!", 700);
    } else if (type === "stagewin") {
      BI.sound.play("stageWin", 0.7);
    } else if (type === "stagelose" || type === "timeup") {
      BI.sound.play("stageLose", 0.6);
    } else if (type === "over") {
      handleResult(data);
    }
  }

  function handleResult(result) {
    const run = state.run;
    BI.sound.stopMusic();

    const humans = result.scores.filter((s) => s.human);
    run.lives = humans.map((s) => s.lives);
    if (result.kind === "win") {
      humans.forEach((s, i) => { run.totals[i] += s.score; });
    }

    // AS2: unless it is a final game over, downed heroes return next stage
    // with one life at the cost of 1000 accumulated points.
    const gameOver = result.kind === "lose" && !humans.some((s) => s.lives > 0);
    if (!gameOver) {
      humans.forEach((s, i) => {
        if (s.lives <= 0) {
          run.lives[i] = 1;
          run.totals[i] = Math.max(0, run.totals[i] - 1000);
        }
      });
    }

    showResult(result);
  }

  /* ---------------- result screen ---------------- */
  function scoreRows(result) {
    const rows = result.scores.map((s) => {
      const tag = s.human ? (s.slot === 1 ? "P1" : "P2") : "AI";
      const detail = `砖 ${s.brickScore} · 击倒 ${s.killScore} · 道具 ${s.toolScore} · 时间 ${s.timeScore} · 奖励 ${s.bonus}`;
      return `<div class="r-row ${s.human ? "human" : ""}">
        <img src="${imgSrc(s.key, 4)}" alt="">
        <div>
          <div class="rname">${s.name} <span class="tag ${s.human ? "" : "ai"}">${tag}</span></div>
          <div class="rbreak">${detail}</div>
        </div>
        <div class="rscore">${s.score}</div>
      </div>`;
    }).join("");

    const total = state.run.totals.map((v, i) => `P${i + 1} ${v}`).join(" · ");
    return rows + `<div class="r-row"><div class="rname">累计总分</div><div class="rscore">${total}</div></div>`;
  }

  function showResult(result) {
    const badge = $("resultBadge");
    const title = $("resultTitle");
    const sub = $("resultSub");
    const actions = $("resultActions");

    badge.className = "result-badge";
    if (result.kind === "win") {
      badge.textContent = result.final ? "ALL CLEAR" : "STAGE CLEAR";
      title.textContent = result.final ? "全部通关！" : `第 ${result.stage} 关完成`;
      sub.textContent = result.final ? "你击败了所有对手，成为泡泡堂之王。" : "干得漂亮！准备进入下一关。";
    } else {
      badge.classList.add("lose");
      badge.textContent = "GAME OVER";
      title.textContent = result.retry ? "挑战失败" : "游戏结束";
      sub.textContent = result.retry ? "还有生命，再试一次吧。" : "生命耗尽，本次冒险到此结束。";
    }

    $("resultStats").innerHTML = scoreRows(result);

    let html = "";
    if (result.kind === "win" && !result.final) {
      html = `<button class="btn btn-primary" data-act="next">下一关<small>NEXT</small></button>`;
    } else if (result.kind === "win" && result.final) {
      html = `<button class="btn btn-primary" data-act="restart">再玩一次<small>NEW RUN</small></button>`;
    } else if (result.retry) {
      html = `<button class="btn btn-primary" data-act="retry">重试本关<small>RETRY</small></button>`;
    } else {
      html = `<button class="btn btn-primary" data-act="restart">重新开始<small>NEW RUN</small></button>`;
    }
    html += `<button class="btn btn-ghost" data-act="menu">返回标题</button>`;
    actions.innerHTML = html;

    resultScreen.classList.add("show");
    if (state.game) state.game.paused = true;
  }

  $("resultActions").addEventListener("click", (e) => {
    const btn = e.target.closest("button[data-act]");
    if (!btn) return;
    resultScreen.classList.remove("show");
    const act = btn.dataset.act;
    if (act === "next") {
      state.run.stage++;
      state.run.specialPair = nextSpecialPair(state.run);
      startStage();
    } else if (act === "retry") {
      startStage();
    } else if (act === "restart") {
      startRun();
    } else {
      returnToTitle();
    }
  });

  function returnToTitle() {
    state.game = null;
    state.run = null;
    BI.sound.stopMusic();
    BI.sound.music("menu");
    hudEl.classList.add("hidden");
    hideScreens();
    showMenu("titleScreen");
  }

  /* ---------------- pause ---------------- */
  function togglePause() {
    const g = state.game;
    if (!g || g.state === "over" || g.state === "ending") return;
    g.paused = !g.paused;
    pauseScreen.classList.toggle("show", g.paused);
    pauseBtn.textContent = g.paused ? "继续" : "暂停";
    if (g.paused) {
      if (BI.sound.bgm) BI.sound.bgm.pause();
    } else if (BI.sound.bgm && BI.sound.enabled) {
      BI.sound.bgm.play().catch(() => {});
    }
  }

  pauseBtn.addEventListener("click", togglePause);
  $("resumeBtn").addEventListener("click", togglePause);
  $("restartBtn").addEventListener("click", () => {
    pauseScreen.classList.remove("show");
    state.run.lives = state.run.livesAtStage.slice();
    startStage();
  });
  $("quitBtn").addEventListener("click", () => {
    pauseScreen.classList.remove("show");
    returnToTitle();
  });

  soundBtn.addEventListener("click", () => {
    BI.sound.toggle();
    setSoundLabel();
    if (!BI.sound.enabled && state.game) BI.sound.stopMusic();
    else if (BI.sound.enabled && state.game && !state.game.paused) {
      BI.sound.music("bgm" + (1 + ((state.run.stage - 1) % 4)));
    }
  });

  window.addEventListener("keydown", (e) => {
    if (e.code === "KeyM") {
      BI.sound.toggle();
      setSoundLabel();
      if (!BI.sound.enabled) BI.sound.stopMusic();
      else if (state.game && !state.game.paused) BI.sound.music("bgm" + (1 + ((state.run.stage - 1) % 4)));
    } else if (e.code === "KeyP" || e.code === "Escape") {
      if (!resultScreen.classList.contains("show")) togglePause();
    } else if ((e.code === "Enter" || e.code === "Space") && resultScreen.classList.contains("show")) {
      const primary = $("resultActions").querySelector("button[data-act]");
      if (primary) primary.click();
    }
  });

  /* ---------------- HUD ---------------- */
  function buildHUD(game) {
    const h = game.hud();
    stageChip.textContent = `第 ${h.stage} / ${h.totalStage} 关 · ${h.scene}`;
    cardsEl.innerHTML = h.slots.map((s) => {
      const tag = s.human ? (s.slot === 1 ? "P1" : "P2") : "AI";
      return `<div class="card" data-slot="${s.slot}" style="--c:${s.color}">
        <img class="avatar" src="${imgSrc(s.key, 4)}" alt="">
        <div class="card-main">
          <div class="card-top">
            <span class="cname">${s.name}</span>
            <span class="tag ${s.human ? "" : "ai"}">${tag}</span>
            <span class="cscore">0</span>
          </div>
          <div class="card-stats">
            <span>弹 <b data-f="bombs">0/1</b></span>
            <span>威 <b data-f="power">1</b></span>
            <span data-f="extra"></span>
          </div>
        </div>
        <div class="hearts" data-f="lives">♥</div>
      </div>`;
    }).join("");

    for (const s of h.slots) {
      const el = cardsEl.querySelector(`.card[data-slot="${s.slot}"]`);
      cardEls[s.slot] = {
        el,
        score: el.querySelector(".cscore"),
        bombs: el.querySelector('[data-f="bombs"]'),
        power: el.querySelector('[data-f="power"]'),
        extra: el.querySelector('[data-f="extra"]'),
        lives: el.querySelector('[data-f="lives"]'),
      };
    }
    updateHUD();
  }

  function setText(el, text) {
    if (el && el.textContent !== text) el.textContent = text;
  }

  function updateHUD() {
    const g = state.game;
    if (!g) return;
    const h = g.hud();

    setText(timerEl, BI.fmtTime(h.time));
    timerEl.classList.toggle("low", h.time <= 30);
    timerBar.style.width = Math.max(0, Math.min(100, (h.time / BI.TOTAL_TIME) * 100)) + "%";

    for (const s of h.slots) {
      const c = cardEls[s.slot];
      if (!c) continue;
      setText(c.score, String(s.score));
      setText(c.bombs, `${s.capacity - s.bombs} / ${s.capacity}`);
      setText(c.power, String(s.power));
      setText(c.lives, s.lives > 0 ? "♥".repeat(Math.min(3, s.lives)) : "—");
      c.el.classList.toggle("out", !s.alive);

      let extra = "";
      if (s.glove) extra += "抢手 ";
      if (s.shield) extra += "盾 ";
      if (s.special) extra += `<span class="special-chip">${s.special} ×${s.specialCount}</span>`;
      if (extra !== c.extra.innerHTML.trim() && c.extra.dataset.v !== extra) {
        c.extra.innerHTML = extra;
        c.extra.dataset.v = extra;
      }
    }
    lastHud = h;
  }

  /* ---------------- countdown ---------------- */
  function updateCountdown() {
    const g = state.game;
    if (g && g.state === "countdown") {
      const n = Math.min(3, Math.ceil(g.countTicks / BI.FPS));
      if (countdownNum.textContent !== String(n)) {
        countdownNum.textContent = String(n);
        countdownNum.style.animation = "none";
        void countdownNum.offsetWidth;
        countdownNum.style.animation = "";
      }
      countdownEl.classList.remove("hidden");
    } else {
      countdownEl.classList.add("hidden");
    }
  }

  /* ---------------- boot ---------------- */
  buildTitleHeroes();
  buildPickers();
  updatePickers();
  setSoundLabel();

  $("soloBtn").disabled = true;
  $("duoBtn").disabled = true;

  BI.assets.onProgress = (p) => {
    loadingText.textContent = `正在载入素材… ${Math.round(p * 100)}%`;
  };

  BI.assets.load(() => {
    state.ready = true;
    $("soloBtn").disabled = false;
    $("duoBtn").disabled = false;
    loadingText.textContent = "素材已就绪 · 点击开始";
    if (state.unlocked) BI.sound.music("menu");
  });

  window.addEventListener("resize", () => {
    if (state.game) state.game.resize();
  });

  const loop = new BI.Loop(
    () => { if (state.game) state.game.step(); },
    () => {
      if (state.game) state.game.render();
      updateHUD();
      updateCountdown();
    }
  );
  loop.start();
})();
