#!/usr/bin/env bash

# go to script directory
cd $(dirname "${BASH_SOURCE[0]}")

# ESP32 DevKit (Rx)
cat ./teleinfo.bat            > init.bat
cat ./esp32/esp32_rx.bat     >> init.bat
zip -j -0 ./esp32/.esp32_rx.zip init.bat

# Wemos Teleinfo
cat ./teleinfo.bat                        > init.bat
cat ./esp32/esp32_wemos_teleinfo.bat     >> init.bat
zip -j -0 ./esp32/.esp32_wemos_teleinfo.zip init.bat

# Olimex POE + EthInfo
cat ./teleinfo.bat                            > init.bat
cat ./esp32/esp32_olimex_poe_ethinfo.bat     >> init.bat
zip -j -0 ./esp32/.esp32_olimex_poe_ethinfo.zip init.bat

# wESP32 (Rx)
cat ./teleinfo.bat                   > init.bat
cat ./esp32/esp32_wesp32_rx.bat     >> init.bat
zip -j -0 ./esp32/.esp32_wesp32_rx.zip init.bat

# WT32-ETH01 (Rx)
cat ./teleinfo.bat                       > init.bat
cat ./esp32/esp32_wt32_eth01_rx.bat     >> init.bat
zip -j -0 ./esp32/.esp32_wt32_eth01_rx.zip init.bat

# Denky D4
cat ./teleinfo.bat                             > init.bat
cat ./esp32_denkyd4/init.bat                  >> init.bat
zip -j -0 ./esp32_denkyd4/DenkyD4_V1.3a.autoconf init.bat

# Wemos C3 Teleinfo
cat ./teleinfo.bat                            > init.bat
cat ./esp32c3/esp32c3_wemos_teleinfo.bat     >> init.bat
zip -j -0 ./esp32c3/.esp32c3_wemos_teleinfo.zip init.bat

# Winky C3
cat ./teleinfo.bat                     > init.bat
cat ./esp32c3_winky/init.bat          >> init.bat
zip -j -0 ./esp32c3_winky/Winky.autoconf init.bat

# Winky C6
cat ./teleinfo.bat                     > init.bat
cat ./esp32c6_winky/init.bat          >> init.bat
zip -j -0 ./esp32c6_winky/Winky.autoconf init.bat

# Wemos S2 Teleinfo
cat ./teleinfo.bat                            > init.bat
cat ./esp32s2/esp32s2_wemos_teleinfo.bat     >> init.bat
zip -j -0 ./esp32s2/.esp32s2_wemos_teleinfo.zip init.bat

# ESP32S3 [GPIO44]
cat ./teleinfo.bat                    > init.bat
cat ./esp32s3/esp32s3_gpio44.bat     >> init.bat
zip -j -0 ./esp32s3/.esp32s3_gpio44.zip init.bat

# ESP32S3 W5500 [GPIO44]
cat ./teleinfo.bat                          > init.bat
cat ./esp32s3/esp32s3_w5500_gpio44.bat     >> init.bat
zip -j -0 ./esp32s3/.esp32s3_w5500_gpio44.zip init.bat

# Wemos S3 Teleinfo
cat ./teleinfo.bat                            > init.bat
cat ./esp32s3/esp32s3_wemos_teleinfo.bat     >> init.bat
zip -j -0 ./esp32s3/.esp32s3_wemos_teleinfo.zip init.bat

rm init.bat
