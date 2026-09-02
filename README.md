
Status : Working
----

---
Dataflow:
ESP32 -> UART -> STM32 -> RADIO WAVES -> STM32 -> PARSE and DISPLAY on dashboard.



### Completed
- UART communication
- Prepare to send it via LORA module
- recieve on 2nd microcontroller
- Serial output on user's computer

### Left
- Network isolation
- spreading factor
- Graphical dashboard

### Pin Configurations
- Power to both stm32
- G16 ( RX ) - PA9
- G 17 ( TX ) - PA10 [Please try both combinations , i forgot which one was rx and which one was tx)
- Gnd
