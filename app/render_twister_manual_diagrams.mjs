#!/usr/bin/env node
// render_twister_manual_diagrams.mjs -- draws MANUAL.md's two MIDI Fighter
// Twister diagrams (as the preset works, and with Shift held) from
// assets/manual/twister-controls.json (GenerateTwisterManualLabels's output),
// in the Twister's own physical layout: the 4x4 encoder grid between a left
// and a right column of three side buttons.
//
// All display text comes from the label file's own "encoders" and
// "sideButtons" rows -- GenerateTwisterManualLabels.cpp already resolved
// every job to the label the Controllers page itself would show, so this
// script never re-derives a label from a raw action name or message type.
//
// It does still walk the label file's raw "preset" dump -- the same
// synth::ToJSON(MidiControllerProfileConfig) output the app's own
// persistence uses -- to confirm every field on each of the 16 encoder
// turns and pushes is one this script (by way of the label file's rows)
// already knows how to draw. A field it does not recognize stops the
// script with an error naming it, rather than silently drawing a blank
// control: a per-encoder shifted job a later change adds, say, must be
// taught here before it reaches either diagram.
//
// Renders in headless Chromium through the Playwright already installed at
// app/browser/e2e/node_modules -- no separate install, no network fetch.
//
// Usage: node render_twister_manual_diagrams.mjs <controls.json> <out-dir>
// Writes <out-dir>/twister-preset.png and <out-dir>/twister-preset-shift.png.

import { readFileSync, mkdirSync } from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const { chromium } = await import(
  pathToFileURL(path.join(HERE, "browser", "e2e", "node_modules", "playwright", "index.mjs")).href
);

function fail(message) {
  console.error(`render_twister_manual_diagrams: ${message}`);
  process.exit(1);
}

const [, , controlsPath, outDir] = process.argv;
if (!controlsPath || !outDir) {
  fail("usage: render_twister_manual_diagrams.mjs <controls.json> <out-dir>");
}

const controls = JSON.parse(readFileSync(controlsPath, "utf8"));

// ---------------------------------------------------------------------------
// Encoders: 16 rows, one per physical grid position, already resolved.
// ---------------------------------------------------------------------------

const ENCODER_COUNT = 16;
const encoderRows = controls.encoders ?? [];
if (encoderRows.length !== ENCODER_COUNT) {
  fail(`expected ${ENCODER_COUNT} "encoders" rows, found ${encoderRows.length}`);
}
const encoders = new Array(ENCODER_COUNT).fill(null);
for (const row of encoderRows) {
  const position = row.position;
  if (!Number.isInteger(position) || position < 0 || position >= ENCODER_COUNT) {
    fail(`"encoders" row has an out-of-range position: ${JSON.stringify(row)}`);
  }
  if (encoders[position] !== null) {
    fail(`two "encoders" rows claim position ${position}`);
  }
  if (typeof row.turn !== "string" || typeof row.push !== "string" || typeof row.shiftedTurn !== "string") {
    fail(`"encoders" row at position ${position} is missing its turn, push, or shiftedTurn label`);
  }
  encoders[position] = { turn: row.turn, push: row.push, shiftedTurn: row.shiftedTurn };
}
if (encoders.some((entry) => entry === null)) {
  fail("not every encoder grid position has an \"encoders\" row");
}

// Confirm every field on the raw preset's own encoder turns and pushes is
// one this script has been taught to draw (via the resolved rows above) --
// a field it does not recognize stops the script here rather than reaching
// either diagram unlabelled.
const KNOWN_ENCODER_MAPPING_FIELDS = new Set(["control", "slotIx", "position", "shiftedJob"]);
function checkKnownEncoderFields(mapping, kind, index) {
  for (const field of Object.keys(mapping)) {
    if (!KNOWN_ENCODER_MAPPING_FIELDS.has(field)) {
      fail(`encoder ${kind} at index ${index} has field "${field}" this script has no label for`);
    }
  }
}
const rawTurns = controls.preset?.encoderInput?.turns ?? [];
const rawPushes = controls.preset?.encoderInput?.pushes ?? [];
if (rawTurns.length !== ENCODER_COUNT || rawPushes.length !== ENCODER_COUNT) {
  fail(
    `expected ${ENCODER_COUNT} raw encoder turns and pushes, found ${rawTurns.length} turns / ` +
      `${rawPushes.length} pushes`
  );
}
rawTurns.forEach((turn, index) => checkKnownEncoderFields(turn, "turn", index));
rawPushes.forEach((push, index) => checkKnownEncoderFields(push, "push", index));

// ---------------------------------------------------------------------------
// Side buttons: 6 rows, one per physical place, already resolved.
// ---------------------------------------------------------------------------

const SIDE_BUTTON_COUNT = 6;
const sideButtons = controls.sideButtons ?? [];
if (sideButtons.length !== SIDE_BUTTON_COUNT) {
  fail(`expected ${SIDE_BUTTON_COUNT} "sideButtons" rows, found ${sideButtons.length}`);
}
for (const button of sideButtons) {
  if (
    typeof button.place !== "string" ||
    typeof button.press !== "string" ||
    typeof button.pressNote !== "string" ||
    typeof button.shiftedPress !== "string" ||
    typeof button.shiftedPressNote !== "string" ||
    typeof button.isShift !== "boolean"
  ) {
    fail(`"sideButtons" row is missing a field this script draws: ${JSON.stringify(button)}`);
  }
}

// ---------------------------------------------------------------------------
// Render.
// ---------------------------------------------------------------------------

function sideButtonHtml(button, shiftHeld) {
  const held = shiftHeld && button.isShift;
  // The Shift button itself has no shifted press of its own (MANUAL.md's
  // side-button table: "Shift | (none)") -- that "(none)" answers "what does
  // Shift + Shift do", not "is Shift held", so it is never shown here. In
  // the Shift diagram the Shift button instead reads that it is the one
  // being held.
  let label;
  let note;
  if (held) {
    label = `${button.press} (held)`;
    note = button.pressNote;
  } else if (shiftHeld) {
    label = button.shiftedPress;
    note = button.shiftedPressNote;
  } else {
    label = button.press;
    note = button.pressNote;
  }
  return `<div class="side-btn${held ? " held" : ""}">${escapeHtml(label)}${escapeHtml(note)}</div>`;
}

function encoderHtml(encoder, shiftHeld) {
  // While Shift is held, a turn with a shifted job ("(none)" otherwise)
  // does that job instead of its ordinary one -- the same swap
  // sideButtonHtml makes for a shifted side button.
  const shifted = shiftHeld && encoder.shiftedTurn !== "(none)";
  const turnLabel = shifted ? encoder.shiftedTurn : encoder.turn;
  return (
    `<div class="encoder${shifted ? " shifted" : ""}">` +
    `<div class="turn">${escapeHtml(turnLabel)}</div>` +
    `<div class="push">${escapeHtml(encoder.push)}</div>` +
    `</div>`
  );
}

function escapeHtml(text) {
  return String(text).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
}

function pageHtml(shiftHeld) {
  const left = sideButtons.slice(0, 3);
  const right = sideButtons.slice(3, 6);
  const title = shiftHeld
    ? `${controls.displayName} -- with Shift held`
    : `${controls.displayName} -- as the preset works`;
  return `<!doctype html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; }
  body { margin: 0; background: #1b1b1b; font-family: Helvetica, Arial, sans-serif; color: #f0f0f0; }
  #diagram { display: inline-flex; flex-direction: column; gap: 12px; padding: 20px; background: #1b1b1b; }
  .title { font-size: 15px; font-weight: 600; }
  .subtitle { font-size: 11px; opacity: 0.75; margin-top: -6px; }
  .row { display: flex; align-items: center; gap: 20px; }
  .side-col { display: flex; flex-direction: column; gap: 14px; }
  .side-btn {
    width: 140px; height: 56px; border: 2px solid #7fa8d9; border-radius: 8px;
    display: flex; align-items: center; justify-content: center; text-align: center;
    font-size: 12px; padding: 4px; line-height: 1.25;
  }
  .side-btn.held { border-color: #ffcc33; background: #3a2f00; color: #ffdd77; }
  .grid { display: grid; grid-template-columns: repeat(4, 84px); grid-template-rows: repeat(4, 84px); gap: 8px; }
  .encoder {
    border: 2px solid #7fd9a8; border-radius: 50%;
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    text-align: center; font-size: 11px;
  }
  .encoder .turn { font-weight: 600; }
  .encoder .push { opacity: 0.7; font-size: 9.5px; margin-top: 2px; }
  .encoder.shifted { border-color: #ffcc33; background: #3a2f00; color: #ffdd77; }
</style>
</head>
<body>
<div id="diagram">
  <div class="title">${escapeHtml(title)}</div>
  <div class="subtitle">The numbered knobs move those slots of whichever bank is shown; the bank sections list them.</div>
  <div class="row">
    <div class="side-col">${left.map((b) => sideButtonHtml(b, shiftHeld)).join("")}</div>
    <div class="grid">${encoders.map((encoder) => encoderHtml(encoder, shiftHeld)).join("")}</div>
    <div class="side-col">${right.map((b) => sideButtonHtml(b, shiftHeld)).join("")}</div>
  </div>
</div>
</body>
</html>`;
}

mkdirSync(outDir, { recursive: true });

const browser = await chromium.launch();
try {
  const page = await browser.newPage({ viewport: { width: 900, height: 500 } });
  for (const [fileName, shiftHeld] of [
    ["twister-preset.png", false],
    ["twister-preset-shift.png", true],
  ]) {
    await page.setContent(pageHtml(shiftHeld));
    const diagram = page.locator("#diagram");
    await diagram.screenshot({ path: path.join(outDir, fileName) });
  }
} finally {
  await browser.close();
}
