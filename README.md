# 串行Flash芯片的数据库

本项目设计一个串行Flash芯片（如SPI接口的Flash芯片）上的数据库

## 特性

* 每个数据都有校验码来确保数据的正确性；
* 数据库有一个统一的抽象层来管理所有类型或长度的数据；
* 底层Flash自动擦除，上层（数据库层）无需关心如何在写之前擦除；
* 使用损耗均衡的算法，延长Flash芯片的使用寿命；
* 跨平台，用纯C编写，支持Windows、Linux、单片机平台；
* 易于移植：封装底层硬件的读写函数；