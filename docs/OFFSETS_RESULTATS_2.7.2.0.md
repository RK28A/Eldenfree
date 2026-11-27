# EldenFree — OFFSETS_RESULTATS_2.7.2.0

Scan AOB statique sur le .exe du disque. ImageBase 0x140000000. Tous les patterns
de la section 2 et les patchs code de la section 9 matchent. (0.1 version = **2.7.2.0**)

## Section 2 — signatures de base (toutes `ok`)

| # | Cible | RVA match | Résolu (RVA / VA) |
|---|---|---|---|
| 2.1 | GameDataMan | 0x256560 | 0x3D64FB8 / 0x143D64FB8 |
| 2.2 | WorldChrMan | 0x3FFC86 | 0x3D6D018 / 0x143D6D018 |
| 2.3 | CSEventFlagMan (principal) | 0xD470C9 | 0x3D6F4D8 / 0x143D6F4D8 |
| 2.3 | CSEventFlagMan (secours) | 0xD47026 | 0x3D6F4D8 (identique ✓) |
| 2.4 | SoloParamRepository | 0xD3174C | 0x3D88F78 / 0x143D88F78 |
| 2.5 | MsgRepository | 0xAFF63B | 0x3D84588 / 0x143D84588 |
| 2.6 | MapItemMan | 0x5ACC9E | 0x3D6EAE0 / 0x143D6EAE0 |
| 2.7 | ItemGib (−0x52) | 0x561902 | fn 0x5618B0 / 0x1405618B0 |
| 2.8 | EzState (−0x47) | 0xEA8837 | fn 0xEA87F0 / 0x140EA87F0 |
| 2.9 | Téléportation (+2) | 0x59AF0E | fn 0x59AF10 / 0x14059AF10 |
| 2.10 | CSLuaEventMan | 0x5B2D66 | 0x3D6EED8 / 0x143D6EED8 (présent ✓, pas introuvable) |
| 2.11 | SpEffect ajout (−0x1D) | 0x3E8DDD | fn 0x3E8DC0 / 0x1403E8DC0 |
| 2.12 | SpEffect retrait | 0x4F7760 | fn 0x4F7760 / 0x1404F7760 |
| 2.13 | Singletons FD4 (par nom) | — | ok, 6671 sites (motif générique balayé, normal) |

## Section 9 — patchs code (toutes `ok`)

| # | Patch | RVA match | VA |
|---|---|---|---|
| 9.5 | Saut infini p1 | 0x475EF5 | 0x140475EF5 |
| 9.6 | Saut infini p2 | 0x475F2F | 0x140475F2F |
| 9.12 | Torrent partout (donjons) | 0xCF28D2 | 0x140CF28D2 |
| 9.13 | Carte en combat (A) | 0x9C4E9B | 0x1409C4E9B |
| 9.13 | Carte en combat (B `74 ?? C7 45`) | 0x1D4E13 | générique (66 sites) — s'applique relativement à A |

**Toutes les signatures de la section 2 et les patchs 9.5/9.6/9.12/9.13 → `ok`.**
Rien d'`INTROUVABLE` sur cette version. 2.10 (marqué ⚠️ "commenté chez eux")
existe bien ici.

## Non résolvable en statique (runtime / data — Cheat Engine requis)
Chaînes de pointeurs (sect. 3, 9, 10, 11), offsets de struct, offsets de params
(sect. 5), IDs de flags (sect. 4), IDs d'objets et noms FR (sect. 6, 7), events
EzState des menus (sect. 8). Ces valeurs sont sourcées (TGA / fromsoftware-rs /
libER) mais se confirment jeu lancé. Les lignes ⚠️/🔧 de ces sections restent à
relever dans CE.

## Note
v2.7.2.0, SteamStub présent mais `.text` principal non chiffré sur le disque.
Après tout patch du jeu, refaire le scan : les RVA changent à chaque version.

---

## Reste de la section 2 (doc mis à jour)

| # | Élément | Statique ? | Résultat |
|---|---|---|---|
| 2.14 | CSLuaEventManager proxy `+08`/`+18` | non | offsets de struct, runtime (CE) |
| 2.15 | Carte en combat, patch B | **partiel** | voir ci-dessous |
| 2.16 | Singletons FD4 par nom (CSMenuMan, MapItemMan) | non | compte N déterminé au runtime |

**2.15 — patch B introuvable à côté de A.** Patch A = `0x1409C4E9B`
(`call … / test al,al / jnz / cmp [rbx+3EC2],al / jnz / and esi,FFFFFFFE`).
Le motif B `74 ?? C7 45` n'a **aucune occurrence adjacente** : la plus proche
est à `0x140997A42` (−0x2D459) ou `0x1409FE288` (+0x393ED). Les seuls octets
`74` près de A (A−0x36, A+0x15, A+0x29) sont des octets `74 24` internes à des
`mov [rsp+40],esi` (`89 74 24 40`), pas des sauts.

→ L'heuristique « prendre le `74 ?? C7 45` le plus proche de A » est **non
fiable** sur 2.7.2.0 : elle vise à ~180 ko de A. Pour B il faut une vraie
signature autonome (côté Erd-Tools-CPP), pas la proximité avec A. Rien à
figer en statique ici tant que la signature B exacte n'est pas fournie.

Tout le reste (sections 3–12 : chaînes de pointeurs, params, flags, IDs, noms,
events de menus) = runtime, Cheat Engine + table TGA, jeu lancé.
