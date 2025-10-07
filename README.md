# IMX8MP RTL8367C Ethernet Switch Porting
Porting RTL8367C Ethernet Switch to IMX8MP platform with custom Linux network driver implementation

## Technical Stack

- **Platform**: NXP IMX8MP
- **Switch IC**: Realtek RTL8367RBI-VH-CG (RTL8367C series)
- **Development**: Linux Kernel Driver, Device Tree, GPIO-MDIO

## Project Background & Requirements

- Porting RTL8367RBI-VH-CG (RTL8367C series) on IMX8MP platform
- Enable test pattern transmission from selected switch ports
- LED control functionality

## Technical Challenges

- Custom platform driver development for ethernet switch
- GPIO-based MDIO communication implementation (no dedicated MII interface)
- Integration with Linux network subsystem
- Implement debugfs node for test pattern transmission

## Project Structure
```
imx8mp-rtl8367c-porting/
├── README.md
├── docs/
│   └── device-tree-config.md
└── kernel/
    └── drivers/
        └── net/
            └── ethernet/
                ├── Kconfig
                ├── Makefile
                └── rtl8367/
                    ├── Kconfig
                    ├── Makefile
                    ├── Put_the_driver_source_code_here
                    ├── rtl8367c.c
                    └── dal/
                        ├── Makefile
                        └── rtl8367c/
                            └── Makefile
```

## Development Progress

- [x] Project planning and requirements analysis
- [x] Device tree configuration design
- [x] Platform driver development
	- [x] Basic driver structure
	- [x] Driver initialization (MDIO interface & reset process)
	- [x] Check communication between IMX8MP and RTL8367C
	- [x] RTL8367C switch initialization
	- [x] Enable UTP ports of RTL8367C 
	- [x] LED behavior control of each speed / activity
	- [x] Debug file system control of test pattern transmission
- [x] mdio-gpio support
    - [x] Upload the original code
    - [x] Add codes for better support rtl8367c
- [x] Set up Kconfig and Makefile
