# Mbed Modules

Mbed OS-based modules providing common functionalities and a standardized way of
structuring the firmware.

By using these, the programmer can focus solely on the application
particularities.

## Directory structure

```
|-- Module
|       Base class of all modules.
|
|-- examples
|       Usage examples.
|
|-- intfs
|   |   Interface modules.
|   |
|   |-- TCPFwUpdateModule
|   |      Remote firmware update interface modules.
|   |
|   |-- ctrl
|   |   |-- CtrlIntfModule
|   |   |       Base class of text-based control interface modules.
|   |   |
|   |   |-- TCPCtrlIntfModule
|   |   |       TCP text-based control interface module.
|   |   |
|   |   |-- UARTCtrlIntfModule
|   |   |       UART text-based control interface module.
|
|   |-- stream
|   |   |-- TCPStreamIntfModule
|   |   |       TCP binary stream interface module.
|
|-- scripts
|   |-- intfs
|   |   |-- TCPFwUpdateModule
|   |   |       Script for interfacing with TCPFwUpdateModule.
```

## Firmware architecture

<img width="2957" height="1705" alt="99c93e9bead91d08bb98c6517eb5f5886233c561e27c7db5d46662b1174b5989" src="https://github.com/user-attachments/assets/d8d3e4c9-8430-4c2a-b54a-5378ec7070be" />


`CtrlCoreModule` is the module responsible for receiving, parsing, acting on and
responding to requests (commands) coming from the interface modules.

## Modules' inheritance diagram

![Image](https://github.com/user-attachments/assets/c937321d-7b6b-4837-a500-573317237896)

## How to use it

Add this repository as a submodule of your application.

	git submodule add git@github.com:cnpem/mbed-os-mods.git


