# Alert tier notes

## Outcome
No net code change. A light-threshold reorder (50/60/70 -> Tier 1/2/3) was tried
and **reverted**, because the assignment spec defines the opposite order.

## Source of the values (assignment spec, mite-risk tiers)
| Tier | Temp (C) | RH (%) | Light (% full sun) | Spec wording |
|------|----------|--------|--------------------|--------------|
| 1 Watch    | 27-30 | 50-60 | > 70 | "If light levels exceed 70%" |
| 2 Alert    | 30-34 | < 50  | > 60 | "If light levels remain above 60%" |
| 3 Critical | > 34  | < 40  | > 50 | "Light levels above 50% in this tier" |

Light thresholds in the spec are conditional escalators within a tier, so a
hotter tier uses a *lower* light number. They are not independent triggers.

## Where the values live in code
Firmware: `Core/Src/main.c` `EvaluateAlertTier()`
- Tier 3: lines 109-111 (temp > 34, RH < 40, light > 50)
- Tier 2: lines 114-116 (temp 30-34, RH < 50, light > 60)
- Tier 1: lines 119-121 (temp 27-30, RH 50-60, light > 70)
- Actions text: `AlertRecommendedAction()`, `main.c` ~137-149

Dashboard: no longer holds thresholds. `dashboard/index.html` `ALERT_TIERS` maps the
tier number the device publishes as ThingSpeak **field6** to display text.
Old entries without field6 show "Alert tier not reported".

## Known deviations from the spec (kept deliberately, mention in report)
1. **OR instead of AND.** Spec says "temperature X *and* humidity Y"; code
   triggers a tier if *any* listed condition matches. Chosen as the cautious
   behaviour (e.g. 32 C / 55% RH raises Tier 2; strict spec gives no tier).
2. **Light acts as a standalone trigger**, not only within a tier's temp/RH
   band, so light > 50% alone yields Tier 3 (the overlap that looked like a bug).
3. RH > 60% matches no tier (spec silent). Shared boundaries (30, 34) resolve
   to the higher tier because tiers are checked from 3 down.

## Single source of truth
Thresholds now live only in firmware (`EvaluateAlertTier()`). `ThingSpeak_Publish()` takes
an `alert_tier` argument and sends it as field6. **Enable Field 6 on the ThingSpeak
channel and reflash** or the dashboard will show "Alert tier not reported".
The tier display text (titles, actions) is still written in both C and the dashboard.

## Not changed / open
- Hysteresis and stale-reading handling.
- Hard-coded WiFi password and ThingSpeak key in `main.c:63-66`.
