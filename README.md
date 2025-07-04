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
└── driver/
└── (development in progress)
```

## Development Progress

- [x] Project planning and requirements analysis
- [x] Device tree configuration design
- [ ] Platform driver development
	- [ ] Basic driver structure
	- [ ] Driver initialization (MDIO interface & reset process)
	- [ ] Check communication between IMX8MP and RTL8367C
	- [ ] RTL8367C switch initialization
	- [ ] LED behavior control of each speed / activity
	- [ ] Debug file system control of test pattern transmission