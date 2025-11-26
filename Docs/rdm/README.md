# Dossier rdm (tests rapides du système de corde)

Ce dossier regroupe une scène de test minimale côté code : `ARdmRopeTestCharacter` (C++).
Il sert uniquement à valider rapidement le système de corde sans setup de contrôles ou de gameplay supplémentaire.

## Utilisation
1. Compilez le projet.
2. Dans l'éditeur, placez un `RdmRopeTestCharacter` dans n'importe quelle carte.
3. Positionnez-le face à une surface : au *BeginPlay*, il tirera automatiquement le grappin dans la direction `InitialFireDirection` (par défaut `(1,0,0.2)`).
4. Ajustez l'angle ou désactivez `FireOnBeginPlay` via les propriétés `Rope|Test` pour des essais manuels.

> Aucun asset Blueprint ou contenu additionnel n'est requis : tout est côté code pour rester léger et jetable.
