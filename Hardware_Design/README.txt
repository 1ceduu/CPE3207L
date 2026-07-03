HARDWARE DESIGN PACKAGE
Integrated Automated Feeding & Watering System with Centralized Data Monitoring
(Raspberry Pi 5 MODBUS master  +  3x ESP32 feeder nodes  +  LoRa/GSM gateway)

Sources: TimTissis.pdf (thesis manuscript) and hardware_connections_layout.md.

CONTENTS
  Hardware_Design_Reference.pdf   Single compiled reference (cover, 4 diagrams, pin tables, rules)
  01_system_block_diagram.svg     Top-level topology (editable vector)
  02_feeder_node_schematic.svg    ESP32 node wiring (editable vector)
  03_central_unit_schematic.svg   Raspberry Pi 5 wiring (editable vector)
  04_power_and_driver_detail.svg  Power rails, star ground, RS-485 term, MOSFET driver
  *.png                           High-res raster of each diagram
  feeder_node.net                 KiCad-format netlist (feeder node) - 29 parts, 35 nets
  central_unit.net                KiCad-format netlist (central unit)
  feeder_node.kicad_sch           Editable KiCad schematic drawing of the feeder node

HOW TO USE THE EDA FILES
  KiCad (schematic):  open feeder_node.kicad_sch directly in KiCad 7/8 Schematic Editor.
     It is a graphical schematic you can annotate and extend. (Symbols are drawn as boxes;
     add library symbols as you formalise it.)
  KiCad (PCB from netlist):  in Pcbnew use "File > Import > Netlist" (or Update PCB from
     Netlist) and point it at feeder_node.net to pull in components and ratsnest.
  Proteus:  ISIS/ARES can import a netlist. Use the netlist (feeder_node.net) as the
     connectivity source. NOTE: Proteus project files (.pdsprj/.dsn) are a closed binary
     format that cannot be generated outside Proteus, so a netlist is provided instead.

KEY DESIGN REMINDERS
  - 3.3V MCUs: resistor dividers on every 5V signal into a GPIO (MAX485 RO, JSN echo,
    MQ-135 AO, YF-S201 pulse).
  - DRV8825: 100uF across VMOT-GND; M0/M1/M2 + RST/SLP to 3.3V.
  - Each solenoid MOSFET: 220R gate, 10k pulldown, 1N4007 flyback.
  - RS-485: shielded twisted pair, 120R at the two extreme ends, shield to Earth at Pi end only.
  - Star ground at the 12V SMPS output. GSM on a dedicated 5V rail >= 2.5A.
