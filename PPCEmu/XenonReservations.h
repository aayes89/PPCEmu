// Copyright 2025 Xenon Emulator Project

#pragma once

#include <mutex>
#include <vector>
#include "Tipos.h"

struct PPU_RES {
  uint8_t ppuID;
  volatile bool V;
  volatile uint64_t resAddr;
};

class XenonReservations {
public:
  XenonReservations();
  virtual bool Register(PPU_RES *Res);
  void Increment(void) {
    std::lock_guard lck(ReservationLock);
    nReservations++;
  }
  void Decrement(void) {
    std::lock_guard lck(ReservationLock);
    nReservations--;
  }
  void Check(u64 x) {
    if (nReservations)
      Scan(x);
  }
  virtual void Scan(u64 PhysAddress);

  void AcquireLock(void) {
    ReservationLock.lock();
  }
  void ReleaseLock(void) {
    ReservationLock.unlock();
  }

private:
  long nReservations;
  std::recursive_mutex ReservationLock;
  int nProcessors;
  struct PPU_RES *Reservations[6];
};
