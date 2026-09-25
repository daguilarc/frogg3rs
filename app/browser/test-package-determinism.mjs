#!/usr/bin/env node
// Proves the content-addressed-package requirement
// from openspec/specs/froggers-browser-package/spec.md ("Immutable,
// content-addressed package"):
//   - same inputs => same build identifier (assembled twice from
//     identical artifacts)
//   - changed inputs => changed build identifier (any artifact's content
//     changes)
//
// This exercises Sheaf's generic `assemblePackage`
// (External/Sheaf/projects/synth/browser/src/package-contract.mjs)
// directly against copies of the ALREADY-BUILT app/browser/dist artifacts
// -- no wasm rebuild needed. buildId is a pure function of artifact bytes
// (External/Sheaf/projects/synth/browser/src/package-contract.mjs,
// `contentBuildId`), so this is a complete
// proof of the packaging step's determinism without re-running emcc
// (machine constraint: reuse app/browser/dist, don't rebuild unless the
// packaging step itself requires it -- it doesn't).
import assert from "node:assert/strict";
import { copyFile, mkdir, mkdtemp, readdir, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import process from "node:process";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";

const REPO_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const SHEAF_BROWSER_DIST_SRC = path.join(
  REPO_ROOT, "External", "Sheaf", "projects", "synth", "browser", "dist", "src",
);
const REAL_SOURCE_DIR = path.join(REPO_ROOT, "app", "browser", "dist", "wasm", "apps", "frogg3rs");

const { assemblePackage } = await import(path.join(SHEAF_BROWSER_DIST_SRC, "package-contract.mjs"));

const ARTIFACTS = Object.freeze({
  entry: "frogg3rs.js",
  wasm: "frogg3rs.wasm",
  pthreadWorker: "frogg3rs.js",
  wasmWorker: "frogg3rs.js",
  audioWorklet: "frogg3rs.js",
});

async function copyDirectory(source, destination) {
  await mkdir(destination, { recursive: true });
  for (const entry of await readdir(source, { withFileTypes: true })) {
    if (!entry.isFile()) throw new Error(`unexpected non-file ${entry.name} in ${source}`);
    await copyFile(path.join(source, entry.name), path.join(destination, entry.name));
  }
}

async function packageFrom(sourceDirectory, outputDirectory) {
  const record = await assemblePackage({
    appId: "frogg3rs",
    sourceDirectory,
    outputDirectory,
    artifacts: ARTIFACTS,
  });
  return record.buildId;
}

const workRoot = await mkdtemp(path.join(tmpdir(), "frogg3rs-determinism-"));
try {
  // 1. Same inputs, assembled twice from independent copies => same id.
  const copyA = path.join(workRoot, "source-a");
  const copyB = path.join(workRoot, "source-b");
  await copyDirectory(REAL_SOURCE_DIR, copyA);
  await copyDirectory(REAL_SOURCE_DIR, copyB);

  const idFromA = await packageFrom(copyA, path.join(workRoot, "out-a"));
  const idFromB = await packageFrom(copyB, path.join(workRoot, "out-b"));
  assert.equal(idFromA, idFromB, "identical artifacts must produce identical build IDs");
  console.log(`OK  same inputs -> same buildId (${idFromA})`);

  // 2. Changed inputs (one byte flipped in the entry module) => different id.
  const copyC = path.join(workRoot, "source-c");
  await copyDirectory(REAL_SOURCE_DIR, copyC);
  const entryPath = path.join(copyC, "frogg3rs.js");
  const original = await import("node:fs/promises").then((fs) => fs.readFile(entryPath));
  const mutated = Buffer.from(original);
  mutated[0] = mutated[0] ^ 0xff;
  await writeFile(entryPath, mutated);

  const idFromC = await packageFrom(copyC, path.join(workRoot, "out-c"));
  assert.notEqual(idFromC, idFromA, "changing one artifact byte must change the build ID");
  console.log(`OK  changed inputs -> different buildId (${idFromC})`);

  console.log("package determinism test: PASS");
} finally {
  await rm(workRoot, { recursive: true, force: true });
}
