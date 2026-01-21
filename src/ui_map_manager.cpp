/* Logique de la carte pour T-Deck Plus
 * Affichage de tuiles de carte hors ligne avec stations utilisant LVGL
 */

#ifdef USE_LVGL_UI

#include <Arduino.h>
#include <FS.h>
#include <lvgl.h>
#include <TFT_eSPI.h> // Pour les définitions de TFT_eSPI si elles sont nécessaires (ex: pour SCREEN_WIDTH/HEIGHT)
#include <TinyGPS++.h>
#include <JPEGDEC.h>
// Undefine macros that conflict between PNGdec and JPEGDEC
#undef INTELSHORT
#undef INTELLONG
#undef MOTOSHORT
#undef MOTOLONG
#include <PNGdec.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ui_map_manager.h"
#include "configuration.h"
#include "station_utils.h"
#include "utils.h"
#include "storage_utils.h"
#include "custom_characters.h" // Pour symbolsAPRS, SYMBOL_WIDTH, SYMBOL_HEIGHT
#include "lvgl_ui.h" // Pour appeler LVGL_UI::open_compose_with_callsign

namespace UIMapManager {

    // Éléments d'interface utilisateur - Écran de carte
    lv_obj_t* screen_map = nullptr;
    lv_obj_t* map_canvas = nullptr;
    lv_color_t* map_canvas_buf = nullptr;
    lv_obj_t* map_title_label = nullptr;
    lv_obj_t* map_container = nullptr;

    // Variables d'état de la carte
    static const int map_available_zooms[] = {8, 10, 12, 14}; // Niveaux de zoom disponibles (seulement les niveaux avec tuiles sur carte SD)
    const int map_zoom_count = sizeof(map_available_zooms) / sizeof(map_available_zooms[0]);
    int map_zoom_index = 0;  // Index dans map_available_zooms (commence au zoom 8)
    int map_current_zoom = map_available_zooms[0]; // Initialiser avec le premier zoom disponible
    float map_center_lat = 0.0f;
    float map_center_lon = 0.0f;
    String map_current_region = "";
    bool map_follow_gps = true;  // Suivre le GPS ou mode de panoramique libre

    // Cache de tuiles en PSRAM
    #define TILE_CACHE_SIZE 12  // Nombre de tuiles à cacher (~1.5MB en PSRAM)
    #define TILE_DATA_SIZE (MAP_TILE_SIZE * MAP_TILE_SIZE * sizeof(uint16_t))  // 128KB par tuile

    struct CachedTile {
        int zoom;
        int tileX;
        int tileY;
        uint16_t* data;      // Pixels de tuiles décodés en PSRAM
        uint32_t lastAccess; // Pour l'éviction LRU
        bool valid;
    };

    static CachedTile tileCache[TILE_CACHE_SIZE];
    static uint32_t tileCacheAccessCounter = 0;
    static bool tileCacheInitialized = false;

    // Initialiser le cache de tuiles
    void initTileCache() {
        if (tileCacheInitialized) return;
        for (int i = 0; i < TILE_CACHE_SIZE; i++) {
            tileCache[i].data = nullptr;
            tileCache[i].valid = false;
            tileCache[i].zoom = -1;
            tileCache[i].tileX = -1;
            tileCache[i].tileY = -1;
            tileCache[i].lastAccess = 0;
        }
        tileCacheInitialized = true;
        Serial.println("[MAP] Cache de tuiles initialisé");
    }

    // Trouver une tuile en cache, retourne l'index ou -1
    int findCachedTile(int zoom, int tileX, int tileY) {
        for (int i = 0; i < TILE_CACHE_SIZE; i++) {
            if (tileCache[i].valid &&
                tileCache[i].zoom == zoom &&
                tileCache[i].tileX == tileX &&
                tileCache[i].tileY == tileY) {
                tileCache[i].lastAccess = ++tileCacheAccessCounter;
                return i;
            }
        }
        return -1;
    }

    // Trouver un emplacement pour une nouvelle tuile (vide ou LRU)
    int findCacheSlot() {
        // Chercher d'abord un emplacement vide
        for (int i = 0; i < TILE_CACHE_SIZE; i++) {
            if (!tileCache[i].valid || tileCache[i].data == nullptr) {
                return i;
            }
        }
        // Trouver le LRU (accès le plus ancien)
        int lruIndex = 0;
        uint32_t oldestAccess = tileCache[0].lastAccess;
        for (int i = 1; i < TILE_CACHE_SIZE; i++) {
            if (tileCache[i].lastAccess < oldestAccess) {
                oldestAccess = tileCache[i].lastAccess;
                lruIndex = i;
            }
        }
        return lruIndex;
    }

    // Décodeur JPEG pour les tuiles de carte
    static JPEGDEC jpeg;

    // Contexte pour le décodage JPEG vers le cache
    struct JPEGCacheContext {
        uint16_t* cacheBuffer;  // Tampon cible du cache
        int tileWidth;
    };

    static JPEGCacheContext jpegCacheContext;

    // Callback JPEGDEC pour le décodage vers le cache - appelé pour chaque bloc MCU
    static int jpegCacheCallback(JPEGDRAW* pDraw) {
        uint16_t* src = pDraw->pPixels;
        for (int y = 0; y < pDraw->iHeight; y++) {
            int destY = pDraw->y + y;
            if (destY >= MAP_TILE_SIZE) break;
            for (int x = 0; x < pDraw->iWidth; x++) {
                int destX = pDraw->x + x;
                if (destX >= MAP_TILE_SIZE) break;
                jpegCacheContext.cacheBuffer[destY * jpegCacheContext.tileWidth + destX] = src[y * pDraw->iWidth + x];
            }
        }
        return 1;
    }

    // Décodeur PNG pour les tuiles de carte
    static PNG png;

    // Contexte pour le décodage PNG vers le cache
    struct PNGCacheContext {
        uint16_t* cacheBuffer;  // Tampon cible du cache
        int tileWidth;
    };

    static PNGCacheContext pngCacheContext;

    // Callback PNGdec pour le décodage vers le cache
    static int pngCacheCallback(PNGDRAW* pDraw) {
        png.getLineAsRGB565(pDraw, &pngCacheContext.cacheBuffer[pDraw->y * pngCacheContext.tileWidth],
                            PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
        return 1;
    }

    // Callbacks de fichier PNG
    static void* pngOpenFile(const char* filename, int32_t* size) {
        File* file = new File(SD.open(filename, FILE_READ));
        if (!file || !*file) {
            delete file;
            return nullptr;
        }
        *size = file->size();
        return file;
    }

    static void pngCloseFile(void* handle) {
        File* file = (File*)handle;
        if (file) {
            file->close();
            delete file;
        }
    }

    static int32_t pngReadFile(PNGFILE* pFile, uint8_t* pBuf, int32_t iLen) {
        File* file = (File*)pFile->fHandle;
        return file->read(pBuf, iLen);
    }

    static int32_t pngSeekFile(PNGFILE* pFile, int32_t iPosition) {
        File* file = (File*)pFile->fHandle;
        return file->seek(iPosition);
    }

    // Callbacks de fichier JPEG
    static void* jpegOpenFile(const char* filename, int32_t* size) {
        File* file = new File(SD.open(filename, FILE_READ));
        if (!file || !*file) {
            delete file;
            return nullptr;
        }
        *size = file->size();
        return file;
    }

    static void jpegCloseFile(void* handle) {
        File* file = (File*)handle;
        if (file) {
            file->close();
            delete file;
        }
    }

    static int32_t jpegReadFile(JPEGFILE* pFile, uint8_t* pBuf, int32_t iLen) {
        File* file = (File*)pFile->fHandle;
        return file->read(pBuf, iLen);
    }

    static int32_t jpegSeekFile(JPEGFILE* pFile, int32_t iPosition) {
        File* file = (File*)pFile->fHandle;
        return file->seek(iPosition);
    }

    // Copier la tuile mise en cache sur le canevas avec décalage et découpage
    void copyTileToCanvas(uint16_t* tileData, lv_color_t* canvasBuffer,
                                 int offsetX, int offsetY, int canvasWidth, int canvasHeight) {
        for (int ty = 0; ty < MAP_TILE_SIZE; ty++) {
            int cy = offsetY + ty;
            if (cy < 0 || cy >= canvasHeight) continue;

            for (int tx = 0; tx < MAP_TILE_SIZE; tx++) {
                int cx = offsetX + tx;
                if (cx < 0 || cx >= canvasWidth) continue;

                int canvasIdx = cy * canvasWidth + cx;
                int tileIdx = ty * MAP_TILE_SIZE + tx;
                canvasBuffer[canvasIdx].full = tileData[tileIdx];
            }
        }
    }

    // Convertir lat/lon en coordonnées de tuile
    void latLonToTile(float lat, float lon, int zoom, int* tileX, int* tileY) {
        int n = 1 << zoom;
        *tileX = (int)((lon + 180.0f) / 360.0f * n);
        float latRad = lat * PI / 180.0f;
        *tileY = (int)((1.0f - log(tan(latRad) + 1.0f / cos(latRad)) / PI) / 2.0f * n);
    }

    // Convertir lat/lon en position de pixel sur l'écran (relativement au centre)
    void latLonToPixel(float lat, float lon, float centerLat, float centerLon, int zoom, int* pixelX, int* pixelY) {
        int centerTileX, centerTileY;
        latLonToTile(centerLat, centerLon, zoom, &centerTileX, &centerTileY);

        int targetTileX, targetTileY;
        latLonToTile(lat, lon, zoom, &targetTileX, &targetTileY);

        // Calculer la position de sous-tuile
        int n = 1 << zoom;
        float subX = ((lon + 180.0f) / 360.0f * n) - targetTileX;
        float subY = ((1.0f - log(tan(lat * PI / 180.0f) + 1.0f / cos(lat * PI / 180.0f)) / PI) / 2.0f * n) - targetTileY;

        float centerSubX = ((centerLon + 180.0f) / 360.0f * n) - centerTileX;
        float centerSubY = ((1.0f - log(tan(centerLat * PI / 180.0f) + 1.0f / cos(centerLat * PI / 180.0f)) / PI) / 2.0f * n) - centerTileY;

        *pixelX = MAP_CANVAS_WIDTH / 2 + (int)(((targetTileX - centerTileX) + (subX - centerSubX)) * MAP_TILE_SIZE);
        *pixelY = MAP_CANVAS_HEIGHT / 2 + (int)(((targetTileY - centerTileY) + (subY - centerSubY)) * MAP_TILE_SIZE);
    }

    // Obtenir la couleur standard APRS pour le symbole
    lv_color_t getAPRSSymbolColor(const char* symbol) {
        if (!symbol || strlen(symbol) < 1) return lv_color_hex(0xffff00);  // Jaune par défaut

        // Le symbole peut être un seul caractère "[" ou avec une table "/["
        char symbolChar;
        if (strlen(symbol) >= 2 && (symbol[0] == '/' || symbol[0] == '\\')) {
            symbolChar = symbol[1];
        } else {
            symbolChar = symbol[0];
        }

        switch (symbolChar) {
            case '[':  // Humain/Joggeur
            case 'b':  // Vélo
                return lv_color_hex(0xff0000);  // Rouge
            case '>':  // Voiture
            case 'U':  // Bus
            case 'j':  // Jeep
            case 'k':  // Camion
            case '<':  // Moto
                return lv_color_hex(0x0000ff);  // Bleu
            case 's':  // Navire/bateau
            case 'Y':  // Yacht
                return lv_color_hex(0x00ffff);  // Cyan
            case '-':  // Maison
            case 'y':  // Maison avec yagi
                return lv_color_hex(0x00ff00);  // Vert
            case 'a':  // Ambulance
            case 'f':  // Camion de pompiers
            case 'u':  // Caserne de pompiers
                return lv_color_hex(0xff6600);  // Orange
            case '^':  // Avion
            case '\'': // Petit avion
            case 'X':  // Hélicoptère
                return lv_color_hex(0x00ffff);  // Cyan
            case '&':  // iGate
                return lv_color_hex(0x800080);  // Violet
            default:
                return lv_color_hex(0xffff00);  // Jaune
        }
    }

    // Dessiner le symbole APRS sur la carte à la position spécifiée
    void drawMapSymbol(lv_obj_t* canvas, int x, int y, const char* symbolChar, lv_color_t color) {
        // Trouver l'index du symbole
        int symbolIndex = -1;
        for (int i = 0; i < symbolArraySize; i++) {
            if (strcmp(symbolChar, symbolArray[i]) == 0) {
                symbolIndex = i;
                break;
            }
        }

        if (symbolIndex < 0) {
            // Symbole inconnu - dessiner un cercle
            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color = color;
            rect_dsc.radius = 4;
            lv_canvas_draw_rect(canvas, x - 4, y - 4, 8, 8, &rect_dsc);
            return;
        }

        const uint8_t* bitMap = symbolsAPRS[symbolIndex];

        // Dessiner le bitmap centré sur x,y
        int startX = x - SYMBOL_WIDTH / 2;
        int startY = y - SYMBOL_HEIGHT / 2;

        for (int sy = 0; sy < SYMBOL_HEIGHT; sy++) {
            for (int sx = 0; sx < SYMBOL_WIDTH; sx++) {
                int px = startX + sx;
                int py = startY + sy;
                if (px >= 0 && px < MAP_CANVAS_WIDTH && py >= 0 && py < MAP_CANVAS_HEIGHT) {
                    int byteIndex = (sy * ((SYMBOL_WIDTH + 7) / 8)) + (sx / 8);
                    int bitIndex = 7 - (sx % 8);
                    if (bitMap[byteIndex] & (1 << bitIndex)) {
                        lv_canvas_set_px_color(canvas, px, py, color);
                    }
                }
            }
        }
    }

    // Suivi du clic sur la station de carte - stocke l'indicatif pour pré-remplir l'écran de composition
    static String map_prefill_callsign = "";

    // Gestionnaire de clic sur la station - ouvre l'écran de composition avec l'indicatif pré-rempli
    void map_station_clicked(lv_event_t* e) {
        int stationIndex = (int)(intptr_t)lv_event_get_user_data(e);
        MapStation* station = STATION_Utils::getMapStation(stationIndex);

        if (station && station->valid && station->callsign.length() > 0) {
            Serial.printf("[MAP] Station cliquée : %s\n", station->callsign.c_str());
            map_prefill_callsign = station->callsign;
            LVGL_UI::open_compose_with_callsign(station->callsign); // Appeler la fonction publique
        }
    }

    // Gestionnaire du bouton Retour de la carte
    void btn_map_back_clicked(lv_event_t* e) {
        Serial.println("[LVGL] Bouton RETOUR MAP appuyé");
        map_follow_gps = true;  // Réinitialiser pour suivre le GPS en quittant la carte
        // Le parent de l'écran de carte est l'écran principal (main_screen), il faut le récupérer
        lv_scr_load_anim(lv_obj_get_parent(lv_obj_get_parent(screen_map)), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    }

    // Gestionnaire du bouton de recentrage de la carte - revient à la position GPS
    void btn_map_recenter_clicked(lv_event_t* e) {
        Serial.println("[MAP] Recentrage sur GPS");
        map_follow_gps = true;
        if (gps.location.isValid()) {
            map_center_lat = gps.location.lat();
            map_center_lon = gps.location.lng();
            Serial.printf("[MAP] Recentré sur GPS : %.4f, %.4f\n", map_center_lat, map_center_lon);
        } else {
            // Pas de GPS - revenir à la position par défaut de l'Ariège
            map_center_lat = 42.9667f;
            map_center_lon = 1.6053f;
            Serial.printf("[MAP] Pas de GPS, recentré sur la position par défaut : %.4f, %.4f\n", map_center_lat, map_center_lon);
        }
        schedule_map_reload();
    }

    // Redessiner uniquement le contenu du canevas sans recréer l'écran (pour le zoom)
    void redraw_map_canvas() {
        Serial.println("[MAP-DEBUG] redraw_map_canvas DÉBUT");

        if (!map_canvas || !map_canvas_buf || !map_title_label) {
            Serial.println("[MAP-DEBUG] Canevas ou titre non initialisé, rechargement complet");
            screen_map = nullptr; // Forcer la recréation
            create_map_screen();
            lv_disp_load_scr(screen_map);
            return;
        }

        // Mettre à jour le titre avec le nouveau niveau de zoom
        char title_text[32];
        snprintf(title_text, sizeof(title_text), "CARTE (Z%d)", map_current_zoom);
        lv_label_set_text(map_title_label, title_text);
        Serial.printf("[MAP-DEBUG] Titre mis à jour : %s\n", title_text);

        // Effacer le canevas avec un fond gris ardoise foncé
        lv_canvas_fill_bg(map_canvas, lv_color_hex(0x2F4F4F), LV_OPA_COVER);

        // Recalculer les positions des tuiles
        int centerTileX, centerTileY;
        latLonToTile(map_center_lat, map_center_lon, map_current_zoom, &centerTileX, &centerTileY);

        int n = 1 << map_current_zoom;
        float tileXf = (map_center_lon + 180.0f) / 360.0f * n;
        float latRad = map_center_lat * PI / 180.0f;
        float tileYf = (1.0f - log(tan(latRad) + 1.0f / cos(latRad)) / PI) / 2.0f * n;

        float fracX = tileXf - centerTileX;
        float fracY = tileYf - centerTileY;
        int subTileOffsetX = (int)(fracX * MAP_TILE_SIZE);
        int subTileOffsetY = (int)(fracY * MAP_TILE_SIZE);

        Serial.printf("[MAP] Tuile centrale : %d/%d, décalage de sous-tuile : %d,%d\n", centerTileX, centerTileY, subTileOffsetX, subTileOffsetY);

        // Charger les tuiles
        bool hasTiles = false;
        if (STORAGE_Utils::isSDAvailable()) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int tileX = centerTileX + dx;
                    int tileY = centerTileY + dy;
                    int offsetX = MAP_CANVAS_WIDTH / 2 - subTileOffsetX + dx * MAP_TILE_SIZE;
                    int offsetY = MAP_CANVAS_HEIGHT / 2 - subTileOffsetY + dy * MAP_TILE_SIZE;

                    if (dx == 0 && dy == 0) {
                        Serial.printf("[MAP] Décalage de la tuile centrale : %d,%d\n", offsetX, offsetY);
                    }

                    if (loadTileFromSD(tileX, tileY, map_current_zoom, map_canvas, offsetX, offsetY)) {
                        hasTiles = true;
                    }
                }
            }
        }

        if (!hasTiles) {
            lv_draw_label_dsc_t label_dsc;
            lv_draw_label_dsc_init(&label_dsc);
            label_dsc.color = lv_color_hex(0xaaaaaa);
            label_dsc.font = &lv_font_montserrat_14;
            lv_canvas_draw_text(map_canvas, 40, MAP_CANVAS_HEIGHT / 2 - 30, 240, &label_dsc,
                "Aucune tuile hors ligne disponible.");
        }

        // Dessiner la propre position
        if (gps.location.isValid()) {
            int myX, myY;
            latLonToPixel(gps.location.lat(), gps.location.lng(),
                          map_center_lat, map_center_lon, map_current_zoom, &myX, &myY);
            if (myX >= 0 && myX < MAP_CANVAS_WIDTH && myY >= 0 && myY < MAP_CANVAS_HEIGHT) {
                Beacon* currentBeacon = &Config.beacons[myBeaconsIndex];
                drawMapSymbol(map_canvas, myX, myY, currentBeacon->symbol.c_str(), getAPRSSymbolColor(currentBeacon->symbol.c_str()));
            }
        }

        // Forcer la mise à jour du canevas
        lv_obj_invalidate(map_canvas);
        Serial.println("[MAP-DEBUG] redraw_map_canvas FIN");
    }

    // Callback du timer pour recharger l'écran de la carte (pour le panoramique/recentrage)
    void map_reload_timer_cb(lv_timer_t* timer) {
        Serial.println("[MAP-DEBUG] Callback du timer DÉBUT");
        lv_timer_del(timer);
        redraw_map_canvas(); // Seul le canevas est redessiné, pas besoin de recréer screen_map
        Serial.println("[MAP-DEBUG] Callback du timer FIN");
    }

    // Fonction d'aide pour planifier le rechargement de la carte avec un délai
    void schedule_map_reload() {
        Serial.println("[MAP-DEBUG] Planification du timer de rechargement (20ms)");
        lv_timer_t* t = lv_timer_create(map_reload_timer_cb, 20, NULL);
        lv_timer_set_repeat_count(t, 1);
        Serial.println("[MAP-DEBUG] Timer créé");
    }

    // Gestionnaire de zoom avant de la carte
    void btn_map_zoomin_clicked(lv_event_t* e) {
        Serial.println("[MAP-DEBUG] === GESTIONNAIRE DE ZOOM AVANT APPELÉ ===");
        if (map_zoom_index < map_zoom_count - 1) {
            map_zoom_index++;
            map_current_zoom = map_available_zooms[map_zoom_index];
            Serial.printf("[MAP] Zoom avant : %d\n", map_current_zoom);
            redraw_map_canvas();  // Redessiner uniquement le canevas, ne pas recréer l'écran
        } else {
            Serial.println("[MAP-DEBUG] Déjà au zoom maximum");
        }
        Serial.println("[MAP-DEBUG] === GESTIONNAIRE DE ZOOM AVANT TERMINÉ ===");
    }

    // Gestionnaire de zoom arrière de la carte
    void btn_map_zoomout_clicked(lv_event_t* e) {
        Serial.println("[MAP-DEBUG] === GESTIONNAIRE DE ZOOM ARRIÈRE APPELÉ ===");
        if (map_zoom_index > 0) {
            map_zoom_index--;
            map_current_zoom = map_available_zooms[map_zoom_index];
            Serial.printf("[MAP] Zoom arrière : %d\n", map_current_zoom);
            redraw_map_canvas();  // Redessiner uniquement le canevas, ne pas recréer l'écran
        } else {
            Serial.println("[MAP-DEBUG] Déjà au zoom minimum");
        }
        Serial.println("[MAP-DEBUG] === GESTIONNAIRE DE ZOOM ARRIÈRE TERMINÉ ===");
    }

    // Calculer l'étape de panoramique en fonction du niveau de zoom (pixels en degrés)
    float getMapPanStep() {
        int n = 1 << map_current_zoom;
        // Déplacer d'environ 50 pixels à la valeur de zoom actuelle
        return 50.0f / MAP_TILE_SIZE / n * 360.0f;
    }

    // Gestionnaires de panoramique de la carte
    void btn_map_up_clicked(lv_event_t* e) {
        map_follow_gps = false;
        float step = getMapPanStep();
        map_center_lat += step;
        Serial.printf("[MAP] Panoramique haut : %.4f, %.4f\n", map_center_lat, map_center_lon);
        schedule_map_reload();
    }

    void btn_map_down_clicked(lv_event_t* e) {
        map_follow_gps = false;
        float step = getMapPanStep();
        map_center_lat -= step;
        Serial.printf("[MAP] Panoramique bas : %.4f, %.4f\n", map_center_lat, map_center_lon);
        schedule_map_reload();
    }

    void btn_map_left_clicked(lv_event_t* e) {
        map_follow_gps = false;
        float step = getMapPanStep();
        map_center_lon -= step;
        Serial.printf("[MAP] Panoramique gauche : %.4f, %.4f\n", map_center_lat, map_center_lon);
        schedule_map_reload();
    }

    void btn_map_right_clicked(lv_event_t* e) {
        map_follow_gps = false;
        float step = getMapPanStep();
        map_center_lon += step;
        Serial.printf("[MAP] Panoramique droit : %.4f, %.4f\n", map_center_lat, map_center_lon);
        schedule_map_reload();
    }

    // Charger une tuile depuis la carte SD (avec mise en cache) et la copier sur le canevas
    bool loadTileFromSD(int tileX, int tileY, int zoom, lv_obj_t* canvas, int offsetX, int offsetY) {
        // Initialiser le cache lors de la première utilisation
        initTileCache();

        // Obtenir le tampon du canevas
        lv_img_dsc_t* dsc = lv_canvas_get_img(canvas);
        lv_color_t* canvasBuffer = (lv_color_t*)dsc->data;

        // Vérifier d'abord le cache
        int cacheIdx = findCachedTile(zoom, tileX, tileY);
        if (cacheIdx >= 0) {
            // Cache hit ! Copier du cache vers le canevas
            Serial.printf("[MAP] Cache hit : %d/%d/%d\n", zoom, tileX, tileY);
            copyTileToCanvas(tileCache[cacheIdx].data, canvasBuffer, offsetX, offsetY,
                             MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT);
            return true;
        }

        // Cache miss - besoin de charger depuis la carte SD
        bool success = false;

        // Protéger l'accès au bus SPI avec le mutex
        if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY) == pdTRUE) {

            if (STORAGE_Utils::isSDAvailable()) {
                String mapsPath = STORAGE_Utils::getMapsPath();
                std::vector<String> regions = STORAGE_Utils::listDirs(mapsPath);

                for (const String& region : regions) {
                    char tilePath[128];

                    // Essayer d'abord JPEG (priorité - décodage plus rapide)
                    snprintf(tilePath, sizeof(tilePath), "%s/%s/%d/%d/%d.jpg",
                             mapsPath.c_str(), region.c_str(), zoom, tileX, tileY);

                    if (STORAGE_Utils::fileExists(tilePath)) {
                        Serial.printf("[MAP] Chargement de la tuile JPEG : %s\n", tilePath);

                        int slot = findCacheSlot();
                        if (tileCache[slot].data == nullptr) {
                            tileCache[slot].data = (uint16_t*)heap_caps_malloc(TILE_DATA_SIZE, MALLOC_CAP_SPIRAM);
                        }

                        if (tileCache[slot].data) {
                            jpegCacheContext.cacheBuffer = tileCache[slot].data;
                            jpegCacheContext.tileWidth = MAP_TILE_SIZE;

                            int rc = jpeg.open(tilePath, jpegOpenFile, jpegCloseFile, jpegReadFile, jpegSeekFile, jpegCacheCallback);
                            if (rc) {
                                jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
                                rc = jpeg.decode(0, 0, 0);
                                jpeg.close();

                                if (rc) {
                                    tileCache[slot].zoom = zoom;
                                    tileCache[slot].tileX = tileX;
                                    tileCache[slot].tileY = tileY;
                                    tileCache[slot].lastAccess = ++tileCacheAccessCounter;
                                    tileCache[slot].valid = true;
                                    copyTileToCanvas(tileCache[slot].data, canvasBuffer, offsetX, offsetY, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT);
                                    success = true;
                                }
                            }
                        }
                    }

                    // Essayer PNG en dernier recours si JPEG non trouvé
                    if (!success) {
                        snprintf(tilePath, sizeof(tilePath), "%s/%s/%d/%d/%d.png",
                                 mapsPath.c_str(), region.c_str(), zoom, tileX, tileY);

                        if (STORAGE_Utils::fileExists(tilePath)) {
                            Serial.printf("[MAP] Chargement de la tuile PNG : %s\n", tilePath);

                            int slot = findCacheSlot();
                            if (tileCache[slot].data == nullptr) {
                                tileCache[slot].data = (uint16_t*)heap_caps_malloc(TILE_DATA_SIZE, MALLOC_CAP_SPIRAM);
                            }

                            if (tileCache[slot].data) {
                                pngCacheContext.cacheBuffer = tileCache[slot].data;
                                pngCacheContext.tileWidth = MAP_TILE_SIZE;

                                int rc = png.open(tilePath, pngOpenFile, pngCloseFile, pngReadFile, pngSeekFile, pngCacheCallback);
                                if (rc == PNG_SUCCESS) {
                                    rc = png.decode(nullptr, 0);
                                    png.close();

                                    if (rc == PNG_SUCCESS) {
                                        tileCache[slot].zoom = zoom;
                                        tileCache[slot].tileX = tileX;
                                        tileCache[slot].tileY = tileY;
                                        tileCache[slot].lastAccess = ++tileCacheAccessCounter;
                                        tileCache[slot].valid = true;
                                        copyTileToCanvas(tileCache[slot].data, canvasBuffer, offsetX, offsetY, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT);
                                        success = true;
                                    }
                                } else {
                                    png.close();
                                }
                            }
                        }
                    }

                    if (success) break; // Si une tuile est trouvée dans une région, arrêter la recherche
                }
            }
            xSemaphoreGiveRecursive(spiMutex); // Relâcher le mutex SPI
        }
        return success;
    }

    // Créer l'écran de la carte
    void create_map_screen() {
        screen_map = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_map, lv_color_hex(0x1a1a2e), 0);

        // Utiliser la position GPS actuelle comme centre si le mode de suivi est activé
        if (map_follow_gps && gps.location.isValid()) {
            map_center_lat = gps.location.lat();
            map_center_lon = gps.location.lng();
            Serial.printf("[MAP] Utilisation de la position GPS : %.4f, %.4f\n", map_center_lat, map_center_lon);
        } else if (map_center_lat == 0.0f && map_center_lon == 0.0f) {
            // Par défaut sur l'Ariège (Foix) si pas de GPS - correspond aux tuiles OCC
            map_center_lat = 42.9667f;
            map_center_lon = 1.6053f;
            Serial.printf("[MAP] Pas de GPS, utilisation de la position par défaut de l'Ariège : %.4f, %.4f\n", map_center_lat, map_center_lon);
        } else {
            Serial.printf("[MAP] Utilisation de la position de panoramique : %.4f, %.4f\n", map_center_lat, map_center_lon);
        }

        // Barre de titre (verte pour la carte)
        lv_obj_t* title_bar = lv_obj_create(screen_map);
        lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x009933), 0);
        lv_obj_set_style_border_width(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, 0);
        lv_obj_set_style_pad_all(title_bar, 5, 0);

        // Bouton Retour
        lv_obj_t* btn_back = lv_btn_create(title_bar);
        lv_obj_set_size(btn_back, 60, 25);
        lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
        lv_obj_add_event_cb(btn_back, btn_map_back_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_back = lv_label_create(btn_back);
        lv_label_set_text(lbl_back, "< RETOUR");
        lv_obj_center(lbl_back);

        // Titre avec niveau de zoom (garder la référence pour les mises à jour)
        map_title_label = lv_label_create(title_bar);
        char title_text[32];
        snprintf(title_text, sizeof(title_text), "CARTE (Z%d)", map_current_zoom);
        lv_label_set_text(map_title_label, title_text);
        lv_obj_set_style_text_color(map_title_label, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(map_title_label, &lv_font_montserrat_18, 0);
        lv_obj_align(map_title_label, LV_ALIGN_CENTER, -30, 0);

        // Boutons de zoom à droite
        lv_obj_t* btn_zoomin = lv_btn_create(title_bar);
        lv_obj_set_size(btn_zoomin, 30, 25);
        lv_obj_set_style_bg_color(btn_zoomin, lv_color_hex(0x16213e), 0);
        lv_obj_align(btn_zoomin, LV_ALIGN_RIGHT_MID, -70, 0);
        lv_obj_add_event_cb(btn_zoomin, btn_map_zoomin_clicked, LV_EVENT_RELEASED, NULL);
        lv_obj_t* lbl_zoomin = lv_label_create(btn_zoomin);
        lv_label_set_text(lbl_zoomin, "+");
        lv_obj_center(lbl_zoomin);

        lv_obj_t* btn_zoomout = lv_btn_create(title_bar);
        lv_obj_set_size(btn_zoomout, 30, 25);
        lv_obj_set_style_bg_color(btn_zoomout, lv_color_hex(0x16213e), 0);
        lv_obj_align(btn_zoomout, LV_ALIGN_RIGHT_MID, -35, 0);
        lv_obj_add_event_cb(btn_zoomout, btn_map_zoomout_clicked, LV_EVENT_RELEASED, NULL);
        lv_obj_t* lbl_zoomout = lv_label_create(btn_zoomout);
        lv_label_set_text(lbl_zoomout, "-");
        lv_obj_center(lbl_zoomout);

        // Bouton de recentrage (icône GPS) - affiche une couleur différente lorsque le GPS n'est pas suivi
        lv_obj_t* btn_recenter = lv_btn_create(title_bar);
        lv_obj_set_size(btn_recenter, 30, 25);
        lv_obj_set_style_bg_color(btn_recenter, map_follow_gps ? lv_color_hex(0x16213e) : lv_color_hex(0xff6600), 0);
        lv_obj_align(btn_recenter, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(btn_recenter, btn_map_recenter_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_recenter = lv_label_create(btn_recenter);
        lv_label_set_text(lbl_recenter, LV_SYMBOL_GPS);
        lv_obj_center(lbl_recenter);

        // Zone du canevas de la carte
        map_container = lv_obj_create(screen_map);
        lv_obj_set_size(map_container, SCREEN_WIDTH, MAP_CANVAS_HEIGHT);
        lv_obj_set_pos(map_container, 0, 35);
        lv_obj_set_style_bg_color(map_container, lv_color_hex(0x2F4F4F), 0);  // Gris ardoise foncé
        lv_obj_set_style_border_width(map_container, 0, 0);
        lv_obj_set_style_radius(map_container, 0, 0);
        lv_obj_set_style_pad_all(map_container, 0, 0);

        // Créer un canevas pour le dessin de la carte
        // Libérer l'ancien tampon s'il existe (prévention des fuites de mémoire)
        if (map_canvas_buf) {
            heap_caps_free(map_canvas_buf);
            map_canvas_buf = nullptr;
        }
        map_canvas_buf = (lv_color_t*)heap_caps_malloc(MAP_CANVAS_WIDTH * MAP_CANVAS_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
        if (map_canvas_buf) {
            map_canvas = lv_canvas_create(map_container);
            lv_canvas_set_buffer(map_canvas, map_canvas_buf, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
            lv_obj_set_pos(map_canvas, 0, 0);

            // Remplir avec la couleur de fond
            lv_canvas_fill_bg(map_canvas, lv_color_hex(0x2F4F4F), LV_OPA_COVER);

            // Calculer la tuile centrale et la position fractionnaire dans la tuile
            int centerTileX, centerTileY;
            latLonToTile(map_center_lat, map_center_lon, map_current_zoom, &centerTileX, &centerTileY);

            // Calculer le décalage de sous-tuile (où se trouve notre point central dans la tuile)
            int n = 1 << map_current_zoom;
            float tileXf = (map_center_lon + 180.0f) / 360.0f * n;
            float latRad = map_center_lat * PI / 180.0f;
            float tileYf = (1.0f - log(tan(latRad) + 1.0f / cos(latRad)) / PI) / 2.0f * n;

            // Partie fractionnaire (0.0 à 1.0) représente la position dans la tuile
            float fracX = tileXf - centerTileX;
            float fracY = tileYf - centerTileY;

            // Convertir en décalage de pixels (combien de pixels décaler les tuiles)
            int subTileOffsetX = (int)(fracX * MAP_TILE_SIZE);
            int subTileOffsetY = (int)(fracY * MAP_TILE_SIZE);

            Serial.printf("[MAP] Tuile centrale : %d/%d, décalage de sous-tuile : %d,%d\n", centerTileX, centerTileY, subTileOffsetX, subTileOffsetY);

            // Essayer de charger les tuiles depuis la carte SD
            bool hasTiles = false;
            if (STORAGE_Utils::isSDAvailable()) {
                // Charger la tuile centrale et les tuiles environnantes (grille 3x3, ou plus si nécessaire)
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int tileX = centerTileX + dx;
                        int tileY = centerTileY + dy;
                        // Appliquer le décalage de sous-tuile pour que le point central soit au centre de l'écran
                        int offsetX = MAP_CANVAS_WIDTH / 2 - subTileOffsetX + dx * MAP_TILE_SIZE;
                        int offsetY = MAP_CANVAS_HEIGHT / 2 - subTileOffsetY + dy * MAP_TILE_SIZE;

                        if (dx == 0 && dy == 0) {
                            Serial.printf("[MAP] Décalage de la tuile centrale : %d,%d\n", offsetX, offsetY);
                        }

                        if (loadTileFromSD(tileX, tileY, map_current_zoom, map_canvas, offsetX, offsetY)) {
                            hasTiles = true;
                        }
                    }
                }
            }

            if (!hasTiles) {
                // Pas de tuiles - afficher un message
                lv_draw_label_dsc_t label_dsc;
                lv_draw_label_dsc_init(&label_dsc);
                label_dsc.color = lv_color_hex(0xaaaaaa);
                label_dsc.font = &lv_font_montserrat_14;
                lv_canvas_draw_text(map_canvas, 40, MAP_CANVAS_HEIGHT / 2 - 30, 240, &label_dsc,
                    "Aucune tuile hors ligne disponible.\nTéléchargez les tuiles OSM et copiez-les dans :\nSD:/LoRa_Tracker/Maps/REGION/z/x/y.png");
            }

            // Dessiner la propre position (si le GPS est valide)
            if (gps.location.isValid()) {
                int myX, myY;
                latLonToPixel(gps.location.lat(), gps.location.lng(),
                              map_center_lat, map_center_lon, map_current_zoom, &myX, &myY);

                if (myX >= 0 && myX < MAP_CANVAS_WIDTH && myY >= 0 && myY < MAP_CANVAS_HEIGHT) {
                    // Obtenir le symbole du balise actuelle
                    Beacon* currentBeacon = &Config.beacons[myBeaconsIndex];
                    drawMapSymbol(map_canvas, myX, myY, currentBeacon->symbol.c_str(), getAPRSSymbolColor(currentBeacon->symbol.c_str()));
                }
            }

            // Dessiner les stations reçues
            STATION_Utils::cleanOldMapStations();
            for (int i = 0; i < MAP_STATIONS_MAX; i++) {
                MapStation* station = STATION_Utils::getMapStation(i);
                if (station && station->valid && station->latitude != 0.0f && station->longitude != 0.0f) {
                    int stX, stY;
                    latLonToPixel(station->latitude, station->longitude,
                                  map_center_lat, map_center_lon, map_current_zoom, &stX, &stY);

                    if (stX >= 0 && stX < MAP_CANVAS_WIDTH && stY >= 0 && stY < MAP_CANVAS_HEIGHT) {
                        // Couleur basée sur la norme de symbole APRS
                        lv_color_t stationColor = getAPRSSymbolColor(station->symbol.c_str());

                        drawMapSymbol(map_canvas, stX, stY, station->symbol.c_str(), stationColor);

                        // Dessiner le label de l'indicatif avec le SSID complet
                        lv_draw_label_dsc_t lbl_dsc;
                        lv_draw_label_dsc_init(&lbl_dsc);
                        lbl_dsc.color = stationColor;
                        lbl_dsc.font = &lv_font_montserrat_14;
                        lv_canvas_draw_text(map_canvas, stX - 30, stY + 10, 80, &lbl_dsc, station->callsign.c_str());

                        // Créer un bouton cliquable invisible au-dessus de la station
                        lv_obj_t* btn_station = lv_btn_create(map_container);
                        lv_obj_set_size(btn_station, 50, 40);  // Zone de toucher autour de la station
                        lv_obj_set_pos(btn_station, stX - 25, stY - 15);  // Centrer sur la station
                        lv_obj_set_style_bg_opa(btn_station, LV_OPA_TRANSP, 0);  // Transparent
                        lv_obj_set_style_border_width(btn_station, 0, 0);
                        lv_obj_set_style_shadow_width(btn_station, 0, 0);
                        lv_obj_add_event_cb(btn_station, map_station_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
                    }
                }
            }

            // Forcer le redessin du canevas après les écritures directes dans le tampon
            lv_canvas_set_buffer(map_canvas, map_canvas_buf, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
            lv_obj_invalidate(map_canvas);
        }

        // Boutons fléchés pour le panoramique (coin inférieur gauche, disposition en D-pad)
        int arrow_size = 28;
        int arrow_x = 5;
        int arrow_y = MAP_CANVAS_HEIGHT - 105;  // Au-dessus de la barre d'information
        lv_color_t arrow_color = lv_color_hex(0x444444);

        // Bouton Haut
        lv_obj_t* btn_up = lv_btn_create(map_container);
        lv_obj_set_size(btn_up, arrow_size, arrow_size);
        lv_obj_set_pos(btn_up, arrow_x + arrow_size, arrow_y);
        lv_obj_set_style_bg_color(btn_up, arrow_color, 0);
        lv_obj_set_style_bg_opa(btn_up, LV_OPA_70, 0);
        lv_obj_add_event_cb(btn_up, btn_map_up_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_up = lv_label_create(btn_up);
        lv_label_set_text(lbl_up, LV_SYMBOL_UP);
        lv_obj_center(lbl_up);

        // Bouton Bas
        lv_obj_t* btn_down = lv_btn_create(map_container);
        lv_obj_set_size(btn_down, arrow_size, arrow_size);
        lv_obj_set_pos(btn_down, arrow_x + arrow_size, arrow_y + arrow_size * 2);
        lv_obj_set_style_bg_color(btn_down, arrow_color, 0);
        lv_obj_set_style_bg_opa(btn_down, LV_OPA_70, 0);
        lv_obj_add_event_cb(btn_down, btn_map_down_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_down = lv_label_create(btn_down);
        lv_label_set_text(lbl_down, LV_SYMBOL_DOWN);
        lv_obj_center(lbl_down);

        // Bouton Gauche
        lv_obj_t* btn_left = lv_btn_create(map_container);
        lv_obj_set_size(btn_left, arrow_size, arrow_size);
        lv_obj_set_pos(btn_left, arrow_x, arrow_y + arrow_size);
        lv_obj_set_style_bg_color(btn_left, arrow_color, 0);
        lv_obj_set_style_bg_opa(btn_left, LV_OPA_70, 0);
        lv_obj_add_event_cb(btn_left, btn_map_left_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_left = lv_label_create(btn_left);
        lv_label_set_text(lbl_left, LV_SYMBOL_LEFT);
        lv_obj_center(lbl_left);

        // Bouton Droit
        lv_obj_t* btn_right = lv_btn_create(map_container);
        lv_obj_set_size(btn_right, arrow_size, arrow_size);
        lv_obj_set_pos(btn_right, arrow_x + arrow_size * 2, arrow_y + arrow_size);
        lv_obj_set_style_bg_color(btn_right, arrow_color, 0);
        lv_obj_set_style_bg_opa(btn_right, LV_OPA_70, 0);
        lv_obj_add_event_cb(btn_right, btn_map_right_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_right = lv_label_create(btn_right);
        lv_label_set_text(lbl_right, LV_SYMBOL_RIGHT);
        lv_obj_center(lbl_right);

        // Barre d'information en bas
        lv_obj_t* info_bar = lv_obj_create(screen_map);
        lv_obj_set_size(info_bar, SCREEN_WIDTH, 25);
        lv_obj_set_pos(info_bar, 0, SCREEN_HEIGHT - 25);
        lv_obj_set_style_bg_color(info_bar, lv_color_hex(0x16213e), 0);
        lv_obj_set_style_border_width(info_bar, 0, 0);
        lv_obj_set_style_radius(info_bar, 0, 0);
        lv_obj_set_style_pad_all(info_bar, 2, 0);

        // Afficher les coordonnées
        lv_obj_t* lbl_coords = lv_label_create(info_bar);
        char coords_text[64];
        snprintf(coords_text, sizeof(coords_text), "Centre : %.4f, %.4f  Stations : %d",
                 map_center_lat, map_center_lon, mapStationsCount);
        lv_label_set_text(lbl_coords, coords_text);
        lv_obj_set_style_text_color(lbl_coords, lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_style_text_font(lbl_coords, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl_coords);

        Serial.println("[LVGL] Écran de carte créé");
    }

} // namespace UIMapManager

#endif // USE_LVGL_UI
