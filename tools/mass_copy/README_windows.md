# copy_to_sd.ps1 - Windows SD Card Copy Tool

[Français](#fr) | [English](#en)

---

<a id="fr"></a>
## Français

Script PowerShell pour copier les tiles NAV sur carte SD sous Windows, avec **anti-fragmentation** (écriture séquentielle triée Z/X/Y). Équivalent Windows de `rsync_copy.sh`.

### Prérequis

- Windows 10 / 11
- PowerShell 5.1+ (inclus dans Windows)
- Carte SD montée avec une lettre de lecteur (ex: `E:`)

> Si PowerShell bloque l'exécution, lancer une fois :
> ```powershell
> Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
> ```

### Utilisation

```powershell
.\copy_to_sd.ps1 -Source <dossier_tiles> -Destination <lettre:> [-DestSubdir <sous-dossier>] [-Mode incremental|full]
```

| Paramètre | Obligatoire | Description |
|---|---|---|
| `-Source` | oui | Dossier contenant les tiles générées |
| `-Destination` | oui | Lettre de la carte SD (ex: `E:`) |
| `-DestSubdir` | non | Sous-dossier sur la SD (défaut : nom du dossier source) |
| `-Mode` | non | `incremental` (défaut) ou `full` |

### Exemples

```powershell
# Sync rapide (copie uniquement les fichiers modifiés)
.\copy_to_sd.ps1 -Source .\output -Destination E: -DestSubdir LoRa_Tracker\VectMaps

# Copie complète (efface + recopie triée, zéro fragmentation)
.\copy_to_sd.ps1 -Source .\output -Destination E: -DestSubdir LoRa_Tracker\VectMaps -Mode full
```

### Modes

| Mode | Comportement | Quand l'utiliser |
|---|---|---|
| **incremental** | Compare taille + date, copie uniquement les fichiers différents | Développement, mises à jour fréquentes |
| **full** | Supprime le dossier destination puis recopie tout dans l'ordre trié | Avant un déploiement terrain ou quand la SD est fragmentée |

### Fonctionnement

1. **Vérification espace** : compare la taille source avec l'espace libre sur la SD
2. **Suppression** (mode full uniquement) : efface le dossier destination
3. **Tri Z/X/Y** : les fichiers sont triés par zoom, puis X, puis Y pour une écriture séquentielle
4. **Création des dossiers** : pré-création de l'arborescence
5. **Copie** : fichier par fichier dans l'ordre trié, avec barre de progression
6. **Vérification** : comptage des fichiers + hash MD5 sur 10 fichiers aléatoires

### Formatage recommandé de la carte SD

Pour des performances optimales avec IceNav, formater en FAT32 avec des clusters de 8 Ko :

1. Ouvrir un terminal administrateur
2. ```
   diskpart
   list disk
   select disk X        # numéro de la carte SD
   clean
   create partition primary
   format fs=fat32 allocation=8192 label="ICENAV" quick
   assign
   exit
   ```

> Utiliser le mode **full** après un formatage frais pour une SD parfaitement ordonnée.

---

<a id="en"></a>
## English

PowerShell script to copy NAV tiles to an SD card on Windows, with **anti-fragmentation** (sequential write in Z/X/Y order). Windows equivalent of `rsync_copy.sh`.

### Requirements

- Windows 10 / 11
- PowerShell 5.1+ (included with Windows)
- SD card mounted with a drive letter (e.g. `E:`)

> If PowerShell blocks execution, run once:
> ```powershell
> Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
> ```

### Usage

```powershell
.\copy_to_sd.ps1 -Source <tiles_dir> -Destination <letter:> [-DestSubdir <subfolder>] [-Mode incremental|full]
```

| Parameter | Required | Description |
|---|---|---|
| `-Source` | yes | Directory containing the generated tiles |
| `-Destination` | yes | SD card drive letter (e.g. `E:`) |
| `-DestSubdir` | no | Subfolder on the SD (default: source directory name) |
| `-Mode` | no | `incremental` (default) or `full` |

### Examples

```powershell
# Quick sync (copies only changed files)
.\copy_to_sd.ps1 -Source .\output -Destination E: -DestSubdir LoRa_Tracker\VectMaps

# Full copy (wipe + sorted recopy, zero fragmentation)
.\copy_to_sd.ps1 -Source .\output -Destination E: -DestSubdir LoRa_Tracker\VectMaps -Mode full
```

### Modes

| Mode | Behavior | When to use |
|---|---|---|
| **incremental** | Compares size + date, copies only different files | Development, frequent updates |
| **full** | Deletes the destination folder then recopies everything in sorted order | Before field deployment or when the SD is fragmented |

### How it works

1. **Space check**: compares source size with free space on the SD
2. **Wipe** (full mode only): deletes the destination folder
3. **Z/X/Y sort**: files are sorted by zoom, then X, then Y for sequential writing
4. **Directory creation**: pre-creates the folder tree
5. **Copy**: file by file in sorted order, with progress bar
6. **Verification**: file count check + MD5 hash on 10 random files

### Recommended SD card formatting

For optimal performance with IceNav, format as FAT32 with 8 KB clusters:

1. Open an administrator terminal
2. ```
   diskpart
   list disk
   select disk X        # SD card number
   clean
   create partition primary
   format fs=fat32 allocation=8192 label="ICENAV" quick
   assign
   exit
   ```

> Use **full** mode after a fresh format for a perfectly ordered SD card.
