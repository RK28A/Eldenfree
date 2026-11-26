# EldenFree

Mod Elden Ring (DLL pour Mod Engine 2 2.1.0.0 / Vortex) avec un panneau en jeu sur **F10**.

| Onglet | Fonction |
|---|---|
| Boss | Réapparition infinie de l'Ancien dragon Greyoll (flag `1050400800`) |
| Monde | Révéler la carte · débloquer les sites de grâce · débloquer les pierres de forge chez les Sœurs jumelles |
| Armes | Dupliquer une arme de l'inventaire (3 exemplaires max) |
| Torrent | Multiplicateurs marche/trot et galop (x1 à x3) |
| Joueur | Aucun dégât de chute · garder ses runes à la mort |
| Diagnostic | État des signatures mémoire |

Installation : voir `dist/LISEZMOI.txt`. Télécharger le zip prêt à l'emploi : [EldenFree.zip](https://github.com/RK28A/Eldenfree/releases/latest/download/EldenFree.zip) (généré par le workflow `build` à chaque push).

## Compiler

```bash
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
