# EldenFree — valeurs à vérifier en jeu

Outil : Cheat Engine 7.5+ avec la table **The Grand Archives** (TGA), jeu lancé **hors ligne** via `EldenFree.exe`.
Pour chaque point : fais le test et note le résultat (valeur ou oui/non). Envoie aussi `EldenFree.log` et `EldenFree.ini`
(dans `%LOCALAPPDATA%\EldenFree`).

## 0. Infos générales

| # | À relever | Où |
|---|---|---|
| 0.1 | Version du jeu | clic droit `eldenring.exe` → Propriétés → Détails → Version du produit |
| 0.2 | DLC installé ? | oui / non |
| 0.3 | Manette : Xbox via USB, Bluetooth, ou via Steam Input ? | |

## 1. Menu de renforcement vide

| # | Test | Résultat attendu |
|---|---|---|
| 1.1 | TGA → `Scripts > Npc Menus > Misc > Blacksmith` (cocher) | le menu s'ouvre : **tes armes sont-elles listées ?** oui / non |
| 1.2 | TGA → `Scripts > Npc Menus > Misc > Smithing Table` | armes listées ? oui / non |
| 1.3 | Dans EldenFree, onglet Menus → « Renforcer une arme » puis « Table de forge » | armes listées ? oui / non |

Si TGA affiche les armes et pas EldenFree → la différence vient de la façon d'appeler le menu, j'adapterai.

## 2. Arôme de sang bouillant

| # | À relever | Où |
|---|---|---|
| 2.1 | Nom exact de l'objet en français (copie-le tel qu'il apparaît dans l'inventaire) | inventaire du jeu |
| 2.2 | ID de l'objet (ex. `0x40000xxx`) | TGA → `Scripts > Build Creation > ItemGib` → liste déroulante, cherche l'arôme : la valeur affichée est l'ID |
| 2.3 | Quantité max en inventaire | description de l'objet / TGA ItemGib |

## 3. Talisman « Arsenal de la Jarre géante » (charge d'équipement)

| # | À relever | Où |
|---|---|---|
| 3.1 | Nom exact en français du talisman qui augmente la charge (Great-Jar's Arsenal) | inventaire / wiki |
| 3.2 | Ligne du journal `Charge : talisman …, speffect …, taux d'origine …` | onglet Diagnostic d'EldenFree |
| 3.3 | Avec le multiplicateur à x5 : la charge max affichée dans l'écran Statut change-t-elle ? et la roulade ? | jeu |

## 4. Vitesse de déplacement pendant les animations accélérées (cendres, fléau)

Valeurs utilisées par EldenFree (chaîne TGA : `WorldChrMan → 10EF8 → 0 → 190 → …`) :

| Champ | Chaîne d'offsets | Valeur actuelle dans le mod |
|---|---|---|
| Vitesse d'animation | `… → 190 → 28 → 17C8` (float) | confirmée (TGA `Hero > Animation > Animation Speed`) |
| Position | `… → 190 → 68 → 70` (x, y, z float) | confirmée |
| Demande de mise à jour physique | `… → 190 → 68 → 91` (byte) | **à confirmer** : passe-t-il à 1 quand tu te téléportes avec TGA ? |
| Multiplicateur de mouvement | `… → 190 → 68 → 1C4` (float) | **à confirmer** : que vaut-il en marchant / en attaquant ? |

| # | Test | Résultat |
|---|---|---|
| 4.1 | TGA `Hero > Animation > Animation Speed` mis à 3, marche normale : le perso va-t-il 3x plus vite ? | oui / non |
| 4.2 | Même test en mettant aussi `…68 → 1C4` à 0.33 : la marche redevient-elle normale ? | oui / non |
| 4.3 | Nom de la/les cendre(s) de guerre qui sont accélérées alors que ce sont des attaques | |

## 5. Torrent

| # | À relever | Où |
|---|---|---|
| 5.1 | Valeur `FallVelocityField` | `EldenFree.ini`, section `[Torrent]` |
| 5.2 | Valeur `Speed` (multiplicateur de vitesse) quand le vol va trop vite | `EldenFree.ini` |
| 5.3 | En vol : la vitesse est-elle trop rapide dès le décollage, ou elle augmente avec le temps ? | |
| 5.4 | Saut infini : après une longue chute + saut, Torrent remonte-t-il ? puis le saut suivant marche-t-il ? | |

## 6. Référence : offsets et signatures utilisés par EldenFree

| Élément | Valeur | Source |
|---|---|---|
| GameDataMan | AOB `48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3` | TGA, Erd-Tools |
| WorldChrMan | AOB `48 8B 05 ?? ?? ?? ?? 48 85 C0 74 0F 48 39 88` | Erd-Tools |
| CSEventFlagMan | AOB `48 8B 3D ?? ?? ?? ?? 48 85 FF ?? ?? 32 C0 E9` | Erd-Tools-CPP |
| SoloParamRepository | AOB `48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 84 ?? ?? ?? ?? 45 33 C0 BA 90` | Erd-Tools-CPP |
| MsgRepository | AOB `48 8B 3D ?? ?? ?? ?? 44 0F B6 30 48 85 FF 75` | TGA |
| ItemGib | AOB `8B 02 83 F8 0A` − 0x52 | TGA |
| MapItemMan | AOB `48 8B 0D ?? ?? ?? ?? C7 44 24 50 FF FF FF FF` / singleton `MapItemMan` | TGA |
| EzState (menus) | AOB `48 8B DA 4C 8B F1 33 FF 89 7D` − 0x47 | TGA |
| Téléportation | AOB `C3 ?? ?? ?? ?? ?? ?? 57 48 83 EC ?? 48 8B FA 44` + 2 | TGA |
| SpEffect ajout / retrait | `0F 28 0D ?? ?? ?? ?? ?? 8D ?? ?? 0F 29 ?? ?? ?? 0F B6 D8` − 0x1D / `48 83 EC 28 8B C2 48 8B 51 08 48 85 D2 ?? ?? 90` | TGA |
| Joueur | `WorldChrMan + 1E508` | fromsoftware-rs |
| Runes | `GameDataMan → 08 → 6C` | TGA |
| PC | `GameDataMan → 08 → 1C` | fromsoftware-rs |
| Nombre de morts | `GameDataMan → 94` | fromsoftware-rs |
| Charge max (affichage) | `GameDataMan → 08 → 98C` | fromsoftware-rs |
| Arme main droite | `GameDataMan → 08 → 5FC + 8 × emplacement (GameDataMan → 08 → 32C)` | TGA |
| Flags perso (NoDead/NoDamage/NoFP/NoStamina) | `… → 190 → 0 → 19B` bits 0/1/2/3 | TGA |
| Chute : minuteur | `… → 190 → 70 → 18` | fromsoftware-rs |
| Monture : monté | `… → 190 → E8 → 163` | fromsoftware-rs |
| Torrent | ChrSet `WorldChrMan + 10F90`, chr id 8000 | fromsoftware-rs |
| Caméra | `WorldChrMan + 1ECF0` → matrice `+10` | fromsoftware-rs |
| Grâces / carte / cloches / Greyoll | listes de flags | TGA Event Flag Manager |
