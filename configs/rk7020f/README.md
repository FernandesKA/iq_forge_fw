# rk7020f

Исходники FPGA-конфигурации для платы rk7020f.

- `*.dts`  — device-tree overlay source, собирается в `.dtbo` через `scripts/dts-to-dtbo.sh`
- `*.bit`  — bitstream из Vivado, конвертируется в `.bin` через `scripts/bit-to-bin.sh`

Сгенерированные `.bin`/`.dtbo` в git не хранятся (см. корневой `.gitignore`),
собираются локально из `.dts`/`.bit` перед прошивкой:

```sh
../../scripts/bit-to-bin.sh zynq_rk7020_ps_wrapper.bit
../../scripts/dts-to-dtbo.sh rk7020f-ad9361-spi.dts
```

## rk7020f-ad9361-spi.dts

Оверлей, добавляющий AD9361 как обычный `spidev` на PS SPI0 (CS0) —
`/dev/spidev0.0` (проверено на железе, см. ниже), доступный из userspace
через `hal::spi_device` + `drivers::ad9361`. Не использует mainline
`adi,ad9361` IIO-драйвер — у этого проекта в PL нет `axi_ad9361`/DMA fabric
cores (только `dds_tx_chain_wrapper`, см. `zynq-rk7020f-iqforge.dts` в
`buildroot_custom`), так что полный ADI-драйвер с DMA-стримингом сюда
не встанет. Для варианта с полным `axi_ad9361` (FMCOMMS2/3, DMA RX/TX)
смотри `RK-ZYNQ7020-F-AD` в `buildroot_custom` — это другой HDL-проект
с другим PL.

Требует, чтобы базовое дерево платы (`zynq-rk7020f-iqforge.dtb`) уже
грузилось с `&spi0 { status = "okay"; ... }` — так и есть по умолчанию.

**`compatible = "rohm,dh2228fv"`, не `"spidev"`.** Ядерный драйвер
`spidev` (`drivers/spi/spidev.c`) намеренно отказывается биндиться к DT-ноде
с буквальным `compatible = "spidev"` — `spi_match_device()` матчит только
записи из собственного `of_device_id`, а "spidev" туда не входит (защита от
"spidev прямо в DT" антипаттерна). Без ошибок в dmesg, без варнингов — нода
просто никогда не получает `/dev/spidev*`. Проверено вживую на плате:
`bind` через sysfs падал с ENODEV именно из-за незаматчившегося
`driver_match_device()`, не из-за отсутствия устройства. `rohm,dh2228fv` —
один из принятых driver'ом placeholder-compatible, стандартная практика
для generic spidev-passthrough ноды.

## zynq_rk7020_ps_wrapper.bit

Bitstream из `/home/fka/dev_fpga/iq_forge` (hdl_project `iq_forge/zynq_rk7020_ps_wrapper`).
PL содержит только `dds_tx_chain_wrapper` — без AXI/регистрового интерфейса,
без DMA. Конвертируется в `.bin` перед загрузкой через fpga_manager.

## spi.json

Конфиг для `iq_forge_app vendor-id` (`hal::spi_config`) — путь к spidev-ноде
AD9361 и параметры транспорта. Подхватывается `deploy.sh` в архив
автоматически, если файл есть в этой папке; на плате `iq_forge_app` ищет
`./spi.json` в текущей директории по умолчанию (можно переопределить через
`--config <path>`, либо разово поверх json — через `--spi <path>`).

`device: /dev/spidev0.0` — **проверено на железе** (20.08.2026, плата
192.168.0.7): после `apply-overlay` появляется ровно `/dev/spidev0.0`,
`&spi0` — единственный активный SPI-мастер в базовом дереве, так что
это стабильно, а не совпадение. Если когда-нибудь конфигурация платы
изменится (появится второй активный SPI-мастер) — перепроверь:

```sh
./iq_forge_app apply-overlay rk7020f rk7020f.dtbo   # оверлей создаёт spidev-ноду
ls /dev/spidev*                                      # реальный путь на плате
```

`mode: 1` — это CPHA=1 (`spi-cpha` в `rk7020f-ad9361-spi.dts`), значение
подобрано по рабочему конфигу AD9361 на этом же carrier board в
`RK-ZYNQ7020-F-AD` (`buildroot_custom`). Если `hal::spi_device` форсирует
`SPI_IOC_WR_MODE` при каждом открытии — оверлейный `spi-cpha` без этого поля
в json был бы молча перезаписан на mode=0.
