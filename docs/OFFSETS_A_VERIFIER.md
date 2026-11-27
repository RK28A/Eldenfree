# EldenFree — tout ce qu'il faut vérifier en jeu

Ce fichier liste **toutes** les valeurs, adresses, signatures et méthodes utilisées par le mod, y compris celles qui
marchent déjà mais reposent sur une estimation ou une détection automatique (« bidouille ») au lieu d'une valeur exacte.
Le but : remplacer chaque bidouille par une valeur précise que tu relèves en jeu.

## Comment faire

- **Outils** : Cheat Engine 7.5+ et la table **The Grand Archives (TGA)** :
  https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA/releases
- **Lancement** : lance le jeu **hors ligne** avec `EldenFree.exe`, puis attache Cheat Engine à `eldenring.exe` et ouvre la table TGA.
- **Chaînes d'adresses** : `WorldChrMan → 10EF8 → 0 → 190 → 68 → 70` veut dire :
  1. Lire le pointeur `WorldChrMan`, puis lire le pointeur à `+10EF8`, puis à `+0`, `+190`, `+68`.
  2. La valeur cherchée est à `+70` de la dernière adresse.
  3. Dans Cheat Engine : *Add address manually → Pointer*. Les offsets s'entrent du dernier au premier.
- **Abréviations** :

  | Abréviation | Chaîne | Ce que c'est |
  |---|---|---|
  | `PGD` | `GameDataMan → 08` | données du joueur |
  | `PlayerIns` | `WorldChrMan → 1E508` | ton personnage |
  | `Modules` | `PlayerIns → 190` | modules du personnage |

  Dans TGA, `PlayerIns` s'obtient aussi par `WorldChrMan → 10EF8 → 0`.
- **Fichiers à joindre** : `EldenFree.log` et `EldenFree.ini` (dans `%LOCALAPPDATA%\EldenFree`).
- **Colonne Fiabilité** :
  - ✅ **confirmé** : valeur reprise d'une source à jour (TGA, fromsoftware-rs, Erd-Tools).
  - ⚠️ **déduit** : calculé ou repris d'une source plus ancienne, pas vérifié sur ta version.
  - 🔧 **bidouille** : détecté automatiquement en jeu par heuristique. Le but est de le remplacer par une valeur fixe.
- **Colonne Ta réponse** : écris la valeur relevée, ou oui / non, ou ce que tu observes.

---

## 0. Infos générales

Déjà relevé : version **2.7.2.0** (voir `OFFSETS_RESULTATS_2.7.2.0.md`).

| # | À relever | Où | Ta réponse |
|---|---|---|---|
| 0.2 | DLC Shadow of the Erdtree installé ? | | |
| 0.3 | Langue du jeu | | |
| 0.4 | Manette : modèle + branchement (USB / Bluetooth / adaptateur sans fil) | | |
| 0.5 | Steam Input activé pour Elden Ring ? | Steam → Elden Ring → Propriétés → Manette | |
| 0.6 | Résolution / plein écran ou fenêtré | options du jeu | |

---

## 1. Démarrage, lanceur, overlay

| # | Élément | Valeur actuelle | Fiabilité | Comment vérifier | Ta réponse |
|---|---|---|---|---|---|
| 1.1 | Lancement hors ligne | variables `SteamAppId=1245620` + `SteamGameId=1245620`, lance `Game\eldenring.exe` | ✅ (lanceur Hexinton) | le jeu démarre-t-il sans EAC ? | |
| 1.2 | Injection de la DLL | `LoadLibraryW` via `CreateRemoteThread` après le chargement de `kernel32` | ✅ | le panneau s'ouvre-t-il avec F10 ? | |
| 1.3 | Hook DX12 | `IDXGISwapChain::Present` (index 8), `Present1` (22), `ResizeBuffers` (13/39), `ExecuteCommandLists` (10) | ✅ | le panneau s'affiche-t-il en plein écran **et** en fenêtré ? | |
| 1.4 | Format du swapchain | lu au démarrage | — | ligne `Overlay initialise (N buffers, format X)` du journal | |
| 1.5 | Souris dans le panneau | `GetCursorPos` + entrée brute + DirectInput, `ClipCursor`/`SetCursorPos` bloqués | 🔧 | la souris bouge-t-elle bien ? clics OK ? | |
| 1.6 | Le jeu lit-il le clavier/la souris via DirectInput ? | hooks `GetDeviceState` / `GetDeviceData` | ⚠️ | ligne `Hooks d'entree : DInput X, XInput Y` du journal | |
| 1.7 | Le jeu lit-il la manette via XInput ? | hook `XInputGetState` (xinput1_4 / 1_3 / 9_1_0) | ⚠️ | même ligne : XInput doit être ≥ 1 | |
| 1.8 | Bouton Menu maintenu 0,5 s → panneau | état START capturé dans XInput | 🔧 | marche ? un appui court ouvre-t-il encore le menu du jeu ? | |
| 1.9 | Navigation manette (croix, A, B, LB/RB, stick droit) | ImGui | — | tout répond ? | |

---

## 2. Signatures (AOB) — adresses de base

Toutes trouvées sur 2.7.2.0 (scan statique, voir `OFFSETS_RESULTATS_2.7.2.0.md`). Il ne reste que :

| # | Élément | Valeur actuelle | Fiabilité | Comment vérifier | Ta réponse |
|---|---|---|---|---|---|
| 2.14 | CSLuaEventManager : proxy / imitation | `CSLuaEventMan → +08` / `+18` | ⚠️ TGA | la téléportation après Greyoll t'emmène-t-elle à Fort Faroth ? | |
| 2.15 | Carte en combat, patch B | signature **complète** Erd-Tools-CPP : `74 ?? C7 45 ?? ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? 48 8D 05 ?? ?? ?? ?? 48 89 45 ?? 48 8D 4D ?? E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 48 83 BF` → octet `74` remplacé par `EB` | ⚠️ Erd-Tools-CPP | refais le scan statique avec **cette** signature entière (pas seulement `74 ?? C7 45`) : combien d'occurrences ? adresse(s) ? | |
| 2.16 | Singletons FD4 trouvés par nom | `CSMenuMan`, `MapItemMan` | ✅ TGA | journal `Singletons FD4 : N trouves` (N = ?) | |

---

## 3. Personnage, inventaire, données

| # | Élément | Valeur actuelle | Fiabilité | Comment vérifier | Ta réponse |
|---|---|---|---|---|---|
| 3.1 | PlayerIns | `WorldChrMan + 1E508` (sinon recherche auto) | ✅ fromsoftware-rs | journal `PlayerIns @ WorldChrMan+0x…` | |
| 3.2 | Modules du perso | `PlayerIns + 190` | ✅ | | |
| 3.3 | PV / PV max | `Modules → 0 → 138` / `13C` | ✅ | compare avec TGA `Hero > PlayerParam > Hp` | |
| 3.4 | Position | `Modules → 68 → 70` (x, y, z) | ✅ | TGA `Coordinates` | |
| 3.5 | Runes | `PGD + 6C` | ✅ TGA | | |
| 3.6 | PC (FP) | `PGD + 1C` | ✅ fromsoftware-rs | | |
| 3.7 | PV dans PGD | `PGD + 10` / `14` | ✅ | | |
| 3.8 | Nombre de morts | `GameDataMan + 94` | ✅ fromsoftware-rs | le nombre affiché dans Diagnostic est-il juste ? | |
| 3.9 | Charge max (affichage) | `PGD + 98C` (float) | ✅ fromsoftware-rs | | |
| 3.10 | Emplacement d'arme actif (main droite) | `PGD + 32C` (0, 1, 2) | ✅ TGA | | |
| 3.11 | Armes main droite 1 / 2 / 3 | `PGD + 5FC` / `604` / `60C` | ✅ TGA | | |
| 3.12 | Armes main gauche 1 / 2 / 3 | `PGD + 5F8` / `600` / `608` | ✅ TGA | | |
| 3.13 | **Liste d'inventaire** | 🔧 recherche auto d'un tableau `{poignée, ID, quantité}` (pas 0x18) près de PGD | 🔧 | donne l'adresse exacte : TGA `Hero > EquipInventoryData` (adresse du tableau d'objets + taille d'une entrée) | |
| 3.14 | Nombre d'entrées utilisées de l'inventaire | non utilisé (limite 3000) | 🔧 | TGA `EquipInventoryData` → `normal_items_len` | |
| 3.15 | Format d'une entrée d'inventaire | `+0` poignée, `+4` ID objet, `+8` quantité | ✅ fromsoftware-rs | | |
| 3.16 | Liste des sorts équipés | 🔧 recherche auto d'un pointeur dont `+8 == PGD+2B0`, puis `+10` sorts, `+80` emplacement sélectionné | 🔧 | TGA `Hero > EquipMagicData` : adresse et offset de l'emplacement sélectionné | |
| 3.17 | Flags perso | `Modules → 0 → 19B` : bit 0 NoDead, 1 NoDamage, 2 NoFP, 3 NoStamina | ✅ TGA | | |
| 3.18 | Objet en cours d'utilisation | `PlayerIns + 160` | ✅ fromsoftware-rs | quand tu bois une fiole merveilleuse, quelle valeur ? | |
| 3.19 | Liste des effets actifs (SpEffect) | `PlayerIns + 178 → 8` (liste chaînée : ID `+8`, suivant `+30`, durée `+48`) | ✅ fromsoftware-rs / TGA | | |
| 3.20 | Vitesse d'animation | `Modules → 28 → 17C8` | ✅ TGA | | |
| 3.21 | Animation en cours | `Modules → 18` : file à `+20` (pas 0x10), index de lecture `+C4` | ✅ fromsoftware-rs | TGA affiche-t-il l'ID d'animation courant ? si oui, compare | |
| 3.22 | Demande de mise à jour de position physique | `Modules → 68 → 91` (byte) | ⚠️ fromsoftware-rs | passe-t-il à 1 quand TGA te téléporte ? | |
| 3.23 | Multiplicateur de mouvement | `Modules → 68 → 1C4` (float) | ⚠️ fromsoftware-rs | valeur normale ? (sans doute 1) | |
| 3.24 | Multiplicateur de gravité | `Modules → 68 → 1CC` (float) | ⚠️ fromsoftware-rs | valeur normale ? | |
| 3.25 | Au sol / en chute | `Modules → 68 → 1D1` (touche le sol) / `1D0` (en chute) | ✅ (confirmé par un patch Hexinton) | | |

---

## 4. Event flags (carte, grâces, boss, cloches)

| # | Élément | Valeur actuelle | Fiabilité | Comment vérifier | Ta réponse |
|---|---|---|---|---|---|
| 4.1 | Lecture/écriture des flags | arbre `CSEventFlagMan +38`, diviseur `+1C`, blocs `+28`, taille `+20`, mode 1 = index, mode 2 = pointeur, bit `7 - (n % 8)` | ✅ fromsoftware-rs | les interrupteurs carte/grâces montrent-ils le bon état ? | |
| 4.2 | Greyoll vaincu | `1050400800` | ✅ TGA | | |
| 4.3 | Greyoll butin déjà donné | `1050407800` | ✅ TGA | | |
| 4.4 | Greyoll « au repos » | `1050400599` (mis à 0 au respawn) | ⚠️ TGA | après respawn, est-elle couchée ? debout ? invisible ? | |
| 4.5 | Grâce de téléportation après Greyoll | Fort Faroth : flag `76453`, entité `1051391950` | ✅ TGA | arrives-tu à la grâce ? | |
| 4.6 | Fragments de carte (base) | `62004–62012, 62020–62022, 62030–62032, 62040–62041, 62050–62052, 62060–62064`, souterrain `82001` | ✅ TGA | | |
| 4.7 | Fragments de carte (DLC) | `62080–62084`, carte de l'Ombre `82002` | ✅ TGA | | |
| 4.8 | Pièces de carte (param) | WorldMapPieceParam : ouverture `+4`, acquisition `+18` | ⚠️ libER | | |
| 4.9 | Objets « Carte : … » au pied des stèles | 🔧 ItemLotParam `+0` objets, `+20` catégories, `+80` flag ramassé, identifiés **par leur nom** (« Carte : » / « Map: ») | 🔧 | les fragments réapparaissent-ils ? donne les ID des objets « Carte : … » (TGA ItemGib) | |
| 4.10 | Grâces jeu de base | 314 flags (71000–76960) | ✅ TGA | | |
| 4.11 | Grâces DLC | 105 flags | ✅ TGA | | |
| 4.12 | **Redécouverte d'une grâce reverrouillée** | seul le flag de la grâce est remis à 0 | ⚠️ | après reverrouillage + rechargement, peux-tu la redécouvrir ? si non, quels flags changent quand tu en découvres une neuve (TGA `Event Flags`) ? | |
| 4.13 | Cloches de mineur (forge) | `11109751–11109759` + persistance NG+ `60701–60709` | ✅ TGA | les pierres sont-elles en vente chez les Sœurs jumelles ? | |

---

## 5. Params (données du jeu)

| # | Élément | Valeur actuelle | Fiabilité | Ta réponse |
|---|---|---|---|---|
| 5.1 | Table des params | `SoloParamRepository + 88 + i×48` → ResCap `+80` → `+80` = en-tête | ✅ Erd-Tools-CPP | journal `Params: N tables` (N = ?) |
| 5.2 | Lignes d'une table | en-tête `+0A` nombre, lignes à `+40` (pas 0x18 : ID `+0`, offset `+8`) | ✅ | |
| 5.3 | SpEffectParam | `stateInfo +156`, `equipWeightChangeRate +E4`, taille ligne `0x390` | ⚠️ libER | |
| 5.4 | Voyage depuis les mini-donjons | SpEffect `4270` → `stateInfo = 0` | ✅ TGA | ça marche ? |
| 5.5 | MagicParam | PC `+10`, type d'effet `+1E` (2 = effet), enchantement `+31` bits 2/3, PC chargé `+5C` | ⚠️ libER | |
| 5.6 | EquipParamGoods | quantité max `+3A`, max coffre `+70`, type `+3E` | ✅ TGA | |
| 5.7 | EquipParamAccessory | effet `+4` | ⚠️ libER | |
| 5.8 | BonfireWarpParam | flag `+4`, entité `+8` | ⚠️ libER | |

---

## 6. Textes du jeu (noms)

| # | Élément | Valeur actuelle | Fiabilité | Comment vérifier | Ta réponse |
|---|---|---|---|---|---|
| 6.1 | Accès aux textes | `MsgRepository → 8 → [index × 8]` | ✅ TGA | | |
| 6.2 | Noms d'armes | index `11, 115, 310, 410` | ✅ TGA | les noms d'armes s'affichent-ils ? journal `Noms d'armes : … (dague=ok)` | |
| 6.3 | Noms d'objets | index `10, 111, 319, 419` | ✅ TGA | | |
| 6.4 | Noms de talismans | index `13, 113, 316, 416` | ✅ TGA | | |
| 6.5 | Format en mémoire | `+0C` groupes, `+18` **pointeur absolu** vers les offsets, groupes à `+28` | ✅ TGA | | |
| 6.6 | **Arôme de sang bouillant** | 🔧 recherche par nom (« arôme »+« bouill », « sang »+« bouill », « bloodboil ») | 🔧 | **nom exact FR** + **ID de l'objet** (TGA `ItemGib`) | |
| 6.7 | **Talisman de charge** | 🔧 recherche par nom (« grande »+« jarre », « jarre »+« géante », « arsenal ») | 🔧 | **nom exact FR** + **ID** + ID de son effet | |
| 6.8 | Fléau | 🔧 arme dont le nom contient « fléau » / « flail » | 🔧 | **nom exact** et **ID** de ton fléau (TGA `ChrAsm 2 > PrimaryRightWep` quand il est équipé) | |

---

## 7. Objets (duplication, arôme)

| # | Élément | Valeur actuelle | Fiabilité | Ta réponse |
|---|---|---|---|---|
| 7.1 | Appel ItemGib | `ItemGib(MapItemMan, données, tampon, 0)` | ✅ TGA | la duplication marche-t-elle ? |
| 7.2 | Données ItemGib | `{1, ID, quantité, 0 (arme) / -1 (autre), cendre -1}` | ✅ TGA | |
| 7.3 | Catégories d'ID | arme `0x0`, armure `0x1`, talisman `0x2`, objet `0x4`, cendre `0x8` (bits hauts) | ✅ | |
| 7.4 | Niveau / affinité conservés | ID complet de l'arme (niveau inclus) | ⚠️ | la copie a-t-elle le même +X et la même affinité ? |
| 7.5 | Cendre de guerre de la copie | par défaut (-1) | — | |

---

## 8. Menus du jeu (onglet Menus)

| # | Menu | Événement EzState + arguments | Fiabilité | Fonctionne ? (oui / non / vide) |
|---|---|---|---|---|
| 8.1 | Renforcer une arme | `24 {0}` | ✅ TGA | **liste vide** — teste TGA `Scripts > Npc Menus > Misc > Blacksmith` : tes armes y sont-elles ? |
| 8.2 | Table de forge | `24 {1}` | ✅ TGA | |
| 8.3 | Cendres de guerre | `48 {}` | ✅ TGA | |
| 8.4 | Renaissance | `113 {}` | ✅ TGA | |
| 8.5 | Apparence | `81 {0}` | ✅ TGA | |
| 8.6 | Monter de niveau | `31 {}` | ✅ TGA | |
| 8.7 | Fioles | `105 {}` | ✅ TGA | |
| 8.8 | Sorts | `28 {0,0}` | ✅ TGA | |
| 8.9 | Physick | `130 {}` | ✅ TGA | |
| 8.10 | Grandes runes | `137 {}` | ✅ TGA | |
| 8.11 | Tenues | `142 {111000,111399}` | ✅ TGA | |
| 8.12 | Coffre | `30 {}` | ✅ TGA | |
| 8.13 | Esprits | `136 {}` | ✅ TGA | |
| 8.14 | Propriétaire du menu | poignée du joueur `PlayerIns + 8` (TGA utilise -1 pour ces menus) | ⚠️ | |

---

## 9. Torrent

| # | Élément | Valeur actuelle | Fiabilité | Comment vérifier | Ta réponse |
|---|---|---|---|---|---|
| 9.1 | Trouver Torrent | ChrSet des invocations `WorldChrMan + 10F90` (capacité `+10`, entrées `+18`, pas 0x10), chr id `8000` à `PlayerIns + 188` | ✅ fromsoftware-rs | Diagnostic : Torrent `OK` quand monté ? | |
| 9.2 | Dernière monture | `Modules → E8 → 18` | ✅ fromsoftware-rs | | |
| 9.3 | Je suis monté | `Modules → E8 → 163` | ✅ fromsoftware-rs | | |
| 9.4 | Vitesse de Torrent | vitesse d'animation de Torrent (`… → 28 → 17C8`), seulement au sol | 🔧 | | |
| 9.5 | Saut infini (patchs trouvés : `0x140475EF5`, `0x140475F2F`) | Hexinton | ✅ signatures | le saut infini marche-t-il ? après une chute ? après une réapparition ? | |
| 9.7 | **Vitesse de chute de Torrent** | 🔧 champ trouvé automatiquement pendant une chute (dans `… → 68`, ou les objets à `+98` / `+A0`) | 🔧 | valeur `FallVelocityField` dans l'ini + dans TGA, la vitesse verticale de Torrent si tu la trouves | |
| 9.8 | Hauteur de saut | multiplie la vitesse verticale (si 9.7 trouvé) sinon amplifie la montée | 🔧 | | |
| 9.9 | **Vol : orientation** | 🔧 décalage d'angle calculé en galopant ; écrit l'orientation `… → 68 → 50` / `60` (quaternion) | 🔧 | Torrent regarde-t-il dans la bonne direction en vol ? | |
| 9.10 | **Vol : direction caméra** | `WorldChrMan + 1ECF0` → matrice : droite `+10`, avant `+30`, position `+40` | ⚠️ fromsoftware-rs | | |
| 9.11 | **Vol : vitesse** | 13 m/s × multiplicateur Torrent | 🔧 | vitesse réelle de course de Torrent (m/s) si tu peux la mesurer | |
| 9.12 | Torrent partout (patch trouvé : `0x140CF28D2`) | met `[rax+36] = 0` (version Hexinton exacte) | ✅ signature | encore des selles en boucle ? peux-tu descendre ? | |
| 9.13 | Carte en combat (patch A trouvé : `0x1409C4E9B`, B : voir 2.15) | Erd-Tools-CPP | ✅ signature A | la carte s'ouvre-t-elle avec des ennemis proches ? voyage rapide possible ? | |

---

## 10. Chute, runes

| # | Élément | Valeur actuelle | Fiabilité | Ta réponse |
|---|---|---|---|---|
| 10.1 | Minuteur de chute | `Modules → 70 → 18` | ✅ fromsoftware-rs | |
| 10.2 | Sol sans dégâts de chute | `Modules → A0 → 1B` | ✅ fromsoftware-rs | |
| 10.3 | Hauteur de chute de la monture | `Modules → E8 → 130` | ✅ fromsoftware-rs | |
| 10.4 | Détection de chute | 🔧 vitesse verticale mesurée < -4 m/s ou `… → 68 → 1D0` | 🔧 | |
| 10.5 | Protection | NoDead + NoDamage pendant la chute et 0,6 s après | 🔧 | prends-tu des dégâts d'une très grande chute ? |
| 10.6 | Garder les runes | 🔧 à PV = 0 : runes mises à 0, rendues 2,5 s après la réapparition | 🔧 | la tache de sang contient-elle 0 rune ? |

---

## 11. Cast rapide, fléau, charge

| # | Élément | Valeur actuelle | Fiabilité | Comment vérifier | Ta réponse |
|---|---|---|---|---|---|
| 11.1 | Sort lancé | 🔧 baisse de PC ≈ coût du sort sélectionné | 🔧 | | |
| 11.2 | Sort = buff | MagicParam type d'effet `+1E == 2` ou enchantement | ⚠️ | liste les sorts de buff qui ne sont **pas** accélérés | |
| 11.3 | Cendre = buff | 🔧 un effet de 15 s à 15 min apparaît sur toi et tu bouges < 1,5 m | 🔧 | **liste les noms des cendres de buff que tu utilises** et celles d'attaque qui sont accélérées à tort | |
| 11.4 | Animations apprises | ini `CastSpellAnimsV3`, `AshOfWarBuffAnimsV3`, `AshOfWarAttackAnimsV3` | 🔧 | envoie ces lignes | |
| 11.5 | Fiole merveilleuse | objet utilisé `PlayerIns + 160` = `0x400000FA` / `0x400000FB` (IDs 250/251) | ⚠️ | valeur réelle quand tu la bois ? | |
| 11.6 | Compensation du déplacement | 🔧 déplacement horizontal divisé par le multiplicateur, écrit avec la mise à jour physique (3.22) | 🔧 | ton perso avance-t-il encore trop vite pendant une cendre / la rotation du fléau ? | |
| 11.7 | Fléau : rotation | 🔧 animations apprises après une baisse de PC avec un fléau en main (ini `FlailSpinAnims`) | 🔧 | ID d'animation de la rotation si TGA l'affiche | |
| 11.8 | Fléau : gratuit | bits NoFP + NoStamina tant que le fléau est en main | ✅ TGA | | |
| 11.9 | Charge d'équipement | effet du talisman : `equipWeightChangeRate` (`+E4`) modifié et appliqué en permanence | ⚠️ | la charge dans l'écran Statut change-t-elle ? la roulade ? | |

---

## 12. Divers

| # | Élément | Valeur actuelle | Fiabilité | Ta réponse |
|---|---|---|---|---|
| 12.1 | Écritures de code (patchs) | `VirtualProtect` + réécriture si le jeu les efface | ✅ | |
| 12.2 | Lectures mémoire protégées | gestionnaire d'exceptions vectoriel | ✅ | crash du jeu ? quand ? |
| 12.3 | Menu Échap / dialogue (ancien bouton) | `CSMenuMan + 90` états UI, `+80 → 180` dialogue (désactivé) | ⚠️ | |

---

## 13. Adresses exactes pour supprimer les bidouilles

Chaque ligne remplace une détection automatique par une adresse fixe. Donne l'**adresse finale** (chaîne d'offsets
depuis `GameDataMan`, `WorldChrMan` ou `PlayerIns`) et un **exemple de valeur** lue.

| # | À trouver | Indice / où chercher | Remplace | Ta réponse |
|---|---|---|---|---|
| 13.1 | Début du tableau d'objets de l'inventaire | TGA `Hero > EquipInventoryData` ; dans fromsoftware-rs : `PGD + 2B0` (EquipGameData) → `equip_inventory_data` → `items_data` → `normal_items_head` | 3.13 | |
| 13.2 | Nombre d'entrées utilisées (`normal_items_len`) | juste après le pointeur de 13.1 | 3.14 | |
| 13.3 | Capacité (`normal_items_capacity`) | juste avant le pointeur de 13.1 | 3.14 | |
| 13.4 | Tableau des objets clés (`key_items_head`) et sa longueur | même structure | 3.13 | |
| 13.5 | Coffre (`storage`) : pointeur | `PGD + …` après `EquipGameData` et `FaceData` | duplication (pour ne pas compter le coffre) | |
| 13.6 | EquipMagicData : adresse | TGA `Hero > EquipMagicData` | 3.16 | |
| 13.7 | Emplacement de sort sélectionné | TGA `EquipMagicData` → `selected_slot` | 3.16 | |
| 13.8 | ID du sort sélectionné (exemple avec un buff) | | 11.1 | |
| 13.9 | EquipItemData : emplacement rapide sélectionné | TGA `Hero > EquipItemData` | fiole merveilleuse | |
| 13.10 | Poignée GaItem de l'arme main droite | TGA `ChrAsm` (gaitem_handles) | identification de la cendre | |
| 13.11 | Table des GaItems (pour lire la cendre montée sur une arme) | `GameDataMan → 30` (gaitem_game_data) ou TGA `GaItem` | identification de la cendre | |
| 13.12 | ID de la cendre de guerre montée sur l'arme droite | TGA / CE | 11.3 (bloquer par nom) | |
| 13.13 | ID de la compétence (SwordArts) en cours d'exécution, s'il existe un champ | cherche une valeur qui change quand tu lances une cendre (ChrIns, module comportement `… → 190 → 28`) | 11.3 | |
| 13.14 | Charge d'équipement **actuelle** (poids porté) | `PGD + …` près de `98C` (float) | 11.9 | |
| 13.15 | Charge max **réellement utilisée** par le jeu pour la roulade | compare avant/après le talisman de charge | 11.9 | |
| 13.16 | Vitesse verticale de Torrent | `Torrent → 190 → 68 → 98` (proxy Havok) → cherche un float qui vaut la vitesse de chute | 9.7 | |
| 13.17 | Vitesse verticale du joueur à pied | même méthode sur `PlayerIns` | chute | |
| 13.18 | Flag « double saut déjà fait » de Torrent | cherche un byte qui passe à 1 au double saut et revient à 0 à l'atterrissage (`Torrent → 190 → E8 …`, `PlayerIns → 190 → E8 …`) | saut infini | |
| 13.19 | Orientation de Torrent (angle) | `Torrent → 190 → 68 → 50` (quaternion x, y, z, w) : valeurs face au nord / à l'est | 9.9 | |
| 13.20 | Angles de la caméra (lacet / tangage) | `WorldChrMan → 1ECF0` (ChrCam) | 9.10 | |
| 13.21 | Vitesse réelle de galop de Torrent (m/s) | mesure la distance parcourue en 1 s | 9.11 | |
| 13.22 | PV de Torrent | `Torrent → 190 → 0 → 138` | chute à cheval | |

## 14. Animations (IDs)

Si TGA ou CE affiche l'ID d'animation courant (`PlayerIns → 190 → 18` : file à `+20`, index `+C4`), note les ID suivants.

| # | Animation | Ta réponse (ID) |
|---|---|---|
| 14.1 | Debout immobile | |
| 14.2 | Marche / course / sprint | |
| 14.3 | Lancer un sort de buff (début + fin) | |
| 14.4 | Lancer un sort d'attaque | |
| 14.5 | Cendre de buff (ex. Hurlement) | |
| 14.6 | Cendre d'attaque (ex. le salto + coup) | |
| 14.7 | Rotation du fléau (début, boucle, fin) | |
| 14.8 | Boire une fiole merveilleuse | |
| 14.9 | Boire une fiole de PV | |
| 14.10 | Torrent : saut, double saut, chute, atterrissage | |

## 15. Effets (SpEffect)

| # | Élément | Où | Ta réponse |
|---|---|---|---|
| 15.1 | ID de l'effet du talisman de charge (Jarre géante) | TGA `Hero > SpecialEffect` avec le talisman équipé | |
| 15.2 | ID de l'effet de l'Arôme de sang bouillant | même méthode après utilisation | |
| 15.3 | Effets ajoutés par une cendre de buff (ID + durée) | TGA `SpecialEffect` juste après la cendre | |
| 15.4 | Effets ajoutés par une cendre d'attaque (ID + durée) | idem | |
| 15.5 | Effets ajoutés par la fiole merveilleuse | idem | |
| 15.6 | Structure d'une entrée d'effet : ID `+8`, suivant `+30`, durée totale `+48`, temps restant `+40` | à confirmer | |

## 16. Menus et dialogues

| # | Élément | Où | Ta réponse |
|---|---|---|---|
| 16.1 | `CSMenuMan` : adresse | singleton (TGA `CSMenuMan`) | |
| 16.2 | Index de l'élément d'interface « menu Échap » | `CSMenuMan + 90` (tableau d'octets, bit 1 = visible) : quel index passe à visible quand tu ouvres le menu ? | |
| 16.3 | Index « menu de grâce » | même tableau, en t'asseyant à une grâce | |
| 16.4 | ID de dialogue de la grâce (talk id) | `CSMenuMan → 80 → 180` quand tu es assis | |
| 16.5 | Menu de renforcement ouvert par TGA : affiche-t-il tes armes ? | TGA `Npc Menus > Misc > Blacksmith` | |
| 16.6 | Menu ouvert actuellement (type) | `… NpcMenuState current_open_menu` si trouvé | |

## 17. Grâces : redécouverte

| # | Test | Ta réponse |
|---|---|---|
| 17.1 | Découvre une grâce neuve : quels flags changent ? (TGA `Event Flags` → recherche des flags qui passent à 1) | |
| 17.2 | Y a-t-il un flag dans la plage `1xxxxxxxxx` (carte locale) qui change aussi ? | |
| 17.3 | Après reverrouillage (interrupteur EldenFree) + rechargement : la grâce redevient-elle « éteinte » ? | |

## 18. Pour de futures fonctions (si tu as le temps)

| # | Élément | Où | Ta réponse |
|---|---|---|---|
| 18.1 | Niveau et attributs (vigueur…arcane) | `PGD + 3C … 58`, niveau `+68` | |
| 18.2 | Endurance actuelle / max | `Modules → 0 → 154 / 158` | |
| 18.3 | PC actuels / max dans le module | `Modules → 0 → 148 / 14C` | |
| 18.4 | Résistances (poison, saignement…) | `PGD + 78 …` | |
| 18.5 | Points de la Fiole (nombre de charges) | TGA `Hero > EquipItemData` | |
| 18.6 | Météo / heure du jeu | TGA `WorldAreaTime` | |
| 18.7 | Position de la dernière grâce utilisée | TGA | |
