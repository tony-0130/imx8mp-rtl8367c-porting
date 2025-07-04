# Device Tree Configuration

This document provides detailed device tree configuration for integrating RTL8367C ethernet switch with IMX8MP platform

## Overview

The device tree configuration consists of two main components:
1. GPIO-based MDIO simulation
2. RTL8367C switch node configuration

## Hardware Pin Mapping
The following table are the pins related to the RTL8367C function.

| Function | IMX8MP Pin | RTL8367C Pin | GPIO |
| -------- | ---------- | ------------ | ---- |
| MDC   | ECSPI1_SCLK | Pin 92 (MDC)    | GPIO5_IO06 |
| MDIO  | ECSPI1_MOSI | Pin 93 (MDIO)   | GPIO5_IO07 |
| Reset | GPIO1_IO07  | Pin 91 (RESET#) | GPIO1_IO07 |

## Configuration Details

### 1. GPIO-based MDIO simulation
- We have no dedicated MDIO interface from IMX8MP to use, so we have to run mdc/mdio through regular GPIOs, Linux Kernel provides `mdio-gpio.c` driver to simulate MDIO interface using regular GPIOs.
- According to [mdio-gpio.txt](https://www.kernel.org/doc/Documentation/devicetree/bindings/net/mdio-gpio.txt), we need to add a `mdio` node and register it the `aliases` section.


```c

/ {
    /* add alias of mdio-gpio node */
    aliases {
        mdio-gpio0 = &mdio_gpio;
    };

    /* add mdio-gpio node compatible with "virtual,mdio-gpio" */
    mdio_gpio: mdio-gpio {
        compatible = "virtual,mdio-gpio";
        #address-cells = <1>;
        #size-cells = <0>;
        pinctrl-names = "default";
        pinctrl-0 = <&pinctrl_mdio_gpio>;

        gpios = <&gpio5 6 GPIO_ACTIVE_HIGH>, /* MDC */
                <&gpio5 7 GPIO_ACTIVE_HIGH>; /* MDIO */

        gpio-names = "mdc", "mdio";
    };
};

/* Pin function group of mdio-gpio */
&iomuxc {
    pinctrl_mdio_gpio: mdio_gpio_grp {
        fsl,pins = <
            MX8MP_IOMUXC_ECSPI1_SCLK__GPIO5_IO06        0x1c4    /* MDC */
            MX8MP_IOMUXC_ECSPI1_MOSI__GPIO5_IO07        0x1c4    /* MDIO */
        >;
    };
};

```

### 2. RTL8367C switch node configuration

- Add the property `mdio-bus` to link up between MDIO interface and RTL8367C ethernet switch, also the set up the property `reset-gpio` for the power sequence of the RTL8367C ethernet switch use.

```c

/ {
    /* add rtl8367c node for platform driver use */
    rtl8367c@0 {
        compatible = "realtek,rtl8367c";
        pinctrl-names = "default";
        pinctrl-0 = <&pinctrl_rtl8367c_rst>;
        reset-gpio = <&gpio1 7 GPIO_ACTIVE_LOW>;
        mdio-bus = <&mdio_gpio>;
        status = "okay";
    };
};

/* Pin function group of rtl8367c */
&iomuxc {
    pinctrl_rtl8367c_rst: rtl8367c_rst_grp {
        fsl,pins = <
            MX8MP_IOMUXC_GPIO1_IO07__GPIO1_IO07        0x1c4    /* RESET# */
        >;
    };
};

```
