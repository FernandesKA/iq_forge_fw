# rk7020f

Исходники FPGA-конфигурации для платы rk7020f.

- `*.dts`  — device-tree overlay source, собирается в `.dtbo` через `scripts/dts-to-dtbo.sh`
- `*.bit`  — bitstream из Vivado, конвертируется в `.bin` через `scripts/bit-to-bin.sh`

Сгенерированные `.bin`/`.dtbo` в git не хранятся (см. корневой `.gitignore`),
собираются локально из `.dts`/`.bit` перед прошивкой:

```sh
../../scripts/bit-to-bin.sh design.bit
../../scripts/dts-to-dtbo.sh overlay.dts
```
