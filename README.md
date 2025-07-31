# Brotocol

*Brotocol* aka *Brock-protocol* aka *OneWire lil' bro-tocol* is a very slow OneWire-inspired communication protocol for loose timing constrained devices.

## Features

- One-wire bus with 1 master and multiple (theoritecally 2^32 slaves)
- Accomodates long distances over shitty wires
- Loose timing constraints -> very very low bitrate (1-100 bps)
- Addressing and fixed-size messages
- Unidirectional + ACK to start with (slave's answer could be added quite easily)

## Motivation

I wanted a single bus with as little hardware as possible where one master (in my case a Raspberry Pi) can send simple addressed signals to thirty slaves (in my case STM32 MCUs).

Existing protocols that didn't fit my requirements :
- SPI, UART, ... : No addressing capabilities, thus must multiply the wires (one or two per slave).
- I2C : Too fast, reliable only over short distance (~20cm) with good wires/traces (shield, same delay, ...).
- CAN, RS485 : Requires additionnal hardware, can't use simple GPIO's.
- OneWire : Seemed a good fit, but 1) the slave implementation is rarely implemented and quite tricky and most of all 2) for low-speed clocked MCU's that already perform many different tasks, getting the real time constraints (~65µs) right is a too harsh requirement for a software implementation. I ended up with a non-reliable communication only because the slave missed the protocol timings while computing something else.

Thus I got the idea of slowing down and simplifying the OneWire protocol, by implementing everything in software and having very loose timing constraints.

## Hardware

The bus is the same as for OneWire.
- One wire (+ ground) which is pulled-up by a single resistor.
- All devices can send a signal by pulling the bus low.

## Parameters

- `T` time unit, constrained by devices timing capabilities. default 10ms
- `A` # of address bits, max 32. default 5 for 32 slaves
- `D` # of data bits, max 32. default 1 for 2 signals

`frame duration = 10 * T * ( 2 + A + 1 + D + 1) = 1s`

`bitrate = frame duration / D = 1bps`


## Symbol protocol

- To start a frame, master pulls low for 18 time units then releases for 2 time units.
```
S : \_________________/-
```
- To send a bit, master pulls low for 2 time units, then either stays low (1) or releases (0) for 6 time units, then releases for 2 time units.
```
0 : \_/-------
1 : \_______/-
    |   |    |
  start |   end
      sample
```
- To receive an ACK, master pull low for 2 time units and releases, then slave keeps pulling low for 6 time units then releases for 2 time units.
```
K : \_______/-
    | | |
master| sample
    slave
```

## Frame protocol

- A frame is made of a Start symbol, A address bits, 1 ACK, D data bits, 1 ACK
```
S A1 A2 .. An K D1 D2 .. Dn K
```
- Address and data are sent LSB first.
- Only the slave with corresponding address acknowledges the address
- Only if the data is valid does the slave ackowledges the data

## Implementation

#### Master

Userspace C program [`brotocol-send`](master/brotocol-send.c) that sends a single message to a slave, implemented for a Raspberry Pi 4 using `pigpio` on GPIO 4 (internal pull-up).

Should be easily portable to other platforms by reimplementing the HAL functions.

#### Slave

Implemented for an STM32F072, should be easily portable by reimplementing the [HAL](slave/brotocol_slave_hal.c).
