# User docs

MANUAL.md, QUICK_DICT.md, README.md and DAISY_MANUAL.md say what the app does now, for a reader
who never saw an earlier version. The reader is "you".

None of the following goes in:

- Before/after or "now/was" comparisons, or anything else that reads as a changelog.
- A defence of why a control was designed the way it was.
- A lab figure the user does not need to use the control. A figure stays when it tells the user
  what they will hear or see — a range, a default, a level change they will notice — written
  plainly.
- Jokes or flourish.
- Defining a control by what it is not.
- Shouting caps.

A change's own implementation report — what changed, why, what it measured — goes in the commit
message or the change's proposal. It never goes in these docs.

Every docs edit is read by a context that wrote none of it. For each changed sentence, that
context answers two questions: does a first-time user, who never saw an earlier version, need
this to use the control, and is it true against the code? Keep the sentence if both are yes;
cut or rewrite it if either is no.
