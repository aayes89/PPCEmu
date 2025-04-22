/* Made by Slam */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "MemoryDevice.h"
#include <iostream>
#include <windows.h>
#include <vector>
#include <cstdint>

class Display : public MemoryDevice {
public:
    // name: identificador del dispositivo, base: dirección base del framebuffer
    Display(const std::string& name, uint64_t baseAddress, int width, int height);
    ~Display();

    // Implementación de MemoryDevice:

    uint8_t Read8(uint64_t a);
    uint16_t Read16(uint64_t a);
    uint32_t Read32(uint64_t a);
    uint64_t Read64(uint64_t a);

    void Write8(uint64_t a, uint8_t v);
    void Write16(uint64_t a, uint16_t v);
    void Write32(uint64_t a, uint32_t v);
    void Write64(uint64_t a, uint64_t v);

    void Read(uint64_t address, void* buffer, size_t size);
    void Write(uint64_t address, const void* buffer, size_t size);
    void MemSet(uint64_t address, uint8_t value, size_t size);   
    uint8_t* GetPointerToAddress(uint64_t address);

    // Ventana y dibujo nativo Win32
    bool ProcessMessages();  // true mientras no WM_QUIT
    void Present();          // actualiza ventana
    
    // Funciones IO para Framebuffer 
    void BlitChar(int x, int y, char c, uint32_t color);                    // generar caracteres
    void BlitText(int x, int y, const std::string& text, uint32_t color);   // Imprime caracteres
    void Clear(uint32_t color = 0x00000000);                                // Limpiar pantalla (default: negro)
    void DrawRect(int x, int y, int w, int h, uint32_t color);              // Dibujar rectangulo
    void FillRect(int x, int y, int w, int h, uint32_t color);              // Rellenar rectangulo
    void DrawLine(int x0, int y0, int x1, int y1, uint32_t color);          // Dibujar linea
    void DrawCircle(int cx, int cy, int r, uint32_t color);                 // Dibujar circulo
    void FillCircle(int cx, int cy, int r, uint32_t color);                 // Rellenar circulo
    void DrawSquare(int x, int y, int size, uint32_t color);                // Dibujar cuadrado
    void FillSquare(int x, int y, int size, uint32_t color);                // Rellenar cuadrado
    void FillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color); // Rellenar triangulo

private:
    HWND        hwnd_;
    HDC         hdcMem_;
    HBITMAP     hBmp_;
    BITMAPINFO  bmi_;
    std::vector<u8> pixels_; // BGRA
    uint64_t    base_;
    int         w_, h_;

    void createWindow();
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    
};
#endif 
