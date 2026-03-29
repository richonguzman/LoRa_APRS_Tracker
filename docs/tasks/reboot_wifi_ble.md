# Mémoire et Coexistence (WiFi / BLE / Map) sur ESP32-S3

Ce document synthétise les analyses et les correctifs appliqués pour résoudre les crashs de type `Malloc failed` (suivi d'un `abort()` matériel) lors de l'utilisation simultanée ou séquentielle du WiFi, du Bluetooth Low Energy (NimBLE) et de l'écran de cartographie vectorielle (LovyanGFX / LVGL).

## Le problème initial

L'ESP32-S3 dispose d'environ 320 Ko de DRAM interne utilisable (SRAM1 et SRAM2).
La configuration matérielle et logicielle du T-Deck sollicite lourdement cette mémoire :
1. **Contrôleur Bluetooth (libbt.a)** : Nécessite une allocation de ~65 Ko de DRAM **contiguë** à son initialisation (buffers HCI, DMA).
2. **Pile WiFi** : Consomme entre 35 Ko et 50 Ko de DRAM lorsqu'elle est active.
3. **Affichage (LVGL & LovyanGFX)** : Alloue dynamiquement de la mémoire pour l'arborescence des widgets, les timers, et les tâches de fond.
4. **Cartographie Vectorielle (Map Z9+)** : 
   - L'ouverture de l'écran Map instancie des files d'attentes FreeRTOS (`Queue`).
   - Le passage en mode vectoriel (NAV) ouvre plusieurs fichiers (`File`) sur la carte SD. La librairie FATFS sous-jacente alloue un buffer de secteur (~600 octets) en DRAM pour chaque fichier ouvert.

**Le symptôme** : 
Après avoir utilisé le WiFi ou la carte, l'activation du Bluetooth échouait avec `BLE_INIT: Malloc failed` suivi de l'erreur HCI `519` (Memory Capacity Exceeded). 
Si le Bluetooth parvenait à démarrer, ouvrir la carte vectorielle réduisait la DRAM libre à moins de 1 Ko. À la première réception d'un paquet BLE (Gatt Write), le système s'effondrait (`abort()`) faute de mémoire pour allouer la structure de traitement.

## L'analyse de la fragmentation (Le "Gruyère")

L'ESP-IDF ne dispose pas de défragmenteur de RAM. Même avec 71 Ko de DRAM libre totale, le contrôleur BT échouait car il ne trouvait pas de **bloc contigu** suffisant (le plus grand bloc n'était que de 31 Ko).

La DRAM était fragmentée par plusieurs éléments :
- **Les piles (stacks) des tâches FreeRTOS** allouées au démarrage, placées en plein milieu de la DRAM.
- **La tâche principale Arduino (`loopTask`)**, configurée par défaut à 16 Ko, qui agissait comme un mur infranchissable.
- Les petites allocations C++ (`String`, objets LVGL) éparpillées.

## Les correctifs appliqués (Optimisation "Bottom-Up")

Une stratégie en plusieurs étapes a été déployée pour maximiser la DRAM contiguë disponible.

### 1. Fuite mémoire de l'écran LVGL
Lorsqu'on quittait l'écran de la carte, la fonction `lv_scr_load_anim` était appelée avec le paramètre `del = false`. L'écran (et ses objets) n'était jamais détruit, créant une fuite de ~7 Ko de DRAM à chaque passage.
- **Fix** : Passage du paramètre à `true` pour forcer la destruction de l'écran précédent et récupérer la DRAM (`src/map/map_input.cpp`).

### 2. Déplacement en PSRAM (MALLOC_CAP_SPIRAM)
Toutes les allocations qui n'ont pas strictement besoin de la DRAM (ex: accès DMA) ont été forcées en mémoire externe (PSRAM) :
- **Stacks des tâches Map** : Utilisation de `xTaskCreatePinnedToCoreWithCaps` pour allouer les piles de `MapRender` (16 Ko) et `TilePreload` (4 Ko) en PSRAM. (Gain: 20 Ko DRAM).
- **Objets LGFX_Sprite** : Bien que le buffer pixel soit en PSRAM (`setPsram(true)`), l'objet C++ lui-même était alloué en DRAM via `new`. Utilisation d'un placement `new` personnalisé (`psram_new`) via `heap_caps_malloc` pour stocker l'instance C++ complète en PSRAM. (Gain: ~9 Ko DRAM pour 18 sprites).
- **Files d'attente (Queues) FreeRTOS** : Remplacement des `xQueueCreate` (qui allouent le buffer en DRAM) par des `xQueueCreateStatic` couplés à des buffers alloués manuellement en PSRAM. (Gain: ~2 Ko DRAM).
- **Seuil d'allocation interne** : Réduction de `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` de 4096 à 256 octets dans `platformio.ini`. Force toutes les allocations C++ moyennes (> 256 octets) à utiliser la PSRAM au lieu d'encombrer la DRAM.

### 3. Coalescence par réduction de la tâche principale
Pour résoudre le problème critique de fragmentation (le plus grand bloc bloqué à 31 Ko), la taille de la pile de la tâche Arduino (`loopTask`) a été diminuée.
- **Fix** : Modification de `board_build.arduino.loop_stack_size` de `16384` à `6144` octets dans `platformio.ini`.
- **Résultat** : Le "mur" au milieu de la DRAM a rétréci de 10 Ko, permettant aux blocs libres adjacents de fusionner (coalescence). Le plus grand bloc contigu est passé de 31 Ko à **+ de 65 Ko**, permettant enfin au contrôleur BT de s'initialiser à chaud.

### 4. La solution finale : Pause/Resume du BLE pour la carte
Malgré toutes ces optimisations, le cumul des allocations incompressibles (Contrôleur BT + Stack NimBLE + LVGL + 5 buffers FATFS pour les tuiles NAV) laissait la DRAM sous le seuil critique des 4 Ko lors de la navigation sur la carte, provoquant des crashs lors des réceptions BLE.
Puisque l'utilisateur ne configure pas l'appareil en Bluetooth tout en regardant la carte vectorielle simultanément, une stratégie de bascule a été implémentée :
- **À l'ouverture de la carte** : Si le Bluetooth est actif, il est temporairement stoppé (`BLE_Utils::stop()`), libérant instantanément ~65 Ko de DRAM pour l'affichage cartographique.
- **À la fermeture de la carte** : Le Bluetooth est relancé automatiquement (`BLE_Utils::setup()`).
- **Fichiers modifiés** : `map_state.h`, `ui_dashboard.cpp`, `map_input.cpp`.

## Bilan
Le système est désormais stable. Il permet la cohabitation asynchrone du WiFi, du BLE et du rendu de cartes vectorielles complexes sur un ESP32-S3 sans saturer la DRAM ni nécessiter de redémarrage matériel.
