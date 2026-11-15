# example_8051 — Keil C51 example

Target: **STC89C52RC / AT89C52**, 11.0592 MHz, UART mode 1 at **9600 8N1**,
Timer0 interrupt every 1 ms.

## Build

1. Keil µVision → *Project → New µVision Project*, device **AT89C52**
   (or STC89C52RC if your pack provides it).
2. Add files to the project:
   - `example_8051/main.c`
   - `Src-C/ElegantDebug_8051.c`
3. *Options for Target → C51* → **Include Paths**: add `..\Src-C`.
4. *Options for Target → Output*: check **Create HEX File**.
5. *Options for Target → Target*: check **Use On-chip ROM**, and make sure the
   RAM size is 256 bytes (AT89C52 default). BL51 must be invoked with
   `RAMSIZE(256)`; µVision does this automatically for that device.
6. Build (F7). Expected:

   ```
   0 Error(s), 0 Warning(s)
   Program Size: data=47.0  xdata=0  code=2038
   ```

7. Flash `Objects\<project>.hex` with STC-ISP / a programmer, power-cycle the
   board (cold start).

## Terminal

```
9600 baud, 8 data bits, no parity, 1 stop bit
```

Use a terminal that renders ANSI escapes (MobaXterm, Xshell, VS Code serial
monitor) to see the colors; a plain ASCII terminal such as STC-ISP's built-in
assistant will show the escape sequences as `←[94m` style text.

Expected output:

```
[00:00:00.000] [INFO] ElegantDebug 8051 port, 9600 8N1
[00:00:00.104] [OK] UART ready
[00:00:00.208] timestamp only, no type prefix
[00:00:00.312] [SUCCESS] ANSI colors are produced by the terminal
[00:00:00.416] [WARNING] [main.c:69] warning with file:line
[00:00:00.520] [ERROR] [main.c:70] error with file:line
[00:00:00.624] counter = 42
[00:00:01.144] tick(ms) = 1000
[00:00:01.664] tick(ms) = 1500
...
```
