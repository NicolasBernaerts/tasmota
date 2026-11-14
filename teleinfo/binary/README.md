Vous trouvez dans cette section les firmware pré-compilés du projet Teleinfo.

Je ne publie classiquement que les versions considérées comme stable. Certaines version Beta sont publiées afin de permettre de tester de nouvelles fonctionnalités.

Tout comme pour le Tasmota officiel, il est conseillé de ne réaliser une mise à jour que si c'est strictement nécessaire.

Chaque binaire permet de programmer une famille d'ESP spécifique :

|         Firmware           |      Target      | LittleFS |           Notice                |
| -------------------------- | ---------------- | -------- | ------------------------------- |
| tasmota-teleinfo           | **ESP8266 1Mb**  |   none   |                                 |
| tasmota-teleinfo-4m        | **ESP8266 4Mb**  |   2 Mb   |                                 |
| tasmota-teleinfo-16m       | **ESP8266 16Mb** |   14 Mb  |                                 |
| tasmota32-teleinfo         | **ESP32 4Mb**    | 1.34 Mb  | To use with Ethernet boards     |
| tasmota32-teleinfo-denkyd4 | **DenkyD4**      |          |                                 |
| tasmota32c3-teleinfo       | **ESP32C3 4Mb**  | 1.34 Mb  | Sonoff basic R4                 |
| tasmota32c6-teleinfo-winky | **Winky**        | 1.34 Mb  | Integrates deepsleep management |
| tasmota32s2-teleinfo       | **ESP32S2 4Mb**  | 1.34 Mb  |                                 |
| tasmota32s3-teleinfo       | **ESP32S3 4Mb**  | 1.34 Mb  |                                 |
| tasmota32s3-teleinfo-16m   | **ESP32S3 16Mb** |   10 Mb  |                                 |

> [!WARNING]
> Tous les **ESP8266 4M+** et les **ESP32** utilisent une partition **LittleFS** maximisée à **1.3M**, au lieu des **320k** du firmware officiel.
> Cela signifie que vous devez faire systématiquement un premier flash en mode **Série** afin de regénérer le partitionnement et d'éviter tout dysfonctionnement.
> Si vous ne faites pas ce flash série, vous seerez face à des reboot intempestifs et inexpliqués !

Voici les images à utiliser en fonction du type d'ESP et du type de programmation :

|        Programmation       |       ESP8266      |         ESP32         |
| -------------------------- | ------------------ | --------------------- |
| Série / USB (esptool)      |   firmware.bin     |  firmware.factory.bin | 
| OTA                        |   firmware.bin.gz  |  firmware.bin         | 

Si vous utilisez les outils publiés dans le section **/tools** de ce repository, voici les commandes à passer :

**ESP8266**

    tasmota-flash --erase --flash 'tasmota-image-name.bin'

**ESP32**

    tasmota-flash --erase --esp32 --flash 'tasmota32-image-name.factory.bin'
