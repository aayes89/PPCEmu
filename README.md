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
* Kit de herramientas para textos y primitivas.
* Logs.

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


