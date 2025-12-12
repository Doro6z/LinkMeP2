# LinkMe - Analyse Stratégique & Roadmap (Révisée)

## Changement de Cap : Priorité à la Robustesse
Suite à la "Beta Lite", la décision est prise de **consolider la technique** avant d'ajouter du "Juice". Le ressenti physique (visuel et mécanique) doit être parfait avant de gamifier.

---

## Phase 1 : Stabilisation Technique (Maintenant)
*Objectif : Une corde qui a l'air vraie et qui ne glitch pas.*

### 1. Rendu Visuel & Tension
*   **Problème** : La corde peut sembler molle ou déconnectée visuellement.
*   **État (Build 6.1)** : 
    *   [x] **Architecture XPBD** : Séparation complète Visuel/Physique.
    *   [x] **Anti-Pop** : Implémentation Logic "Segment-Based" pour éliminer le jitter.
    *   [ ] **Reste à faire** : Influence Gravitationnelle sur les "PinPoints" pour plus de "Juice".

### 2. Sockets & Attachement
*   **Problème** : Le point d'attache (Hook) et le point de départ (Main/Torse) flottent parfois.
*   **Solution** :
    *   Fixer rigoureusement les Sockets sur le `HookActor` et le `CharacterMesh`.
    *   S'assurer que le premier segment de corde part *exactement* du canon/main.

### 3. Robustesse du Wrapping (LOS Check)
*   **Problème** : L'algorithme actuel de détection de "Unwrap" peut être instable (faux positifs).
*   **Solution** :
    *   Remplacer le check actuel par un **Line Of Sight (LOS) robuste**.
    *   Vérifier si "Dernier Pin" <-> "Joueur" est libre. Si oui -> Unwrap.
    *   Gérer les cas limites (coins très aigus, surfaces fines).

---

## Phase 2 : Gameplay Loop (Ensuite)
*   Checkpoints.
*   Win/Fail Conditions.

## Phase 3 : Juice (Plus tard)
*   Sons, VFX, Camera Shake.
