Base de contrats Teleleinfo
--------


Vous trouvez ici la base de tous les contrats reconnus nativement par la dernière génération des firmware Teleinfo (à partir de la v16.2beta2).

Principe de fonctionnement est le suivant :

| Etape | ESP32 | ESP8266 4M+ | ESP8266 | Description |   
|  ---  |  ---  |    ---      |   ---   |     ---     |
|   1   |   x   |     x       |         |  Au démarrage, le firmware charge le contrat depuis **/contrat/_CODE_.json** |
|   2   |   x   |     x       |    x    |  Si contrat non trouvé et mode distributeur, chargement d'une version générique et enregistrement sous **/contrat/DISTRIBUTEUR.json** (si LittleFS) |
|   3   |   x   |             |         |  Si contrat non chargé et accès réseau est disponible, recherche depuis le repository Github et enregistrement sous **/contrat/_CODE_.json** |
|   4   |   x   |     x       |    x    |  Si contrat non chargé, recherche depuis la base embarquée par le firmware et enregistrement sous **/contrat/_CODE_.json** (si LittleFS) |
|   5   |   x   |     x       |    x    |  Si contrat non chargé, positionnement en recherche de contrat inconnu  |


En cas de contrat utilisant les index distributeurs (command **energyconfig distri=1** ), comme ces contrats sont totalement génériques, vous pouvez mettre à jour les données de vos périodes distributeur dans le fichier **/contrat/DISTRIBUTEUR.json**. Elles seront reconnues nativement au prochain démarrage.

En cas de **contrat inconnu**, un enregistrement sera réalisé sous **/contrat/_CODE_.json** à chaque nouvelle période détectée si l'ESP dispose d'un LittleFS.
Vous pouvez mettre à jour les caractéristiques des périodes dans le fichier JSON en fonction de votre contrat fournisseur. Elles seront reconnues nativement au prochain démarrage.

Pour les contrats inconnus, une fois vos différentes périodes détectées, merci d'ouvrir un ticket sur Github en fournissant le JSON et les données de base de votre contrat.
Cela permettra de mettre la base des contrats à jour et d'en faire profiter tous les utilisateurs.

Si un contrat inconnu vient d'être publié sur **Github**, il vous suffit de le supprimer sous **/contrat** et de redémarrer l'ESP. Il ira récupérer la version officielle lors de son redémarrage.
