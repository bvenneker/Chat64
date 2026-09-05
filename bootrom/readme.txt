If you are using an 8K EEPROM, such as the Atmel 28C64, use the 8K file. The bootloader will reside at address 0x0000 in the EEPROM.

For a 16K EEPROM, like theAtmel 28C128, use the 16K file. In this case, the bootloader is located at address 0x2000.

For a 32K EEPROM, like theAtmel 28C256, use the 32K file. In this case, the bootloader is located at address 0x6000.

When using a 64K EEPROM, such as the Winbond W27C512, use the 64K file, the bootloader resides at address 0xE000.
