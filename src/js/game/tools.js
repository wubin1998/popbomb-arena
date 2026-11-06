/* ============================================================
   tools.js — 12 tools, original drop table and effect metadata
   ============================================================ */
(function () {
  "use strict";
  const BI = window.BI;

  // frame ids match DefineSprite_294 (1..12)
  BI.TOOLS = [
    { id: 1, name: "生命加1", kind: "life", desc: "生命 +1" },
    { id: 2, name: "抢手", kind: "glove", desc: "可以推动炸弹" },
    { id: 3, name: "炸弹糖", kind: "bomb", desc: "可放炸弹 +1" },
    { id: 4, name: "盾牌", kind: "shield", desc: "抵挡一次伤害" },
    { id: 5, name: "威力药", kind: "power", desc: "爆炸范围 +1" },
    { id: 6, name: "飞鞋", kind: "speed", desc: "移动速度提升" },
    { id: 7, name: "火箭", kind: "rocket", desc: "直线飞行，命中即爆" },
    { id: 8, name: "地雷", kind: "mine", desc: "敌人踩中即爆" },
    { id: 9, name: "手榴弹", kind: "grenade", desc: "向前投掷两格" },
    { id: 10, name: "汽弹", kind: "gas", desc: "令敌人减速" },
    { id: 11, name: "冰箭", kind: "ice", desc: "冻结三格" },
    { id: 12, name: "火焰箭", kind: "fire", desc: "贯穿三格火焰" },
  ];

  BI.toolById = function (id) {
    return BI.TOOLS[id - 1] || BI.TOOLS[0];
  };

  BI.isSpecial = function (def) {
    return !!def && def.id >= 7;
  };

  // Original: 65% drop, then 80% common / 20% special.
  // Common weights: life 1, glove 14, bomb 25, shield 10, power 25, speed 25.
  BI.rollTool = function (specialPair) {
    if (Math.random() >= 0.65) return null;
    if (Math.random() < 0.8) {
      const r = Math.random() * 100;
      if (r < 1) return BI.toolById(1);
      if (r < 15) return BI.toolById(2);
      if (r < 40) return BI.toolById(3);
      if (r < 50) return BI.toolById(4);
      if (r < 75) return BI.toolById(5);
      return BI.toolById(6);
    }
    const pair = specialPair && specialPair.length ? specialPair : [7, 8];
    return BI.toolById(pair[Math.floor(Math.random() * pair.length)]);
  };
})();
