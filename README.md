# PPCEmu
Emulador de PowerPC escrito en C++ con Visual Studio 2022 para Windows.

# ¿Porqué?
<p>
Este proyecto empezó como inspiración de otro que se encuentra en mi repositorio <a href="https://github.com/aayes89/CHIP8Emu">CHIP8Emu</a>, el cual, por su simplicidad, me hizo pensar si sería posible hacer algo más desafiante: un emulador para la arquitectura PowerPC basado en Xenon.<br>
 Actualmente existen programas cuyo objetivo es actuar como máquinas virtuales para diversas arquitecturas y hardware:<br>
 <b>Pearpc, Qemu, VirtualBox, VMWare</b><br><br>
 Ninguno de ellos es capaz de emular el comportamiento de la consola Xbox 360 como máquina virtual.<br>
 Emuladores como <a href="#">Cxbox</a> y <a href="https://github.com/xenia-project/xenia">Xenia</a>, están destinados sólo a procesar los juegos directamente y el proyecto <a href="https://github.com/hedge-dev/XenonRecomp">XenonRecomp</a> a convertirlos en ejecutables para x86.<br>
 Razones suficientes para darle una oportunidad a esta magnífica creación que tantos dolores de cabeza ha traído a muchos y que ahora regresa debido al afán de no pocos, con el surgimiento de <a href="https://github.com/grimdoomer/Xbox360BadUpdate">BadUpdate de Grimdoomer</a>.
</p>

# Objetivos

* Emular el comportamiento de un hardware basado en PPC.
* Emular el comportamiento de la consola Xbox 360.
* Minimalista.
* Migrar a otras plataformas: <b>Android</b> {seguro} e <b>iOS</b> {improbable?} 

# Implementado

* Memoria virtual.
* Conjunto de Instrucciones para PPC.
* Cargar: <b>elf32, elf64</b> y <b>bin (RAW)</b>
* <b>Framebuffer</b> con WinAPI.
* Logs.
* Kit de herramientas para textos y primitivas:
```
 void PutChar(char c);
 void BlitChar(int x, int y, char c, uint32_t color);
 void BlitText(int x, int y, const std::string& text, uint32_t color);
 void BlitTextFromMemory(uint32_t addr, size_t max_len, int x, int y, uint32_t color);
 void Clear(uint32_t color = 0);
 void Draw1bppBitmap(uint8_t* src, uint32_t width, uint32_t height, uint32_t fb_base, uint32_t pitch, uint32_t fg_color, uint32_t bg_color);
 void ScrollUp(int lines);
 void ScrollDown(int lines);
 void DrawRect(int x, int y, int w, int h, uint32_t color);
 void FillRect(int x, int y, int w, int h, uint32_t color);
 void DrawLine(int x0, int y0, int x1, int y1, uint32_t color);
 void DrawCircle(int cx, int cy, int r, uint32_t color);
 void FillCircle(int cx, int cy, int r, uint32_t color);
 void DrawSquare(int x, int y, int size, uint32_t color);
 void FillSquare(int x, int y, int size, uint32_t color);
 void FillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
```

# Pendientes y Mejoras

* Cargar binarios: <b>xex</b>
* Emular el comportamiento de la Xbox 360 desde el arranque.
* Implementar manejo de SoC (más robusto)
* Emular estructuras críticas de la consola. (<b>1BL, NAND, Dashboard, CD/DVD, HDD, ...</b>)
* Optimizar código para reducir a lo indispensable (sanitizar)

# Capturas

![imagen](https://github.com/user-attachments/assets/17b4352c-9b19-4979-a74d-8cf9c637c068)

# Agradecimientos

* Comunidad del proyecto <a href="https://github.com/xenon-emu/xenon/tree/main">Xenon (2025)</a> y en particular <a href="https://github.com/bitsh1ft3r">bitsh1ft3r</a>
* A mi esposa (por su enorme paciencia)
* A mi gato (siempre presente, incluso cuando no lo llamaban)

# Descargo de responsabilida

<b>Este programa no tiene carácter comercial.<br>
 Su uso queda reservado exclusivamente para estudio personal.<br></b>
 <h3>NO soy responsable del mal empleo que pueda darle, por acción directa o indirecta.</h3>


