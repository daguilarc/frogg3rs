# Active-change overlap sweep — `External/Sheaf` submodule

The rule requires enumerating other active changes, submodules included, and
reporting overlap per change. An earlier version of this change's proposal
enumerated only the main repository and omitted the submodule entirely. A
second version enumerated it in prose but recorded no command, which is the
same defect in a smaller shape: a zero is a result that must be measured, and
a measurement that lives only in a session transcript decays with the session.

## The ten active changes

```
$ ls -d External/Sheaf/openspec/changes/*/ | grep -v archive
External/Sheaf/openspec/changes/app-midi-catalog/
External/Sheaf/openspec/changes/bank-addressed-absolute-write/
External/Sheaf/openspec/changes/browser-slider-value-readout/
External/Sheaf/openspec/changes/fix-out-of-tree-app-gaps/
External/Sheaf/openspec/changes/fix-task-analyzer-plan-derived-tasks/
External/Sheaf/openspec/changes/launchpad-model-on-the-row/
External/Sheaf/openspec/changes/rework-controllers-block-editing/
External/Sheaf/openspec/changes/shift-and-file-export/
External/Sheaf/openspec/changes/shorten-deadline-readout-window/
External/Sheaf/openspec/changes/ui-state-before-audio/
```

## The sweep, by operand, case-insensitive

Searched by the narrowest operands the concept cannot avoid sharing rather
than by expression: the header this change edits, the test file it edits, and
the three DSP concepts it touches.

```
$ grep -rEli 'delay\.hpp|froggersdspparitytests|stereo width|widthspread|crossfeedpair' External/Sheaf/openspec/changes/ | grep -v archive
(no output — zero hits)
```

## Result

FOUND 0, CHANGED 0, across all ten. Zero overlap with this change.
