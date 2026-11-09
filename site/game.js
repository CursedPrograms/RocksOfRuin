// Rocks of Ruin - an Arcade Phantasm game by Cursed Entertainment.
// Fly an enormous asteroid field. Rocks split big > medium > small > dust; some hide alien nests, and what crawls out
// hunts you. Pick up salvage and spend it in the salvage bay (U). The field turns nastier the deeper you go from the
// beacon at its heart, and the longer you last: denser, faster rocks, more nests, debris storms.
(function () {
  "use strict";
  var canvas = document.getElementById("game");
  var ctx = canvas.getContext("2d");
  var splash = document.getElementById("splash");
  var upPanel = document.getElementById("upgrades");
  var upList = document.getElementById("upList");
  var salvageLine = document.getElementById("salvageLine");

  var C = { navy: "#001725", deep: "#005184", blue: "#007ab7", sky: "#34a9e3", pink: "#f088e9",
            acid: "#39ff6a", black: "#020611", white: "#ffffff" };
  var SHOT = location.hash === "#shot";
  var TAU = Math.PI * 2;

  // a seeded random for the screenshot mode, so the README shot is the same every time
  var seed = 20261109;
  function seeded() {
    seed |= 0; seed = seed + 0x6D2B79F5 | 0;
    var t = Math.imul(seed ^ seed >>> 15, 1 | seed);
    t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
    return ((t ^ t >>> 14) >>> 0) / 4294967296;
  }
  var rnd = SHOT ? seeded : Math.random;
  function rr(a, b) { return a + rnd() * (b - a); }
  function pick(list) { return list[Math.floor(rnd() * list.length)]; }
  function dist2(a, b) { var dx = a.x - b.x, dy = a.y - b.y; return dx * dx + dy * dy; }
  function clamp(v, a, b) { return v < a ? a : v > b ? b : v; }
  function angDiff(a, b) { var d = (b - a) % TAU; if (d > Math.PI) d -= TAU; if (d < -Math.PI) d += TAU; return d; }

  var FIELD = 7000;                       // the field is a disc this big around the beacon
  var ROCK = {                            // per size: radius, hits, score, salvage chance and value
    r: [74, 42, 22, 10], hp: [4, 2, 1, 1], score: [20, 50, 100, 10],
    drop: [0.55, 0.35, 0.18, 0.04], val: [5, 3, 2, 1], hurt: [34, 20, 11, 4]
  };
  var ALIEN = {
    skitter: { r: 12, hp: 1, speed: 250, bite: 10, score: 150, salvage: 4, col: C.acid },
    spitter: { r: 16, hp: 3, speed: 170, bite: 12, score: 250, salvage: 6, col: C.pink },
    leech:   { r: 26, hp: 8, speed: 110, bite: 22, score: 400, salvage: 12, col: C.acid },
    wraith:  { r: 18, hp: 2, speed: 210, bite: 16, score: 300, salvage: 8, col: C.sky }
  };
  var UPGRADES = [
    { id: "rate",   name: "Rate of Fire",    desc: "Fire faster",                      cost: [15, 30, 50, 80, 120] },
    { id: "spread", name: "Spread Shot",     desc: "3, then 5 rounds, then a tail gun", cost: [40, 90, 160] },
    { id: "pierce", name: "Piercing Rounds", desc: "Rounds punch through targets",      cost: [35, 80, 140] },
    { id: "homing", name: "Homing",          desc: "Rounds curve onto targets",         cost: [50, 100, 170] },
    { id: "shield", name: "Shields",         desc: "A recharging shield over the hull", cost: [25, 55, 95, 150] },
    { id: "engine", name: "Thrusters",       desc: "More thrust, more top speed",       cost: [20, 50, 90] },
    { id: "repair", name: "Hull Repair",     desc: "Patch 40 hull",                     cost: [20], repeat: true }
  ];

  var W = 0, H = 0, DPR = 1, Z = 1, VW = 0, VH = 0;   // Z zooms out on small screens; VW x VH is the view in world units
  var stars = [], nebulae = [];
  var ship, rocks, aliens, bullets, spits, salvage, parts, rings, texts;
  var score, best = parseInt(localStorage.getItem("rocksOfRuinBest") || "0", 10);
  var salv, lv, elapsed, danger, state, cam = { x: 0, y: 0 }, shake, popTimer, storm, nextStorm, banner, newBest;
  var started = false, last = 0;
  var keys = {}, touchFire = false, stick = { x: 0, y: 0, on: false };

  // ---------------------------------------------------------------- setup
  function resize() {
    DPR = Math.min(window.devicePixelRatio || 1, 2);
    W = window.innerWidth; H = window.innerHeight;
    canvas.width = Math.floor(W * DPR); canvas.height = Math.floor(H * DPR);
    ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
    Z = clamp(Math.min(W, H) / 720, 0.55, 1);
    VW = W / Z; VH = H / Z;
  }

  function makeStars() {                 // three parallax layers, each a 1024 tile
    stars = [];
    for (var l = 0; l < 3; l++) {
      var layer = [];
      for (var i = 0; i < 110; i++) layer.push({ x: rnd() * 1024, y: rnd() * 1024, r: rr(0.4, 1.2 + l * 0.6), t: rnd() * TAU });
      stars.push(layer);
    }
    nebulae = [];
    for (var n = 0; n < 26; n++) {
      var a = rnd() * TAU, d = Math.sqrt(rnd()) * FIELD;
      nebulae.push({ x: Math.cos(a) * d, y: Math.sin(a) * d, r: rr(500, 1300), c: pick([C.deep, C.pink, C.blue, C.navy]) });
    }
  }

  function jagged(r) {                   // a procedural rock: a ring of points pushed in and out
    var n = Math.floor(rr(9, 15)), pts = [];
    for (var i = 0; i < n; i++) {
      var a = (i / n) * TAU + rr(-0.18, 0.18);
      var k = rnd() < 0.25 ? rr(0.55, 0.75) : rr(0.8, 1.08);
      pts.push(a, r * k);
    }
    var cracks = [];                     // a few surface lines, drawn faint
    for (var c = 0; c < 2 + n / 5; c++) cracks.push(rnd() * TAU, rr(0.15, 0.5) * r, rnd() * TAU, rr(0.2, 0.6) * r);
    return { pts: pts, cracks: cracks };
  }

  function depthAt(x, y) { return Math.sqrt(x * x + y * y) / FIELD; }   // 0 at the beacon, 1 at the edge

  function newRock(x, y, size, vx, vy, opts) {
    var r = ROCK.r[size] * rr(0.85, 1.15), shape = jagged(r);
    var rock = { x: x, y: y, vx: vx, vy: vy, r: r, size: size, a: rnd() * TAU, spin: rr(-1, 1) * (0.2 + size * 0.35),
                 pts: shape.pts, cracks: shape.cracks, iron: false, nest: null, hp: ROCK.hp[size], flash: 0, wob: rnd() * 10 };
    if (opts) { if (opts.iron) rock.iron = true; if (opts.nest) rock.nest = opts.nest; }
    if (rock.iron) rock.hp *= 2;
    rocks.push(rock);
    return rock;
  }

  function nestChance(d) { return Math.min(0.38, 0.03 + d * 0.22 + danger * 0.02); }
  function nestKind() {
    var k = ["skitter", "skitter", "spitter"];
    if (danger > 2) k.push("leech", "spitter");
    if (danger > 3) k.push("wraith", "leech");
    if (danger > 5) k.push("wraith", "wraith");
    return pick(k);
  }

  function seedRock(x, y, spd) {         // a fresh big or medium rock, maybe iron, maybe a nest
    var d = depthAt(x, y), size = rnd() < 0.7 ? 0 : 1, a = rnd() * TAU, v = rr(10, spd);
    var opts = {};
    if (rnd() < 0.12 + d * 0.15) opts.iron = true;
    if (rnd() < nestChance(d)) opts.nest = nestKind();
    return newRock(x, y, size, Math.cos(a) * v, Math.sin(a) * v, opts);
  }

  function reset() {
    ship = { x: 0, y: 0, vx: 0, vy: 0, a: -Math.PI / 2, hull: 100, shield: 0, regen: 0, inv: 2, cool: 0, thrust: false, dead: 0 };
    rocks = []; aliens = []; bullets = []; spits = []; salvage = []; parts = []; rings = []; texts = [];
    score = 0; salv = 0; elapsed = 0; danger = 1; shake = 0; popTimer = 0; newBest = false;
    lv = { rate: 0, spread: 0, pierce: 0, homing: 0, shield: 0, engine: 0, repair: 0 };
    storm = null; nextStorm = 70; banner = { text: "FIND THE SALVAGE. MIND THE ROCKS.", t: 3.5 };
    cam = { x: 0, y: 0 };
    // the field: sparse round the beacon, thicker towards the edge
    for (var i = 0; i < 1500; i++) {
      var a = rnd() * TAU, d = 700 + Math.sqrt(rnd()) * (FIELD - 700);
      if (rnd() > 0.35 + 0.65 * d / FIELD) continue;
      seedRock(Math.cos(a) * d, Math.sin(a) * d, 30 + 90 * d / FIELD);
    }
    for (var j = 0; j < 12; j++) {      // a few easy rocks round the beacon to warm up on
      var sa = j / 12 * TAU + rr(-0.2, 0.2), sd = rr(330, 800);
      newRock(Math.cos(sa) * sd, Math.sin(sa) * sd, rnd() < 0.6 ? 0 : 1, rr(-15, 15), rr(-15, 15), j === 5 ? { nest: "skitter" } : null);
    }
    state = "playing";
    closeMenu(true);
  }

  // ---------------------------------------------------------------- effects
  function burst(x, y, n, speed, cols, life, size) {
    for (var i = 0; i < n; i++) {
      var a = rnd() * TAU, v = speed * (0.2 + rnd() * 0.8), l = (life || 0.6) * (0.5 + rnd() * 0.8);
      parts.push({ x: x, y: y, vx: Math.cos(a) * v, vy: Math.sin(a) * v, life: l, max: l, c: pick(cols), s: size || 2.2 });
    }
  }
  function ring(x, y, r, c, life) { rings.push({ x: x, y: y, r: r, c: c, life: life || 0.45, max: life || 0.45 }); }
  function floatText(x, y, t, c) { texts.push({ x: x, y: y, t: t, c: c || C.white, life: 1, max: 1 }); }
  function addShake(v) {
    var d = Math.sqrt(dist2(ship, cam));
    shake = Math.min(18, shake + v);
    if (d > 1200) shake *= 0.5;
  }
  function sfx(name, arg) { if (window.Sfx && !SHOT) Sfx[name](arg); }

  // ---------------------------------------------------------------- breaking things
  function breakRock(rock, idx, bvx, bvy) {
    rocks.splice(idx, 1);
    score += ROCK.score[rock.size];
    var cols = rock.iron ? [C.blue, C.sky, C.white] : [C.sky, C.white, C.deep];
    burst(rock.x, rock.y, 10 + (3 - rock.size) * 12, 120 + (3 - rock.size) * 60, cols, 0.8, 2.5);
    ring(rock.x, rock.y, rock.r * 1.6, rock.iron ? C.blue : C.sky);
    addShake([7, 4, 2, 0.5][rock.size]);
    sfx("boom", rock.size);
    if (rock.size < 3) {
      var n = rock.size === 1 ? 2 : rock.size === 0 ? (rnd() < 0.5 ? 2 : 3) : (rnd() < 0.5 ? 2 : 3);
      for (var i = 0; i < n; i++) {
        var a = rnd() * TAU, v = rr(40, 120) * (1 + rock.size * 0.25) * (1 + danger * 0.05);
        var child = newRock(rock.x + Math.cos(a) * rock.r * 0.35, rock.y + Math.sin(a) * rock.r * 0.35, rock.size + 1,
                            rock.vx + Math.cos(a) * v + bvx * 0.05, rock.vy + Math.sin(a) * v + bvy * 0.05,
                            { iron: rock.iron });
        child.flash = 0.15;
      }
    }
    if (rnd() < ROCK.drop[rock.size] * (rock.iron ? 1.6 : 1)) dropSalvage(rock.x, rock.y, ROCK.val[rock.size] * (rock.iron ? 2 : 1));
    if (rock.nest) hatch(rock);
  }

  function hatch(rock) {                 // the rock was a nest: something crawls out
    var kind = rock.nest, n = kind === "skitter" ? 3 + Math.floor(danger / 2) : kind === "leech" ? 1 : 1 + Math.floor(danger / 3);
    n = Math.min(n, 7);
    for (var i = 0; i < n; i++) spawnAlien(kind, rock.x + rr(-15, 15), rock.y + rr(-15, 15), 0.7 + i * 0.12);
    burst(rock.x, rock.y, 50, 260, [C.acid, C.acid, C.pink, C.white], 1, 3);
    ring(rock.x, rock.y, rock.r * 2.6, C.acid, 0.7);
    sfx("crack");
    floatText(rock.x, rock.y - rock.r, "SOMETHING CRAWLS OUT", C.acid);
    addShake(6);
  }

  function spawnAlien(kind, x, y, emerge) {
    var d = ALIEN[kind];
    aliens.push({ kind: kind, x: x, y: y, vx: rr(-60, 60), vy: rr(-60, 60), r: d.r, hp: d.hp + Math.floor(danger / 4) * (kind === "leech" ? 2 : 0),
                  emerge: emerge || 0, emergeMax: emerge || 1, t: rnd() * 10, fire: rr(1, 2.2), dash: rr(2, 4), dashing: 0,
                  phase: rr(0.5, 2), phased: false, flash: 0, a: rnd() * TAU });
  }

  function killAlien(al, idx) {
    var d = ALIEN[al.kind];
    aliens.splice(idx, 1);
    score += d.score;
    floatText(al.x, al.y, "+" + d.score, d.col);
    burst(al.x, al.y, 34, 220, [d.col, C.acid, C.white], 0.9, 3);
    ring(al.x, al.y, al.r * 3, d.col);
    dropSalvage(al.x, al.y, d.salvage);
    addShake(3);
    sfx("squelch");
    if (al.kind === "leech") {           // a leech splits into skitters
      spawnAlien("skitter", al.x - 10, al.y, 0.25);
      spawnAlien("skitter", al.x + 10, al.y, 0.25);
    }
  }

  function dropSalvage(x, y, val) {
    var n = Math.max(1, Math.round(val / 3));
    for (var i = 0; i < n; i++) {
      var a = rnd() * TAU, v = rr(30, 110);
      salvage.push({ x: x, y: y, vx: Math.cos(a) * v, vy: Math.sin(a) * v, val: Math.max(1, Math.round(val / n)), life: 30, a: rnd() * TAU });
    }
  }

  function hurt(amount, fromX, fromY) {
    if (ship.inv > 0 || state !== "playing") return;
    ship.regen = 2.5;
    if (ship.shield > 0) {
      var soak = Math.min(ship.shield, amount);
      ship.shield -= soak; amount -= soak;
      ring(ship.x, ship.y, 40, C.sky, 0.35);
      sfx("shield");
    }
    if (amount > 0) {
      ship.hull -= amount;
      burst(ship.x, ship.y, 22, 200, [C.pink, C.white], 0.6, 2.5);
      addShake(8);
      sfx("hit");
    }
    ship.inv = 0.7;
    var dx = ship.x - fromX, dy = ship.y - fromY, d = Math.sqrt(dx * dx + dy * dy) || 1;
    ship.vx += dx / d * 220; ship.vy += dy / d * 220;
    if (ship.hull <= 0) die();
  }

  function die() {
    ship.hull = 0;
    state = "over";
    ship.dead = 0;
    burst(ship.x, ship.y, 120, 420, [C.pink, C.white, C.sky, C.acid], 1.6, 3.5);
    ring(ship.x, ship.y, 160, C.pink, 1);
    ring(ship.x, ship.y, 90, C.white, 0.7);
    addShake(18);
    sfx("thrust", false);
    sfx("over");
    if (score > best) { best = score; newBest = true; localStorage.setItem("rocksOfRuinBest", String(best)); }
  }

  // ---------------------------------------------------------------- shooting
  function fire() {
    var interval = 0.24 * Math.pow(0.8, lv.rate);
    ship.cool = interval;
    var angles = [0];
    if (lv.spread >= 1) angles = [-0.13, 0, 0.13];
    if (lv.spread >= 2) angles = [-0.26, -0.13, 0, 0.13, 0.26];
    if (lv.spread >= 3) angles.push(Math.PI);
    for (var i = 0; i < angles.length; i++) {
      var a = ship.a + angles[i];
      bullets.push({ x: ship.x + Math.cos(a) * 16, y: ship.y + Math.sin(a) * 16,
                     vx: Math.cos(a) * 900 + ship.vx, vy: Math.sin(a) * 900 + ship.vy,
                     life: 0.95, pierce: lv.pierce, hits: [], target: null, seek: 0 });
    }
    sfx("shoot");
  }

  function nearestTarget(b) {
    var bestT = null, bd = 520 * 520;
    for (var i = 0; i < aliens.length; i++) {
      var al = aliens[i];
      if (al.phased || al.emerge > 0) continue;
      var d = dist2(b, al) * 0.5;        // aliens first
      if (d < bd && b.hits.indexOf(al) < 0) { bd = d; bestT = al; }
    }
    for (var j = 0; j < rocks.length; j++) {
      var r = rocks[j];
      if (Math.abs(r.x - b.x) > 520 || Math.abs(r.y - b.y) > 520) continue;
      var d2 = dist2(b, r);
      if (d2 < bd && b.hits.indexOf(r) < 0) { bd = d2; bestT = r; }
    }
    return bestT;
  }

  // ---------------------------------------------------------------- the world getting worse
  function populate(dt) {
    popTimer -= dt;
    if (popTimer > 0) return;
    popTimer = 0.8;
    var near = 0;
    for (var i = 0; i < rocks.length; i++) if (Math.abs(rocks[i].x - ship.x) < 2400 && Math.abs(rocks[i].y - ship.y) < 2400) near++;
    var want = Math.min(150, 34 + danger * 11);
    for (var n = 0; n < 3 && near < want; n++, near++) {
      var a = rnd() * TAU, d = rr(Math.max(VW, VH) * 0.75 + 120, 2300);
      var x = ship.x + Math.cos(a) * d, y = ship.y + Math.sin(a) * d;
      if (x * x + y * y > FIELD * FIELD * 0.97) continue;
      seedRock(x, y, 40 + danger * 14);
    }
    // keep the count sane: drop rocks far from the ship once there are lots
    if (rocks.length > 2400) {
      for (var k = rocks.length - 1; k >= 0 && rocks.length > 2200; k--) if (dist2(rocks[k], ship) > 5000 * 5000) rocks.splice(k, 1);
    }
  }

  function updateStorm(dt) {
    if (!storm) {
      if (danger < 2.3) return;
      nextStorm -= dt;
      if (nextStorm <= 0) {
        storm = { a: rnd() * TAU, t: 16, warn: 2.5, spawn: 0 };
        banner = { text: "DEBRIS STORM INCOMING", t: 3, c: C.pink };
        sfx("storm");
      }
      return;
    }
    if (storm.warn > 0) { storm.warn -= dt; return; }
    storm.t -= dt;
    storm.spawn -= dt;
    if (storm.spawn <= 0) {             // small, fast rocks blown across the view from one side
      storm.spawn = Math.max(0.05, 0.22 - danger * 0.012);
      var back = storm.a + Math.PI, R = Math.max(VW, VH) * 0.7 + 60;
      var side = rr(-1, 1) * Math.max(VW, VH) * 0.7;
      var x = ship.x + Math.cos(back) * R + Math.cos(storm.a + Math.PI / 2) * side;
      var y = ship.y + Math.sin(back) * R + Math.sin(storm.a + Math.PI / 2) * side;
      var v = rr(380, 560) + danger * 15, aa = storm.a + rr(-0.12, 0.12);
      newRock(x, y, rnd() < 0.35 ? 2 : 3, Math.cos(aa) * v, Math.sin(aa) * v).storm = true;
    }
    if (storm.t <= 0) {
      storm = null;
      nextStorm = rr(45, 75) - Math.min(25, danger * 2);
      banner = { text: "THE STORM PASSES", t: 2, c: C.sky };
    }
  }

  // ---------------------------------------------------------------- update
  function update(dt) {
    if (state === "over") { ship.dead += dt; updateFx(dt); return; }
    if (state !== "playing") return;
    elapsed += dt;
    var deep = depthAt(ship.x, ship.y);
    danger = 1 + elapsed / 55 + deep * 5;

    // the ship: rotation, thrust and inertia
    var turn = 0, thrust = false, brake = false;
    if (keys.arrowleft || keys.a) turn -= 1;
    if (keys.arrowright || keys.d) turn += 1;
    if (keys.arrowup || keys.w) thrust = true;
    if (keys.arrowdown || keys.s) brake = true;
    if (stick.on) {
      var m = Math.sqrt(stick.x * stick.x + stick.y * stick.y);
      if (m > 0.25) {
        var want = Math.atan2(stick.y, stick.x), d = angDiff(ship.a, want);
        turn = clamp(d * 4, -1, 1);
        if (m > 0.55 && Math.abs(d) < 1.2) thrust = true;
      }
    }
    var power = 1 + lv.engine * 0.2;
    ship.a += turn * 4.4 * dt;
    if (thrust) {
      ship.vx += Math.cos(ship.a) * 430 * power * dt;
      ship.vy += Math.sin(ship.a) * 430 * power * dt;
      if (rnd() < 0.8) {
        var ba = ship.a + Math.PI + rr(-0.3, 0.3);
        parts.push({ x: ship.x - Math.cos(ship.a) * 12, y: ship.y - Math.sin(ship.a) * 12,
                     vx: Math.cos(ba) * rr(120, 260) + ship.vx, vy: Math.sin(ba) * rr(120, 260) + ship.vy,
                     life: 0.35, max: 0.35, c: pick([C.pink, C.pink, C.white, C.sky]), s: 2 });
      }
    }
    if (thrust !== ship.thrust) { ship.thrust = thrust; sfx("thrust", thrust); }
    var drag = brake ? 2.2 : 0.18;
    ship.vx -= ship.vx * drag * dt; ship.vy -= ship.vy * drag * dt;
    var sp = Math.sqrt(ship.vx * ship.vx + ship.vy * ship.vy), top = 560 * (1 + lv.engine * 0.12);
    if (sp > top) { ship.vx *= top / sp; ship.vy *= top / sp; }
    ship.x += ship.vx * dt; ship.y += ship.vy * dt;
    // the edge of the field pushes back
    var rd = Math.sqrt(ship.x * ship.x + ship.y * ship.y);
    if (rd > FIELD) {
      var nx = ship.x / rd, ny = ship.y / rd, out = ship.vx * nx + ship.vy * ny;
      if (out > 0) { ship.vx -= nx * out * 1.6; ship.vy -= ny * out * 1.6; }
      ship.x = nx * FIELD; ship.y = ny * FIELD;
    }
    ship.inv = Math.max(0, ship.inv - dt);
    ship.cool -= dt;
    if ((keys[" "] || touchFire || SHOT) && ship.cool <= 0) fire();
    var smax = lv.shield * 25;
    ship.regen -= dt;
    if (ship.regen <= 0 && ship.shield < smax) ship.shield = Math.min(smax, ship.shield + 9 * dt);

    // rocks
    for (var i = rocks.length - 1; i >= 0; i--) {
      var r = rocks[i];
      r.x += r.vx * dt; r.y += r.vy * dt; r.a += r.spin * dt; r.flash = Math.max(0, r.flash - dt);
      if (r.storm && dist2(r, ship) > 2600 * 2600) { rocks.splice(i, 1); continue; }
      var rr2 = r.x * r.x + r.y * r.y;
      if (rr2 > FIELD * FIELD * 1.1) {   // rocks drifting off the edge come back round
        if (r.storm) { rocks.splice(i, 1); continue; }
        r.vx = -r.vx; r.vy = -r.vy;
      }
      var dx = ship.x - r.x, dy = ship.y - r.y, rad = r.r * 0.85 + 11;
      if (state === "playing" && dx * dx + dy * dy < rad * rad) {
        var dd = Math.sqrt(dx * dx + dy * dy) || 1, nx2 = dx / dd, ny2 = dy / dd;
        var rel = (ship.vx - r.vx) * nx2 + (ship.vy - r.vy) * ny2;
        if (rel < 0) { ship.vx -= nx2 * rel * 1.5; ship.vy -= ny2 * rel * 1.5; }
        ship.x = r.x + nx2 * rad; ship.y = r.y + ny2 * rad;
        if (ship.inv <= 0) {
          hurt(ROCK.hurt[r.size] * (r.storm ? 1.3 : 1), r.x, r.y);
          if (r.size >= 2) breakRock(r, rocks.indexOf(r), 0, 0);
        }
      }
    }

    // bullets
    for (var b = bullets.length - 1; b >= 0; b--) {
      var bu = bullets[b];
      bu.life -= dt;
      if (lv.homing > 0) {
        bu.seek -= dt;
        if (bu.seek <= 0) { bu.seek = 0.12; bu.target = nearestTarget(bu); }
        var tg = bu.target;
        if (tg && (rocks.indexOf(tg) >= 0 || aliens.indexOf(tg) >= 0)) {
          var cur = Math.atan2(bu.vy, bu.vx), want2 = Math.atan2(tg.y - bu.y, tg.x - bu.x);
          var na = cur + clamp(angDiff(cur, want2), -1, 1) * (2.2 + lv.homing * 2.2) * dt;
          var bs = Math.sqrt(bu.vx * bu.vx + bu.vy * bu.vy);
          bu.vx = Math.cos(na) * bs; bu.vy = Math.sin(na) * bs;
        }
      }
      bu.x += bu.vx * dt; bu.y += bu.vy * dt;
      var gone = bu.life <= 0;
      for (var a = aliens.length - 1; a >= 0 && !gone; a--) {
        var al = aliens[a];
        if (al.phased || bu.hits.indexOf(al) >= 0) continue;
        if (dist2(bu, al) < (al.r + 4) * (al.r + 4)) {
          al.hp--; al.flash = 0.1; bu.hits.push(al);
          burst(bu.x, bu.y, 6, 140, [ALIEN[al.kind].col, C.white], 0.3, 2);
          if (al.hp <= 0) killAlien(al, a);
          if (bu.pierce-- <= 0) gone = true;
        }
      }
      for (var k = rocks.length - 1; k >= 0 && !gone; k--) {
        var ro = rocks[k];
        if (Math.abs(ro.x - bu.x) > ro.r + 4 || Math.abs(ro.y - bu.y) > ro.r + 4 || bu.hits.indexOf(ro) >= 0) continue;
        if (dist2(bu, ro) < (ro.r * 0.9 + 3) * (ro.r * 0.9 + 3)) {
          ro.hp--; ro.flash = 0.08; bu.hits.push(ro);
          burst(bu.x, bu.y, 4, 120, [ro.iron ? C.blue : C.sky, C.white], 0.3, 1.8);
          if (ro.hp <= 0) breakRock(ro, k, bu.vx, bu.vy);
          if (bu.pierce-- <= 0) gone = true;
        }
      }
      if (gone) bullets.splice(b, 1);
    }

    updateAliens(dt);

    // alien spit
    for (var s = spits.length - 1; s >= 0; s--) {
      var sp2 = spits[s];
      sp2.x += sp2.vx * dt; sp2.y += sp2.vy * dt; sp2.life -= dt;
      if (dist2(sp2, ship) < 15 * 15) { hurt(10, sp2.x, sp2.y); burst(sp2.x, sp2.y, 10, 120, [C.pink, C.acid], 0.4); spits.splice(s, 1); continue; }
      if (sp2.life <= 0) spits.splice(s, 1);
    }

    // salvage: drifts, slows, and is pulled in when close
    for (var g = salvage.length - 1; g >= 0; g--) {
      var sv = salvage[g];
      sv.life -= dt; sv.a += dt * 3;
      var gx = ship.x - sv.x, gy = ship.y - sv.y, gd = Math.sqrt(gx * gx + gy * gy) || 1;
      if (gd < 170) { sv.vx += gx / gd * 1600 * dt; sv.vy += gy / gd * 1600 * dt; }
      sv.vx -= sv.vx * 1.5 * dt; sv.vy -= sv.vy * 1.5 * dt;
      sv.x += sv.vx * dt; sv.y += sv.vy * dt;
      if (gd < 20) {
        salv += sv.val; score += sv.val * 5;
        floatText(sv.x, sv.y, "+" + sv.val, C.acid);
        sfx("pick");
        salvage.splice(g, 1);
        continue;
      }
      if (sv.life <= 0) salvage.splice(g, 1);
    }

    populate(dt);
    updateStorm(dt);
    updateFx(dt);
    if (banner && banner.t > 0) banner.t -= dt;
  }

  function updateAliens(dt) {
    var spd = 1 + danger * 0.05;
    for (var i = aliens.length - 1; i >= 0; i--) {
      var al = aliens[i], d = ALIEN[al.kind];
      al.t += dt; al.flash = Math.max(0, al.flash - dt);
      if (al.emerge > 0) { al.emerge -= dt; al.x += al.vx * dt * 0.3; al.y += al.vy * dt * 0.3; continue; }
      var dx = ship.x - al.x, dy = ship.y - al.y, dd = Math.sqrt(dx * dx + dy * dy) || 1, ux = dx / dd, uy = dy / dd;
      var tx = 0, ty = 0, steer = 3;
      if (state !== "playing") { ux = -ux; uy = -uy; }
      if (al.kind === "skitter") {       // runs straight at you, zig-zagging
        var w = Math.sin(al.t * 9) * 90;
        tx = ux * d.speed * spd - uy * w; ty = uy * d.speed * spd + ux * w;
      } else if (al.kind === "spitter") { // keeps its distance, circles, spits
        var k = dd > 380 ? 1 : dd < 250 ? -1 : 0;
        tx = ux * d.speed * spd * k - uy * 110; ty = uy * d.speed * spd * k + ux * 110;
        al.fire -= dt;
        if (al.fire <= 0 && dd < 700 && state === "playing") {
          al.fire = Math.max(0.8, 2 - danger * 0.08);
          var lead = dd / 340, px = ship.x + ship.vx * lead * 0.6, py = ship.y + ship.vy * lead * 0.6;
          var la = Math.atan2(py - al.y, px - al.x);
          spits.push({ x: al.x, y: al.y, vx: Math.cos(la) * 340, vy: Math.sin(la) * 340, life: 2.4 });
          sfx("spit");
        }
      } else if (al.kind === "leech") {  // lumbers after you, then lunges
        al.dash -= dt;
        if (al.dashing > 0) {
          al.dashing -= dt; steer = 0.3;
          if (al.dashing <= 0) al.dash = rr(2.2, 3.6);
        } else if (al.dash <= 0 && dd < 600) {
          al.dashing = 0.65; al.vx = ux * 520 * spd; al.vy = uy * 520 * spd;
          burst(al.x, al.y, 10, 100, [C.acid], 0.4);
        }
        tx = ux * d.speed * spd; ty = uy * d.speed * spd;
        if (al.dash < 0.5 && al.dashing <= 0) { tx *= 0.1; ty *= 0.1; }   // the tell: it stops and swells before a lunge
      } else if (al.kind === "wraith") { // fades out of reach, circles, then comes back solid and dashes
        al.phase -= dt;
        if (al.phase <= 0) {
          al.phased = !al.phased;
          al.phase = al.phased ? rr(1.6, 2.4) : rr(1.2, 1.8);
          if (!al.phased) { al.vx = ux * 460 * spd; al.vy = uy * 460 * spd; ring(al.x, al.y, 50, C.sky, 0.3); }
        }
        if (al.phased) { tx = (ux * 0.6 - uy) * d.speed * spd * 1.4; ty = (uy * 0.6 + ux) * d.speed * spd * 1.4; }
        else { tx = ux * d.speed * spd; ty = uy * d.speed * spd; steer = 0.8; }
      }
      al.vx += (tx - al.vx) * Math.min(1, steer * dt); al.vy += (ty - al.vy) * Math.min(1, steer * dt);
      al.x += al.vx * dt; al.y += al.vy * dt;
      al.a = Math.atan2(al.vy, al.vx);
      if (!al.phased && state === "playing" && dd < al.r + 11) {
        hurt(d.bite, al.x, al.y);
        al.vx = -ux * 300; al.vy = -uy * 300;
        if (al.kind === "leech") al.dashing = 0;
      }
      if (dd > 4200) aliens.splice(i, 1); // lost track of you
    }
  }

  function updateFx(dt) {
    for (var i = parts.length - 1; i >= 0; i--) {
      var p = parts[i];
      p.life -= dt;
      if (p.life <= 0) { parts.splice(i, 1); continue; }
      p.x += p.vx * dt; p.y += p.vy * dt; p.vx *= 1 - 1.8 * dt; p.vy *= 1 - 1.8 * dt;
    }
    if (parts.length > 1600) parts.splice(0, parts.length - 1600);
    for (var j = rings.length - 1; j >= 0; j--) { rings[j].life -= dt; if (rings[j].life <= 0) rings.splice(j, 1); }
    for (var k = texts.length - 1; k >= 0; k--) { texts[k].life -= dt; texts[k].y -= 30 * dt; if (texts[k].life <= 0) texts.splice(k, 1); }
    shake = Math.max(0, shake - 30 * dt);
    var lx = ship.x + ship.vx * 0.3, ly = ship.y + ship.vy * 0.3;
    cam.x += (lx - cam.x) * Math.min(1, 5 * dt); cam.y += (ly - cam.y) * Math.min(1, 5 * dt);
  }

  // ---------------------------------------------------------------- drawing
  function glow(col, blur) { ctx.shadowColor = col; ctx.shadowBlur = blur; }
  function noGlow() { ctx.shadowBlur = 0; }

  function drawBack(now) {
    ctx.fillStyle = C.black;
    ctx.fillRect(0, 0, W, H);
    if (!started) { cam.x += 0.25; cam.y += 0.08; }
    // nebulae, far behind: soft clouds of the Cursed blues and pink
    for (var n = 0; n < nebulae.length; n++) {
      var nb = nebulae[n], sx = (nb.x - cam.x) * 0.35 + W / 2, sy = (nb.y - cam.y) * 0.35 + H / 2, r = nb.r * 0.6;
      if (sx < -r || sx > W + r || sy < -r || sy > H + r) continue;
      var gr = ctx.createRadialGradient(sx, sy, 0, sx, sy, r);
      gr.addColorStop(0, nb.c === C.pink ? "rgba(240,136,233,0.10)" : nb.c === C.navy ? "rgba(0,23,37,0.5)" : "rgba(0,122,183,0.13)");
      gr.addColorStop(1, "rgba(2,6,17,0)");
      ctx.fillStyle = gr;
      ctx.fillRect(sx - r, sy - r, r * 2, r * 2);
    }
    for (var l = 0; l < stars.length; l++) {
      var f = 0.1 + l * 0.22, ox = -cam.x * f, oy = -cam.y * f;
      ctx.fillStyle = l === 2 ? C.white : l === 1 ? "#a9d8f0" : "#6f8fa8";
      for (var i = 0; i < stars[l].length; i++) {
        var s = stars[l][i];
        var x = ((s.x + ox) % 1024 + 1024) % 1024, y = ((s.y + oy) % 1024 + 1024) % 1024;
        ctx.globalAlpha = 0.45 + 0.55 * Math.abs(Math.sin(s.t + now / 1400));
        for (var tx = x; tx < W; tx += 1024) for (var ty = y; ty < H; ty += 1024) ctx.fillRect(tx, ty, s.r, s.r);
      }
    }
    ctx.globalAlpha = 1;
  }

  function onScreen(x, y, r) {
    return x + r > cam.x - VW / 2 - 40 && x - r < cam.x + VW / 2 + 40 && y + r > cam.y - VH / 2 - 40 && y - r < cam.y + VH / 2 + 40;
  }

  function drawRock(r, now) {
    ctx.save();
    ctx.translate(r.x, r.y);
    if (r.nest) { var tw = Math.sin(now / 90 + r.wob) * (Math.sin(now / 900 + r.wob) > 0.7 ? 2 : 0); ctx.translate(tw, 0); }
    ctx.rotate(r.a);
    ctx.beginPath();
    for (var i = 0; i < r.pts.length; i += 2) {
      var x = Math.cos(r.pts[i]) * r.pts[i + 1], y = Math.sin(r.pts[i]) * r.pts[i + 1];
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.closePath();
    ctx.fillStyle = r.iron ? "#001a2e" : C.navy;
    ctx.fill();
    var col = r.flash > 0 ? C.white : r.iron ? C.blue : r.storm ? C.pink : C.sky;
    glow(col, r.size === 3 ? 6 : 14);
    ctx.strokeStyle = col;
    ctx.lineWidth = r.size === 0 ? 2.4 : r.size === 3 ? 1.4 : 2;
    ctx.stroke();
    noGlow();
    ctx.globalAlpha = 0.35;
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (var c = 0; c < r.cracks.length; c += 4) {
      ctx.moveTo(Math.cos(r.cracks[c]) * r.cracks[c + 1], Math.sin(r.cracks[c]) * r.cracks[c + 1]);
      ctx.lineTo(Math.cos(r.cracks[c + 2]) * r.cracks[c + 3], Math.sin(r.cracks[c + 2]) * r.cracks[c + 3]);
    }
    ctx.stroke();
    ctx.globalAlpha = 1;
    if (r.nest) {                        // a nest: acid veins pulsing faintly under the surface
      var p = 0.25 + 0.35 * Math.max(0, Math.sin(now / 420 + r.wob));
      ctx.globalAlpha = p;
      glow(C.acid, 10);
      ctx.strokeStyle = C.acid;
      ctx.lineWidth = 1.3;
      ctx.beginPath();
      for (var v = 0; v < r.cracks.length; v += 4) {
        ctx.moveTo(0, 0);
        ctx.quadraticCurveTo(Math.cos(r.cracks[v]) * r.cracks[v + 1], Math.sin(r.cracks[v]) * r.cracks[v + 1],
                             Math.cos(r.cracks[v + 2]) * r.cracks[v + 3], Math.sin(r.cracks[v + 2]) * r.cracks[v + 3]);
      }
      ctx.stroke();
      ctx.fillStyle = C.acid;
      ctx.beginPath(); ctx.arc(0, 0, r.r * 0.09, 0, TAU); ctx.fill();
      noGlow();
      ctx.globalAlpha = 1;
    }
    ctx.restore();
  }

  function drawAlien(al, now) {
    var d = ALIEN[al.kind], col = al.flash > 0 ? C.white : d.col;
    var k = al.emerge > 0 ? 1 - al.emerge / al.emergeMax : 1;
    if (k <= 0) return;
    ctx.save();
    ctx.translate(al.x, al.y);
    ctx.globalAlpha = al.phased ? 0.18 + 0.1 * Math.sin(now / 80) : 1;
    ctx.scale(k, k);
    glow(col, 16);
    ctx.strokeStyle = col; ctx.fillStyle = col; ctx.lineWidth = 2;
    var t = al.t, r = al.r;
    if (al.kind === "skitter") {         // a tick: a body and six twitching legs
      ctx.rotate(al.a);
      ctx.beginPath();
      for (var i = 0; i < 6; i++) {
        var side = i < 3 ? 1 : -1, j = i % 3, ba = (j - 1) * 0.7, tw = Math.sin(t * 22 + i) * 0.35;
        ctx.moveTo(Math.cos(ba) * r * 0.4, side * r * 0.4);
        ctx.lineTo(Math.cos(ba + tw) * r * 1.3, side * (r * 1.1 + Math.sin(ba + tw) * 4));
      }
      ctx.stroke();
      ctx.beginPath(); ctx.ellipse(0, 0, r * 0.85, r * 0.6, 0, 0, TAU); ctx.fillStyle = C.black; ctx.fill(); ctx.stroke();
      ctx.fillStyle = C.pink; ctx.beginPath(); ctx.arc(r * 0.5, -3, 2, 0, TAU); ctx.arc(r * 0.5, 3, 2, 0, TAU); ctx.fill();
    } else if (al.kind === "spitter") {  // a floating eye with a frill
      ctx.beginPath();
      for (var f = 0; f < 14; f++) {
        var fa = f / 14 * TAU + t * 0.6, fr = r * (1.25 + 0.2 * Math.sin(t * 6 + f));
        ctx.moveTo(Math.cos(fa) * r * 0.9, Math.sin(fa) * r * 0.9); ctx.lineTo(Math.cos(fa) * fr, Math.sin(fa) * fr);
      }
      ctx.stroke();
      ctx.beginPath(); ctx.arc(0, 0, r, 0, TAU); ctx.fillStyle = C.black; ctx.fill(); ctx.stroke();
      var ea = Math.atan2(ship.y - al.y, ship.x - al.x);
      ctx.fillStyle = C.white; ctx.beginPath(); ctx.arc(Math.cos(ea) * r * 0.35, Math.sin(ea) * r * 0.35, r * 0.45, 0, TAU); ctx.fill();
      ctx.fillStyle = C.black; ctx.beginPath(); ctx.ellipse(Math.cos(ea) * r * 0.5, Math.sin(ea) * r * 0.5, r * 0.1, r * 0.3, ea, 0, TAU); ctx.fill();
    } else if (al.kind === "leech") {    // a fat, ribbed slug with tendrils
      ctx.rotate(al.a);
      var swell = al.dash < 0.5 && al.dashing <= 0 ? 1.18 : 1;
      ctx.beginPath();
      for (var tn = 0; tn < 5; tn++) {
        var ty = (tn - 2) * r * 0.3;
        ctx.moveTo(-r * 0.8, ty);
        ctx.quadraticCurveTo(-r * 1.4, ty + Math.sin(t * 5 + tn) * 10, -r * 1.9, ty + Math.sin(t * 4 + tn * 2) * 14);
      }
      ctx.stroke();
      ctx.beginPath(); ctx.ellipse(0, 0, r * swell, r * 0.7 * swell, 0, 0, TAU); ctx.fillStyle = C.black; ctx.fill(); ctx.stroke();
      ctx.beginPath();
      for (var rb = -2; rb <= 2; rb++) { ctx.moveTo(rb * r * 0.3, -r * 0.6 * swell); ctx.quadraticCurveTo(rb * r * 0.3 + 5, 0, rb * r * 0.3, r * 0.6 * swell); }
      ctx.globalAlpha *= 0.6; ctx.stroke(); ctx.globalAlpha = al.phased ? 0.2 : 1;
      ctx.fillStyle = C.pink; ctx.beginPath(); ctx.arc(r * 0.85, 0, r * 0.28, 0, TAU); ctx.fill();
      ctx.fillStyle = C.black; ctx.beginPath(); ctx.arc(r * 0.9, 0, r * 0.14, 0, TAU); ctx.fill();
    } else {                             // the wraith: a hooded shape trailing ribbons
      ctx.rotate(al.a + Math.PI / 2);
      ctx.beginPath();
      ctx.moveTo(0, -r * 1.2);
      ctx.quadraticCurveTo(r * 1.1, -r * 0.4, r * 0.8, r * 0.9);
      for (var wv = 0; wv < 5; wv++) ctx.lineTo(r * (0.6 - wv * 0.35), r * (1.2 + Math.sin(t * 10 + wv) * 0.35 + (wv % 2) * 0.4));
      ctx.quadraticCurveTo(-r * 1.1, -r * 0.4, 0, -r * 1.2);
      ctx.fillStyle = C.black; ctx.fill(); ctx.stroke();
      ctx.fillStyle = C.white;
      ctx.beginPath(); ctx.arc(-r * 0.3, -r * 0.35, 2.5, 0, TAU); ctx.arc(r * 0.3, -r * 0.35, 2.5, 0, TAU); ctx.fill();
    }
    noGlow();
    ctx.restore();
  }

  function drawShip(now) {
    if (state === "over") return;
    ctx.save();
    ctx.translate(ship.x, ship.y);
    if (lv.shield > 0 && ship.shield > 0.5) {
      ctx.globalAlpha = 0.25 + 0.5 * ship.shield / (lv.shield * 25);
      glow(C.sky, 14); ctx.strokeStyle = C.sky; ctx.lineWidth = 1.5;
      ctx.beginPath(); ctx.arc(0, 0, 24 + Math.sin(now / 150) * 1.5, 0, TAU); ctx.stroke();
      ctx.globalAlpha = 1;
    }
    ctx.rotate(ship.a);
    if (ship.inv > 0 && Math.floor(now / 70) % 2) ctx.globalAlpha = 0.4;
    if (ship.thrust) {
      glow(C.pink, 20); ctx.fillStyle = C.pink;
      ctx.beginPath(); ctx.moveTo(-9, -5); ctx.lineTo(-18 - Math.random() * 12, 0); ctx.lineTo(-9, 5); ctx.fill();
    }
    glow(C.pink, 18);
    ctx.beginPath();
    ctx.moveTo(17, 0); ctx.lineTo(-11, -11); ctx.lineTo(-6, 0); ctx.lineTo(-11, 11); ctx.closePath();
    ctx.fillStyle = C.black; ctx.fill();
    ctx.strokeStyle = C.pink; ctx.lineWidth = 2.4; ctx.stroke();
    ctx.fillStyle = C.white; ctx.beginPath(); ctx.arc(4, 0, 2.5, 0, TAU); ctx.fill();
    noGlow();
    ctx.restore();
    ctx.globalAlpha = 1;
  }

  function drawWorld(now) {
    var sx = shake ? (Math.random() - 0.5) * shake : 0, sy = shake ? (Math.random() - 0.5) * shake : 0;
    ctx.save();
    ctx.translate(Math.round(W / 2 + sx), Math.round(H / 2 + sy));
    ctx.scale(Z, Z);
    ctx.translate(-cam.x, -cam.y);

    // the beacon at the heart of the field, and the edge of the field
    if (onScreen(0, 0, 200)) {
      var bp = 0.5 + 0.5 * Math.sin(now / 500);
      glow(C.acid, 20); ctx.strokeStyle = C.acid; ctx.lineWidth = 2;
      ctx.beginPath(); ctx.arc(0, 0, 30 + bp * 6, 0, TAU); ctx.stroke();
      ctx.globalAlpha = 0.3; ctx.beginPath(); ctx.arc(0, 0, 120 + bp * 40, 0, TAU); ctx.stroke(); ctx.globalAlpha = 1;
      noGlow();
    }
    var rd = Math.sqrt(cam.x * cam.x + cam.y * cam.y);
    if (rd > FIELD - Math.max(VW, VH)) {
      glow(C.pink, 16); ctx.strokeStyle = C.pink; ctx.lineWidth = 3; ctx.setLineDash([24, 18]);
      ctx.beginPath(); ctx.arc(0, 0, FIELD, 0, TAU); ctx.stroke(); ctx.setLineDash([]); noGlow();
    }

    for (var g = 0; g < salvage.length; g++) {
      var sv = salvage[g];
      if (!onScreen(sv.x, sv.y, 10)) continue;
      ctx.save(); ctx.translate(sv.x, sv.y); ctx.rotate(sv.a);
      ctx.globalAlpha = sv.life < 4 ? (Math.floor(now / 120) % 2 ? 0.3 : 1) : 1;
      glow(C.acid, 12); ctx.fillStyle = C.acid;
      ctx.beginPath(); ctx.moveTo(0, -6); ctx.lineTo(5, 0); ctx.lineTo(0, 6); ctx.lineTo(-5, 0); ctx.fill();
      ctx.restore();
    }
    noGlow();
    for (var i = 0; i < rocks.length; i++) if (onScreen(rocks[i].x, rocks[i].y, rocks[i].r)) drawRock(rocks[i], now);
    for (var a = 0; a < aliens.length; a++) if (onScreen(aliens[a].x, aliens[a].y, 60)) drawAlien(aliens[a], now);

    glow(C.pink, 12); ctx.fillStyle = C.pink;
    for (var s = 0; s < spits.length; s++) {
      ctx.beginPath(); ctx.arc(spits[s].x, spits[s].y, 5 + Math.sin(now / 50 + s) * 1.5, 0, TAU); ctx.fill();
    }
    glow(C.white, 10); ctx.strokeStyle = C.white; ctx.lineWidth = 2.5; ctx.lineCap = "round";
    ctx.beginPath();
    for (var b = 0; b < bullets.length; b++) {
      var bu = bullets[b], bl = Math.sqrt(bu.vx * bu.vx + bu.vy * bu.vy) || 1;
      ctx.moveTo(bu.x, bu.y); ctx.lineTo(bu.x - bu.vx / bl * 12, bu.y - bu.vy / bl * 12);
    }
    ctx.stroke();
    if (lv.pierce > 0 || lv.homing > 0) {
      ctx.strokeStyle = lv.homing > 0 ? C.acid : C.sky; ctx.lineWidth = 1; glow(ctx.strokeStyle, 8);
      ctx.beginPath();
      for (var b2 = 0; b2 < bullets.length; b2++) { var u = bullets[b2]; ctx.moveTo(u.x + 4, u.y); ctx.arc(u.x, u.y, 4, 0, TAU); }
      ctx.stroke();
    }
    noGlow();
    drawShip(now);

    ctx.globalCompositeOperation = "lighter";
    for (var p = 0; p < parts.length; p++) {
      var pt = parts[p], k = pt.life / pt.max;
      ctx.globalAlpha = k; ctx.fillStyle = pt.c;
      ctx.fillRect(pt.x - pt.s * k, pt.y - pt.s * k, pt.s * 2 * k + 0.5, pt.s * 2 * k + 0.5);
    }
    ctx.globalCompositeOperation = "source-over";
    for (var rg = 0; rg < rings.length; rg++) {
      var ri = rings[rg], kk = ri.life / ri.max;
      ctx.globalAlpha = kk; glow(ri.c, 14); ctx.strokeStyle = ri.c; ctx.lineWidth = 3 * kk + 0.5;
      ctx.beginPath(); ctx.arc(ri.x, ri.y, ri.r * (1 - kk * 0.8), 0, TAU); ctx.stroke();
    }
    noGlow();
    ctx.font = "bold 14px Arial, sans-serif"; ctx.textAlign = "center";
    for (var t = 0; t < texts.length; t++) {
      ctx.globalAlpha = Math.min(1, texts[t].life * 2); ctx.fillStyle = texts[t].c;
      ctx.fillText(texts[t].t, texts[t].x, texts[t].y);
    }
    ctx.globalAlpha = 1;
    ctx.restore();

    if (storm && storm.warn <= 0) {      // storm streaks across the screen
      ctx.save();
      ctx.strokeStyle = C.pink; ctx.lineWidth = 1;
      ctx.globalAlpha = 0.18;
      var ca = Math.cos(storm.a), sa = Math.sin(storm.a);
      ctx.beginPath();
      for (var st = 0; st < 40; st++) {
        var px = ((st * 397 + now * 1.1 * ca) % W + W) % W, py = ((st * 211 + now * 1.1 * sa) % H + H) % H;
        ctx.moveTo(px, py); ctx.lineTo(px - ca * 60, py - sa * 60);
      }
      ctx.stroke();
      ctx.restore();
    }
  }

  function bar(x, y, w, h, k, col, label) {
    ctx.strokeStyle = col; ctx.lineWidth = 1.5; ctx.strokeRect(x, y, w, h);
    glow(col, 10); ctx.fillStyle = col; ctx.fillRect(x + 2, y + 2, Math.max(0, (w - 4) * k), h - 4); noGlow();
    ctx.fillStyle = C.white; ctx.font = "bold 11px Arial, sans-serif"; ctx.textAlign = "left";
    ctx.fillText(label, x, y - 5);
  }

  function drawHud(now) {
    var small = W < 600;
    ctx.textAlign = "left"; ctx.font = "bold " + (small ? 16 : 22) + "px Arial, sans-serif";
    glow(C.pink, 10); ctx.fillStyle = C.pink;
    ctx.fillText("SCORE " + score, 14, 30);
    noGlow();
    ctx.font = (small ? 12 : 14) + "px Arial, sans-serif"; ctx.fillStyle = "#c2c2c2";
    ctx.fillText("BEST " + Math.max(best, score), 14, 50);
    glow(C.acid, 10); ctx.fillStyle = C.acid; ctx.font = "bold " + (small ? 14 : 18) + "px Arial, sans-serif";
    ctx.fillText("SALVAGE " + salv + (small ? "" : "   U: SALVAGE BAY"), 14, 74);
    noGlow();
    ctx.textAlign = "center"; ctx.font = "bold " + (small ? 14 : 18) + "px Arial, sans-serif";
    ctx.fillStyle = danger > 5 ? C.pink : C.sky;
    ctx.fillText("DEPTH " + danger.toFixed(1), W / 2, 28);
    if (!small) {
      ctx.textAlign = "right"; ctx.font = "13px Arial, sans-serif"; ctx.fillStyle = "#c2c2c2";
      ctx.fillText("Arrows/WASD fly · Space fire · U upgrades · P pause · M " + (window.Sfx && Sfx.isMuted() ? "unmute" : "mute"), W - 14, 24);
    }
    // hull and shield
    var bw = small ? 120 : 180, by = H - (small ? 190 : 46);
    bar(14, by, bw, 12, ship.hull / 100, C.pink, "HULL");
    if (lv.shield > 0) bar(24 + bw, by, bw * 0.7, 12, ship.shield / (lv.shield * 25), C.sky, "SHIELD");
    // the minimap
    var ms = small ? 110 : 170, mx = W - ms - 14, my = small ? H - ms - 200 : H - ms - 14, mr = ms / 2, sc = mr / FIELD;
    var cx = mx + mr, cy = my + mr;
    ctx.fillStyle = "rgba(0,23,37,0.75)";
    ctx.beginPath(); ctx.arc(cx, cy, mr, 0, TAU); ctx.fill();
    ctx.strokeStyle = C.deep; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.arc(cx, cy, mr * 0.33, 0, TAU); ctx.arc(cx + mr * 0.66, cy, 0, 0, 0); ctx.stroke();
    ctx.beginPath(); ctx.arc(cx, cy, mr * 0.66, 0, TAU); ctx.stroke();
    ctx.strokeStyle = C.pink; ctx.beginPath(); ctx.arc(cx, cy, mr, 0, TAU); ctx.stroke();
    ctx.fillStyle = "#6f8fa8";
    for (var i = 0; i < rocks.length; i++) {
      var r = rocks[i]; if (r.size > 1) continue;
      ctx.fillRect(cx + r.x * sc - 0.6, cy + r.y * sc - 0.6, r.size ? 1 : 1.6, r.size ? 1 : 1.6);
    }
    ctx.fillStyle = C.acid;
    for (var a = 0; a < aliens.length; a++) ctx.fillRect(cx + aliens[a].x * sc - 1.5, cy + aliens[a].y * sc - 1.5, 3, 3);
    ctx.fillRect(cx - 1.5, cy - 1.5, 3, 3);
    // the view, and you
    ctx.strokeStyle = C.sky; ctx.globalAlpha = 0.6;
    ctx.strokeRect(cx + (cam.x - VW / 2) * sc, cy + (cam.y - VH / 2) * sc, VW * sc, VH * sc);
    ctx.globalAlpha = 1;
    ctx.save(); ctx.translate(cx + ship.x * sc, cy + ship.y * sc); ctx.rotate(ship.a);
    glow(C.pink, 8); ctx.fillStyle = C.pink;
    ctx.beginPath(); ctx.moveTo(6, 0); ctx.lineTo(-4, -4); ctx.lineTo(-4, 4); ctx.fill(); noGlow();
    ctx.restore();

    // threat arrows: aliens hunting you from off screen
    for (var e = 0; e < aliens.length; e++) {
      var al = aliens[e], dx = al.x - cam.x, dy = al.y - cam.y;
      if (Math.abs(dx) < VW / 2 && Math.abs(dy) < VH / 2) continue;
      var ang = Math.atan2(dy, dx), ex = W / 2 + Math.cos(ang) * (W / 2 - 30), ey = H / 2 + Math.sin(ang) * (H / 2 - 30);
      ex = clamp(ex, 20, W - 20); ey = clamp(ey, 90, H - 20);
      ctx.save(); ctx.translate(ex, ey); ctx.rotate(ang);
      ctx.fillStyle = ALIEN[al.kind].col; ctx.globalAlpha = 0.5 + 0.5 * Math.sin(now / 120);
      ctx.beginPath(); ctx.moveTo(10, 0); ctx.lineTo(-6, -7); ctx.lineTo(-6, 7); ctx.fill();
      ctx.restore();
    }
    ctx.globalAlpha = 1;

    if (banner && banner.t > 0) {
      ctx.globalAlpha = Math.min(1, banner.t);
      ctx.textAlign = "center"; ctx.font = "bold " + Math.min(34, W / 16) + "px 'Arial Black', Arial, sans-serif";
      var bwid = ctx.measureText(banner.text).width;
      if (bwid > W - 24) ctx.font = "bold " + Math.floor(Math.min(34, W / 16) * (W - 24) / bwid) + "px 'Arial Black', Arial, sans-serif";
      glow(banner.c || C.sky, 18); ctx.fillStyle = banner.c || C.sky;
      ctx.fillText(banner.text, W / 2, H * 0.28);
      noGlow(); ctx.globalAlpha = 1;
    }
  }

  function drawOverlay() {
    if (state !== "paused" && !(state === "over" && ship.dead > 1)) return;
    ctx.fillStyle = "rgba(2, 6, 17, 0.6)";
    ctx.fillRect(0, 0, W, H);
    ctx.textAlign = "center";
    glow(C.pink, 24); ctx.fillStyle = C.pink;
    ctx.font = "bold " + Math.min(72, W / 14) + "px 'Arial Black', Arial, sans-serif";
    ctx.fillText(state === "over" ? "LOST IN THE ROCKS" : "PAUSED", W / 2, H / 2 - 20);
    noGlow();
    ctx.fillStyle = C.white; ctx.font = (W < 600 ? 14 : 20) + "px Arial, sans-serif";
    if (state === "over") {
      ctx.fillText("Score " + score + "  ·  Depth " + danger.toFixed(1) + (newBest ? "  ·  NEW BEST!" : "  ·  Best " + best), W / 2, H / 2 + 24);
      ctx.fillStyle = C.sky; ctx.fillText("Space, R or tap to fly again", W / 2, H / 2 + 56);
    } else {
      ctx.fillText("P to carry on", W / 2, H / 2 + 24);
    }
  }

  function draw(now) {
    drawBack(now);
    if (!started || !ship) return;
    drawWorld(now);
    drawHud(now);
    drawOverlay();
  }

  // ---------------------------------------------------------------- the salvage bay
  function costOf(u) { var l = lv[u.id]; return u.repeat ? u.cost[0] : l < u.cost.length ? u.cost[l] : null; }

  function buildMenu() {
    salvageLine.textContent = "Salvage " + salv;
    upList.innerHTML = "";
    UPGRADES.forEach(function (u, i) {
      var li = document.createElement("li"), btn = document.createElement("button"), cost = costOf(u);
      btn.type = "button";
      var pips = "";
      if (!u.repeat) for (var p = 0; p < u.cost.length; p++) pips += p < lv[u.id] ? "◆" : "◇";
      var maxed = cost === null, full = u.id === "repair" && ship.hull >= 100;
      btn.disabled = maxed || full || salv < cost;
      btn.innerHTML = '<span class="key">' + (i + 1) + '</span><span class="name">' + u.name + "<small>" + u.desc +
                      '</small></span><span class="pips">' + pips + '</span><span class="cost">' +
                      (maxed ? "MAX" : full ? "FULL" : cost) + "</span>";
      btn.addEventListener("click", function () { buy(i); });
      li.appendChild(btn); upList.appendChild(li);
    });
  }

  function buy(i) {
    var u = UPGRADES[i], cost = costOf(u);
    if (cost === null || salv < cost || (u.id === "repair" && ship.hull >= 100)) { sfx("deny"); return; }
    salv -= cost;
    if (u.id === "repair") ship.hull = Math.min(100, ship.hull + 40);
    else { lv[u.id]++; if (u.id === "shield") ship.shield = lv.shield * 25; }
    sfx("buy");
    buildMenu();
  }

  function openMenu() {
    if (state !== "playing") return;
    state = "menu";
    keys = {};
    sfx("thrust", false);
    buildMenu();
    upPanel.hidden = false;
  }
  function closeMenu(quiet) {
    upPanel.hidden = true;
    if (!quiet && state === "menu") state = "playing";
  }
  document.getElementById("upClose").addEventListener("click", function () { closeMenu(); });

  // ---------------------------------------------------------------- input
  function start() {
    if (started) return;
    started = true;
    splash.classList.add("gone");
    document.body.classList.add("playing");
    reset();
  }
  splash.addEventListener("click", start);

  function togglePause() {
    if (state === "playing") { state = "paused"; sfx("thrust", false); }
    else if (state === "paused") state = "playing";
  }

  document.addEventListener("keydown", function (e) {
    var k = e.key.toLowerCase();
    if (!started) { start(); e.preventDefault(); return; }
    if (state === "menu") {
      if (k >= "1" && k <= "7") buy(parseInt(k, 10) - 1);
      else if (k === "u" || k === "escape" || k === "enter") closeMenu();
      else if (k === "m") Sfx.toggleMute();
      e.preventDefault();
      return;
    }
    if (k === "p") togglePause();
    else if (k === "m") { if (window.Sfx) Sfx.toggleMute(); }
    else if (k === "u" || k === "tab") openMenu();
    else if ((k === " " || k === "r") && state === "over" && ship.dead > 1) reset();
    else if (k === "escape" && state === "paused") togglePause();
    keys[k] = true;
    if (k.indexOf("arrow") === 0 || k === " " || k === "tab") e.preventDefault();
  });
  document.addEventListener("keyup", function (e) { keys[e.key.toLowerCase()] = false; });
  window.addEventListener("blur", function () { keys = {}; if (state === "playing") togglePause(); });

  // touch: a stick on the left, fire / salvage bay / pause buttons on the right
  var stickEl = document.getElementById("stick"), knob = document.getElementById("knob"), stickId = null;
  document.addEventListener("touchstart", function () { document.body.classList.add("touch"); }, { passive: true, capture: true });
  canvas.addEventListener("touchstart", function () {
    if (!started) { start(); return; }
    if (state === "over" && ship.dead > 1) reset();
  }, { passive: true });
  function stickMove(t) {
    var b = stickEl.getBoundingClientRect(), dx = t.clientX - (b.left + b.width / 2), dy = t.clientY - (b.top + b.height / 2);
    var m = Math.sqrt(dx * dx + dy * dy), max = b.width / 2;
    if (m > max) { dx *= max / m; dy *= max / m; }
    stick.x = dx / max; stick.y = dy / max; stick.on = true;
    knob.style.transform = "translate(" + dx + "px," + dy + "px)";
  }
  stickEl.addEventListener("touchstart", function (e) { e.preventDefault(); stickId = e.changedTouches[0].identifier; stickMove(e.changedTouches[0]); }, { passive: false });
  stickEl.addEventListener("touchmove", function (e) {
    e.preventDefault();
    for (var i = 0; i < e.changedTouches.length; i++) if (e.changedTouches[i].identifier === stickId) stickMove(e.changedTouches[i]);
  }, { passive: false });
  function stickEnd(e) {
    for (var i = 0; i < e.changedTouches.length; i++) if (e.changedTouches[i].identifier === stickId) {
      stickId = null; stick.on = false; stick.x = stick.y = 0; knob.style.transform = "";
    }
  }
  stickEl.addEventListener("touchend", stickEnd); stickEl.addEventListener("touchcancel", stickEnd);
  var fireBtn = document.getElementById("tFire");
  fireBtn.addEventListener("touchstart", function (e) { e.preventDefault(); touchFire = true; if (state === "over" && ship.dead > 1) reset(); }, { passive: false });
  fireBtn.addEventListener("touchend", function () { touchFire = false; });
  fireBtn.addEventListener("touchcancel", function () { touchFire = false; });
  document.getElementById("tUp").addEventListener("touchstart", function (e) { e.preventDefault(); if (state === "menu") closeMenu(); else openMenu(); }, { passive: false });
  document.getElementById("tPause").addEventListener("touchstart", function (e) { e.preventDefault(); togglePause(); }, { passive: false });

  // ---------------------------------------------------------------- run
  function loop(now) {
    var dt = last ? Math.min(0.033, (now - last) / 1000) : 0.016;
    last = now;
    if (started) update(dt);
    draw(now);
    requestAnimationFrame(loop);
  }

  window.addEventListener("resize", resize);
  resize();
  makeStars();

  // a peek at the game for tests
  window.rocksOfRuin = { info: function () {
    return { started: started, state: state, score: score, salvage: salv, hull: ship && ship.hull, rocks: rocks && rocks.length,
             aliens: aliens && aliens.length, danger: danger, x: ship && ship.x, y: ship && ship.y };
  } };

  if (SHOT) {                            // for README screenshots: a busy moment deep in the field, then frozen
    splash.style.display = "none";
    start();
    lv.rate = 3; lv.spread = 2; lv.homing = 1; lv.shield = 2; lv.pierce = 1;
    salv = 137; score = 48650; best = Math.max(best, 61200); elapsed = 260;
    ship.x = 3600; ship.y = -1500; ship.a = -0.5; ship.vx = 120; ship.vy = -60; ship.shield = 35; ship.hull = 72; ship.inv = 0;
    cam.x = ship.x; cam.y = ship.y;
    var ring0 = [[260, -120, 0, null], [-300, 140, 0, "skitter"], [420, 170, 1, null], [-120, -300, 1, null], [520, -260, 0, null],
                 [-480, -150, 1, null], [120, 300, 2, null], [-560, 280, 0, null], [640, 60, 2, null], [330, 330, 1, null], [-250, 20, 2, null]];
    ring0.forEach(function (q, i) {
      var r = newRock(ship.x + q[0], ship.y + q[1], q[2], rr(-30, 30), rr(-30, 30), { nest: q[3], iron: i === 4 });
      if (i === 2) r.iron = true;
    });
    spawnAlien("skitter", ship.x - 180, ship.y + 110); spawnAlien("skitter", ship.x - 220, ship.y + 60);
    spawnAlien("spitter", ship.x + 240, ship.y + 200); spawnAlien("leech", ship.x - 60, ship.y - 230);
    spawnAlien("wraith", ship.x + 330, ship.y - 60);
    dropSalvage(ship.x + 90, ship.y + 60, 12); dropSalvage(ship.x - 120, ship.y - 80, 6);
    for (var f = 0; f < 34; f++) {
      ship.inv = 1; ship.hull = 72;
      ship.a += 0.03;
      update(1 / 60);
      if (f === 20) { var big = rocks[rocks.length - 6]; if (big) breakRock(big, rocks.indexOf(big), 0, 0); }
    }
    ship.inv = 0;
    banner = null;
    var now0 = performance.now();
    draw(now0);
    return;
  }
  requestAnimationFrame(loop);
})();
