# EldenFree

Mod Elden Ring avec un panneau en jeu (**F10** ou bouton EldenFree dans le menu Échap / de grâce). `EldenFree.exe` lance le jeu hors ligne et injecte le mod tout seul.

| Onglet | Fonction |
|---|---|
| Boss | Réapparition infinie de l'Ancien dragon Greyoll (flag `1050400800`) |
| Monde | Révéler la carte · sites de grâce · pierres de forge chez les Sœurs jumelles · carte/voyage rapide en combat · Torrent partout · voyage rapide depuis les mini-donjons |
| Menus | Renforcer une arme, table de forge, cendres de guerre, renaissance, apparence et tous les menus de site de grâce |
| Armes | Dupliquer une arme de l'inventaire (3 exemplaires max) |
| Torrent | Multiplicateurs marche/trot et galop (x1 à x3) · saut infini |
| Joueur | Aucun dégât de chute · cast rapide · garder ses runes à la mort |
| Diagnostic | État des signatures mémoire |

Télécharger le zip prêt à l'emploi : [EldenFree.zip](https://github.com/RK28A/Eldenfree/releases/latest/download/EldenFree.zip) (généré par le workflow `build` à chaque push).

## Compiler

```bash
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Références utilisées pour les offsets / flags

- [The Grand Archives – Elden Ring CT](https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA) : flags (grâces, carte, cloches, Greyoll), ItemGib, NoDead/NoDamage, vitesse d'animation, runes
- [fromsoftware-rs](https://github.com/vswarte/fromsoftware-rs) : structures ChrIns (modules chute, matériau, monture, physique), event flags, inventaire
- [Hexinton Elden Ring CT](https://github.com/Hexinton/eldenringcheatengine) : patchs carte en combat, Torrent partout, saut infini
- [Erd-Tools](https://github.com/Nordgaren/Erd-Tools) / [Erd-Tools-CPP](https://github.com/Nordgaren/Erd-Tools-CPP) : signatures AOB, SoloParamRepository
