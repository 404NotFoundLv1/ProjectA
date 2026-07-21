# ProjectRift v0.8.0 Mission Contracts and Rift Selection Design

## Scope

v0.8.0 turns the existing `ProjectRiftMission` Primary Asset into the single source for progression and mission-contract configuration. The server resolves a compact contract selection into an immutable, replicated mission definition and carries only scalar travel context into the Rift. The existing session, lobby, travel, settlement, reward, and profile systems remain authoritative.

Formal WBP layout work and three production test-contract assets remain UE editor manual items. Code provides reflected APIs, validation, native fallback presentation, deterministic generation, and automation coverage. Project version becomes 0.8.0 while profile schema remains v9.

## Chosen architecture

`UPRMissionProgressionDataAsset` remains the only mission Primary Asset. It owns an `FPRMissionContract` value in addition to its existing progression fields. This avoids a second Mission/Contract identity registry and preserves all existing `MissionId` consumers.

The runtime model is split into focused value types:

- `FPRMissionRewardPreview`: reward categories, minimum/maximum budget and rarity-range hints. It never contains generated item instances.
- `FPRMissionContract`: authored contract version, biome, difficulty, modifier identifiers and reward preview.
- `FPRMissionDefinition`: immutable resolved mission identity, contract version, biome, difficulty, modifiers, reward preview, Seed and deterministic generation signature.
- `FPRMissionTravelContext`: compact ContractId, ContractVersion and Seed values serialized into the existing ServerTravel URL.

All identifiers are `FName`; UObject references stay on the Primary Asset and never enter replicated/travel structs.

## Validation and determinism

A valid contract requires:

- a positive contract version;
- non-empty BiomeId and DifficultyId;
- unique, non-empty ModifierIds;
- a structurally valid reward preview with non-negative ordered ranges;
- all existing mission progression requirements to remain valid.

`UPRMissionProgressionDataAsset::BuildMissionDefinition(Seed)` rejects zero Seed or invalid authored data. It produces a normalized definition and a deterministic signature derived from a versioned string containing ContractId, ContractVersion, BiomeId, DifficultyId, sorted ModifierIds and Seed. Repeating the same inputs produces the same definition/signature; changing any contract input or Seed changes the signature.

The Seed is allocated by the authoritative ship lobby. Clients select only a ContractId and cannot supply a Seed or contract version through the selection RPC.

## Lobby authority and replication

`APRGameState` replaces its mission-id-only replicated state with a replicated `FPRMissionDefinition`, while retaining `GetSelectedTeamMissionId()` as a compatibility view over `Definition.ContractId`.

The selection flow is:

1. A local controller requests a ContractId through a reliable server RPC.
2. `APRShipLobbyGameMode` rejects non-host callers and non-lobby execution.
3. The server loads the registered `ProjectRiftMission` asset, validates the contract, and verifies the host profile is eligible for its chapter and story prerequisites.
4. The server allocates a non-zero Seed, builds the definition, replicates it through `APRGameState`, clears team mission readiness, and resets every player's Ready state.
5. Every client renders the same replicated definition and reward preview.

Mission start repeats authoritative asset, contract-version, definition-signature, host eligibility and team-readiness validation. An RPC cannot bypass a locked chapter.

Lobby BeginPlay selects the existing starter contract through the same server path. Returning from a Rift creates a fresh lobby GameState and a fresh selection Seed, so the host can choose the next mission.

## Travel and Rift initialization

The existing ServerTravel flow remains. `FPRMissionTravelContext` serializes `ContractId`, `ContractVersion` and `Seed`; the URL contains no UObject path or reward payload. Legacy `MissionId` parsing remains accepted only as a compatibility alias when the new ContractId option is absent.

`APRRiftGameMode::InitGame` parses the context, reloads the registered mission asset, rebuilds the definition (including its deterministic signature) and rejects:

- missing/unknown ContractId;
- zero or malformed Seed;
- a contract-version mismatch;
- invalid authored contract data;
- an invalid authored contract that cannot rebuild a definition.

`StartRiftMission` uses the travel Seed as `CurrentRunSeed` instead of generating a new random value. The resolved definition is copied into `APRRiftGameState`, making biome/difficulty/modifiers available to later v0.8.x systems without reparsing travel options.

## Results, persistence and compatibility

`FPRRiftSettlementData` and `FPRPlayerSettlementReceipt` record ContractVersion alongside the existing MissionId/RunSeed data. MissionId remains the canonical ContractId compatibility field. Settlement validation compares the receipt version to the currently registered contract before applying progression or rewards.

No mission selection is written into the persistent profile. Selection is lobby session state; completion remains the only persistent progression change. Profile save schema stays v9.

Existing public entry points remain valid:

- `UPRMissionProgressionDataAsset` and `ProjectRiftMission` PrimaryAsset type;
- `GetSelectedTeamMissionId()`;
- `BuildRiftTravelURL()`;
- `StartRiftMission()` and `ServerStartRiftMission()`;
- settlement `MissionId` and deterministic reward `RunSeed`.

## Native fallback presentation

A native `UPRMissionSelectionWidget` binds to replicated `APRGameState` state and a local catalog view. It exposes Blueprint-friendly rows and actions for:

- contract display name and lock reason;
- biome, difficulty and modifiers;
- reward type/range preview;
- selected state and host-only selection action;
- controller-safe next/previous focus and confirmation.

The widget never predicts selection success and never owns authoritative state. Missing optional icons or WBP assets degrade to text-only presentation.

## Error handling

All invalid selection and start requests fail closed, leave the previous selected definition unchanged, and return a concise owner-only diagnostic. Non-host requests do not rotate the Seed or clear player readiness. A failed travel-context parse prevents the Rift mission from starting.

Changing the selected contract is atomic from the replicated-state perspective: a fully validated definition replaces the previous definition in one setter call.

## Tests and verification

Focused automation will cover:

- reflected types and public compatibility contracts;
- authored-contract validation and reward-preview range validation;
- deterministic definition generation and signature sensitivity;
- host-only selection, locked chapter rejection and unchanged state after rejection;
- replicated definition identity and team Ready reset after successful selection;
- compact travel URL round trip, legacy MissionId compatibility and malformed/tampered context rejection;
- Rift reuse of the authoritative Seed and definition;
- settlement/receipt ContractVersion recording and validation;
- return-to-lobby reselection and existing lobby/Rift regressions;
- native fallback widget/ViewModel contracts without formal WBP assets.

Final verification runs the focused tests, Quick/Smoke, full ProjectRift regression, Development package plus LocalSmoke, Shipping package, test-module exclusion, NullRHI launch, and the shared `UnrealEditor.modules` hash guard.

Manual acceptance in ProjectA 8001 two-player PIE verifies that both players see identical contract/reward data, non-host and locked-contract RPCs fail, the same Seed reproduces the same definition, and a returned lobby can select another mission.

## Explicit exclusions

- No second session system or second mission PrimaryAsset registry.
- No final mission-table WBP, icons, animations or three production contract assets.
- No final equipment instance generation in reward previews.
- No objective graph, optional-objective execution, encounter director, enemy ecology or boss behavior from later v0.8.x versions.
- No profile schema upgrade.
