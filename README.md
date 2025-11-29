# LinkMeProject

Developed with Unreal Engine 5

La nouvelle méthode est la suivante :
Lorsqu'un bendpoint est crée, on le met dans un array. Cela crée deux segments de cordes : Hook[Anchor] et bendpoint[Anchor] et bendpoint[Anchor] et character[Anchor]. Le segment Hook[Anchor] et bendpoint[Anchor] n'a plus besoin d'être calculé dans la rope entière, il devient un segment indépendant. 

La longueur totale de la corde du joueur est donc la somme des longueurs des segments de cordes.

Seul le segment Bendpoint.last[Anchor] et Character[Anchor] est calculé dynamiquement. 
On peut donc imaginer que le rope system instancie des rope render pour chaque segments de cordes. Visuellement ils sont connectés entre eux. On poura ensuite ajouter une constante gravitationnelle sur chaque segment.