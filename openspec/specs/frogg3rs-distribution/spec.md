# frogg3rs-distribution Specification

## Purpose
What the published site, the desktop download and the plugin download each are.
## Requirements
### Requirement: Every host ships from one app and one shell
The standalone, the plugin and the browser build SHALL be produced from the same
application sources and SHALL present the same runtime shell. A host SHALL NOT
be made to build on a platform by substituting a different application shell for
Sheaf's own, because that ships an application that differs from the documented
one in the controls it offers.

#### Scenario: A platform port keeps the shell
- **WHEN** a host is made to build on an additional platform
- **THEN** it presents the same runtime shell, including audio device selection
- **AND** every control documented in the manual is present on both platforms

### Requirement: The published site is built from the current app
The published site SHALL be produced from the current application's browser
build. It SHALL reference its own assets relatively, so that the page survives
being served from any base path, and SHALL NOT carry a repository or product
name that no longer resolves.

#### Scenario: The site survives a repository rename
- **WHEN** the repository or its published base path changes
- **THEN** the site's stylesheet, scripts and images still resolve
- **AND** the page renders styled, with its interactive parts running

#### Scenario: The site offers both downloads
- **WHEN** an operator opens the published site
- **THEN** the desktop application and the audio plugin are both offered
- **AND** each link resolves to that artifact's own release, not to whichever
  release happened to be published most recently

### Requirement: Each artifact has its own release, named for what it is
The desktop application and the audio plugin SHALL be released independently,
each on its own tag, so that either can be published without republishing the
other. A release SHALL state which platforms it contains, and SHALL NOT present
the absence of a platform-specific artifact as a failure.

A platform's artifact SHALL be absent from a release only while it genuinely
cannot be produced. When a platform builds, its artifact SHALL ship in that
release and the documentation SHALL stop describing it as pending, so that the
stated coverage and the actual coverage cannot drift apart.

#### Scenario: The plugin is released without the desktop app
- **WHEN** the plugin's tag is pushed
- **THEN** the plugin release is published with its plugin formats
- **AND** the desktop release is untouched

#### Scenario: A release names its platforms
- **WHEN** a release contains artifacts for some platforms and not others
- **THEN** the release states which platforms it covers

#### Scenario: A platform that builds is shipped, not described as pending
- **WHEN** the desktop release is published and the Windows build succeeds
- **THEN** the Windows artifact is attached to that release
- **AND** no operator-facing document still calls the Windows build pending

### Requirement: A publishing trigger that cannot fire is a defect
A tag intended to publish SHALL actually publish. A workflow SHALL NOT declare a
trigger whose pattern cannot match the tag it documents, nor a permission set
that forbids the publication it exists to perform.

#### Scenario: The documented tag produces the release
- **WHEN** the tag named in the release process is pushed
- **THEN** the workflow runs and publishes the artifacts for that tag

#### Scenario: A superseded release does not outrank the current one
- **WHEN** a release exists for a product line that no longer ships
- **THEN** it does not present itself as the current download

### Requirement: Docs-only pushes do not rebuild the deliverables

A docs-only push to `main` SHALL NOT start the site or VST builds, where
docs-only means every touched file is the repository README or an openspec
artifact. A push touching anything else SHALL start them as before. Tag
pushes and manual
dispatches are exempt from path filtering, so cutting a release is never
blocked by what a push contains.

#### Scenario: A docs-only push is quiet

- **WHEN** a push to `main` changes only files under `openspec/` or the
  repository `README.md`
- **THEN** no Pages run and no VST run starts for that push

#### Scenario: A code push still builds

- **WHEN** a push to `main` changes any other file
- **THEN** the Pages and VST builds run as they always have

#### Scenario: Releases are unaffected

- **WHEN** the `frogg3rs_vst` tag is pushed or a workflow is dispatched
  manually
- **THEN** the corresponding workflow runs regardless of which files recent
  pushes touched

### Requirement: A downloadable build carries a signature matching its contents
Every bundle a release ships SHALL carry a code signature that covers the bundle
as assembled, including files placed into it after its executable was linked,
on every platform where this project holds a signing identity.
This applies to application bundles and to plug-in bundles alike, since both are
downloaded by the same person onto the same operating system. Signing SHALL
happen after assembly is complete, and every packaged result SHALL be verified
before it is published.

An operating system that quarantines downloads treats a bundle whose signature
does not match itself as damaged rather than as unsigned, which tells the person
who downloaded it that the file is broken and offers them no way forward. A
build signed only at link time, before its bundle is assembled, is in exactly
that state. So is a build signed partway through assembly, before the last of
its resources is copied in.

Where this project holds no signing identity for a platform, its artifact
SHALL ship ad-hoc signed or unsigned, and the release SHALL state what the
operator sees instead and the exact steps that open it. An unsigned artifact
under an unqualified signing rule is a requirement violated by the first
thing that ships under it; naming the condition is what keeps the rule true.
Holding no identity is a STANDING DECISION here, not a blocked task: neither
an Apple Developer Program membership nor a Windows code-signing certificate
is being purchased. Documentation SHALL describe the resulting step as
permanent rather than as a fix awaiting work, so that nobody reads it as a
defect to be closed.

#### Scenario: Every shipped bundle verifies
- **WHEN** a release artifact is packaged on a platform where this project
  holds a signing identity
- **THEN** verifying its signature against its own contents succeeds
- **AND** a build whose signature does not match its contents fails the build
  rather than reaching a release
- **AND** this holds for every bundle the release publishes, not only the
  application

#### Scenario: A downloaded build is assessed, not rejected as damaged
- **WHEN** a packaged build is downloaded and carries the quarantine attribute
- **THEN** the operating system evaluates its signature and reports a verdict
- **AND** it does not report the file as damaged

#### Scenario: A platform this project holds no identity for ships anyway, and says so
- **WHEN** a release contains an artifact for a platform where this project
  holds no signing certificate
- **THEN** that artifact ships unsigned rather than being withheld
- **AND** the operator manual states what the operating system shows on first
  open and the step that gets past it

### Requirement: A release states what opening it requires
A release SHALL state what the person downloading it will see and what they must
do to open it, whenever its artifacts are not signed by an identity the
operating system recognises. That statement SHALL appear both in the operator
manual and in the release itself, since someone who downloads an artifact has
not necessarily read the manual, and SHALL be written once and carried to each
release body from that one source.

#### Scenario: The extra step is documented where it is met
- **WHEN** a release ships artifacts an operating system will not open directly
- **THEN** the release body states what the operator will see and the step that
  opens it
- **AND** the operator manual states the same

#### Scenario: Recognised signing removes the step
- **WHEN** artifacts are signed by an identity the operating system recognises
- **THEN** the download opens without an extra step, and the documented step is
  removed rather than left standing

### Requirement: The published site carries the application's mark
The published site SHALL show the application's own logo in its header,
resolving from a file the site build stages rather than from an external host.
The site is the first thing a downloader sees, and a title alone does not
identify the application the downloads belong to.

The header is the required position, not an incidental one. The blank-frame
guard measures the header's box and samples only the band beneath it, so that
the header's own colour cannot stand in for a rendering application surface. A
mark placed outside the header enters that sampled band and lets the guard pass
over a blank deployment.

#### Scenario: The header identifies the application
- **WHEN** the published site loads
- **THEN** the application's logo renders inside the site header
- **AND** the image resolves, rather than rendering as a broken reference

#### Scenario: The blank-surface guard still fails on a blank surface
- **WHEN** the application surface renders blank beneath the header
- **THEN** the guard fails, with the logo present in the header

### Requirement: The plugin ships for every platform the application ships for

The VST3 plugin SHALL be released for both macOS and Windows. The Audio Unit
SHALL be released for macOS only, because the format exists only there, and a
build off macOS SHALL NOT request it.

A plugin release SHALL carry operator documentation inside every format it
ships, on every platform. Documentation shipping with the plugin is not a
macOS-specific requirement and SHALL NOT be dropped where the bundle layout
differs.

Windows artifacts SHALL ship unsigned while no Authenticode certificate exists,
and the release body SHALL say so rather than leaving a first-load warning
unexplained.

#### Scenario: The plugin release carries both platforms
- **WHEN** a plugin release is published
- **THEN** it carries a macOS VST3, a macOS Audio Unit, and a Windows VST3

#### Scenario: The Audio Unit is not attempted off macOS
- **WHEN** the plugin is built on Windows
- **THEN** only the VST3 format is requested and the build does not fail on a
  format the platform cannot produce

#### Scenario: Documentation ships on every platform
- **WHEN** any released plugin artifact is opened
- **THEN** the manual and quick dictionary are inside it

#### Scenario: Release notes match what shipped
- **WHEN** a plugin release body is generated
- **THEN** it does not state that the release carries no Windows VST3

### Requirement: A shipped bundle carries the declarations its platform requires of it

A released artifact SHALL carry every platform declaration the operating system
requires for the capabilities that artifact actually uses. An application that
opens a capture device on macOS SHALL declare a microphone usage description in
the bundle that ships.

The declaration SHALL live on the build path that produces the shipped artifact
for that platform. Where a project builds different platforms through different
build systems, a declaration made in one of them SHALL NOT be counted as
satisfying the other. A declaration on a path that does not produce the artifact
is absent from the artifact.

#### Scenario: The shipped macOS application declares its microphone use
- **WHEN** the released macOS application bundle is inspected
- **THEN** it declares a microphone usage description

#### Scenario: A declaration is checked in the artifact, not in the source
- **WHEN** a platform declaration is claimed to be present
- **THEN** the check reads the built artifact for that platform

### Requirement: A version that appears in more than one place has one definition

The browser ABI version SHALL have a single definition. Every other site that
names it SHALL read that definition, or SHALL be generated from it, or SHALL be
asserted equal to it by a check that fails when they diverge.

Fixtures and test doubles are sites. A synthesized fixture that hard-codes the
version is the same defect as a hand-maintained mirror, and is harder to notice
because it is not near the definition.

#### Scenario: Moving the version moves every mirror
- **WHEN** the single definition is changed
- **THEN** every site that names the version reflects the change, or a check
  fails naming the site that did not

#### Scenario: A fixture cannot silently disagree
- **WHEN** a fixture declares an ABI version that differs from the definition
- **THEN** a check fails rather than a test asserting against the stale value

### Requirement: A test run does not trust a server it cannot date

A test run SHALL NOT trust an already-running server whose own code it cannot
date. Where a test configuration reuses such a server rather than starting its
own, the run SHALL establish that the server's code matches the tree before
trusting its responses, or SHALL refuse to reuse it.

A server that serves files from disk while answering from in-process handlers
SHALL NOT be treated as current on the strength of the files alone. The stale
case SHALL be reported loudly enough that it is not mistaken for a defect in the
code under test.

#### Scenario: A server older than the code is not silently reused
- **WHEN** a run finds an existing server whose code predates the working tree
- **THEN** the run restarts it or fails loudly, rather than testing against
  responses the tree no longer produces

#### Scenario: The stale case names itself
- **WHEN** a run refuses or restarts a stale server
- **THEN** the reason given identifies the server as stale rather than reporting
  a failed assertion

### Requirement: The repository carries no published artifact the deploy does not produce
The repository SHALL NOT commit a catalog, package, or build output that the publishing workflow does not itself generate and upload, so that the committed tree never describes a build that differs from the one being served.

#### Scenario: The served catalog is the generated one
- **WHEN** the site is deployed
- **THEN** the catalog and package it serves are generated from the current application during that deploy
- **AND** no other catalog or package for the application exists in the committed tree

#### Scenario: A stale committed catalog is a defect
- **WHEN** a committed catalog declares a protocol version older than the one the application is built against
- **THEN** it is removed rather than left beside the generated one

