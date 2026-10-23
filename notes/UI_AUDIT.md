# Presentation audit: the game's UI layer against the client

What the headless logic leaves out of the game, and where the client plays
it instead. The inventory comes from the game assembly
(`Spire.ApiDump --ui-surface logic/lib/game/sts2.dll work/ui_surface.json`,
`tools/ui_audit.py`):

- **ui_calls**: UI methods game code calls (the 'ui' patch records them
  as cues; the client must play the ones that show or sound something).
- **godot_calls**: Godot objects game code drives itself (tweens, node
  positions, visibility): null headless, so skipped unless ported.
- **test_mode**: game methods that skip presentation in TestMode.
- **events**: game events UI classes subscribe to (their handlers are
  presentation the client must reproduce from the steps).

Status: `ported` (where), `added` (this audit), `n/a` (multiplayer, debug,
platform, no visible effect), `open`.

## Infrastructure

| Item | Status |
| --- | --- |
| Fire-and-forget presentation tasks (TaskHelper.RunSafely in game code) run in parallel with the turn; headless they ran inline and their waits delayed everything after them | added (Steps branches: `delay` on their cues) |
| Animation lengths the UI returns (NCreature.StartDeathAnim: AfterDeath's deathAnimLength) are 0 headless | added (anim_lengths.json from the .pck, StandIns' StartDeathAnim answer) |
| Creature.SetNodeVisible (CanvasItem.Visible on the creature node) is not recorded (setters are skipped) | added (node_visible cue) |
| Card views for cards moving between piles (the client only knows the hand's cards) | added (card_create carries the card's view) |
| Creature.GetVfxContainer / GetBackVfxContainer are null in TestMode: `container?.AddChildSafely(X.Create(...))` never even makes the effect | added (stand-in containers: block sparks, heal numbers, every attack's custom hit effect) |
| A UI method bound as a delegate (`WithHitVfxNode(NBigSlashVfx.Create)`) is no call the redirect sees: its stripped body would run | added (AttackCommand's custom effects wrapped; the call is recorded) |
| The adapter queued card plays itself, skipping CardModel.EnqueueManualPlay's OnEnqueuePlayVfx | added (Inflame, Fan of Knives, Hellraiser, Pyre) |
| SpineAnimationAccess (a struct in the UI's namespace): its methods are not redirected and call MegaSprite / MegaAnimationState inside that namespace | added (spine.access.*: those calls go through the 'ui' hook) |
| Effects whose lambdas read TestMode-null values (the Mecha Knight's VfxCmd.GetSideCenter(...).Value) | added (vfx.side_center; wrapped effects never stop an attack) |
| TestMode-off branches with presentation only | added (testmode.presentation.is_off: Tunneler, Vantom, Waterfall Giant, Bygone Effigy, Infested, Bouncing Flask, Hand of Greed; hooks reading TestMode as on for Nightmare, Doll Room, Sandpit) |

## Combat: cards and piles

| Game behaviour | Status |
| --- | --- |
| CardPileCmd tweens awaited (draw, play, exhaust from a pile) | ported (card.pile_tween) |
| A card exhausted from the draw / discard pile: created at that pile, to the play pile, 0.5 s (Fast 0.2), burns | added (card_create) |
| A card autoplayed from the draw pile (Havoc, Mayhem): created at the draw pile, to the play pile | added (card_create) |
| A card moved pile to pile without a node (draw <-> discard, exhaust -> draw ...): NCardFlyShuffleVfx silhouette | added (card_fly) |
| A card into the hand from the discard / exhaust pile: created at that pile | added (from the pile's button) |
| NCombatCardPile.AddCard / RemoveCard: a pile's count rises as each card lands (with a bump), the shuffle's discard count falls as each leaves | added |
| NCard.PlayRandomizeCostAnim (Confused, Slither, Snecko Oil) | added (card_randomize_cost) |
| NHandCardHolder.Flash on EnergyCostChanged, KeywordsChanged, ReplayCountChanged, StarCostChanged, Upgraded, AfflictionChanged | added (card_flash) |
| CardModel.OnEnqueuePlayVfx (Inflame, Fan of Knives ...) | added (adapter; NFanOfKnivesVfx ported) |
| CardCmd.FlashRelics (NCard.FlashRelicOnCard: the relic's flash on a card in the deck preview) | open |
| NCard.OnEnchantmentStatusChanged / OnEnchantmentChanged / OnAfflictionChanged (the card's look) | ported (the hand's card views at each sync) |

## Combat: creatures

| Game behaviour | Status |
| --- | --- |
| NBlockSparkVfx / NDamageBlockedVfx (CreatureCmd.Damage, fully blocked) | added |
| NHealNumVfx (CreatureCmd.Heal) | added |
| NCreatureStateDisplay.AnimateInBlock (block from 0) | added |
| InfestedPower: Wrigglers revealed after the death animation, elite second phase music | added |
| StockPower: the replacement revealed after the death animation | added (branch, death length) |
| Monster moves aiming a node before the attack (BowlbugSilk, BygoneEffigy, KinFollower, LeafSlimeM, TwigSlimeM, MagiKnight, SlumberingBeetle, Tunneler, ThievingHopper, FuzzyWurmCrawler) | added (SpecialNode.AimAt / ReachTo / SetPosition) |
| TorchHeadAmalgam.OnDieToDoom (its lights out) | added |
| Fabricator's bots: SetBotFallPosition | added |
| Vantom: charge-ups (waits, sounds, track-1 animations, music), ShrinkPower's ScaleTo | added |
| WaterfallGiant build-up track and music | added |
| SurroundedPower.FaceDirection (bodies and form effects flipped, music) | added |
| SandpitPower (The Insatiable): the player drawn in, pulled in at the end | added |
| OstyCmd.Summon fade-in | added |
| CreatureCmd.Escape (no death animation) | added |
| DoomPower / ReattachPower: AnimDisableUi, AnimHideIntent | added |
| Test Subject's burn (NTestSubjectBurnVfx) | added |
| SpineAnimationAccess (LagavulinMatriarch, Queen, SoulNexus, WaterfallGiant) | added |
| ReattachPower's last segment: one NMonsterDeathVfx over all segments | open |
| RollingBoulderPower (NRollingBoulderVfx rolling over the enemies, each hit as it passes) | open |
| NMinionDiveBombVfx (Minion Dive Bomb) | open |

## Rooms, rewards, top bar

| Game behaviour | Status |
| --- | --- |
| NBossMapPoint (the act's boss on the map: spine or placeholder) | added |
| NNormalMapPoint's shader (tones near the icon's tan take the map's colour) | added (EFFECT_MAP_POINT) |
| NTopBarBossIcon.RefreshBossIcon | ported (top bar from the context) |
| NRelicInventory.AnimateRelic (RelicCmd.Obtain) | ported (top bar, from the player's relics) |
| NPotionContainer.AnimatePotion / PlayAddFailedAnim / Discard | ported (top bar, from the player's potions) |
| NGlobalUi.ReparentCard (the card reward's card to the deck) | added (DeckPreviews.fly_to_deck) |
| NTopBarHp.LerpAtNeow | added |
| NSimpleCardsViewScreen (Pandora's Box) | open |
| LordsParasol.PurchaseEverything | added (branch) |
| KindleRestSiteOption.PlayKindleVfx, NRestSmokeVfx, NDesaturateTransitionVfx, NRestSiteCharacter.Shake | open |
| NMerchantInventory.OnCardRemovalUsed, merchant purchase events | open |
| Nightmare's vignette and hands (NNightmareHandsVfx) | open (vignette ported) |
| Doll Room ambience | added |
